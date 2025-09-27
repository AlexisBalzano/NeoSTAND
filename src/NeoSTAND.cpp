#include "NeoSTAND.h"
#include <numeric>
#include <chrono>
#include <httplib.h>

#include "Version.h"
#include "core/CompileCommands.h"
#include "core/TagFunctions.h"
#include "core/TagItems.h"

#ifdef DEV
#define LOG_DEBUG(loglevel, message) logger_->log(loglevel, message)
#else
#define LOG_DEBUG(loglevel, message) void(0)
#endif

using namespace stand;

NeoSTAND::NeoSTAND() : m_stop(false), controllerDataAPI_(nullptr) {};
NeoSTAND::~NeoSTAND() = default;

void NeoSTAND::Initialize(const PluginMetadata &metadata, CoreAPI *coreAPI, ClientInformation info)
{
    metadata_ = metadata;
    clientInfo_ = info;
    CoreAPI *lcoreAPI = coreAPI;
    aircraftAPI_ = &lcoreAPI->aircraft();
    airportAPI_ = &lcoreAPI->airport();
    chatAPI_ = &lcoreAPI->chat();
    flightplanAPI_ = &lcoreAPI->flightplan();
    fsdAPI_ = &lcoreAPI->fsd();
    controllerDataAPI_ = &lcoreAPI->controllerData();
    logger_ = &lcoreAPI->logger();
    tagInterface_ = lcoreAPI->tag().getInterface();
	dataManager_ = std::make_unique<DataManager>(this);
    dataManager_->PopulateActiveAirports();
	configVersion = getLatestConfigVersion();

#ifndef DEV
	std::pair<bool, std::string> updateAvailable = newVersionAvailable();
	if (updateAvailable.first) {
		DisplayMessage("A new version of NeoSTAND is available: " + updateAvailable.second + " (current version: " + NEOSTAND_VERSION + ")", "");
	}
#endif // !DEV

    try
    {
        this->RegisterTagItems();
        this->RegisterTagActions();
        this->RegisterCommand();

        initialized_ = true;
    }
    catch (const std::exception &e)
    {
        logger_->error("Failed to initialize NeoSTAND: " + std::string(e.what()));
    }

    this->m_stop = false;
    this->m_worker = std::thread(&NeoSTAND::run, this);
}

std::pair<bool, std::string> stand::NeoSTAND::newVersionAvailable()
{
    httplib::SSLClient cli("api.github.com");
    httplib::Headers headers = { {"User-Agent", "NeoSTANDversionChecker"} };
    std::string apiEndpoint = "/repos/AlexisBalzano/NeoSTAND/releases/latest";

    auto res = cli.Get(apiEndpoint.c_str(), headers);
    if (res && res->status == 200) {
        try
        {
            auto json = nlohmann::json::parse(res->body);
            std::string latestVersion = json["tag_name"];
            if (latestVersion != NEOSTAND_VERSION) {
                logger_->warning("A new version of NeoSTAND is available: " + latestVersion + " (current version: " + NEOSTAND_VERSION + ")");
                return { true, latestVersion };
            }
            else {
                logger_->log(Logger::LogLevel::Info, "NeoSTAND is up to date.");
                return { false, "" };
            }
        }
        catch (const std::exception& e)
        {
            logger_->error("Failed to parse version information from GitHub: " + std::string(e.what()));
            return { false, "" };
        }
    }
    else {
        logger_->error("Failed to check for NeoSTAND updates. HTTP status: " + std::to_string(res ? res->status : 0));
        return { false, "" };
    }
}

bool stand::NeoSTAND::downloadAirportConfig(std::string icao)
{
    std::transform(icao.begin(), icao.end(), icao.begin(), ::toupper);

    httplib::SSLClient cli("raw.githubusercontent.com");
    cli.set_follow_location(true);
    cli.set_connection_timeout(5, 0);
    cli.set_read_timeout(5, 0);

    httplib::Headers headers = { {"User-Agent", "NEOSTANDconfigDownloader"}, {"Accept", "application/json"} };
    std::string repoUrl = dataManager_->getConfigUrl(); // OWNER/REPO/BRANCH

    if (repoUrl.empty()) {
        logger_->error("Configuration URL is not set.");
        return false;
    }

    std::string apiEndpoint = "/" + repoUrl + "/NeoSTAND/" + icao + ".json";

    bool success = false;

    if (auto res = cli.Get(apiEndpoint.c_str(), headers); res && res->status == 200) {
        try {
            nlohmann::ordered_json json = nlohmann::ordered_json::parse(res->body);
            success = dataManager_->saveDownloadedAirportConfig(json, icao);
        }
        catch (const std::exception& e) {
            logger_->error(std::string("Failed to parse airport configuration from GitHub: ") + e.what());
        }
    }
    else {
        int status = res ? res->status : 0;
        std::string extra;
        if (res && (status == 301 || status == 302 || status == 307 || status == 308)) {
            auto it = res->headers.find("Location");
            if (it != res->headers.end()) {
                extra = " Redirect Location: " + it->second;
            }
        }
        logger_->error("Failed to download airport configuration. HTTP status: " + std::to_string(status) + extra);
    }

    return success;
}

std::string stand::NeoSTAND::getLatestConfigVersion()
{
    httplib::SSLClient cli("raw.githubusercontent.com");
    cli.set_follow_location(true);
    cli.set_connection_timeout(5, 0);
    cli.set_read_timeout(5, 0);

    httplib::Headers headers = { {"User-Agent", "NEOSTANDconfigDownloader"} };
    std::string repoUrl = dataManager_->getConfigUrl(); // OWNER/REPO/BRANCH

    if (repoUrl.empty()) {
        logger_->error("Configuration URL is not set.");
        return "";
    }

    std::string apiEndpoint = "/" + repoUrl + "/version.json";

    if (auto res = cli.Get(apiEndpoint.c_str(), headers); res && res->status == 200) {
        try {
            nlohmann::ordered_json json = nlohmann::ordered_json::parse(res->body);
            return json["version"].get<std::string>();
        }
        catch (const std::exception& e) {
            logger_->error(std::string("Failed to parse version information from GitHub: ") + e.what());
            return "";
        }
    }
    else {
        int status = res ? res->status : 0;
        std::string extra;
        if (res && (status == 301 || status == 302 || status == 307 || status == 308)) {
            auto it = res->headers.find("Location");
            if (it != res->headers.end()) {
                extra = " Redirect Location: " + it->second;
            }
        }
        logger_->error("Failed to check for latest configuration version. HTTP status: " + std::to_string(status) + extra);
        return "";
    }
}

void NeoSTAND::Shutdown()
{
    if (initialized_)
    {
        initialized_ = false;
        LOG_DEBUG(Logger::LogLevel::Info, "NeoSTAND shutdown complete");
    }

    this->m_stop = true;
    if (this->m_worker.joinable())
        this->m_worker.join();

    if (dataManager_) dataManager_.reset();

    this->unegisterCommand();
}

void stand::NeoSTAND::Reset()
{
    autoMode = true;
}

void NeoSTAND::run() {
    int counter = 0;
    while (!this->m_stop) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        counter++;
        OnTimer(counter);
	}
}


void NeoSTAND::DisplayMessage(const std::string &message, const std::string &sender) {
    Chat::ClientTextMessageEvent textMessage;
    textMessage.sentFrom = "NeoSTAND";
    (sender.empty()) ? textMessage.message = ": " + message : textMessage.message = sender + ": " + message;
    textMessage.useDedicatedChannel = true;

    chatAPI_->sendClientMessage(textMessage);
}

void NeoSTAND::runScopeUpdate() {
    updateStandMenuButtons("LFPG"); // need to find a way to get the current pilot ICAO

    if (!dataManager_) return;
	dataManager_->updateAllPilots();

	std::vector<DataManager::Pilot> pilots = dataManager_->getAllPilots();
    for (auto& pilot : pilots) {
        if (!dataManager_->isArrival(pilot.callsign)) {
            std::optional<double> distance = aircraftAPI_->getDistanceFromOrigin(pilot.callsign);
            if (distance.has_value() && distance.value() > 5.0) {
                dataManager_->removePilot(pilot.callsign);
            }
        }
        if (pilot.stand.empty()) dataManager_->assignStands(pilot.callsign);
        this->UpdateTagItems(pilot.callsign);
	}
}

void NeoSTAND::OnTimer(int Counter) {
    if (Counter % dataManager_->getUpdateInterval() == 0 && autoMode) this->runScopeUpdate();
}

void stand::NeoSTAND::OnAirportConfigurationsUpdated(const Airport::AirportConfigurationsUpdatedEvent* event)
{
	LOG_DEBUG(Logger::LogLevel::Info, "Airport configurations updated, reloading data.");
    ClearAllTagCache();
    dataManager_->removeAllPilots();
    dataManager_->PopulateActiveAirports();
}

void stand::NeoSTAND::OnPositionUpdate(const Aircraft::PositionUpdateEvent* event)
{
    for (const auto& aircraft : event->aircrafts) {
        if (aircraft.callsign.empty())
            continue;
		std::optional<Flightplan::Flightplan> fp = flightplanAPI_->getByCallsign(aircraft.callsign);

        if (ignoredCallsigns_.contains(aircraft.callsign)) {
            continue;
        }

        if (aircraft.position.groundSpeed > 3) {
            if (!dataManager_->isArrival(aircraft.callsign)) {
				dataManager_->removePilot(aircraft.callsign);
                continue;
            }
        }
        if (!fp.has_value()) {
			// static & no flightplan -> check against all Stands
			std::vector<std::string> activeAirports = dataManager_->getAllActiveAirports();
            for (const auto& icao : activeAirports) {
                std::string currentStand = dataManager_->isAircraftOnStand(aircraft.callsign, icao);
                if (!currentStand.empty()) {
                    std::string icao = currentStand.substr(currentStand.length() - 4, 4);
                    currentStand = currentStand.substr(0, currentStand.length() - 5);
                    DataManager::Stand stand;
                    stand.name = currentStand;
                    stand.callsign = aircraft.callsign;
                    stand.icao = icao;
                    dataManager_->addStandToOccupied(stand);
                    break;
                }
			}
			ignoredCallsigns_.insert(aircraft.callsign);
			continue;
        }

        std::string currentStand = dataManager_->isAircraftOnStand(aircraft.callsign);
        if (!currentStand.empty()) {
            std::string icao = currentStand.substr(currentStand.length() - 4, 4);
            currentStand = currentStand.substr(0, currentStand.length() - 5);

            DataManager::Stand stand;
            stand.name = currentStand;
            stand.callsign = aircraft.callsign;
            stand.icao = icao;
            dataManager_->addStandToOccupied(stand);

        }
    }
}

void stand::NeoSTAND::OnFlightplanUpdated(const Flightplan::FlightplanUpdatedEvent* event)
{
	dataManager_->removePilot(event->callsign); // Force recompute
	ignoredCallsigns_.erase(event->callsign);
	ClearTagCache(event->callsign);
	dataManager_->updatePilot(event->callsign);
}

void stand::NeoSTAND::OnFlightplanRemoved(const Flightplan::FlightplanRemovedEvent* event)
{
    dataManager_->removePilot(event->callsign);
	ignoredCallsigns_.insert(event->callsign);
	ClearTagCache(event->callsign);
}

void stand::NeoSTAND::OnAircraftDisconnected(const Aircraft::AircraftDisconnectedEvent* event)
{
    dataManager_->removePilot(event->callsign);
	ignoredCallsigns_.erase(event->callsign);
	ClearTagCache(event->callsign);
}

void NeoSTAND::UpdateTagItems(std::string callsign) {
    DataManager::Pilot* pilot = dataManager_->getPilotByCallsign(callsign);
    if (!pilot || pilot->empty()) return;

    Tag::TagContext tagContext;
    tagContext.callsign = callsign;
    tagContext.colour = ColorizeStand();

	std::string stand = pilot->stand.empty() ? "N/A" : pilot->stand;

    updateTagValueIfChanged(callsign, standItemId_, stand, tagContext);
}

bool NeoSTAND::updateTagValueIfChanged(const std::string& callsign, const std::string& tagId, const std::string& value, Tag::TagContext& context)
{
    bool needsUpdate = false;
    
    {
        std::lock_guard<std::mutex> lock(tagCacheMutex_);
        auto& perCallsign = tagCache_[callsign];
        auto it = perCallsign.find(tagId);
        if (it == perCallsign.end()
            || it->second.value != value
            || it->second.colour != context.colour
            || it->second.background != context.backgroundColour)
        {
            needsUpdate = true;
        }
    }

    if (!needsUpdate)
        return false;

    if (!tagInterface_)
        return false;
    
    tagInterface_->UpdateTagValue(tagId, value, context);
    
    {
        std::lock_guard<std::mutex> lock(tagCacheMutex_);
        auto& perCallsign = tagCache_[callsign];
        perCallsign[tagId] = { value, context.colour, context.backgroundColour };
    }

    return true;
}

void NeoSTAND::ClearTagCache(const std::string& callsign)
{
    std::lock_guard<std::mutex> lock(tagCacheMutex_);
    tagCache_.erase(callsign);
}

void NeoSTAND::ClearAllTagCache()
{
    std::lock_guard<std::mutex> lock(tagCacheMutex_);
    tagCache_.clear();
}

bool NeoSTAND::toggleAutoMode()
{
    autoMode = !autoMode;
    return autoMode;
}

PluginSDK::PluginMetadata NeoSTAND::GetMetadata() const
{
    return {"NeoSTAND", PLUGIN_VERSION, "French vACC"};
}

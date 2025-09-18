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
    if (!dataManager_) return;
	LOG_DEBUG(Logger::LogLevel::Info, "Running scope update...");
	dataManager_->updateAllPilots();

	std::vector<DataManager::Pilot> pilots = dataManager_->getAllPilots();
	LOG_DEBUG(Logger::LogLevel::Info, "Updating tags for " + std::to_string(pilots.size()) + " pilots.");
    for (auto& pilot : pilots) {
        if (pilot.stand.empty()) dataManager_->assignStands(pilot.callsign);
        this->UpdateTagItems(pilot.callsign);
	}
}

void NeoSTAND::OnTimer(int Counter) {
    if (Counter % 5 == 0 && autoMode) this->runScopeUpdate();
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
    std::vector<DataManager::Stand> occupiedStands = dataManager_->getOccupiedStands();
    std::vector<std::string> occupiersCallsigns;
    for (const auto& stand : occupiedStands) {
        if (!stand.callsign.empty())
            occupiersCallsigns.push_back(stand.callsign);
    }

    for (const auto& aircraft : event->aircrafts) {
        if (aircraft.callsign.empty())
            continue;
	    
        if (aircraft.position.onGround == true) {
			// This aircraft is on the ground
		}

        if (std::find(occupiersCallsigns.begin(), occupiersCallsigns.end(), aircraft.callsign) != occupiersCallsigns.end()) {
            // This aircraft is holding stand
        }
		
    }
}

void stand::NeoSTAND::OnFlightplanUpdated(const Flightplan::FlightplanUpdatedEvent* event)
{
	dataManager_->removePilot(event->callsign); // Force recompute
	ClearTagCache(event->callsign);
	dataManager_->updatePilot(event->callsign);
}

void stand::NeoSTAND::OnFlightplanRemoved(const Flightplan::FlightplanRemovedEvent* event)
{
    dataManager_->removePilot(event->callsign);
	ClearTagCache(event->callsign);
}

void stand::NeoSTAND::OnAircraftDisconnected(const Aircraft::AircraftDisconnectedEvent* event)
{
    dataManager_->removePilot(event->callsign);
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

PluginSDK::PluginMetadata NeoSTAND::GetMetadata() const
{
    return {"NeoSTAND", PLUGIN_VERSION, "French vACC"};
}

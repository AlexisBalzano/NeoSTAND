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

	if (dataManager_) dataManager_.reset();

    this->m_stop = true;
    this->m_worker.join();

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
	//std::lock_guard<std::mutex> lock(callsignsMutex);
    //UpdateTagItems();
}

void NeoSTAND::OnTimer(int Counter) {
    if (Counter % 5 == 0 && autoMode) this->runScopeUpdate();
}

PluginSDK::PluginMetadata NeoSTAND::GetMetadata() const
{
    return {"NeoSTAND", PLUGIN_VERSION, "French vACC"};
}

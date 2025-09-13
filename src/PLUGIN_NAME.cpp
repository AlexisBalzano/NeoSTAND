#include "NeoVSID.h"
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

using namespace PLUGIN_NAMESPACE;

PLUGIN_NAME::PLUGIN_NAME() : m_stop(false), controllerDataAPI_(nullptr) {};
PLUGIN_NAME::~PLUGIN_NAME() = default;

void PLUGIN_NAME::Initialize(const PluginMetadata &metadata, CoreAPI *coreAPI, ClientInformation info)
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
		DisplayMessage("A new version of PLUGIN_NAME is available: " + updateAvailable.second + " (current version: " + PLUGIN_NAME_VERSION + ")", "");
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
        logger_->error("Failed to initialize PLUGIN_NAME: " + std::string(e.what()));
    }

    this->m_stop = false;
    this->m_worker = std::thread(&PLUGIN_NAME::run, this);
}

std::pair<bool, std::string> PLUGIN_NAMESPACE::PLUGIN_NAME::newVersionAvailable()
{
    httplib::SSLClient cli("api.github.com");
    httplib::Headers headers = { {"User-Agent", "PLUGIN_NAMEversionChecker"} };
    std::string apiEndpoint = "/repos/AlexisBalzano/PLUGIN_NAME/releases/latest";

    auto res = cli.Get(apiEndpoint.c_str(), headers);
    if (res && res->status == 200) {
        try
        {
            auto json = nlohmann::json::parse(res->body);
            std::string latestVersion = json["tag_name"];
            if (latestVersion != NEOVSID_VERSION) {
                logger_->warning("A new version of PLUGIN_NAME is available: " + latestVersion + " (current version: " + PLUGIN_NAME_VERSION + ")");
                return { true, latestVersion };
            }
            else {
                logger_->log(Logger::LogLevel::Info, "PLUGIN_NAME is up to date.");
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
        logger_->error("Failed to check for PLUGIN_NAME updates. HTTP status: " + std::to_string(res ? res->status : 0));
        return { false, "" };
    }
}

void PLUGIN_NAME::Shutdown()
{
    if (initialized_)
    {
        initialized_ = false;
        LOG_DEBUG(Logger::LogLevel::Info, "PLUGIN_NAME shutdown complete");
    }

	if (dataManager_) dataManager_.reset();

    this->m_stop = true;
    this->m_worker.join();

    this->unegisterCommand();
}

void vsid::PLUGIN_NAME::Reset()
{
    autoModeState = true;
	requestingClearance.clear();
	requestingPush.clear();
	requestingTaxi.clear();
	callsignsScope.clear();
}

void PLUGIN_NAME::DisplayMessage(const std::string &message, const std::string &sender) {
    Chat::ClientTextMessageEvent textMessage;
    textMessage.sentFrom = "PLUGIN_NAME";
    (sender.empty()) ? textMessage.message = ": " + message : textMessage.message = sender + ": " + message;
    textMessage.useDedicatedChannel = true;

    chatAPI_->sendClientMessage(textMessage);
}

void PLUGIN_NAME::runScopeUpdate() {
	std::lock_guard<std::mutex> lock(callsignsMutex);
    UpdateTagItems();
}

void PLUGIN_NAME::OnTimer(int Counter) {
    if (Counter % 5 == 0 && autoModeState) this->runScopeUpdate();
}

PluginSDK::PluginMetadata PLUGIN_NAME::GetMetadata() const
{
    return {"PLUGIN_NAME", PLUGIN_VERSION, "PLUGIN_AUTHOR"};
}

#pragma once
#include <memory>
#include <thread>
#include <vector>

#include "NeoRadarSDK/SDK.h"
#include "core/PLUGIN_NAMECommandProvider.h"
#include "core/DataManager.h"

constexpr const char* PLUGIN_NAME_VERSION = "v0.0.1";

using namespace PluginSDK;

namespace PLUGIN_NAMESPACE {

    class PLUGIN_NAMECommandProvider;

    class PLUGIN_NAME : public BasePlugin
    {
    public:
        PLUGIN_NAME();
        ~PLUGIN_NAME();

		// Plugin lifecycle methods
        void Initialize(const PluginMetadata& metadata, CoreAPI* coreAPI, ClientInformation info) override;
        std::pair<bool, std::string> newVersionAvailable();
        void Shutdown() override;
        void Reset();
        PluginMetadata GetMetadata() const override;

        // Radar commands
        void DisplayMessage(const std::string& message, const std::string& sender = "");
		
        // Scope events
        void OnTimer(int Counter);

        // Command handling
        void TagProcessing(const std::string& callsign, const std::string& actionId, const std::string& userInput = "");

		// API Accessors
        PluginSDK::Logger::LoggerAPI* GetLogger() const { return logger_; }
        Aircraft::AircraftAPI* GetAircraftAPI() const { return aircraftAPI_; }
        Airport::AirportAPI* GetAirportAPI() const { return airportAPI_; }
        Chat::ChatAPI* GetChatAPI() const { return chatAPI_; }
        Flightplan::FlightplanAPI* GetFlightplanAPI() const { return flightplanAPI_; }
        Fsd::FsdAPI* GetFsdAPI() const { return fsdAPI_; }
        PluginSDK::ControllerData::ControllerDataAPI* GetControllerDataAPI() const { return controllerDataAPI_; }
		Tag::TagInterface* GetTagInterface() const { return tagInterface_; }
        DataManager* GetDataManager() const { return dataManager_.get(); }

    private:
        void runScopeUpdate();
        void run();

    public:
        // Command IDs
        std::string commandId_;

    private:
        // Plugin state
        bool initialized_ = false;
        std::thread m_worker;
        bool m_stop;

        // APIs
        PluginMetadata metadata_;
        ClientInformation clientInfo_;
        Aircraft::AircraftAPI* aircraftAPI_ = nullptr;
        Airport::AirportAPI* airportAPI_ = nullptr;
        Chat::ChatAPI* chatAPI_ = nullptr;
        Flightplan::FlightplanAPI* flightplanAPI_ = nullptr;
        Fsd::FsdAPI* fsdAPI_ = nullptr;
        PluginSDK::Logger::LoggerAPI* logger_ = nullptr;
        PluginSDK::ControllerData::ControllerDataAPI* controllerDataAPI_ = nullptr;
        Tag::TagInterface* tagInterface_ = nullptr;
        std::unique_ptr<DataManager> dataManager_;
        std::shared_ptr<NeoVSIDCommandProvider> CommandProvider_;

        // Tag Items
        void RegisterTagItems();
        void RegisterTagActions();
        void RegisterCommand();
        void unegisterCommand();
        void OnTagAction(const Tag::TagActionEvent* event) override;
        void OnTagDropdownAction(const Tag::DropdownActionEvent* event) override;
        void UpdateTagItems();
        void UpdateTagItems(std::string Callsign);

	    // TAG Items IDs
		std::string tagItemId_;

        // TAG Action IDs
        std::string tagActionId_;
    };
} // namespace PLUGIN_NAMESPACE
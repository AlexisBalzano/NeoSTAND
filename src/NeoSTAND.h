#pragma once
#include <memory>
#include <thread>
#include <vector>

#include "NeoRadarSDK/SDK.h"
#include "core/NeoSTANDCommandProvider.h"
#include "core/DataManager.h"
#include "utils/Color.h"

constexpr const char* NEOSTAND_VERSION = "v0.0.1";

using namespace PluginSDK;

namespace stand {

    class NeoSTANDCommandProvider;

    class NeoSTAND : public BasePlugin
    {
    public:
        NeoSTAND();
        ~NeoSTAND();

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
        void OnAirportConfigurationsUpdated(const Airport::AirportConfigurationsUpdatedEvent* event) override;
        void OnPositionUpdate(const Aircraft::PositionUpdateEvent* event) override;
        void OnFlightplanUpdated(const Flightplan::FlightplanUpdatedEvent* event) override;
        void OnFlightplanRemoved(const Flightplan::FlightplanRemovedEvent* event) override;
		void OnAircraftDisconnected(const Aircraft::AircraftDisconnectedEvent* event) override;

        // Command handling
        void TagProcessing(const std::string& callsign, const std::string& actionId, const std::string& userInput = "");
		bool toggleAutoMode();

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
		bool updateTagValueIfChanged(const std::string& callsign, const std::string& tagId, const std::string& value, Tag::TagContext& context);
		void ClearTagCache(const std::string& callsign);
		void ClearAllTagCache();

        void run();

    public:
        // Command IDs
        std::string versionCommandId_;
        std::string helpCommandId_;
		std::string toggleModeCommandId_;
		std::string airportsCommandId_;

    private:
        // Plugin state
        bool initialized_ = false;
        std::thread m_worker;
        bool m_stop;
		bool autoMode = true;
        struct TagRenderState {
            std::string value;
            Color colour;
            Color background;
        };
        std::unordered_map<std::string, std::unordered_map<std::string, TagRenderState>> tagCache_;
        std::mutex tagCacheMutex_;

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
        std::shared_ptr<NeoSTANDCommandProvider> CommandProvider_;

        // Tag Items
        void RegisterTagItems();
        void RegisterTagActions();
        void RegisterCommand();
        void unegisterCommand();
        void OnTagAction(const Tag::TagActionEvent* event) override;
        void OnTagDropdownAction(const Tag::DropdownActionEvent* event) override;
        void UpdateTagItems(std::string Callsign);
        Color ColorizeStand();

	    // TAG Items IDs
		std::string standItemId_;
		std::string standMenuId_;

        // TAG Action IDs
        std::string assignActionId_;
    };
} // namespace stand
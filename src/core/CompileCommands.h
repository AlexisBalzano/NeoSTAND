#include <algorithm>
#include <string>

#include "NeoSTAND.h"


namespace stand {
void NeoSTAND::RegisterCommand() {
    try
    {
        CommandProvider_ = std::make_shared<NeoSTANDCommandProvider>(this, logger_, chatAPI_, fsdAPI_);

        PluginSDK::Chat::CommandDefinition definition;
        definition.name = "stand version";
        definition.description = "Display NeoSTAND Version";
        definition.lastParameterHasSpaces = false;
		definition.parameters.clear();

        versionCommandId_ = chatAPI_->registerCommand(definition.name, definition, CommandProvider_);
        
        definition.name = "stand help";
        definition.description = "Display all available commands";
        definition.lastParameterHasSpaces = false;
		definition.parameters.clear();

        helpCommandId_ = chatAPI_->registerCommand(definition.name, definition, CommandProvider_);
        
        definition.name = "stand toggle";
        definition.description = "Toggle auto stand assignation";
        definition.lastParameterHasSpaces = false;
		definition.parameters.clear();

        toggleModeCommandId_ = chatAPI_->registerCommand(definition.name, definition, CommandProvider_);
        
        definition.name = "stand airports";
        definition.description = "Display all active airports";
        definition.lastParameterHasSpaces = false;
		definition.parameters.clear();

        airportsCommandId_ = chatAPI_->registerCommand(definition.name, definition, CommandProvider_);
        
        definition.name = "stand dump";
        definition.description = "Dump current config to log file";
        definition.lastParameterHasSpaces = false;
		definition.parameters.clear();

        dumpCommandId_ = chatAPI_->registerCommand(definition.name, definition, CommandProvider_);

        definition.name = "stand occupied";
        definition.description = "Display all occupied stands";
        definition.lastParameterHasSpaces = false;
        definition.parameters.clear();

        occupiedCommandId_ = chatAPI_->registerCommand(definition.name, definition, CommandProvider_);

        definition.name = "stand blocked";
        definition.description = "Display all blocked stands";
        definition.lastParameterHasSpaces = false;
        definition.parameters.clear();

        blockedCommandId_ = chatAPI_->registerCommand(definition.name, definition, CommandProvider_);

        definition.name = "stand pilot";
        definition.description = "check if a pilot is on a stand";
        definition.lastParameterHasSpaces = false;
        definition.parameters.clear();

		Chat::CommandParameter param;
		param.name = "callsign";
		param.required = false;
		definition.parameters.push_back(param);

        pilotCommandId_ = chatAPI_->registerCommand(definition.name, definition, CommandProvider_);
    }
    catch (const std::exception& ex)
    {
        logger_->error("Error registering command: " + std::string(ex.what()));
    }
}

inline void NeoSTAND::unegisterCommand()
{
    if (CommandProvider_)
    {
        chatAPI_->unregisterCommand(versionCommandId_);
        chatAPI_->unregisterCommand(helpCommandId_);
        chatAPI_->unregisterCommand(toggleModeCommandId_);
        chatAPI_->unregisterCommand(airportsCommandId_);
        chatAPI_->unregisterCommand(dumpCommandId_);
		chatAPI_->unregisterCommand(occupiedCommandId_);
		chatAPI_->unregisterCommand(blockedCommandId_);
        chatAPI_->unregisterCommand(pilotCommandId_);
        CommandProvider_.reset();
	}
}

void generateDumpLog(std::vector<std::string>& lines, std::string time, NeoSTAND* neoSTAND) 
{
    lines.push_back("NeoSTAND Configuration Dump :" + time);
    lines.push_back("Version: " + std::string(NEOSTAND_VERSION));
    lines.push_back("Update Interval: " + std::to_string(neoSTAND->GetDataManager()->getUpdateInterval()) + " seconds");
    lines.push_back("Max Altitude: " + std::to_string(neoSTAND->GetDataManager()->getMaxAltitude()) + " feet");
    lines.push_back("Max Distance: " + std::to_string(neoSTAND->GetDataManager()->getMaxDistance()) + " NM");
    lines.push_back("Config URL: " + neoSTAND->GetDataManager()->getConfigUrl());
    lines.push_back("Active Airports:");
    std::vector<std::string> airports = neoSTAND->GetDataManager()->getAllActiveAirports();
    if (airports.empty()) {
        lines.push_back("  None");
    }
    else {
        for (const std::string& airport : airports) {
            lines.push_back("  " + airport);
        }
    }
    lines.push_back("Pilots:");
    std::vector<DataManager::Pilot> pilots = neoSTAND->GetDataManager()->getAllPilots();
    if (pilots.empty()) {
        lines.push_back("  None");
    }
    else {
        for (const DataManager::Pilot& pilot : pilots) {
            lines.push_back("  " + pilot.callsign + " - Dest:" + pilot.destination + " - WTC:" + pilot.aircraftWTC + " - " +
                (pilot.isSchengen ? "Schengen" : "Non-Schengen") + " - " +
                (pilot.isNational ? "National" : "International") + " - Stand: " + (pilot.stand.empty() ? "None" : pilot.stand));
        }
    }
    lines.push_back("Occupied Stands:");
    std::vector<DataManager::Stand> occupiedStands = neoSTAND->GetDataManager()->getOccupiedStands();
    if (occupiedStands.empty()) {
        lines.push_back("  None");
    }
    else {
        for (const DataManager::Stand& stand : occupiedStands) {
            lines.push_back("  " + stand.name + " (" + stand.icao + ") - " + stand.callsign);
        }
    }
    lines.push_back("Blocked Stands:");
    std::vector<DataManager::Stand> blockedStands = neoSTAND->GetDataManager()->getBlockedStands();
    if (blockedStands.empty()) {
        lines.push_back("  None");
    }
    else {
        for (const DataManager::Stand& stand : blockedStands) {
            lines.push_back("  " + stand.name + " (" + stand.icao + ") - " + stand.callsign);
        }
    }
    lines.push_back("Ignored Callsigns:");
	std::unordered_set<std::string> ignoredCallsigns = neoSTAND->GetIgnoredCallsigns();
    if (ignoredCallsigns.empty()) {
        lines.push_back("  None");
    }
    else {
        for (const std::string& callsign : ignoredCallsigns) {
            lines.push_back("  " + callsign);
        }
	}
    lines.push_back("Cargo Types: ");
    for (const std::string& type : neoSTAND->GetDataManager()->getCargoTypes()) {
        lines.push_back("  " + type);
    }
    lines.push_back("Helicopter Types: ");
    for (const std::string& type : neoSTAND->GetDataManager()->getHeliTypes()) {
        lines.push_back("  " + type);
    }
    lines.push_back("Military Types: ");
    for (const std::string& type : neoSTAND->GetDataManager()->getMilitaryTypes()) {
        lines.push_back("  " + type);
    }
    lines.push_back("General Aviation Types: ");
    for (const std::string& type : neoSTAND->GetDataManager()->getGATypes()) {
        lines.push_back("  " + type);
    }
	lines.push_back("End of Configuration Dump.");
}


Chat::CommandResult NeoSTANDCommandProvider::Execute( const std::string &commandId, const std::vector<std::string> &args)
{
    if (commandId == neoSTAND_->versionCommandId_)
    {
		neoSTAND_->DisplayMessage("NeoSTAND Version: " + std::string(NEOSTAND_VERSION));
        return { true, std::nullopt };
	}
    else if (commandId == neoSTAND_->helpCommandId_)
    {
        for (const char* line : {
          "NeoSTAND available commands:",
          ".stand version",
		  ".stand toggle",
		  ".stand airports",
		  ".stand occupied",
		  ".stand blocked",
		  ".stand pilot <callsign>",
          ".stand dump"
            })
        {
            neoSTAND_->DisplayMessage(line);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        return { true, std::nullopt };
	}
    else if (commandId == neoSTAND_->toggleModeCommandId_)
    {
		bool state = neoSTAND_->toggleAutoMode();
		neoSTAND_->DisplayMessage("NeoSTAND Auto Mode: " + std::string(state ? "ON" : "OFF"));
        return { true, std::nullopt };
    }
    else if (commandId == neoSTAND_->airportsCommandId_)
    {
        std::vector<std::string> airports = neoSTAND_->GetDataManager()->getAllActiveAirports();
        if (airports.empty()) {
            neoSTAND_->DisplayMessage("No active airports found.");
        }
        else {
            airports.emplace(airports.begin(), "Active Airports:");
            for (const std::string& line : airports)
            {
                neoSTAND_->DisplayMessage(line);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        return { true, std::nullopt };
    }
    else if (commandId == neoSTAND_->occupiedCommandId_)
    {
        std::vector<DataManager::Stand> stands = neoSTAND_->GetDataManager()->getOccupiedStands();
        if (stands.empty()) {
            neoSTAND_->DisplayMessage("No occupied stands found.");
        }
        else {
			neoSTAND_->DisplayMessage("Occupied Stands:");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            for (const DataManager::Stand& stand : stands)
            {
                std::string line = stand.name + " (" + stand.icao + ") - " + stand.callsign;
                neoSTAND_->DisplayMessage(line);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        return { true, std::nullopt };
    }
    else if (commandId == neoSTAND_->blockedCommandId_)
    {
        std::vector<DataManager::Stand> stands = neoSTAND_->GetDataManager()->getBlockedStands();
        if (stands.empty()) {
            neoSTAND_->DisplayMessage("No blocked stands found.");
        }
        else {
			neoSTAND_->DisplayMessage("Blocked Stands:");
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
            for (const DataManager::Stand& stand : stands)
            {
                std::string line = stand.name + " (" + stand.icao + ") - " + stand.callsign;
                neoSTAND_->DisplayMessage(line);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
        }
        return { true, std::nullopt };
	}
    else if (commandId == neoSTAND_->dumpCommandId_)
    {
        std::vector<std::string> lines;
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
		generateDumpLog(lines, std::string(std::ctime(&now_c)), neoSTAND_);       
        std::tm tm{};
#if defined(_WIN32)
        if (localtime_s(&tm, &now_c) != 0) {
            neoSTAND_->DisplayMessage("Failed to get local time.");
            return { true, std::nullopt };
        }
#else
        if (localtime_r(&now_c, &tm) == nullptr) {
            neoSTAND_->DisplayMessage("Failed to get local time.");
            return { true, std::nullopt };
        }
#endif
        std::ostringstream oss;
        oss << "NeoSTAND_ConfigDump_"
            << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S")
            << ".txt";
        std::string fileName = oss.str();

        bool success = neoSTAND_->GetDataManager()->printToFile(lines, fileName);
        if (!success) {
            neoSTAND_->DisplayMessage("Failed to write dump log.");
            return { true, std::nullopt };
		}
        neoSTAND_->DisplayMessage("Configuration dumped to " + fileName);
        return { true, std::nullopt };
	}
    else if (commandId == neoSTAND_->pilotCommandId_)
    {
        if (args.empty()) {
            neoSTAND_->DisplayMessage("Usage: .stand pilot <callsign>");
            return { true, "error" };
        }
        std::string callsign = args[0];
		std::transform(callsign.begin(), callsign.end(), callsign.begin(), ::toupper);
        std::string stand = neoSTAND_->GetDataManager()->isAircraftOnStand(callsign);
        if (stand.empty()) {
            neoSTAND_->DisplayMessage("Pilot " + callsign + " is not on a stand.");
        }
        else {
            neoSTAND_->DisplayMessage("Pilot " + callsign + " is on stand: " + stand);
        }
        return { true, std::nullopt };
	}
    else {
        return { false, "error" };
    }

	return { true, std::nullopt };
}
}  // namespace stand
#include <algorithm>
#include <fstream>
#include <cmath>

#include "../NeoSTAND.h"
#include "DataManager.h"

#ifdef DEV
#define LOG_DEBUG(loglevel, message) loggerAPI_->log(loglevel, message)
#else
#define LOG_DEBUG(loglevel, message) void(0)
#endif

DataManager::DataManager(stand::NeoSTAND* neoSTAND)
	: neoSTAND_(neoSTAND) {
	aircraftAPI_ = neoSTAND_->GetAircraftAPI();
	flightplanAPI_ = neoSTAND_->GetFlightplanAPI();
	airportAPI_ = neoSTAND_->GetAirportAPI();
	chatAPI_ = neoSTAND_->GetChatAPI();
	loggerAPI_ = neoSTAND_->GetLogger();
	controllerDataAPI_ = neoSTAND_->GetControllerDataAPI();

	configPath_ = getDllDirectory();
	loadSettingJson();
	aircraftWingspans_.clear();
	bool success = parseSettings();
	configsError_.clear();
	configsDownloaded_.clear();
	callsignError_.clear();
}


std::filesystem::path DataManager::getDllDirectory()
{
	return neoSTAND_->GetClientInfo().documentsPath;
}

void DataManager::clearData()
{
	configPath_.clear();
	pilots_.clear();
	activeAirports_.clear();
	occupiedStands_.clear();
	blockedStands_.clear();
	configsError_.clear();
	configsDownloaded_.clear();
	callsignError_.clear();
	aircraftWingspans_.clear();
	if (aircraftAPI_)
		aircraftAPI_ = nullptr;
	if (flightplanAPI_)
		flightplanAPI_ = nullptr;
	if (airportAPI_)
		airportAPI_ = nullptr;
}

void DataManager::clearJson()
{
	std::lock_guard<std::mutex> lock(dataMutex_);
	configJson_.clear();
	settingJson_.clear();
}

void DataManager::DisplayMessageFromDataManager(const std::string& message, const std::string& sender)
{
	Chat::ClientTextMessageEvent textMessage;
	textMessage.sentFrom = "NeoSTAND";
	(sender.empty()) ? textMessage.message = ": " + message : textMessage.message = sender + ": " + message;
	textMessage.useDedicatedChannel = true;

	chatAPI_->sendClientMessage(textMessage);
}

	
int DataManager::retrieveConfigJson(const std::string& icao)
{
	std::string icaoUpper = icao;
	std::transform(icaoUpper.begin(), icaoUpper.end(), icaoUpper.begin(), ::toupper);
	const std::string fileName = icaoUpper + ".json";
	const std::filesystem::path jsonPath = configPath_ / "Plugins/NeoSTAND" / fileName;

	nlohmann::ordered_json tempJson;
	bool alreadyDownloaded = false;

	{
		std::lock_guard<std::mutex> lock(dataMutex_);
		if (configsDownloaded_.contains(icaoUpper)) alreadyDownloaded = true;
	}

	for (int attempt = 0; attempt < 2; ++attempt)
	{
		std::ifstream config(jsonPath);
		if (!config.is_open())
		{
			if (!alreadyDownloaded)
			{
				configJson_.clear();
				bool downloadOk = neoSTAND_->downloadAirportConfig(icao);
				alreadyDownloaded = true;
				if (!downloadOk) return -1;
				continue;
			}
			bool firstErrorForFile;
			{
				std::lock_guard<std::mutex> lock(dataMutex_);
				firstErrorForFile = !configsError_.contains(icaoUpper);
				configsError_.insert(icaoUpper);
			}
			if (firstErrorForFile)
			{
				DisplayMessageFromDataManager("Could not open JSON file: " + jsonPath.string(), "DataManager");
				loggerAPI_->log(Logger::LogLevel::Error, "Could not open JSON file: " + jsonPath.string());
			}

			return -1;
		}

		try {
			config >> tempJson;
		}
		catch (...) {
			DisplayMessageFromDataManager("Error parsing JSON file: " + jsonPath.string(), "DataManager");
			loggerAPI_->log(Logger::LogLevel::Error, "Error parsing JSON file: " + jsonPath.string());
			return -1;
		}

		if (!tempJson.contains("version"))
		{
			if (!alreadyDownloaded)
			{
				configJson_.clear();
				bool downloadOk = neoSTAND_->downloadAirportConfig(icao);
				alreadyDownloaded = true;
				if (!downloadOk) return -1;
				std::this_thread::sleep_for(std::chrono::milliseconds(300));
				continue;
			}
			{
				std::lock_guard<std::mutex> lock(dataMutex_);
				if (!configsError_.contains(icaoUpper))
					configsError_.insert(icaoUpper);
				else return -1;
			}
			DisplayMessageFromDataManager("Config version missing in JSON file: " + fileName, "DataManager");
			loggerAPI_->log(Logger::LogLevel::Error, "Config version missing in JSON file: " + fileName);
			return -1;
		}

		const std::string versionRead = tempJson["version"].get<std::string>();
		std::string version = neoSTAND_->getConfigVersion();
		if (!version.empty() && versionRead != version)
		{
			bool firstErrorForFile;
			{
				std::lock_guard<std::mutex> lock(dataMutex_);
				firstErrorForFile = !configsError_.contains(icaoUpper);
				configsError_.insert(icaoUpper);
			}

			if (firstErrorForFile)
			{
				DisplayMessageFromDataManager("Config version mismatch! Expected: " + version + ", Found: " + versionRead + " (" + fileName + ")", "DataManager");
				loggerAPI_->log(Logger::LogLevel::Error, "Config version mismatch! Expected: " + version + ", Found: " + versionRead + " " + fileName);
			}

			if (!alreadyDownloaded)
			{
				configJson_.clear();
				bool downloadOk = neoSTAND_->downloadAirportConfig(icao);
				alreadyDownloaded = true;
				if (!downloadOk)
				{
					loggerAPI_->log(Logger::LogLevel::Warning, "Download attempt after version mismatch failed: " + fileName);
					return -1;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(300));
				continue;
			}
			return 0;
		}
		break;
	}

	{
		std::lock_guard<std::mutex> lock(dataMutex_);
		if (configsError_.contains(icaoUpper)) {
			configsError_.erase(icaoUpper);
			DisplayMessageFromDataManager("Successfully redownloaded config for: " + icaoUpper, "DataManager");
			loggerAPI_->log(Logger::LogLevel::Info, "Successfully redownloaded config for: " + icaoUpper);
		}
		configJson_ = tempJson;
		configsDownloaded_.insert(icaoUpper);
	}
	return 0;
}

bool DataManager::retrieveCorrectConfigJson(const std::string& icao)
{
	if (!(configJson_.contains("ICAO") && configJson_["ICAO"].get<std::string>() == icao) || configJson_.empty()) {
		if (retrieveConfigJson(icao) == -1) return false;
	}
	return true;
}

bool DataManager::isCorrectJsonVersion(const std::string& config_version, const std::string& fileName)
{
	if (config_version == NEOSTAND_VERSION) {
		return true;
	}
	else {
		DisplayMessageFromDataManager("Config version mismatch! Expected: " + std::string(NEOSTAND_VERSION) + ", Found: " + config_version + ", please update your config files.", fileName);
		loggerAPI_->log(Logger::LogLevel::Error, "Config version mismatch! Expected: " + std::string(NEOSTAND_VERSION) + ", Found: " + config_version + fileName);
	}
	return false;
}

void DataManager::loadSettingJson()
{
	std::lock_guard<std::mutex> lock(dataMutex_);
	std::filesystem::path jsonPath = configPath_ / "plugins" / "NeoSTAND" / "config.json";
	std::ifstream configFile(jsonPath);
	if (!configFile.is_open()) {
		DisplayMessageFromDataManager("Could not open config data JSON file: " + jsonPath.string(), "DataManager");
		loggerAPI_->log(Logger::LogLevel::Error, "Could not open config data JSON file: " + jsonPath.string());
		return;
	}
	try {
		configJson_ = nlohmann::json::parse(configFile);
	}
	catch (...) {
		DisplayMessageFromDataManager("Error parsing config data JSON file: " + jsonPath.string(), "DataManager");
		loggerAPI_->log(Logger::LogLevel::Error, "Error parsing config data JSON file: " + jsonPath.string());
		return;
	}
}

bool DataManager::parseSettings()
{
	std::lock_guard<std::mutex> lock(dataMutex_);

	auto readInt = [&](const char* key, int defVal) -> int {
		if (configJson_.contains(key)) {
			const auto& v = configJson_[key];
			if (v.is_number_integer()) {
				int x = v.get<int>();
				return x;
			}
			if (v.is_number_float()) {
				int x = static_cast<int>(v.get<double>());
				return x;
			}
		}
		loggerAPI_->log(Logger::LogLevel::Warning, std::string(key) + " missing or not an integer in config.json, using default");
		DisplayMessageFromDataManager(std::string(key) + " missing or not an integer in config.json, using default", "DataManager");
		return defVal;
		};

	auto readDouble = [&](const char* key, double defVal) -> double {
		if (configJson_.contains(key)) {
			const auto& v = configJson_[key];
			if (v.is_number()) {
				return v.get<double>();
			}
		}
		loggerAPI_->log(Logger::LogLevel::Warning, std::string(key) + " missing or not a number in config.json, using default");
		DisplayMessageFromDataManager(std::string(key) + " missing or not a number in config.json, using default", "DataManager");
		return defVal;
		};

	if (configJson_.contains("config_github_url") && configJson_["config_github_url"].is_string()) {
		configUrl_ = configJson_["config_github_url"].get<std::string>();
	}

	updateInterval_ = readInt("update_interval", stand::DEFAULT_UPDATE_INTERVAL);
	if (updateInterval_ <= 0) {
		loggerAPI_->log(Logger::LogLevel::Warning, "update_interval <= 0, using default");
		DisplayMessageFromDataManager("update_interval <= 0, using default", "DataManager");
		updateInterval_ = stand::DEFAULT_UPDATE_INTERVAL;
	}

	maxAltitude_ = readInt("max_alt", stand::MAX_ALTITUDE);
	if (maxAltitude_ <= 0) {
		loggerAPI_->log(Logger::LogLevel::Warning, "max_alt <= 0, using default");
		DisplayMessageFromDataManager("max_alt <= 0, using default", "DataManager");
		maxAltitude_ = stand::MAX_ALTITUDE;
	}

	maxDistance_ = readDouble("max_distance", stand::MAX_DISTANCE);
	if (maxDistance_ < 0) {
		loggerAPI_->log(Logger::LogLevel::Warning, "max_distance < 0, using default");
		DisplayMessageFromDataManager("max_distance < 0, using default", "DataManager");
		maxDistance_ = stand::MAX_DISTANCE;
	}
	
	if (configJson_.contains("CargoOperator") && configJson_["CargoOperator"].is_array()) {
		cargo = configJson_["CargoOperator"].get<std::unordered_set<std::string>>();
	}
	if (cargo.empty()) {
		loggerAPI_->log(Logger::LogLevel::Warning, "No valid cargo operator list in config.json");
		DisplayMessageFromDataManager("No valid cargo operator list in config.json, Cargo stand won't be assignable.");
	}

	if (configJson_.contains("Helicopters") && configJson_["Helicopters"].is_array()) {
		heliTypes = configJson_["Helicopters"].get<std::unordered_set<std::string>>();
	}
	if (heliTypes.empty()) {
		loggerAPI_->log(Logger::LogLevel::Warning, "No valid helicopter list in config.json");
		DisplayMessageFromDataManager("No valid helicopter list in config.json.");
	}

	if (configJson_.contains("Military") && configJson_["Military"].is_array()) {
		militaryTypes = configJson_["Military"].get<std::unordered_set<std::string>>();
	}
	if (militaryTypes.empty()) {
		loggerAPI_->log(Logger::LogLevel::Warning, "No valid military list in config.json");
		DisplayMessageFromDataManager("No valid military list in config.json.");
	}

	if (configJson_.contains("GeneralAviation") && configJson_["GeneralAviation"].is_array()) {
		gaTypes = configJson_["GeneralAviation"].get<std::unordered_set<std::string>>();
	}
	if (gaTypes.empty()) {
		loggerAPI_->log(Logger::LogLevel::Warning, "No valid General Aviation list in config.json");
		DisplayMessageFromDataManager("No valid General Aviation list in config.json.");
	}

	if (configJson_.contains("AircraftWingspans") && configJson_["AircraftWingspans"].is_object()) {
		for (auto& [k, v] : configJson_["AircraftWingspans"].items()) {
			if (v.is_number()) {
				std::string key = k;
				std::transform(key.begin(), key.end(), key.begin(), ::toupper);
				aircraftWingspans_[key] = v.get<double>();
			}
		}
	}
	if (aircraftWingspans_.empty()) {
		loggerAPI_->log(Logger::LogLevel::Warning, "No valid Aircraft Wingspan list in config.json");
		DisplayMessageFromDataManager("No valid Aircraft Wingspan list in config.json, Size Constrained Stand won't be assignable.");
	}

	return true;
}

bool DataManager::removePilot(const std::string& callsign)
{
    std::lock_guard<std::mutex> lock(dataMutex_);
    const size_t initial = pilots_.size();

    pilots_.erase(std::remove_if(pilots_.begin(), pilots_.end(),
        [&callsign](const Pilot& p) { return p.callsign == callsign; }), pilots_.end());

    occupiedStands_.erase(std::remove_if(occupiedStands_.begin(), occupiedStands_.end(),
        [&callsign](const Stand& s) { return s.callsign == callsign; }), occupiedStands_.end());

    blockedStands_.erase(std::remove_if(blockedStands_.begin(), blockedStands_.end(),
        [&callsign](const Stand& s) { return s.callsign == callsign; }), blockedStands_.end());

    return pilots_.size() < initial;
}

void DataManager::assignStands(const std::string& callsign)
{
	Pilot *pilot = getPilotByCallsign(callsign);
	if (!pilot) return;

	std::vector<std::string> errorMessages;
	errorMessages.reserve(300);

	// Check if configJSON is already the right one, if not, retrieve it
	std::string icao = pilot->destination;
	std::transform(icao.begin(), icao.end(), icao.begin(), ::toupper);
	if (!retrieveCorrectConfigJson(icao)) {
		loggerAPI_->log(Logger::LogLevel::Warning, "Failed to retrieve config when assigning Stand for: " + callsign);
		pilot->stand = "";
		return;
	}
	std::lock_guard<std::mutex> lock(dataMutex_);
	
	// If aircraft occupies stand already, assign the occupied stand
	auto occupiedIt = std::find_if(occupiedStands_.begin(), occupiedStands_.end(), [&callsign](const Stand& stand) { return callsign == stand.callsign; });
	if (occupiedIt != occupiedStands_.end()) {
		pilot->stand = occupiedIt->name;
		LOG_DEBUG(Logger::LogLevel::Info, "Pilot: " + pilot->callsign + " already occupies stand: " + pilot->stand);
		return;
	}

	nlohmann::json standsJson;
	if (configJson_.contains("Stands")) {
		standsJson = configJson_["Stands"];
		LOG_DEBUG(Logger::LogLevel::Info, "Assigning stand for pilot: " + pilot->callsign + " at " + pilot->destination);
	}
	else {
		loggerAPI_->log(Logger::LogLevel::Warning, "No STAND section in config for: " + icao);
		pilot->stand = "";
		return;
	}

	errorMessages.push_back("Total stands available before filtering: " + std::to_string(standsJson.size()));

	// Filter stands based on criteria
	auto it = standsJson.begin();

	while (it != standsJson.end()) {
		const auto& stand = *it;
		// Check Size Code
		if (stand.contains("Code")) {
			std::string code = stand["Code"].get<std::string>();
			if (code.find(pilot->aircraftCode) == std::string::npos) {
				errorMessages.push_back("Removing stand " + it.key() + " due to code mismatch. Stand: " + code + " Pilot: " + pilot->aircraftCode);
				it = standsJson.erase(it);
				continue;
			}
		}

		// Check USE
		if (stand.contains("Use")) {
			std::string use = stand["Use"].get<std::string>();
			std::string pilotType;
			switch (pilot->aircraftType) {
			case AircraftType::airliner: pilotType = "A"; break;
			case AircraftType::generalAviation: pilotType = "P"; break;
			case AircraftType::helicopter: pilotType = "H"; break;
			case AircraftType::military: pilotType = "M"; break;
			case AircraftType::cargo: pilotType = "C"; break;
			default: pilotType = ""; break;
			}
			if (use.find(pilotType) == std::string::npos) {
				errorMessages.push_back("Removing stand " + it.key() + " due to Use mismatch. Stand: " + use + " Pilot: " + pilotType);
				it = standsJson.erase(it);
				continue;
			}
		}

		// Check SCHENGEN
		if (stand.contains("Schengen")) {
			bool schegen = stand["Schengen"].get<bool>();
			if (schegen == true && pilot->isSchengen == false) {
				errorMessages.push_back("Removing stand " + it.key() + " due to Schengen mismatch. Stand: " + (schegen ? "true" : "false") + " Pilot: " + (pilot->isSchengen ? "true" : "false"));
				it = standsJson.erase(it);
				continue;
			}
		}

		// Check NATIONAL
		if (stand.contains("National")) {
			bool national = stand["National"].get<bool>();
			if (national != pilot->isNational) {
				errorMessages.push_back("Removing stand " + it.key() + " due to National mismatch. Stand: " + (national ? "true" : "false") + " Pilot: " + (pilot->isNational ? "true" : "false"));
				it = standsJson.erase(it);
				continue;
			}
		}

		// Check Callsigns
		if (stand.contains("Callsigns")) {
			std::vector<std::string> callsigns = stand["Callsigns"].get<std::vector<std::string>>();
			if (callsign.length() < 3 || std::find(callsigns.begin(), callsigns.end(), pilot->callsign.substr(0, 3)) == callsigns.end()) {
				errorMessages.push_back("Removing stand " + it.key() + " due to Callsign mismatch. Pilot: " + pilot->callsign);
				it = standsJson.erase(it);
				continue;
			}
		}

		// Check if stand is occupied
		if (std::find_if(occupiedStands_.begin(), occupiedStands_.end(), [&it, icao](const Stand& stand){ return it.key() == stand.name && icao == stand.icao;}) != occupiedStands_.end()) {
			errorMessages.push_back("Removing stand " + it.key() + " because it is already occupied.");
			it = standsJson.erase(it);
			continue;
		}

		// Check if stand is blocked
		if (std::find_if(blockedStands_.begin(), blockedStands_.end(), [&it, icao](const Stand& stand) { return it.key() == stand.name && icao == stand.icao; }) != blockedStands_.end()) {
			errorMessages.push_back("Removing stand " + it.key() + " because it is blocked.");
			it = standsJson.erase(it);
			continue;
		}

		++it; // Only increment if not erased
	}

	if (standsJson.empty()) {
		if (!callsignError_.contains(pilot->callsign)) {
			loggerAPI_->log(Logger::LogLevel::Warning, "No suitable stand found for pilot: " + pilot->callsign + " at " + pilot->destination);
			DisplayMessageFromDataManager("No suitable stand found for pilot: " + pilot->callsign + " at " + pilot->destination + ". Printed error to log File Documents/NeoRadar/logs/plugins/NeoSTAND/NeoSTAND_debug_" + pilot->callsign + ".log");
			callsignError_.insert(pilot->callsign);
			printToFile(errorMessages, "NeoSTAND_debug_" + pilot->callsign + ".log");
		}
		pilot->stand = "";
		return;
	}

	LOG_DEBUG(Logger::LogLevel::Info, "Total stands available after filtering: " + std::to_string(standsJson.size()));

	// Determine lowest priority first (keep all stands sharing that value)
	int lowestPriority = std::numeric_limits<int>::max();
	bool anyPriority = false;
	for (auto& [standName, stand] : standsJson.items()) {
		if (stand.contains("Priority") && stand["Priority"].is_number_integer()) {
			int p = stand["Priority"].get<int>();
			if (p < lowestPriority) lowestPriority = p;
			anyPriority = true;
		}
	}

	if (anyPriority) {
		// Erase every stand whose priority != lowestPriority.
		// Use iterator loop to avoid invalidation issues.
		for (auto it = standsJson.begin(); it != standsJson.end(); ) {
			auto& stand = it.value();
			if (stand.contains("Priority") && stand["Priority"].is_number_integer()) {
				int p = stand["Priority"].get<int>();
				if (p != lowestPriority) {
					it = standsJson.erase(it);
					continue;
				}
			}
			else {
				// If a stand has no Priority while some priorities exist, drop it.
				it = standsJson.erase(it);
				continue;
			}
			++it;
		}
	}

	// Only stands with lowest priority remain or all stands if none had priority
	std::string selectedStandName = standsJson.begin().key();
	auto selectedStand = standsJson.begin().value();
	pilot->stand = selectedStandName;

	LOG_DEBUG(Logger::LogLevel::Info, "Assigned stand " + pilot->stand + " to pilot: " + pilot->callsign);

	// Mark the stand as occupied
	Stand stand;
	stand.name = pilot->stand;
	stand.icao = pilot->destination;
	stand.callsign = pilot->callsign;
	occupiedStands_.push_back(stand);

	// Check if the stand is blocking other stands
	if (selectedStand.contains("Block") && selectedStand["Block"].is_array())
	{
		for (const auto& blockedStandName : selectedStand["Block"]) {
			Stand blockedStand;
			blockedStand.name = blockedStandName.get<std::string>();
			blockedStand.icao = pilot->destination;
			blockedStand.callsign = pilot->callsign;
			blockedStands_.push_back(blockedStand);
			LOG_DEBUG(Logger::LogLevel::Info, "Also blocking stand " + blockedStand.name + " due to assignment of " + pilot->stand);
		}
	}
}

void DataManager::assignStandToPilot(Pilot& pilot, const std::string& standName)
{
	// Check if configJSON is already the right one, if not, retrieve it
	std::string icao = pilot.destination;
	std::transform(icao.begin(), icao.end(), icao.begin(), ::toupper);
	if (!retrieveCorrectConfigJson(icao)) {
		loggerAPI_->log(Logger::LogLevel::Warning, "Failed to retrieve config when assigning Stand for: " + pilot.callsign);
		pilot.stand = "";
		return;
	}

	std::lock_guard<std::mutex> lock(dataMutex_);
	// Check if the stand is already occupied
	if (std::find_if(occupiedStands_.begin(), occupiedStands_.end(),
		[&standName](const Stand& stand) { return stand.name == standName; }) != occupiedStands_.end()) {
		loggerAPI_->log(Logger::LogLevel::Warning, "Stand " + standName + " is already occupied, cannot assign to pilot: " + pilot.callsign);
		DisplayMessageFromDataManager("Stand " + standName + " is already occupied, cannot assign to pilot: " + pilot.callsign);
		pilot.stand = "";
		return;
	}
	// Check if the stand is blocked
	if (std::find_if(blockedStands_.begin(), blockedStands_.end(),
		[&standName](const Stand& stand) { return stand.name == standName; }) != blockedStands_.end()) {
		loggerAPI_->log(Logger::LogLevel::Warning, "Stand " + standName + " is blocked, cannot assign to pilot: " + pilot.callsign);
		DisplayMessageFromDataManager("Stand " + standName + " is blocked, cannot assign to pilot: " + pilot.callsign);
		pilot.stand = "";
		return;
	}
	pilot.stand = standName;

	// Release any previously occupied stand by this pilot
	occupiedStands_.erase(std::remove_if(occupiedStands_.begin(), occupiedStands_.end(),
		[&pilot](const Stand& s) { return s.callsign == pilot.callsign; }), occupiedStands_.end());
	blockedStands_.erase(std::remove_if(blockedStands_.begin(), blockedStands_.end(),
		[&pilot](const Stand& s) { return s.callsign == pilot.callsign; }), blockedStands_.end());

	// Mark the stand as occupied
	Stand stand;
	stand.name = pilot.stand;
	stand.icao = pilot.destination;
	stand.callsign = pilot.callsign;
	occupiedStands_.push_back(stand);
	loggerAPI_->log(Logger::LogLevel::Info, "Manually assigned stand " + pilot.stand + " to pilot: " + pilot.callsign);

	// Check if the stand is blocking other stands
	if (configJson_.contains("Stands") && configJson_["Stands"].contains(stand.name)) {
		const auto& standJson = configJson_["Stands"][stand.name];
		if (standJson.contains("Block") && standJson["Block"].is_array())
		{
			for (const auto& blockedStandName : standJson["Block"]) {
				Stand blockedStand;
				blockedStand.name = blockedStandName.get<std::string>();
				blockedStand.icao = stand.icao;
				blockedStand.callsign = stand.callsign;
				if (std::find(blockedStands_.begin(), blockedStands_.end(), blockedStand) == blockedStands_.end()) {
					blockedStands_.push_back(blockedStand);
					loggerAPI_->log(Logger::LogLevel::Info, "Also blocking stand " + blockedStand.name + " due to assignment of " + pilot.stand);
				}
			}
		}
	}
}

void DataManager::freeStand(const std::string& standName)
{
	std::lock_guard<std::mutex> lock(dataMutex_);
	// Find the occupying pilot for this stand
	auto itOccupied = std::find_if(occupiedStands_.begin(), occupiedStands_.end(),
		[&standName](const Stand& s) { return s.name == standName; });
	if (itOccupied == occupiedStands_.end()) return;

	const std::string callsign = itOccupied->callsign;

	occupiedStands_.erase(std::remove_if(occupiedStands_.begin(), occupiedStands_.end(),
		[&standName](const Stand& s) { return s.name == standName; }), occupiedStands_.end());

	blockedStands_.erase(std::remove_if(blockedStands_.begin(), blockedStands_.end(),
		[callsign](const Stand& s) { return s.callsign == callsign; }), blockedStands_.end());
}

void DataManager::addStandToOccupied(const Stand& stand)
{
	{
		std::lock_guard<std::mutex> lock(dataMutex_);
		if (std::find(occupiedStands_.begin(), occupiedStands_.end(), stand) == occupiedStands_.end()) {
			occupiedStands_.push_back(stand);
		}
	}

	// Check if stand blocking other stands
	std::vector<Stand> stands = getAllStandsForAirport(stand.icao);

	auto it = std::find_if(stands.begin(), stands.end(), [&stand](const Stand& s) { return s.name == stand.name; });
	if (it != stands.end()) {
		// Check if the stand is blocking other stands
		if (configJson_.contains("Stands") && configJson_["Stands"].contains(stand.name)) {
			const auto& standJson = configJson_["Stands"][stand.name];
			if (standJson.contains("Block") && standJson["Block"].is_array())
			{
				for (const auto& blockedStandName : standJson["Block"]) {
					Stand blockedStand;
					blockedStand.name = blockedStandName.get<std::string>();
					blockedStand.icao = stand.icao;
					blockedStand.callsign = stand.callsign;
					std::lock_guard<std::mutex> lock(dataMutex_);
					if (std::find(blockedStands_.begin(), blockedStands_.end(), blockedStand) == blockedStands_.end()) {
						blockedStands_.push_back(blockedStand);
					}
				}
			}
		}
	}
}

bool DataManager::saveDownloadedAirportConfig(const nlohmann::ordered_json& json, std::string icao)
{
	std::lock_guard<std::mutex> lock(dataMutex_);
	std::transform(icao.begin(), icao.end(), icao.begin(), ::toupper);
	std::string fileName = icao + ".json";
	std::filesystem::path jsonPath = configPath_ / "Plugins/NeoSTAND" / fileName;
	std::ofstream configFile(jsonPath);
	if (!configFile.is_open()) {
		loggerAPI_->log(Logger::LogLevel::Error, "Could not open file to save downloaded config: " + jsonPath.string());
		return false;
	}
	try {
		configFile << std::setw(4) << json << std::endl;
		configsDownloaded_.insert(icao);
	}
	catch (...) {
		loggerAPI_->log(Logger::LogLevel::Error, "Error writing to file: " + jsonPath.string());
		return false;
	}

	// Always update in-memory representation with the freshly downloaded JSON.
	configJson_ = json;

	return true;
}

bool DataManager::printToFile(const std::vector<std::string>& lines, const std::string& fileName)
{
	std::lock_guard<std::mutex> lock(dataMutex_);
	std::filesystem::path dir = configPath_ / "logs" / "plugins" / "NeoSTAND";
	std::error_code ec;
	if (!std::filesystem::exists(dir))
	{
		if (!std::filesystem::create_directories(dir, ec))
		{
			loggerAPI_->log(Logger::LogLevel::Error,
				"Failed to create log directory: " + dir.string() + " ec=" + ec.message());
			return false;
		}
	}
	std::filesystem::path filePath = dir / fileName;
	std::ofstream outFile(filePath);
	if (!outFile.is_open()) {
		loggerAPI_->log(Logger::LogLevel::Error, "Could not open file to write: " + filePath.string());
		return false;
	}
	for (const auto& line : lines) {
		outFile << line << std::endl;
	}
	outFile.close();
	return true;
}

std::string DataManager::isAircraftOnStand(const std::string& callsign, const std::string& icao)
{
	std::optional<Aircraft::Aircraft> aircraftOpt = aircraftAPI_->getByCallsign(callsign);
	std::optional<Flightplan::Flightplan> flightplanOpt = flightplanAPI_->getByCallsign(callsign);
	if (!aircraftOpt.has_value()) return "";

	Aircraft::Aircraft aircraft = *aircraftOpt;

	if (!aircraft.position.onGround) return "";

	std::string icaoFromAircraft;
	if (icao.empty()) {
		std::optional<double> distOrigin = aircraftAPI_->getDistanceFromOrigin(callsign);
		std::optional<double> distDest = aircraftAPI_->getDistanceToDestination(callsign);
		if (!distOrigin.has_value() || !distDest.has_value()) return "";
		if (*distOrigin - *distDest > 0.) icaoFromAircraft = flightplanOpt->destination;
		else icaoFromAircraft = flightplanOpt->origin;
	}
	else {
		icaoFromAircraft = icao;
	}

	std::vector<std::string> activeAirports = getAllActiveAirports();
	if (std::find(activeAirports.begin(), activeAirports.end(), icaoFromAircraft) == activeAirports.end()) return "";

	auto haversineMeters = [](double lat1Deg, double lon1Deg, double lat2Deg, double lon2Deg) -> double {
		constexpr double kPi = 3.14159265358979323846;
		constexpr double kR = 6371000.0; // meters
		auto rad = [&](double d) { return d * kPi / 180.0; };
		double lat1 = rad(lat1Deg), lon1 = rad(lon1Deg);
		double lat2 = rad(lat2Deg), lon2 = rad(lon2Deg);
		double dLat = lat2 - lat1;
		double dLon = lon2 - lon1;
		double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
			std::cos(lat1) * std::cos(lat2) *
			std::sin(dLon / 2) * std::sin(dLon / 2);
		double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
		return kR * c;
		};

	// Load stands for the airport
	std::transform(icaoFromAircraft.begin(), icaoFromAircraft.end(), icaoFromAircraft.begin(), ::toupper);
	if (!retrieveCorrectConfigJson(icaoFromAircraft)) {
		loggerAPI_->log(Logger::LogLevel::Warning, "Failed to retrieve config when assigning Stand for: " + icaoFromAircraft);
		return "";
	}

	std::lock_guard<std::mutex> lock(dataMutex_);
	nlohmann::json standsJson;
	if (configJson_.contains("Stands")) {
		standsJson = configJson_["Stands"];
	}
	else {
		loggerAPI_->log(Logger::LogLevel::Warning, "No STAND section in config for: " + icaoFromAircraft);
		return "";
	}

	constexpr double kDefaultRadiusMeters = 30.0;

	for (auto it = standsJson.begin(); it != standsJson.end(); ++it) {
		const auto& stand = *it;
		if (!stand.contains("Coordinates")) continue;

		const std::string coordStr = stand["Coordinates"].get<std::string>();

		// Split "lat:lon[:radius]"
		size_t p1 = coordStr.find(':');
		if (p1 == std::string::npos) continue;
		size_t p2 = coordStr.find(':', p1 + 1);

		std::string latStr = coordStr.substr(0, p1);
		std::string lonStr = (p2 == std::string::npos) ? coordStr.substr(p1 + 1)
			: coordStr.substr(p1 + 1, p2 - p1 - 1);
		std::string radiusStr = (p2 == std::string::npos) ? std::string() : coordStr.substr(p2 + 1);

		double standLat = 0.0, standLon = 0.0;
		try {
			standLat = std::stod(latStr); // decimal degrees; allow negative for S/W
			standLon = std::stod(lonStr);
		}
		catch (...) {
			continue; // skip malformed entries
		}

		double radiusMeters = kDefaultRadiusMeters;
		if (!radiusStr.empty()) {
			try {
				radiusMeters = std::stod(radiusStr);
				if (radiusMeters <= 0) radiusMeters = kDefaultRadiusMeters;
			}
			catch (...) {
				radiusMeters = kDefaultRadiusMeters;
			}
		}

		double distanceMeters = haversineMeters(
			aircraft.position.latitude, aircraft.position.longitude,
			standLat, standLon
		);

		if (distanceMeters <= radiusMeters) {
			return it.key() + " " + icaoFromAircraft;
		}
	}

	return "";
}

void DataManager::PopulateActiveAirports()
{
	std::vector<Airport::AirportConfig> airports = airportAPI_->getConfigurations();
	std::vector<std::string> activeAirports;

	for (const auto& airport : airports) {
		if (airport.status == Airport::AirportStatus::Active) {
			activeAirports.push_back(airport.icao);
		}
	}

	std::lock_guard<std::mutex> lock(dataMutex_);
	activeAirports_ = std::move(activeAirports);
}

std::vector<std::string> DataManager::getAllActiveAirports()
{
	std::lock_guard<std::mutex> lock(dataMutex_);
	return activeAirports_;
}

std::vector<DataManager::Pilot> DataManager::getAllPilots()
{
	std::lock_guard<std::mutex> lock(dataMutex_);
	return pilots_;
}

bool DataManager::pilotExists(const std::string& callsign)
{
	if (callsign.empty()) return false;
	std::lock_guard<std::mutex> lock(dataMutex_);
	return std::any_of(pilots_.begin(), pilots_.end(),
		[&callsign](const Pilot& p) { return p.callsign == callsign; });
}

DataManager::Pilot* DataManager::getPilotByCallsign(const std::string& callsign)
{
	std::lock_guard<std::mutex> lock(dataMutex_);
	for (auto& pilot : pilots_)
	{
		if (pilot.callsign == callsign)
			return &pilot;
	}
	return nullptr;
}

void DataManager::updateAllPilots()
{
	std::vector<Flightplan::Flightplan> flightplans = flightplanAPI_->getAll();
	for (const auto& fp : flightplans) {
		updatePilot(fp.callsign);
	}
}

void DataManager::updatePilot(const std::string& callsign)
{
	if (callsign.empty()) return;
	
	std::optional<Aircraft::Aircraft> aircraftOpt = aircraftAPI_->getByCallsign(callsign);
	if (!aircraftOpt.has_value()) return;
	
	Aircraft::Aircraft aircraft = *aircraftOpt;
	if (aircraft.position.altitude > getMaxAltitude()) return;

	std::optional<Flightplan::Flightplan> flightplan = flightplanAPI_->getByCallsign(aircraft.callsign);
	if (!flightplan.has_value()) return;

	std::optional<double> distanceToDest = aircraftAPI_->getDistanceToDestination(aircraft.callsign);
	if (!distanceToDest.has_value() || *distanceToDest > getMaxDistance()) return;

	if (!isConcernedAircraft(*flightplan)) return;

	std::string previousStand = "";

	{
		std::lock_guard<std::mutex> lock(dataMutex_);
		auto itPilot = std::find_if(pilots_.begin(), pilots_.end(),
			[&callsign](const Pilot& p) { return p.callsign == callsign; });

		std::string freedIcao;
		if (itPilot != pilots_.end()) {
			previousStand = itPilot->stand;
		}
		pilots_.erase(std::remove_if(pilots_.begin(), pilots_.end(), [&aircraft](const Pilot& p) { return p.callsign == aircraft.callsign; }), pilots_.end());
	}

	Pilot pilot;
	pilot.callsign = aircraft.callsign;
	pilot.destination = flightplan->destination;
	pilot.isSchengen = isSchengen(*flightplan);
	pilot.isNational = isNational(*flightplan);
	pilot.aircraftType = getAircraftType(*flightplan);
	pilot.aircraftCode = getAircraftCode(flightplan->acType);
	pilot.stand = previousStand;

	{
		std::lock_guard<std::mutex> lock(dataMutex_);
		pilots_.push_back(pilot);
	}
}

void DataManager::removeAllPilots()
{
	std::lock_guard<std::mutex> lock(dataMutex_);
	pilots_.clear();
	occupiedStands_.clear();
	blockedStands_.clear();
}

DataManager::AircraftType DataManager::getAircraftType(const Flightplan::Flightplan& fp)
{
	//IMPROVE: parse from Config.json all the types so it can be modified by user
	std::string callsign = fp.callsign;
	std::transform(callsign.begin(), callsign.end(), callsign.begin(), ::toupper);

	if (callsign.size() < 3) return AircraftType::generalAviation;
	if (callsign[1] == '-' || callsign[2] == '-') return AircraftType::generalAviation;
	
	if (cargo.contains(callsign.substr(0, 3))) return AircraftType::cargo;
	
	std::string acType = fp.acType;
	if (heliTypes.contains(acType)) return AircraftType::helicopter;

	if (militaryTypes.contains(acType)) return AircraftType::military;

	if (gaTypes.contains(acType)) return AircraftType::generalAviation;

	return AircraftType::airliner;
}

std::string DataManager::getAircraftCode(const std::string& acType)
{
	std::lock_guard<std::mutex> lock(dataMutex_);
	
	std::string acTypeUpper = acType;
	std::transform(acTypeUpper.begin(), acTypeUpper.end(), acTypeUpper.begin(), ::toupper);

	auto it = aircraftWingspans_.find(acTypeUpper);
	if (it == aircraftWingspans_.end()) return "F";
	
	double span = it->second;

	// A: <15, B: <24, C: <36, D: <52, E: <65, F: <80
	if (span < 15.0) return "A";
	if (span < 24.0) return "B";
	if (span < 36.0) return "C";
	if (span < 52.0) return "D";
	if (span < 65.0) return "E";
	if (span < 80.0) return "F";
	return "F";
}

std::vector<DataManager::Stand> DataManager::getOccupiedStands()
{
	std::lock_guard<std::mutex> lock(dataMutex_);
	return occupiedStands_;
}

std::vector<DataManager::Stand> DataManager::getBlockedStands()
{
	std::lock_guard<std::mutex> lock(dataMutex_);
	return blockedStands_;
}

std::vector<DataManager::Stand> DataManager::getAllStandsForAirport(const std::string& icao)
{
	if (!retrieveCorrectConfigJson(icao)) {
		loggerAPI_->log(Logger::LogLevel::Warning, "Failed to retrieve config when assigning Stand for: " + icao);
		return {};
	}

	std::lock_guard<std::mutex> lock(dataMutex_);
	nlohmann::json standsJson;
	if (configJson_.contains("Stands")) {
		standsJson = configJson_["Stands"];
	}
	else {
		loggerAPI_->log(Logger::LogLevel::Warning, "No STAND section in config for: " + icao);
		return {};
	}
	std::vector<Stand> stands;
	for (auto it = standsJson.begin(); it != standsJson.end(); ++it) {
		Stand stand;
		stand.name = it.key();
		stand.icao = icao;
		stand.callsign = "";
		stands.push_back(stand);
	}

	return stands;
}

std::vector<DataManager::Stand> DataManager::getAvailableStandsForAirport(const std::string& icao)
{
	std::vector<Stand> allStands = getAllStandsForAirport(icao);
	std::vector<Stand> availableStands;
	for (const auto& stand : allStands) {
		bool isOccupied = std::find_if(occupiedStands_.begin(), occupiedStands_.end(),
			[&stand](const Stand& s) { return s.name == stand.name && s.icao == stand.icao; }) != occupiedStands_.end();
		bool isBlocked = std::find_if(blockedStands_.begin(), blockedStands_.end(),
			[&stand](const Stand& s) { return s.name == stand.name && s.icao == stand.icao; }) != blockedStands_.end();
		if (!isOccupied && !isBlocked) {
			availableStands.push_back(stand);
		}
	}
	return availableStands;
}

bool DataManager::isConcernedAircraft(const Flightplan::Flightplan& fp)
{
	std::lock_guard<std::mutex> lock(dataMutex_);
	std::vector<std::string> activeAirports = activeAirports_;
	auto itOrigin = std::find(activeAirports.begin(), activeAirports.end(), fp.origin);
	auto itDestination = std::find(activeAirports.begin(), activeAirports.end(), fp.destination);
	if (itOrigin != activeAirports.end() || itDestination != activeAirports.end())
		return true;
	return false;
}

bool DataManager::isArrival(const std::string& callsign)
{
	std::optional<Flightplan::Flightplan> flightplanOpt = flightplanAPI_->getByCallsign(callsign);
	if (!flightplanOpt.has_value()) return false;
	Flightplan::Flightplan fp = *flightplanOpt;

	std::vector<std::string> activeAirports = getAllActiveAirports();

	return std::find(activeAirports.begin(), activeAirports.end(), fp.destination) != activeAirports.end();
}

bool DataManager::isSchengen(const Flightplan::Flightplan& fp)
{
	auto isInSchengen = [](std::string icao) -> bool {
		if (icao.size() < 2) return false;

		std::transform(icao.begin(), icao.end(), icao.begin(), ::toupper);
		
		const std::string prefix = icao.substr(0, 2);

		return
			prefix == "LF" || // France
			prefix == "LS" || // Switzerland
			prefix == "ED" || // Germany (civil)
			prefix == "ET" || // Germany (military)
			prefix == "LO" || // Austria
			prefix == "EB" || // Belgium
			prefix == "EL" || // Luxembourg
			prefix == "EH" || // Netherlands
			prefix == "EK" || // Denmark
			prefix == "ES" || // Sweden
			prefix == "EN" || // Norway
			prefix == "EF" || // Finland
			prefix == "EE" || // Estonia
			prefix == "EV" || // Latvia
			prefix == "EY" || // Lithuania
			prefix == "EP" || // Poland
			prefix == "LK" || // Czech Republic
			prefix == "LZ" || // Slovakia
			prefix == "LH" || // Hungary
			prefix == "LJ" || // Slovenia
			prefix == "LD" || // Croatia
			prefix == "LI" || // Italy
			prefix == "LG" || // Greece
			prefix == "LE" || // Spain
			prefix == "LP" || // Portugal
			prefix == "LM" || // Malta
			prefix == "BI" || // Iceland
			prefix == "LB" || // Bulgaria
			prefix == "LR";   // Romania
		};

	return isInSchengen(fp.origin) && isInSchengen(fp.destination);
}

bool DataManager::isNational(const Flightplan::Flightplan& fp)
{
	std::string origin = fp.origin;
	std::string destination = fp.destination;
	return (origin.substr(0, 2) == destination.substr(0, 2));
}

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
	bool success = parseSettings();
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
    std::string fileName = icao + ".json";
    std::filesystem::path jsonPath = configPath_ / "Plugins" / "NeoSTAND" / fileName;

    std::ifstream config(jsonPath);
    if (!config.is_open()) {
        DisplayMessageFromDataManager("Could not open JSON file: " + jsonPath.string(), "DataManager");
        loggerAPI_->log(Logger::LogLevel::Error, "Could not open JSON file: " + jsonPath.string());
        return -1;
    }

    nlohmann::ordered_json parsed;
    try {
        config >> parsed;
        if (parsed.contains("version")) {
            if (!isCorrectJsonVersion(parsed["version"].get<std::string>(), fileName)) {
                return -1;
            }
        }
        else {
            DisplayMessageFromDataManager("Config version missing in JSON file: " + fileName, "DataManager");
        }
    }
    catch (...) {
        DisplayMessageFromDataManager("Error parsing JSON file: " + jsonPath.string(), "DataManager");
        loggerAPI_->log(Logger::LogLevel::Error, "Error parsing JSON file: " + jsonPath.string());
        return -1;
    }

    configJson_ = std::move(parsed);
    

    return 0;
}

bool DataManager::retrieveCorrectConfigJson(const std::string& icao)
{
	if (!configJson_.contains(icao) || configJson_.empty()) {
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
	std::lock_guard<std::mutex> lock(dataMutex_);
	if (!pilot) return;
	// Check if configJSON is already the right one, if not, retrieve it
	std::string icao = pilot->destination;
	std::transform(icao.begin(), icao.end(), icao.begin(), ::toupper);
	if (!retrieveCorrectConfigJson(icao)) {
		loggerAPI_->log(Logger::LogLevel::Warning, "Failed to retrieve config when assigning Stand for: " + icao);
		pilot->stand = "";
		return;
	}
	
	nlohmann::json standsJson;
	if (configJson_.contains("Stands")) {
		standsJson = configJson_["Stands"];
		loggerAPI_->log(Logger::LogLevel::Info, "Assigning stand for pilot: " + pilot->callsign + " at " + pilot->destination);
	}
	else {
		loggerAPI_->log(Logger::LogLevel::Warning, "No STAND section in config for: " + icao);
		pilot->stand = "";
		return;
	}

	LOG_DEBUG(Logger::LogLevel::Info, "Total stands available before filtering: " + std::to_string(standsJson.size()));

	// Filter stands based on criteria
	auto it = standsJson.begin();

	while (it != standsJson.end()) {
		const auto& stand = *it;
		// Check WTC
		if (stand.contains("WTC")) {
			std::string wtc = stand["WTC"].get<std::string>();
			if (wtc.find(pilot->aircraftWTC) == std::string::npos) {
				LOG_DEBUG(Logger::LogLevel::Info, "Removing stand " + it.key() + " due to WTC mismatch. Stand: " + wtc + " Pilot: " + pilot->aircraftWTC);
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
			if (use != pilotType) {
				LOG_DEBUG(Logger::LogLevel::Info, "Removing stand " + it.key() + " due to Use mismatch. Stand: " + use + " Pilot: " + pilotType);
				it = standsJson.erase(it);
				continue;
			}
		}

		// Check SCHENGEN
		if (stand.contains("Schengen")) {
			bool schegen = stand["Schengen"].get<bool>();
			if (schegen == true && pilot->isSchengen == false) {
				LOG_DEBUG(Logger::LogLevel::Info, "Removing stand " + it.key() + " due to Schengen mismatch. Stand: " + (schegen ? "true" : "false") + " Pilot: " + (pilot->isSchengen ? "true" : "false"));
				it = standsJson.erase(it);
				continue;
			}
		}

		// Check NATIONAL
		if (stand.contains("National")) {
			bool national = stand["National"].get<bool>();
			if (national != pilot->isNational) {
				LOG_DEBUG(Logger::LogLevel::Info, "Removing stand " + it.key() + " due to National mismatch. Stand: " + (national ? "true" : "false") + " Pilot: " + (pilot->isNational ? "true" : "false"));
				it = standsJson.erase(it);
				continue;
			}
		}

		// Check Callsigns
		if (stand.contains("Callsigns")) {
			std::vector<std::string> callsigns = stand["Callsigns"].get<std::vector<std::string>>();
			if (callsign.length() < 3 || std::find(callsigns.begin(), callsigns.end(), pilot->callsign.substr(0, 3)) == callsigns.end()) {
				LOG_DEBUG(Logger::LogLevel::Info, "Removing stand " + it.key() + " due to Callsign mismatch. Pilot: " + pilot->callsign);
				it = standsJson.erase(it);
				continue;
			}
		}

		// Check if stand is occupied
		if (std::find_if(occupiedStands_.begin(), occupiedStands_.end(), [&it, icao](const Stand& stand){ return it.key() == stand.name && icao == stand.icao;}) != occupiedStands_.end()) {
			LOG_DEBUG(Logger::LogLevel::Info, "Removing stand " + it.key() + " because it is already occupied.");
			it = standsJson.erase(it);
			continue;
		}
		/*else if (isAircraftOnStand(stand["Coordinates"].get<std::string>())) {
			it = standsJson.erase(it);
			continue;
		}*/

		// Check if stand is blocked
		if (std::find_if(blockedStands_.begin(), blockedStands_.end(), [&it, icao](const Stand& stand) { return it.key() == stand.name && icao == stand.icao; }) != blockedStands_.end()) {
			LOG_DEBUG(Logger::LogLevel::Info, "Removing stand " + it.key() + " because it is blocked.");
			it = standsJson.erase(it);
			continue;
		}

		++it; // Only increment if not erased
	}

	if (standsJson.empty()) {
		loggerAPI_->log(Logger::LogLevel::Warning, "No suitable stand found for pilot: " + pilot->callsign + " at " + pilot->destination);
		DisplayMessageFromDataManager("No suitable stand found for pilot: " + pilot->callsign + " at " + pilot->destination);
		pilot->stand = "";
		return;
	}

	LOG_DEBUG(Logger::LogLevel::Info, "Total stands available after filtering: " + std::to_string(standsJson.size()));

	// Randomly select a stand from the filtered list (object-safe)
	std::srand(static_cast<unsigned int>(std::time(nullptr)));
	const size_t count = standsJson.size();

	auto itSel = standsJson.begin();
	std::advance(itSel, std::rand() % count);

	const std::string selectedStandName = itSel.key();
	const auto& selectedStand = itSel.value();
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
	std::lock_guard<std::mutex> lock(dataMutex_);
	if (std::find(occupiedStands_.begin(), occupiedStands_.end(), stand) == occupiedStands_.end()) {
		occupiedStands_.push_back(stand);
	}
}

std::string DataManager::isAircraftOnStand(const std::string& callsign)
{
	std::optional<Aircraft::Aircraft> aircraftOpt = aircraftAPI_->getByCallsign(callsign);
	std::optional<Flightplan::Flightplan> flightplanOpt = flightplanAPI_->getByCallsign(callsign);
	if (!aircraftOpt.has_value() || !flightplanOpt.has_value()) return "";

	Aircraft::Aircraft aircraft = *aircraftOpt;

	if (!aircraft.position.onGround) return "";

	std::string icao;
	std::optional<double> distOrigin = aircraftAPI_->getDistanceFromOrigin(callsign);
	std::optional<double> distDest = aircraftAPI_->getDistanceToDestination(callsign);
	if (!distOrigin.has_value() || !distDest.has_value()) return "";
	if (*distOrigin - *distDest > 0.) icao = flightplanOpt->destination;
	else icao = flightplanOpt->origin;

	std::vector<std::string> activeAirports = getAllActiveAirports();
	if (std::find(activeAirports.begin(), activeAirports.end(), icao) == activeAirports.end()) return "";

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
	std::lock_guard<std::mutex> lock(dataMutex_);
	std::transform(icao.begin(), icao.end(), icao.begin(), ::toupper);
	if (!retrieveCorrectConfigJson(icao)) {
		loggerAPI_->log(Logger::LogLevel::Warning, "Failed to retrieve config when assigning Stand for: " + icao);
		return "";
	}

	nlohmann::json standsJson;
	if (configJson_.contains("Stands")) {
		standsJson = configJson_["Stands"];
	}
	else {
		loggerAPI_->log(Logger::LogLevel::Warning, "No STAND section in config for: " + icao);
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
			return it.key() + " " + icao;
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
	LOG_DEBUG(Logger::LogLevel::Info, "Total flightplans retrieved (updateAllPilot): " + std::to_string(flightplans.size()));
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
	pilot.aircraftWTC = flightplan->wakeCategory;
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
	
	static const std::unordered_set<std::string> cargo = {
			"FDX","UPS","GTI","CLX","CKS","BCS","GEC","ABW","NCA","RCH","SQC","CMB","BOX","MPH","TAY","QAJ","ICV","KYE","ACX","BRQ"
	};
	if (cargo.contains(callsign.substr(0, 3))) return AircraftType::cargo;
	
	static const std::unordered_set<std::string> heliTypes = {
			"H145","H135","EC135","EC145","EC130","EC120","EC155","AS350","AS355","AS365","AS565",
			"R22","R44","R66","B06","B407","B412","B429","B430","S76","S92","MD500","MD520","MD530",
			"AW109","AW139","AW169","AW189","UH60","H60","MI8","MI17","SA330","SA341","SA342"
	};
	std::string acType = fp.acType;
	if (heliTypes.contains(acType)) return AircraftType::helicopter;

	static const std::unordered_set<std::string> militaryTypes = {
			"F16","F18","F22","F35","A10","B52","C130","C17","KC135","KC10","E3","E6","P8","T38",
			"AH64","UH60","CH47","F15","AV8B","EA18G","C5M","C40B","C37A","C37B","C32B"
	};
	if (militaryTypes.contains(acType)) return AircraftType::military;

	static const std::unordered_set<std::string> gaTypes = {
		"C150","C152","C172","C175","C177","C182","C185","C206","C207","C210","C337","C340","C402","C414","C421","C208",
		"PA18","PA28","P28A","PA32","PA34","PA44","PA46","P46T","PA24","PA30",
		"BE33","BE35","BE36","BE55","BE58","BE9L","BE10","BE20","B350",
		"SR20","SR22","SR22T", "DA20","DA40","DA42","DA50","DA62", "M20P","M20T","M20R","M20J",
		"DR40","DR221","DR253","DR300","DR315","DR400", "TB10","TB20","TB21","TBM7","TBM8","TBM9",
		"PC12","PC6", "RV6","RV7","RV8","RV9","RV10","RV12", "C510","C525","C25A","C25B","C25C","C560","C56X","C650","C680","C68A","C700",
		"E50P","E55P","E545","E35L","E545","E550", "LJ24","LJ31","LJ35","LJ40","LJ45","LJ55","LJ60","LJ70","LJ75",
		"FA20","FA50","FA7X","FA8X","FA900", "GLF2","GLF3","GLF4","GLF5","GLF6","GLF650","GLF7","GLF8",
		"CL30","CL35","CL60","G150","G200","G280"
	};
	if (gaTypes.contains(acType)) return AircraftType::generalAviation;

	return AircraftType::airliner;
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

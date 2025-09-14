#include <algorithm>
#include <fstream>

#include "../NeoSTAND.h"
#include "DataManager.h"

#if defined(_WIN32)
#include <Windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#elif defined(__APPLE__) || defined(__linux__)
#include <dlfcn.h>
#include <cstdlib>
#endif

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
}


std::filesystem::path DataManager::getDllDirectory()
{
#if defined(_WIN32)
	PWSTR path = nullptr;
	HRESULT hr = SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &path);
	std::filesystem::path documentsPath;
	if (SUCCEEDED(hr)) {
		documentsPath = path;
		CoTaskMemFree(path);
	}
	return documentsPath / "NeoRadar/plugins";
#elif defined(__APPLE__) || defined(__linux__)
	const char* homeDir = std::getenv("HOME");
	if (homeDir) {
		return std::filesystem::path(homeDir) / "Documents" / "NeoRadar/plugins";
	}
	return std::filesystem::path(); // Return empty path if HOME is not set
#else
	return std::filesystem::path(); // Return an empty path for unsupported platforms
#endif
}

void DataManager::clearData()
{
	configPath_.clear();
	pilots_.clear();
	activeAirports_.clear();
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
    std::filesystem::path jsonPath = configPath_ / "NeoSTAND" / fileName;

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

    {
        std::lock_guard<std::mutex> lock(dataMutex_);
        configJson_ = std::move(parsed);
    }

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

bool DataManager::removePilot(const std::string& callsign)
{
    std::lock_guard<std::mutex> lock(dataMutex_);
    const size_t initial = pilots_.size();
    pilots_.erase(std::remove_if(pilots_.begin(), pilots_.end(),
        [&callsign](const Pilot& p) { return p.callsign == callsign; }), pilots_.end());
    return pilots_.size() < initial;
}

void DataManager::clearPilots()
{
	std::lock_guard<std::mutex> lock(dataMutex_);
	pilots_.clear();
}

void DataManager::assignStands(Pilot& pilot)
{
	std::lock_guard<std::mutex> lock(dataMutex_);
	// Check if configJSON is already the right one, if not, retrieve it
	if (!retrieveCorrectConfigJson(pilot.destination)) {
		loggerAPI_->log(Logger::LogLevel::Warning, "Failed to retrieve config when assigning CFL for: " + pilot.destination);
		pilot.stand = "";
		return;
	}
	
	nlohmann::json standsJson;
	if (configJson_.contains("STAND"))
		standsJson = configJson_["STAND"];
	else {
		loggerAPI_->log(Logger::LogLevel::Warning, "No STAND section in config for: " + pilot.destination);
		pilot.stand = "";
		return;
	}

	// Filter stands based on criteria
	auto it = standsJson.begin();

	while (it != standsJson.end()) {
		const auto& stand = *it;
		// Check WTC
		if (stand.contains("WTC")) {
			std::string wtc = stand["WTC"].get<std::string>();
			if (wtc != "A" && wtc != pilot.aircraftWTC) {
				it = standsJson.erase(it);
				continue;
			}
		}

		// Check USE
		if (stand.contains("use")) {
			std::string use = stand["use"].get<std::string>();
			std::string pilotType;
			switch (pilot.aircraftType) {
			case AircraftType::airliner: pilotType = "A"; break;
			case AircraftType::generalAviation: pilotType = "P"; break;
			case AircraftType::helicopter: pilotType = "H"; break;
			case AircraftType::military: pilotType = "M"; break;
			case AircraftType::cargo: pilotType = "C"; break;
			default: pilotType = ""; break;
			}
			if (use != pilotType) {
				it = standsJson.erase(it);
				continue;
			}
		}

		// Check SHENGEN
		if (stand.contains("shengen")) {
			bool shengen = stand["shengen"].get<bool>();
			if (shengen != pilot.isShengen) {
				it = standsJson.erase(it);
				continue;
			}
		}

		// Check NATIONAL
		if (stand.contains("national")) {
			bool national = stand["national"].get<bool>();
			if (national != pilot.isNational) {
				it = standsJson.erase(it);
				continue;
			}
		}

		// Check if stand is occupied
		// Check if stand is blocked

		++it; // Only increment if not erased
	}

	if (standsJson.empty()) {
		loggerAPI_->log(Logger::LogLevel::Warning, "No suitable stand found for pilot: " + pilot.callsign + " at " + pilot.destination);
		pilot.stand = "";
		return;
	}

	// Randomly select a stand from the filtered list



	// Assigner le stand au pilot
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

DataManager::Pilot DataManager::getPilotByCallsign(const std::string& callsign)
{
	std::lock_guard<std::mutex> lock(dataMutex_);
	for (const auto& pilot : pilots_)
	{
		if (pilot.callsign == callsign)
			return pilot;
	}
	return Pilot{};
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
	if (aircraft.position.altitude > stand::MAX_ALTITUDE) return;

	std::optional<Flightplan::Flightplan> flightplan = flightplanAPI_->getByCallsign(aircraft.callsign);
	if (!flightplan.has_value()) return;

	std::optional<double> distanceToDest = aircraftAPI_->getDistanceToDestination(aircraft.callsign);
	if (!distanceToDest.has_value() || *distanceToDest > stand::MAX_DISTANCE) return;

	if (!isConcernedAircraft(*flightplan)) return;

	{
		std::lock_guard<std::mutex> lock(dataMutex_);
		if (!pilots_.empty() && std::any_of(pilots_.begin(), pilots_.end(),
			[&aircraft](const Pilot& p) { return p.callsign == aircraft.callsign; })) {
			return; // Skip if pilot already exists
		}
	}

	Pilot pilot;
	pilot.callsign = aircraft.callsign;
	pilot.destination = flightplan->destination;
	pilot.isShengen = isShengen(*flightplan);
	pilot.isNational = isNational(*flightplan);
	pilot.aircraftType = getAircraftType(*flightplan);
	pilot.aircraftWTC = flightplan->wakeCategory;
	pilot.stand = "";

	{
		std::lock_guard<std::mutex> lock(dataMutex_);
		pilots_.push_back(pilot);
	}
}

void DataManager::removeAllPilots()
{
	std::lock_guard<std::mutex> lock(dataMutex_);
	pilots_.clear();
}

DataManager::AircraftType DataManager::getAircraftType(const Flightplan::Flightplan& fp)
{
	//IMPROVE: parse from Config.json all the types so it can be modified by user
	std::string callsign = fp.callsign;
	std::transform(callsign.begin(), callsign.end(), callsign.begin(), ::toupper);
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

bool DataManager::isShengen(const Flightplan::Flightplan& fp)
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

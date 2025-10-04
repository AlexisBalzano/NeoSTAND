#pragma once
#include <vector>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <mutex>
#include <unordered_set>

namespace stand
{
	// Default settings value
	constexpr const double MAX_DISTANCE = 50.0; // Max distance to consider an aircraft (in NM)
	constexpr const int MAX_ALTITUDE = 10000; // Max altitude to consider an aircraft (in feet)
	constexpr const int DEFAULT_UPDATE_INTERVAL = 5; // Interval to update stands (in seconds)
}

class DataManager {
public:
	enum class AircraftType
	{
		airliner = 0,
		generalAviation,
		helicopter,
		military,
		cargo
	};

	struct Pilot {
		std::string callsign;
		std::string origin;
		std::string destination;
		std::string aircraftCode;
		AircraftType aircraftType;
		std::string stand;
		bool isSchengen;

		bool empty() const {
			return callsign.empty();
		}
	};

	struct Stand {
		std::string name;
		std::string icao;
		std::string callsign;

		bool operator==(const Stand& other) const {
			return name == other.name && icao == other.icao && callsign == other.callsign;
		}
	};

public:
	DataManager(stand::NeoSTAND* neoSTAND);
	~DataManager() = default;
	DataManager(const DataManager&) = delete;
	DataManager& operator=(const DataManager&) = delete;

	void clearData();
	void clearJson();

	std::filesystem::path getDllDirectory();
	void DisplayMessageFromDataManager(const std::string& message, const std::string& sender = "");
	int retrieveConfigJson(const std::string& icao);
	bool retrieveCorrectConfigJson(const std::string& icao);
	bool isCorrectJsonVersion(const std::string& config_version, const std::string& fileName);
	void loadSettingJson();
	bool parseSettings();
	void PopulateActiveAirports();
	void updateAllPilots();
	void updatePilot(const std::string& callsign);
	void removeAllPilots();
	bool removePilot(const std::string& callsign);
	void assignStands(const std::string& callsign);
	void assignStandToPilot(const std::string& callsign, const std::string& standName);
	void freeStand(const std::string& standName);
	void addStandToOccupied(const Stand& stand);
	bool saveDownloadedAirportConfig(const nlohmann::ordered_json& json, std::string icao);
	bool printToFile(const std::vector<std::string>& lines, const std::string& fileName);
	bool updatePilotStand(const std::string& callsign, const std::string& standName);

	std::vector<std::string> getAllActiveAirports();
	std::vector<Pilot> getAllPilots();
	bool pilotExists(const std::string& callsign);
	std::optional<Pilot> getPilotByCallsign(const std::string& callsign);
	AircraftType getAircraftType(const Flightplan::Flightplan& fp);
	std::string getAircraftCode(const std::string& acType);
	std::vector<Stand> getOccupiedStands();
	std::vector<Stand> getBlockedStands();
	int getUpdateInterval() const { return updateInterval_; }
	int getMaxAltitude() const { return maxAltitude_; }
	double getMaxDistance() const { return maxDistance_; }
	std::vector<Stand> getAllStandsForAirport(const std::string& icao);
	std::vector<Stand> getAvailableStandsForAirport(const std::string& icao);
	std::string getConfigUrl() const { return configUrl_; }
	std::unordered_set<std::string> getCargoTypes() const { return cargo; }
	std::unordered_set<std::string> getHeliTypes() const { return heliTypes; }
	std::unordered_set<std::string> getMilitaryTypes() const { return militaryTypes; }
	std::unordered_set<std::string> getGATypes() const { return gaTypes; }
	
	bool isConcernedAircraft(const Flightplan::Flightplan& fp);
	bool isArrival(const std::string& callsign);
	bool isSchengen(const Flightplan::Flightplan& fp);
	std::string isAircraftOnStand(const std::string& callsign, const std::string& icao="");

private:
	static std::string toUpperCase(const std::string& str);

private:
	Aircraft::AircraftAPI* aircraftAPI_ = nullptr;
	Flightplan::FlightplanAPI* flightplanAPI_ = nullptr;
	Airport::AirportAPI* airportAPI_ = nullptr;
	PluginSDK::ControllerData::ControllerDataAPI* controllerDataAPI_ = nullptr;
	Chat::ChatAPI* chatAPI_ = nullptr;
	stand::NeoSTAND* neoSTAND_ = nullptr;
	PluginSDK::Logger::LoggerAPI* loggerAPI_ = nullptr;

	std::mutex dataMutex_;
	std::filesystem::path configPath_;
	nlohmann::ordered_json configJson_;
	nlohmann::ordered_json settingJson_;
	std::string configUrl_;
	std::vector<Pilot> pilots_;
	std::vector<std::string> activeAirports_;
	std::vector<Stand> occupiedStands_;
	std::vector<Stand> blockedStands_;

	std::unordered_set<std::string> configsError_;
	std::unordered_set<std::string> configsDownloaded_;
	std::unordered_set<std::string> callsignError_;

	std::unordered_set<std::string> gaTypes;
	std::unordered_set<std::string> militaryTypes;
	std::unordered_set<std::string> heliTypes;
	std::unordered_set<std::string> cargo;

	std::unordered_map<std::string, double> aircraftWingspans_;

	int updateInterval_;
	int maxAltitude_;
	double maxDistance_;
};
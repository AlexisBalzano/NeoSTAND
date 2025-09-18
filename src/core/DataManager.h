#pragma once
#include <vector>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <mutex>
#include <unordered_set>

using namespace PluginSDK;

namespace stand
{
	constexpr const int MAX_DISTANCE = 50; // Max distance to consider an aircraft (in NM)
	constexpr const int MAX_ALTITUDE = 10000; // Max altitude to consider an aircraft (in feet)
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
		std::string destination;
		std::string aircraftWTC;
		AircraftType aircraftType;
		std::string stand;
		bool isSchengen;
		bool isNational;

		bool empty() const {
			return callsign.empty();
		}
	};

	struct Stand {
		std::string name;
		std::string icao;
		std::string callsign;

		bool operator==(const Stand& other) const {
			return name == other.name;
		}
	};

public:
	DataManager(stand::NeoSTAND* neoSTAND);
	~DataManager() = default;

	void clearData();
	void clearJson();

	std::filesystem::path getDllDirectory();
	void DisplayMessageFromDataManager(const std::string& message, const std::string& sender = "");
	int retrieveConfigJson(const std::string& icao);
	bool retrieveCorrectConfigJson(const std::string& icao);
	bool isCorrectJsonVersion(const std::string& config_version, const std::string& fileName);
	void PopulateActiveAirports();
	void updateAllPilots();
	void updatePilot(const std::string& callsign);
	void removeAllPilots();
	bool removePilot(const std::string& callsign);
	void assignStands(const std::string& callsign);
	void assignStandToPilot(Pilot& pilot, const std::string& standName);
	void freeStand(const std::string& standName);
	void addStandToOccupied(const Stand& stand);
	std::string isAircraftOnStand(const std::string& callsign);

	std::vector<std::string> getAllActiveAirports();
	std::vector<Pilot> getAllPilots();
	bool pilotExists(const std::string& callsign);
	Pilot* getPilotByCallsign(const std::string& callsign);
	AircraftType getAircraftType(const Flightplan::Flightplan& fp);
	std::vector<Stand> getOccupiedStands();
	std::vector<Stand> getBlockedStands();
	
	bool isConcernedAircraft(const Flightplan::Flightplan& fp);
	bool isSchengen(const Flightplan::Flightplan& fp);
	bool isNational(const Flightplan::Flightplan& fp);

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
	std::vector<Pilot> pilots_;
	std::vector<std::string> activeAirports_;
	std::vector<Stand> occupiedStands_;
	std::vector<Stand> blockedStands_;

};
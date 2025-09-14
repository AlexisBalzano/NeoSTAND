#pragma once
#include <vector>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <mutex>
#include <unordered_set>

using namespace PluginSDK;

namespace stand
{
	constexpr const int MAX_DISTANCE = 25; // Max distance to consider an aircraft (in NM)
	constexpr const int MAX_ALTITUDE = 5000; // Max altitude to consider an aircraft (in feet)
}

class DataManager {
public:
	enum class AircraftType
	{
		commercial = 0,
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
		bool isShengen;
		bool isNational;

		bool empty() const {
			return callsign.empty();
		}
	};

public:
	DataManager(stand::NeoSTAND* neoSTAND);
	~DataManager() = default;

	void clearData();
	void clearJson();

	static std::filesystem::path getDllDirectory();
	void DisplayMessageFromDataManager(const std::string& message, const std::string& sender = "");
	int retrieveConfigJson(const std::string& oaci);
	bool retrieveCorrectConfigJson(const std::string& oaci);
	bool isCorrectJsonVersion(const std::string& config_version, const std::string& fileName);
	void PopulateActiveAirports();
	void updateAllPilots();
	void updatePilot(const std::string& callsign);
	void voidremoveAllPilots();
	bool removePilot(const std::string& callsign);
	void clearPilots();

	std::vector<std::string> getAllActiveAirports();
	std::vector<Pilot> getAllPilots();
	bool pilotExists(const std::string& callsign);
	Pilot getPilotByCallsign(const std::string& callsign);
	AircraftType getAircraftType(const Flightplan::Flightplan& fp);
	
	bool isConcernedAircraft(const std::string& callsign);
	bool isShengen(const Flightplan::Flightplan& fp);
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
};
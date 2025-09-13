#pragma once
#include <vector>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <mutex>
#include <unordered_set>

using namespace PluginSDK;

class DataManager {
public:
	DataManager(PLUGIN_NAMESPACE::PLUGIN_NAME* PLUGIN_NAME);
	~DataManager() = default;

	void clearData();
	void clearJson();

	static std::filesystem::path getDllDirectory();
	void DisplayMessageFromDataManager(const std::string& message, const std::string& sender = "");
	int retrieveConfigJson(const std::string& oaci);
	bool retrieveCorrectConfigJson(const std::string& oaci);
	bool isCorrectJsonVersion(const std::string& config_version, const std::string& fileName);

private:
	Aircraft::AircraftAPI* aircraftAPI_ = nullptr;
	Flightplan::FlightplanAPI* flightplanAPI_ = nullptr;
	Airport::AirportAPI* airportAPI_ = nullptr;
	PluginSDK::ControllerData::ControllerDataAPI* controllerDataAPI_ = nullptr;
	Chat::ChatAPI* chatAPI_ = nullptr;
	PLUGIN_NAMESPACE::PLUGIN_NAME* PLUGIN_NAME_ = nullptr;
	PluginSDK::Logger::LoggerAPI* loggerAPI_ = nullptr;

	std::filesystem::path configPath_;
	nlohmann::ordered_json configJson_;

	std::mutex dataMutex_;
};
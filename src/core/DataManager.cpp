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

DataManager::DataManager(stand::NeoSTAND* NeoSTAND)
	: NeoSTAND_(NeoSTAND) {
	aircraftAPI_ = NeoSTAND_->GetAircraftAPI();
	flightplanAPI_ = NeoSTAND_->GetFlightplanAPI();
	airportAPI_ = NeoSTAND_->GetAirportAPI();
	chatAPI_ = NeoSTAND_->GetChatAPI();
	loggerAPI_ = NeoSTAND_->GetLogger();
	controllerDataAPI_ = NeoSTAND->GetControllerDataAPI();

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
	configJson_.clear();
	configPath_.clear();
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

	
int DataManager::retrieveConfigJson(const std::string& oaci)
{
	std::lock_guard<std::mutex> lock(dataMutex_);
	std::string fileName = oaci + ".json";
	std::filesystem::path jsonPath = configPath_ / "NeoSTAND" / fileName;

	std::ifstream config(jsonPath);
	if (!config.is_open()) {
		DisplayMessageFromDataManager("Could not open JSON file: " + jsonPath.string(), "DataManager");
		loggerAPI_->log(Logger::LogLevel::Error, "Could not open JSON file: " + jsonPath.string());
		return -1;
	}

	try {
		config >> configJson_;
		if (configJson_.contains("version")) {
			if (!isCorrectJsonVersion(configJson_["version"].get<std::string>(), fileName)) {
				configJson_.clear();
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
	
	return 0;
}

bool DataManager::retrieveCorrectConfigJson(const std::string& oaci)
{
	if (!configJson_.contains(oaci) || configJson_.empty()) {
		if (retrieveConfigJson(oaci) == -1) return false;
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
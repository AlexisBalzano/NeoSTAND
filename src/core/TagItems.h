#pragma once
#include <chrono>
#include <format>
#include <string>

#include "NeoSTAND.h"

#ifdef DEV
#define LOG_DEBUG(loglevel, message) logger_->log(loglevel, message)
#else
#define LOG_DEBUG(loglevel, message) void(0)
#endif


namespace stand {
void NeoSTAND::RegisterTagItems()
{
    PluginSDK::Tag::TagItemDefinition tagDef;

    // Tag item def
    tagDef.name = "STAND";
    tagDef.defaultValue = "---";
    std::string tagID = tagInterface_->RegisterTagItem(tagDef);
    standItemId_ = tagID;
}

Color NeoSTAND::ColorizeStand() {
	//TODO: implement color logic based on stand assignment status
	return std::array<unsigned int, 3>{255, 255, 255};
}

}  // namespace stand
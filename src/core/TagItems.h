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

using namespace stand;

namespace stand {
void NeoSTAND::RegisterTagItems()
{
    PluginSDK::Tag::TagItemDefinition tagDef;

    // Tag item def
    tagDef.name = "TAG ITEM";
    tagDef.defaultValue = "PLACEHOLDER";
    std::string tagID = tagInterface_->RegisterTagItem(tagDef);
    tagItemId_ = tagID;
}


// TAG ITEM UPDATE FUNCTIONS

}  // namespace vsid
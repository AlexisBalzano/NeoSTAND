#pragma once
#include <chrono>
#include <format>
#include <string>

#include "TagItemsColor.h"
#include "PLUGIN_NAME.h"

#ifdef DEV
#define LOG_DEBUG(loglevel, message) logger_->log(loglevel, message)
#else
#define LOG_DEBUG(loglevel, message) void(0)
#endif

sing namespace PLUGIN_NAMESPACE::tagitems;

namespace PLUGIN_NAMESPACE {
void PLUGIN_NAME::RegisterTagItems()
{
    PluginSDK::Tag::TagItemDefinition tagDef;

    // Tag item def
    tagDef.name = "TAG ITEM";
    tagDef.defaultValue = "PLACEHOLDER";
    std::string tagID = tagInterface_->RegisterTagItem(tagDef);
    tagItemId_ = tagID;
}


// TAG ITEM UPDATE FUNCTIONS
void PLUGIN_NAME::updateTagItem() {
    Tag::TagContext tagContext;
	tagContext.callsign = callsign;
    tagContext.colour = color;
    param.tagInterface_->UpdateTagValue(param.tagId_, cfl_string, tagContext);
}

// Update all tag items for all pilots
void PLUGIN_NAME::UpdateTagItems() {
	callsignsScope = dataManager_->getAllDepartureCallsigns();
    for (auto &callsign : callsignsScope)
    {
		UpdateTagItems(callsign);
    }
}

// Update all tag items for a specific callsign
void PLUGIN_NAME::UpdateTagItems(std::string callsign) {
    updateTagItem();
}
}  // namespace vsid
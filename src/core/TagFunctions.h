#pragma once
#include "PLUGIN_NAME.h"

namespace PLUGIN_NAMESPACE {
void PLUGIN_NAME::RegisterTagActions()
{
    PluginSDK::Tag::TagActionDefinition tagDef;

    // TAG function def
    tagDef.name = "ITEM_NAME";
	tagDef.description = "ITEM_DESCRIPTION";
	tagDef.requiresInput = false;
    tagActionId_ = tagInterface_->RegisterTagAction(tagDef);
    
	// Dropdown Menu def
    PluginSDK::Tag::DropdownDefinition dropdownDef;
    dropdownDef.title = "MENU TITLE";
    dropdownDef.width = 75;
    dropdownDef.maxHeight = 150;

    PluginSDK::Tag::DropdownComponent dropdownComponent;

    dropdownComponent.id = "Button1";
    dropdownComponent.type = PluginSDK::Tag::DropdownComponentType::Button;
    dropdownComponent.text = "Text";
    dropdownComponent.requiresInput = false;
    dropdownDef.components.push_back(dropdownComponent);

    tagInterface_->SetActionDropdown(requestMenuId_, dropdownDef);
}

void PLUGIN_NAME::OnTagAction(const PluginSDK::Tag::TagActionEvent *event)
{
    if (!initialized_ || !event )
    {
        return;
    }

    std::string input;
	if (event->userInput) input = event->userInput.value();
    TagProcessing(event->callsign, event->actionId, input);
}

void PLUGIN_NAME::OnTagDropdownAction(const PluginSDK::Tag::DropdownActionEvent *event)
{
    if (!initialized_ || !event )
    {
        return;
    }

}

void PLUGIN_NAME::TagProcessing(const std::string &callsign, const std::string &actionId, const std::string &userInput)
{
    
}
}  // namespace vsid
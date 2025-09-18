#pragma once
#include "NeOSTAND.h"

namespace stand {
void NeoSTAND::RegisterTagActions()
{
    PluginSDK::Tag::TagActionDefinition tagDef;

    // TAG function def
    tagDef.name = "ASSIGN";
	tagDef.description = "Assign Stand";
	tagDef.requiresInput = false;
    assignActionId_ = tagInterface_->RegisterTagAction(tagDef);
    
	// Dropdown Menu def
    tagDef.name = "StandMenu";
    tagDef.description = "open the stand menu";
    standMenuId_ = tagInterface_->RegisterTagAction(tagDef);

    PluginSDK::Tag::DropdownDefinition dropdownDef;
    dropdownDef.title = "STAND SELECT";
    dropdownDef.width = 75;
    dropdownDef.maxHeight = 150;

    PluginSDK::Tag::DropdownComponent dropdownComponent;

    dropdownComponent.id = "STAND1";
    dropdownComponent.type = PluginSDK::Tag::DropdownComponentType::Button;
    dropdownComponent.text = "48A";
    dropdownComponent.requiresInput = false;
    dropdownDef.components.push_back(dropdownComponent);

    tagInterface_->SetActionDropdown(standMenuId_, dropdownDef);
}

void NeoSTAND::OnTagAction(const PluginSDK::Tag::TagActionEvent *event)
{
    if (!initialized_ || !event )
    {
        return;
    }

    std::string input;
	if (event->userInput) input = event->userInput.value();
    TagProcessing(event->callsign, event->actionId, input);
}

void NeoSTAND::OnTagDropdownAction(const PluginSDK::Tag::DropdownActionEvent *event)
{
    if (!initialized_ || !event )
    {
        return;
    }

	DisplayMessage("Assigning Stand 48A for: " + event->callsign);
}

void NeoSTAND::TagProcessing(const std::string &callsign, const std::string &actionId, const std::string &userInput)
{
    if (actionId == assignActionId_)
    {
        DisplayMessage("Assigning Stand for " + callsign, "TagProcessing");
	}
}

inline bool NeoSTAND::toggleAutoMode()
{
    autoMode = !autoMode;
	return autoMode;
}
}  // namespace vsid
#pragma once
#include "NeoSTAND.h"

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
    dropdownDef.title = "STAND";
    dropdownDef.width = 75;
    dropdownDef.maxHeight = 150;

    PluginSDK::Tag::DropdownComponent dropdownComponent;
	PluginSDK::Tag::DropdownComponentStyle style;

	style.textAlign = PluginSDK::Tag::DropdownAlignmentType::Center;

    dropdownComponent.id = "STAND1";
    dropdownComponent.type = PluginSDK::Tag::DropdownComponentType::Button;
    dropdownComponent.text = "48A";
    dropdownComponent.requiresInput = false;
	dropdownComponent.style = style;
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
    std::optional<DataManager::Pilot> pilot = dataManager_->getPilotByCallsign(event->callsign);
	if (!pilot || pilot->empty()) return;

    if (event->componentId == "None")
    {
        if (!pilot->stand.empty())
        {
            dataManager_->freeStand(pilot->stand);
            pilot->stand.clear();
			dataManager_->updatePilotStand(pilot->callsign, pilot->stand);
            UpdateTagItems(pilot->callsign);
        }
        return;
	}

	dataManager_->assignStandToPilot(pilot->callsign, event->componentId);
    UpdateTagItems(pilot->callsign);
}

void NeoSTAND::TagProcessing(const std::string &callsign, const std::string &actionId, const std::string &userInput)
{
    if (actionId == assignActionId_)
    {
        DisplayMessage("Assigning Stand for " + callsign, "TagProcessing");
	}
}

inline void NeoSTAND::updateStandMenuButtons(const std::string& icao)
{
    std::vector<DataManager::Stand> stands = dataManager_->getAvailableStandsForAirport(icao);

    PluginSDK::Tag::DropdownDefinition dropdownDef;
    dropdownDef.title = "STAND";
    dropdownDef.width = 75;
    dropdownDef.maxHeight = 150;

    PluginSDK::Tag::DropdownComponent scrollArea;
	scrollArea.id = "SCROLL";
	scrollArea.type = PluginSDK::Tag::DropdownComponentType::ScrollArea;


    PluginSDK::Tag::DropdownComponent dropdownComponent;
    PluginSDK::Tag::DropdownComponentStyle style;

    style.textAlign = PluginSDK::Tag::DropdownAlignmentType::Center;

    dropdownComponent.id = "None";
    dropdownComponent.type = PluginSDK::Tag::DropdownComponentType::Button;
    dropdownComponent.text = "None";
    dropdownComponent.requiresInput = false;
    dropdownComponent.style = style;
    scrollArea.children.push_back(dropdownComponent);


    for (const auto& stand : stands) {
        dropdownComponent.id = stand.name;
        dropdownComponent.type = PluginSDK::Tag::DropdownComponentType::Button;
        dropdownComponent.text = stand.name;
        dropdownComponent.requiresInput = false;
		dropdownComponent.style = style;
        scrollArea.children.push_back(dropdownComponent);
	}

	dropdownDef.components.push_back(scrollArea);

    tagInterface_->UpdateActionDropdown(standMenuId_, dropdownDef);
}

}  // namespace stand
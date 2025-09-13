#include <algorithm>
#include <string>

#include "NeoSTAND.h"


namespace stand {
void NeoSTAND::RegisterCommand() {
    try
    {
        CommandProvider_ = std::make_shared<NeoSTANDCommandProvider>(this, logger_, chatAPI_, fsdAPI_);

        PluginSDK::Chat::CommandDefinition definition;
        definition.name = "stand version";
        definition.description = "Display NeoSTAND Version";
        definition.lastParameterHasSpaces = false;
		definition.parameters.clear();

        versionCommandId_ = chatAPI_->registerCommand(definition.name, definition, CommandProvider_);
        
        definition.name = "stand help";
        definition.description = "Display all available commands";
        definition.lastParameterHasSpaces = false;
		definition.parameters.clear();

        helpCommandId_ = chatAPI_->registerCommand(definition.name, definition, CommandProvider_);
        
        definition.name = "stand toggle";
        definition.description = "Toggle auto stand assignation";
        definition.lastParameterHasSpaces = false;
		definition.parameters.clear();

        toggleModeCommandId_ = chatAPI_->registerCommand(definition.name, definition, CommandProvider_);
    }
    catch (const std::exception& ex)
    {
        logger_->error("Error registering command: " + std::string(ex.what()));
    }
}

inline void NeoSTAND::unegisterCommand()
{
    if (CommandProvider_)
    {
        chatAPI_->unregisterCommand(versionCommandId_);
        chatAPI_->unregisterCommand(helpCommandId_);
        chatAPI_->unregisterCommand(toggleModeCommandId_);
        CommandProvider_.reset();
	}
}

Chat::CommandResult NeoSTANDCommandProvider::Execute( const std::string &commandId, const std::vector<std::string> &args)
{
    if (commandId == neoSTAND_->versionCommandId_)
    {
		neoSTAND_->DisplayMessage("NeoSTAND Version: " + std::string(NEOSTAND_VERSION));
        return { true, std::nullopt };
	}
    else if (commandId == neoSTAND_->helpCommandId_)
    {
        for (const char* line : {
          "NeoRAS available commands:",
          ".stand version",
		  ".stand toggle",
            })
        {
            neoSTAND_->DisplayMessage(line);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        return { true, std::nullopt };
	}
    else if (commandId == neoSTAND_->toggleModeCommandId_)
    {
		bool state = neoSTAND_->toggleAutoMode();
		neoSTAND_->DisplayMessage("NeoSTAND Auto Mode: " + std::string(state ? "ON" : "OFF"));
        return { true, std::nullopt };
    }
    else {
        return { false, "error" };
    }

	return { true, std::nullopt };
}
}  // namespace stand
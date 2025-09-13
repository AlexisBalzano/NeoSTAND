#include <algorithm>
#include <string>

#include "PLUGIN_NAME.h"


namespace PLUGIN_NAMESPACE {
void PLUGIN_NAME::RegisterCommand() {
    try
    {
        CommandProvider_ = std::make_shared<PLUGIN_NAMECommandProvider>(this, logger_, chatAPI_, fsdAPI_);

        PluginSDK::Chat::CommandDefinition definition;
        definition.name = "COMMAND_NAME";
        definition.description = "COMMAND_DESCRIPTION";
        definition.lastParameterHasSpaces = false;
		definition.parameters.clear();

        commandId_ = chatAPI_->registerCommand(definition.name, definition, CommandProvider_);
    }
    catch (const std::exception& ex)
    {
        logger_->error("Error registering command: " + std::string(ex.what()));
    }
}

inline void PLUGIN_NAME::unegisterCommand()
{
    if (CommandProvider_)
    {
        chatAPI_->unregisterCommand(commandId_);
        CommandProvider_.reset();
	}
}

Chat::CommandResult PLUGIN_NAMECommandProvider::Execute( const std::string &commandId, const std::vector<std::string> &args)
{
    if (commandId == PLUGIN_NAME->commandId_)
    {
        return { true, std::nullopt };
	}
    else {
        return { false, error };
    }

	return { true, std::nullopt };
}
}  // namespace PLUGIN_NAMESPACE
#pragma once
#include "PLUGIN_NAME.h"

using namespace PluginSDK;

namespace PLUGIN_NAMESPACE {

class PLUGIN_NAME;

class PLUGIN_NAMECommandProvider : public PluginSDK::Chat::CommandProvider
{
public:
    PLUGIN_NAMECommandProvider(PLUGIN_NAMESPACE::PLUGIN_NAME *PLUGIN_NAME, PluginSDK::Logger::LoggerAPI *logger, Chat::ChatAPI *chatAPI, Fsd::FsdAPI *fsdAPI)
            : PLUGIN_NAME_(PLUGIN_NAME), logger_(logger), chatAPI_(chatAPI), fsdAPI_(fsdAPI) {}
		
	Chat::CommandResult Execute(const std::string &commandId, const std::vector<std::string> &args) override;

private:
    Logger::LoggerAPI *logger_ = nullptr;
    Chat::ChatAPI *chatAPI_ = nullptr;
    Fsd::FsdAPI *fsdAPI_ = nullptr;
    PLUGIN_NAME *PLUGIN_NAME_ = nullptr;
};
}  // namespace PLUGIN_NAMESPACE
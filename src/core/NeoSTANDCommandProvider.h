#pragma once
#include "NeoSTAND.h"

using namespace PluginSDK;

namespace stand {

class NeoSTAND;

class NeoSTANDCommandProvider : public PluginSDK::Chat::CommandProvider
{
public:
    NeoSTANDCommandProvider(stand::NeoSTAND *NeoSTAND, PluginSDK::Logger::LoggerAPI *logger, Chat::ChatAPI *chatAPI, Fsd::FsdAPI *fsdAPI)
            : neoSTAND_(NeoSTAND), logger_(logger), chatAPI_(chatAPI), fsdAPI_(fsdAPI) {}
		
	Chat::CommandResult Execute(const std::string &commandId, const std::vector<std::string> &args) override;

private:
    Logger::LoggerAPI *logger_ = nullptr;
    Chat::ChatAPI *chatAPI_ = nullptr;
    Fsd::FsdAPI *fsdAPI_ = nullptr;
    NeoSTAND *neoSTAND_ = nullptr;
};
}  // namespace stand
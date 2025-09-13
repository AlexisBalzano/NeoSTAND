#include "PLUGIN_NAME.h"

extern "C" PLUGIN_API PluginSDK::BasePlugin *CreatePluginInstance()
{
    try
    {
        return new PLUGIN_NAMESPACE::PLUGIN_NAME();
    }
    catch (const std::exception &e)
    {
        return nullptr;
    }
}
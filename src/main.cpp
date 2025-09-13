#include "NeoSTAND.h"

extern "C" PLUGIN_API PluginSDK::BasePlugin *CreatePluginInstance()
{
    try
    {
        return new stand::NeoSTAND();
    }
    catch (const std::exception &e)
    {
        return nullptr;
    }
}
// Persists nodecfg::NodeConfig in NVS via Arduino Preferences, namespace
// "hlmesh" (separate from the lift's "eeprom" namespace). Missing or invalid
// keys fall back to the defaults.
#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H
#include "NodeConfig.h"

namespace configstore {
void load(nodecfg::NodeConfig& c, uint8_t defaultId, const char* defaultAlias);
bool save(const nodecfg::NodeConfig& c);
}

#endif

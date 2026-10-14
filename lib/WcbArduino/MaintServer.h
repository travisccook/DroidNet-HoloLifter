// Maintenance mode transport for an Arduino node (docs/MAINTENANCE_API.md): join a
// stored Wi-Fi profile, announce <alias>.local over mDNS and serve the
// maintenance HTTP API on port 80 from a FreeRTOS task on core 0, so the
// loop task (limits, e-stop, motion) keeps running during a long OTA upload.
//
//   GET  /api/info  {"node":..,"fw":..,"slot":..,"mode":"maint","spi":false}
//   POST /api/ota   raw app image, Content-Length required; 200 then reboot
//   POST /api/done  200, then the node returns to the mesh
#ifndef MAINT_SERVER_H
#define MAINT_SERVER_H
#include <Arduino.h>
#include "NodeConfig.h"

namespace maintserver {

// Start joining `p` (Wi-Fi must already be in STA mode, mesh stopped).
void join(const nodecfg::WifiProfile& p);
bool hasIp();
// Start mDNS + HTTP. `node` is the alias, `fw` the firmware version.
bool startServer(const char* node, const char* fw);
// Stop HTTP + mDNS and drop the Wi-Fi association (STA mode is kept).
void stop();
// One-shot flags set by the HTTP task.
bool takeDone();
bool takeReboot();   // a verified OTA image is staged; reboot now

}  // namespace maintserver

#endif

// Arduino-ESP32 (3.x) adapter between the ESP-NOW radio and wcb::WcbNode:
// implements wcb::WcbPlatform and brings the radio up as a WCB mesh node
// (STA mode, power save off, fixed channel, MAC 02:oct2:oct3:00:00:id).
// Bring-up order follows WCB_Client 1.17.2 (MIT), which is field-proven on
// Arduino 3.x. Only one MeshRadio may exist (the ESP-NOW receive callback is
// a plain function).
#ifndef MESH_RADIO_H
#define MESH_RADIO_H
#include <Arduino.h>
#include "NodeConfig.h"
#include "WcbNode.h"

class MeshRadio : public wcb::WcbPlatform {
 public:
  // Bring the radio up for `cfg` and route received frames to node.ingest().
  // Does not call node.begin(). False if ESP-NOW could not start.
  bool start(const nodecfg::NodeConfig& cfg, wcb::WcbNode& node);
  // Leave the mesh: unregister the callback and de-initialise ESP-NOW.
  // Wi-Fi stays in STA mode so the caller can join another network.
  void stop();
  bool v2() const { return version_ >= 2; }
  bool macOk() const { return macOk_; }

  // wcb::WcbPlatform (called only from the loop task)
  uint32_t nowMs() override { return millis(); }
  bool send(const uint8_t mac[6], const uint8_t* data, size_t len) override;
  bool addPeer(const uint8_t mac[6]) override;  // ESP_ERR_ESPNOW_EXIST counts as success

 private:
  uint32_t version_ = 0;
  bool     macOk_ = false;
};

#endif

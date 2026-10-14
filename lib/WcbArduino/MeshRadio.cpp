#include "MeshRadio.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

static wcb::WcbNode* s_node = nullptr;

// Runs on the Wi-Fi task: copy into the node's ring and return.
static void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  wcb::WcbNode* n = s_node;
  if (n && info && info->src_addr && len > 0) n->ingest(info->src_addr, data, (size_t)len);
}

bool MeshRadio::start(const nodecfg::NodeConfig& cfg, wcb::WcbNode& node) {
  WiFi.persistent(false);                 // never write Wi-Fi credentials to flash
  if (!WiFi.mode(WIFI_STA)) Serial.println(F("[mesh] WARNING: WiFi.mode(STA) failed"));
  WiFi.disconnect();
  esp_wifi_set_ps(WIFI_PS_NONE);          // modem sleep drops ESP-NOW frames
  esp_err_t err = esp_wifi_set_channel(cfg.channel, WIFI_SECOND_CHAN_NONE);
  if (err != ESP_OK) Serial.printf("[mesh] WARNING: set_channel(%u) failed (%d)\n", cfg.channel, err);
  {
    uint8_t primary = 0; wifi_second_chan_t second;
    esp_wifi_get_channel(&primary, &second);
    if (primary != cfg.channel) {
      delay(100);
      esp_wifi_set_channel(cfg.channel, WIFI_SECOND_CHAN_NONE);
      esp_wifi_get_channel(&primary, &second);
      if (primary != cfg.channel) Serial.printf("[mesh] WARNING: radio on channel %u, mesh is on %u\n", primary, cfg.channel);
    }
  }

  uint8_t mac[6], readBack[6] = {0};
  wcb::nodeMac(cfg.oct2, cfg.oct3, cfg.id, mac);
  err = esp_wifi_set_mac(WIFI_IF_STA, mac);
  esp_wifi_get_mac(WIFI_IF_STA, readBack);
  macOk_ = (err == ESP_OK) && memcmp(mac, readBack, 6) == 0;
  Serial.printf("[mesh] STA MAC %02X:%02X:%02X:%02X:%02X:%02X %s\n", readBack[0], readBack[1], readBack[2],
                readBack[3], readBack[4], readBack[5], macOk_ ? "OK" : "(set_mac FAILED: WCBs will drop our frames)");

  err = esp_now_init();
  if (err != ESP_OK) {
    Serial.printf("[mesh] ERROR: esp_now_init failed: %s\n", esp_err_to_name(err));
    return false;
  }
  version_ = 0;
  esp_now_get_version(&version_);
  Serial.printf("[mesh] ESP-NOW version %lu%s\n", (unsigned long)version_, version_ >= 2 ? "" : " (v1: 252-byte WCB frames will FAIL)");
  s_node = &node;
  esp_now_register_recv_cb(onRecv);
  return true;
}

void MeshRadio::stop() {
  esp_now_unregister_recv_cb();
  s_node = nullptr;
  esp_now_deinit();
}

bool MeshRadio::send(const uint8_t mac[6], const uint8_t* data, size_t len) {
  return esp_now_send(mac, data, len) == ESP_OK;
}

bool MeshRadio::addPeer(const uint8_t mac[6]) {
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = 0;          // the current channel
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;      // WCB meshes are unencrypted
  esp_err_t err = esp_now_add_peer(&peer);
  return err == ESP_OK || err == ESP_ERR_ESPNOW_EXIST;
}

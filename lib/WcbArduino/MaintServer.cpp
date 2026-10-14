#include "MaintServer.h"
#include <ESPmDNS.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <atomic>
#include <esp_ota_ops.h>

namespace maintserver {

static WebServer             s_server(80);
static TaskHandle_t          s_task = nullptr;
static std::atomic<bool>     s_run{false};
static std::atomic<bool>     s_done{false};
static std::atomic<bool>     s_reboot{false};
static bool                  s_otaOk = false;
static char                  s_node[25] = {0};
static char                  s_fw[28] = {0};

void join(const nodecfg::WifiProfile& p) {
  WiFi.setAutoReconnect(true);
  WiFi.begin(p.ssid, p.pass[0] ? p.pass : nullptr);
}

bool hasIp() { return WiFi.status() == WL_CONNECTED && (uint32_t)WiFi.localIP() != 0; }

static void handleInfo() {
  const esp_partition_t* run = esp_ota_get_running_partition();
  char body[160];
  snprintf(body, sizeof(body), "{\"node\":\"%s\",\"fw\":\"%s\",\"slot\":\"%s\",\"mode\":\"maint\",\"spi\":false}",
           s_node, s_fw, run ? run->label : "?");
  s_server.send(200, "application/json", body);
}

// Streams the raw request body into the inactive OTA slot.
static void otaBody() {
  HTTPRaw& raw = s_server.raw();
  switch (raw.status) {
    case RAW_START: {
      int len = s_server.clientContentLength();
      s_otaOk = len > 0 && Update.begin((size_t)len);
      break;
    }
    case RAW_WRITE:
      if (s_otaOk && Update.write(raw.buf, raw.currentSize) != raw.currentSize) s_otaOk = false;
      break;
    case RAW_END:
      if (s_otaOk) s_otaOk = Update.end(false);   // verifies the image, sets the boot slot
      else if (Update.isRunning()) Update.abort();
      break;
    case RAW_ABORTED:
      if (Update.isRunning()) Update.abort();
      s_otaOk = false;
      break;
  }
}

static void otaDone() {
  if (s_server.clientContentLength() <= 0) { s_server.send(400, "application/json", "{\"ok\":false,\"err\":\"length\"}"); return; }
  if (!s_otaOk) {
    char body[96];
    snprintf(body, sizeof(body), "{\"ok\":false,\"err\":\"%s\"}", Update.errorString());
    s_server.send(500, "application/json", body);
    return;
  }
  s_server.send(200, "application/json", "{\"ok\":true}");
  s_reboot = true;
}

static void handleDone() {
  s_server.send(200, "application/json", "{\"ok\":true}");
  s_done = true;
}

static void serverTask(void*) {
  while (s_run) {
    s_server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(2));
  }
  s_task = nullptr;
  vTaskDelete(nullptr);
}

bool startServer(const char* node, const char* fw) {
  strncpy(s_node, node, sizeof(s_node) - 1);
  strncpy(s_fw, fw, sizeof(s_fw) - 1);
  char host[25];
  nodecfg::hostnameFromAlias(node, host, sizeof(host));
  if (MDNS.begin(host)) MDNS.addService("http", "tcp", 80);
  static bool routed = false;
  if (!routed) {
    s_server.on("/api/info", HTTP_GET, handleInfo);
    s_server.on("/api/ota", HTTP_POST, otaDone, otaBody);
    s_server.on("/api/done", HTTP_POST, handleDone);
    s_server.onNotFound([] { s_server.send(404, "application/json", "{\"ok\":false}"); });
    routed = true;
  }
  s_server.begin();
  s_run = true;
  return xTaskCreatePinnedToCore(serverTask, "maint_http", 8192, nullptr, 1, &s_task, 0) == pdPASS;
}

void stop() {
  s_run = false;
  for (int i = 0; i < 100 && s_task; i++) delay(5);   // let the task leave handleClient()
  s_server.stop();
  MDNS.end();
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false);
}

bool takeDone() { return s_done.exchange(false); }
bool takeReboot() { return s_reboot.exchange(false); }

}  // namespace maintserver

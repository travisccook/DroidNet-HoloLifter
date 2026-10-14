#include "HoloMesh.h"
#include <ESP32Encoder.h>
#include <WiFi.h>
#include <atomic>
#include <esp_ota_ops.h>
#include "ConfigStore.h"
#include "LiftCommands.h"
#include "MaintFsm.h"
#include "MaintServer.h"
#include "MeshRadio.h"
#include "MotionController.h"
#include "MotionProfile.h"
#include "MotorControl.h"
#include "NodeConfig.h"
#include "TermTee.h"
#include "ValidityGate.h"
#include "WcbNode.h"
#include "config.h"

// Owned by main.cpp.
extern MotionController motion;
extern MotorControl motor;
extern ESP32Encoder encoder;
void setHolofanPower(bool on);

static constexpr uint8_t DEFAULT_ID = 15;
static constexpr const char* DEFAULT_ALIAS = "HoloLifter";
static constexpr uint8_t LOCAL_SENDER = 0;      // "hl" bench commands: print, never send

static TermTee            s_console(Serial);
Print&                    Console = s_console;
static nodecfg::NodeConfig s_cfg;
static wcb::WcbNode       s_node;
static MeshRadio          s_radio;
static bool               s_radioUp = false;
static maint::MaintFsm    s_maint;
static maint::ValidityGate s_gate;
static std::atomic<bool>  s_stop{false};
static char               s_slot[17] = "?";

// ---- helpers ----------------------------------------------------------------

static lift::State liftState(MotionState m) {
  switch (m) {
    case IDLE:             return lift::State::IDLE;
    case HOMING:           return lift::State::HOMING;
    case MOVING_TO_TARGET: return lift::State::MOVING;
    case MANUAL_UP:        return lift::State::MOVING_UP;
    case MANUAL_DOWN:      return lift::State::MOVING_DOWN;
    case STOPPED:          return lift::State::STOPPED;
    case ERROR:            return lift::State::ERROR;
  }
  return lift::State::ERROR;
}

static lift::Status snapshot() {
  lift::Status s;
  s.state = liftState(motion.getState());
  s.pos = motion.getPosition();
  s.target = motion.getTarget();
  s.homed = motion.isHomed();
  s.speed = motion.getSpeed();
  s.upper = motion.getUpperLimit();
  s.lower = motion.getLowerLimit();
  s.fan = digitalRead(RELAY_PIN) == HOLOFAN_ON;
  s.enc = encoder.getCount();
  s.error = motion.getErrorMessage();
  s.fw = HOLOLIFTER_VERSION;
  s.slot = s_slot;
  return s;
}

static nodecfg::NodeRuntime runtime() {
  nodecfg::NodeRuntime rt;
  wcb::WcbCounters c = s_node.counters();
  rt.fw = HOLOLIFTER_VERSION;
  rt.slot = s_slot;
  rt.v2 = s_radio.v2();
  rt.meshUp = s_node.started();
  rt.rx = c.rxOk;
  rt.rej = c.rejMac + c.rejSize + c.rejPassword + c.rejSpoof + c.rejTarget;
  rt.crc = c.crcFail;
  rt.dup = c.dupDrop;
  rt.retry = c.retries;
  rt.gaveUp = c.gaveUp;
  return rt;
}

// Query replies are broadcast on the mesh (docs/SERIAL_COMMANDS.md): every WCB writes the
// line to its broadcast-out serial ports (?BCAST,OUT, incl. USB for
// DroidNet). "hl" bench commands print locally instead.
static void reply(uint8_t sender, const char* text) {
  if (sender == LOCAL_SENDER) { Console.println(text); return; }
  if (!s_cfg.replies || !s_node.started()) return;
  s_node.broadcast(text);
}

static bool radioStart() {
  if (!s_cfg.password[0]) return false;
  s_radioUp = s_radio.start(s_cfg, s_node);
  return s_radioUp;
}

// ---- maintenance ------------------------------------------------------------

static void enterMaint(uint8_t profile, uint8_t sender) {
  bool set = profile >= 1 && profile <= 2 && s_cfg.profile[profile - 1].ssid[0];
  maint::Refusal r = s_maint.request(profile, set, !motion.isMoving(), millis());
  if (r != maint::Refusal::NONE) {
    Console.printf("[maint] refused: %s\n", r == maint::Refusal::NOT_SAFE ? "lift is moving"
                                           : r == maint::Refusal::BUSY ? "already in maintenance" : "profile not set");
    (void)sender;
  }
}

static void maintLoop() {
  switch (s_maint.poll(millis(), maintserver::hasIp())) {
    case maint::Action::LEAVE_MESH_AND_JOIN: {
      motor.disable();                              // no motion while off the mesh
      Console.printf("[maint] leaving mesh, joining profile %u (%s)\n", s_maint.profile(),
                     s_cfg.profile[s_maint.profile() - 1].ssid);
      s_node.suspend();
      if (s_radioUp) { s_radio.stop(); s_radioUp = false; }
      maintserver::join(s_cfg.profile[s_maint.profile() - 1]);
      break;
    }
    case maint::Action::START_SERVER:
      if (!maintserver::startServer(s_cfg.alias, HOLOLIFTER_VERSION)) {
        Console.println(F("[maint] HTTP server failed to start; returning to mesh"));
        s_maint.done();                               // next poll returns to the mesh
        break;
      }
      Console.printf("[maint] serving http://%s/ (mDNS %s.local)\n", WiFi.localIP().toString().c_str(), s_cfg.alias);
      break;
    case maint::Action::RETURN_TO_MESH:
      Console.println(F("[maint] returning to mesh"));
      maintserver::stop();
      if (radioStart()) s_node.rejoin();
      if (motion.isMoving()) motion.stop();   // a move commanded while away must not resume on its own
      motor.enable();
      break;
    case maint::Action::NONE:
      break;
  }
  if (maintserver::takeDone()) s_maint.done();
  if (maintserver::takeReboot()) {
    Console.println(F("[maint] OTA image staged; rebooting"));
    delay(300);                                       // let the HTTP 200 leave
    ESP.restart();
  }
}

// ---- dispatch -----------------------------------------------------------------

static void runLift(const lift::LiftCmd& c, uint8_t sender) {
  using lift::Cmd;
  switch (c.cmd) {
    case Cmd::UP:        motion.moveUp(0); break;
    case Cmd::DOWN:      motion.moveDown(0); break;
    case Cmd::STOP:      motion.stop(); break;
    case Cmd::HOME:      motion.home(); break;
    case Cmd::CLEAR:     motion.clearError(); break;
    case Cmd::GOTO:      motion.gotoPosition(c.value); break;
    case Cmd::SPEED:     motion.setSpeed(motionprofile::clampSpeed(c.value)); break;
    case Cmd::MARGIN:    motion.setSoftLimitMargin(c.value); break;
    case Cmd::MOTOR_ON:  motor.enable(); break;
    case Cmd::MOTOR_OFF: motor.disable(); break;
    case Cmd::SAVE:      motion.saveConfig(); break;
    case Cmd::LOAD:      motion.loadConfig(); break;
    case Cmd::CFG_RESET: motion.resetConfig(); break;
    case Cmd::REBOOT:    motion.stop(); delay(50); ESP.restart(); break;
    case Cmd::FAN_ON:    setHolofanPower(true); break;
    case Cmd::FAN_OFF:   setHolofanPower(false); break;
    default: {
      char out[200];
      if (lift::formatReply(c.cmd, snapshot(), out, sizeof(out)) > 0) reply(sender, out);
      break;
    }
  }
}

static void runZ(const nodecfg::ZCmd& z, uint8_t sender) {
  switch (nodecfg::applyZ(z, s_cfg)) {
    case nodecfg::ZResult::SAVED:        configstore::save(s_cfg); break;
    case nodecfg::ZResult::SAVED_REBOOT: configstore::save(s_cfg); Console.println(F("[mesh] saved; applies after reboot")); break;
    case nodecfg::ZResult::MAINT:        enterMaint((uint8_t)z.num, sender); break;
    case nodecfg::ZResult::QUERY: {
      char out[200];
      if (nodecfg::formatNodeReply(s_cfg, runtime(), out, sizeof(out)) > 0) reply(sender, out);
      break;
    }
  }
}

// Dispatch one mesh (or "hl" bench) line. Unknown commands are ignored
// silently on the mesh (actions never reply), reported on USB.
static void dispatch(uint8_t sender, char* raw) {
  char* line = lift::normalizeLine(raw);
  lift::LiftCmd lc;
  nodecfg::ZCmd zc;
  if (lift::parseLift(line, lc)) runLift(lc, sender);
  else if (nodecfg::parseZ(line, zc)) runZ(zc, sender);
  else if (sender == LOCAL_SENDER) Console.printf("#HL:ERROR badcmd (%s)\n", line);
}

static void onPeek(void*, uint8_t, const char* cmd) {
  if (lift::isStopLine(cmd)) s_stop.store(true, std::memory_order_release);
}

// ---- public -------------------------------------------------------------------

void holoMeshSetup() {
  const esp_partition_t* run = esp_ota_get_running_partition();
  if (run) strncpy(s_slot, run->label, sizeof(s_slot) - 1);
  esp_ota_img_states_t st = ESP_OTA_IMG_UNDEFINED;
  bool pending = run && esp_ota_get_state_partition(run, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY;

  configstore::load(s_cfg, DEFAULT_ID, DEFAULT_ALIAS);
  s_gate.begin(pending, s_cfg.password[0] != '\0', false, millis());
  if (pending) Console.println(F("[ota] new image: must reach the mesh within 30 s or it rolls back"));

  if (!s_cfg.password[0]) {
    Console.println(F("[mesh] OFF: no mesh password set (USB: mesh pw <password>, then mesh restart)"));
    return;
  }
#ifdef HOLOMESH_ROLLBACK_TEST
  // Bench-only image (env esp32thing_plus_rollbacktest): never reaches the
  // mesh, so after an OTA the validity gate must roll it back within 30 s.
  Console.println(F("[test] ROLLBACK TEST IMAGE: mesh deliberately not started"));
  return;
#endif
  wcb::WcbConfig wc;
  wc.oct2 = s_cfg.oct2;
  wc.oct3 = s_cfg.oct3;
  wc.id = s_cfg.id;
  wc.floorQ = s_cfg.floorQ;
  strncpy(wc.password, s_cfg.password, sizeof(wc.password) - 1);
  wc.checksum = s_cfg.checksum;
  strncpy(wc.identity.devType, s_cfg.alias, sizeof(wc.identity.devType) - 1);
  strncpy(wc.identity.fw, HOLOLIFTER_VERSION, sizeof(wc.identity.fw) - 1);
  strncpy(wc.identity.hwRev, "ThingPlus", sizeof(wc.identity.hwRev) - 1);
  strncpy(wc.identity.caps, "lift.motor holo.power", sizeof(wc.identity.caps) - 1);
  s_node.setPeekHook(onPeek, nullptr);                // before begin(): ingest reads it unlocked
  if (!radioStart() || !s_node.begin(wc, s_radio)) {
    Console.println(F("[mesh] start FAILED (see messages above)"));
    return;
  }
  s_console.setTarget(&s_node, 0);
  Console.printf("[mesh] node %u \"%s\" on channel %u, replies %s\n", s_cfg.id, s_cfg.alias, s_cfg.channel, s_cfg.replies ? "broadcast" : "off");
}

static bool s_purgeMoves = false;   // a mesh #HLS fired: discard moves queued before it

bool holoMeshTakeStop() {
  bool stop = s_stop.exchange(false, std::memory_order_acq_rel);
  if (stop) s_purgeMoves = true;
  return stop;
}

void holoMeshLoop() {
  s_node.poll();
  if (s_node.started()) s_gate.meshUp();
  switch (s_gate.poll(millis())) {
    case maint::Verdict::VALID:
      esp_ota_mark_app_valid_cancel_rollback();
      Console.println(F("[ota] new image marked valid"));
      break;
    case maint::Verdict::INVALID:
      Console.println(F("[ota] mesh not up within 30 s: rolling back"));
      motion.stop();
      delay(50);
      delay(100);
      esp_ota_mark_app_invalid_rollback_and_reboot();
      break;
    case maint::Verdict::NONE:
      break;
  }
  uint8_t sender;
  char line[wcb::COMMAND_FIELD + 1];
  if (s_purgeMoves) {
    s_purgeMoves = false;
    for (size_t i = 0; i < wcb::WcbNode::CMD_QUEUE_N && s_maint.state() == maint::State::MESH &&
                       s_node.popCommand(sender, line, sizeof(line)); i++) {
      char* norm = lift::normalizeLine(line);
      lift::LiftCmd lc;
      if (lift::parseLift(norm, lc) && lift::isMotionStart(lc.cmd)) {
        Console.printf("[mesh] dropped %s queued before #HLS\n", norm);
        continue;
      }
      s_console.setTarget(&s_node, sender);
      dispatch(sender, line);
    }
  }
  for (int i = 0; i < 4 && s_maint.state() == maint::State::MESH && s_node.popCommand(sender, line, sizeof(line)); i++) {
    s_console.setTarget(&s_node, sender);           // RTERM goes to whoever talked to us last
    dispatch(sender, line);
  }
  maintLoop();
}

void holoMeshUsb(const char* rawLine) {
  nodecfg::UsbCmd u;
  char err[96];
  if (!nodecfg::parseUsb(rawLine, u, err, sizeof(err))) { Console.printf("ERROR %s\n", err); return; }
  switch (u.type) {
    case nodecfg::UsbType::SHOW: {
      char out[640];
      if (nodecfg::formatShow(s_cfg, runtime(), out, sizeof(out)) > 0) Console.println(out);
      return;
    }
    case nodecfg::UsbType::MAINT:   enterMaint((uint8_t)u.a, LOCAL_SENDER); return;
    case nodecfg::UsbType::RESTART: Console.println(F("restarting...")); motion.stop(); delay(50); delay(100); ESP.restart(); return;
    default:
      if (nodecfg::applyUsb(u, s_cfg) && configstore::save(s_cfg)) Console.println(F("OK saved (mesh restart to apply mesh settings)"));
      else Console.println(F("ERROR save failed"));
      return;
  }
}

void holoMeshBench(const char* line) {
  char buf[wcb::COMMAND_FIELD + 1];
  strncpy(buf, line, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  dispatch(LOCAL_SENDER, buf);
}

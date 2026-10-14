#include "ConfigStore.h"
#include <Preferences.h>
#include <string.h>

namespace configstore {

static void getStr(Preferences& p, const char* key, char* out, size_t size) {
  char tmp[72] = {0};
  if (p.isKey(key) && p.getString(key, tmp, sizeof(tmp)) > 0) {
    strncpy(out, tmp, size - 1);
    out[size - 1] = '\0';
  }
}

void load(nodecfg::NodeConfig& c, uint8_t defaultId, const char* defaultAlias) {
  nodecfg::setDefaults(c, defaultId, defaultAlias);
  Preferences p;
  if (!p.begin("hlmesh", true)) return;    // first boot: nothing stored yet
  uint8_t id = p.getUChar("id", defaultId);
  if (nodecfg::validId(id)) c.id = id;
  char alias[25] = {0};
  getStr(p, "alias", alias, sizeof(alias));
  if (nodecfg::validAlias(alias)) strcpy(c.alias, alias);
  c.replies = p.getBool("reply", true);
  uint8_t q = p.getUChar("q", 3);
  if (q <= 20) c.floorQ = q;
  uint8_t ch = p.getUChar("ch", 1);
  if (nodecfg::validChannel(ch)) c.channel = ch;
  c.oct2 = p.getUChar("oct2", 0);
  c.oct3 = p.getUChar("oct3", 0);
  char pw[40] = {0};
  getStr(p, "pw", pw, sizeof(pw));
  if (nodecfg::validPassword(pw)) strcpy(c.password, pw);
  c.checksum = p.getBool("crc", true);
  const char* sk[2] = {"s1", "s2"};
  const char* pk[2] = {"p1", "p2"};
  for (int i = 0; i < 2; i++) {
    nodecfg::WifiProfile w;
    getStr(p, sk[i], w.ssid, sizeof(w.ssid));
    getStr(p, pk[i], w.pass, sizeof(w.pass));
    if (nodecfg::validSsid(w.ssid) && nodecfg::validPass(w.pass)) c.profile[i] = w;
  }
  p.end();
}

bool save(const nodecfg::NodeConfig& c) {
  Preferences p;
  if (!p.begin("hlmesh", false)) return false;
  bool ok = true;
  ok &= p.putUChar("id", c.id) > 0;
  ok &= p.putString("alias", c.alias) > 0;
  ok &= p.putBool("reply", c.replies) > 0;
  ok &= p.putUChar("q", c.floorQ) > 0;
  ok &= p.putUChar("ch", c.channel) > 0;
  ok &= p.putUChar("oct2", c.oct2) > 0;
  ok &= p.putUChar("oct3", c.oct3) > 0;
  p.putString("pw", c.password);          // may be empty (mesh off)
  ok &= p.putBool("crc", c.checksum) > 0;
  p.putString("s1", c.profile[0].ssid);
  p.putString("p1", c.profile[0].pass);
  p.putString("s2", c.profile[1].ssid);
  p.putString("p2", c.profile[1].pass);
  p.end();
  return ok;
}

}  // namespace configstore

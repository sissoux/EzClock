#include "Config.hpp"
#include <Preferences.h>

static const char* NS = "cfg";

bool Config::load() {
  Preferences p;
  if (!p.begin(NS, true)) return false;
  wifi.ssid = p.getString("w_ssid", "");
  wifi.password = p.getString("w_pwd", "");
  ntp.server = p.getString("n_srv", ntp.server);
  ntp.timezone = p.getString("n_tz", ntp.timezone);
  mqtt.enabled = p.getBool("m_en", mqtt.enabled);
  mqtt.host = p.getString("m_host", mqtt.host);
  mqtt.port = p.getUShort("m_port", mqtt.port);
  mqtt.user = p.getString("m_user", mqtt.user);
  mqtt.pass = p.getString("m_pass", mqtt.pass);
  mqtt.baseTopic = p.getString("m_base", mqtt.baseTopic);
  net.hostname = p.getString("net_host", net.hostname);
  led.colorHex = p.getString("l_hex", led.colorHex);
  led.brightness = p.getUChar("l_bri", led.brightness);
  led.fadeMs = p.getUShort("l_fade", led.fadeMs);
  led.autoHue = p.getBool("l_ah_en", led.autoHue);
  led.autoHueDegPerMin = p.getUShort("l_ah_dpm", led.autoHueDegPerMin);
  led.ambientMinPct = p.getUChar("l_ab_min", led.ambientMinPct);
  led.ambientMaxPct = p.getUChar("l_ab_max", led.ambientMaxPct);
  led.ambientFullPowerThreshold = p.getUShort("l_ab_thr", led.ambientFullPowerThreshold);
  led.ambientSampleMs = p.getUShort("l_ab_ms", led.ambientSampleMs);
  led.ambientAvgCount = p.getUChar("l_ab_cnt", led.ambientAvgCount);
  p.end();
  return true;
}

bool Config::save() const {
  Preferences p;
  if (!p.begin(NS, false)) return false;
  p.putString("w_ssid", wifi.ssid);
  p.putString("w_pwd", wifi.password);
  p.putString("n_srv", ntp.server);
  p.putString("n_tz", ntp.timezone);
  p.putBool("m_en", mqtt.enabled);
  p.putString("m_host", mqtt.host);
  p.putUShort("m_port", mqtt.port);
  p.putString("m_user", mqtt.user);
  p.putString("m_pass", mqtt.pass);
  p.putString("m_base", mqtt.baseTopic);
  p.putString("net_host", net.hostname);
  p.putString("l_hex", led.colorHex);
  p.putUChar("l_bri", led.brightness);
  p.putUShort("l_fade", led.fadeMs);
  p.putBool("l_ah_en", led.autoHue);
  p.putUShort("l_ah_dpm", led.autoHueDegPerMin);
  p.putUChar("l_ab_min", led.ambientMinPct);
  p.putUChar("l_ab_max", led.ambientMaxPct);
  p.putUShort("l_ab_thr", led.ambientFullPowerThreshold);
  p.putUShort("l_ab_ms", led.ambientSampleMs);
  p.putUChar("l_ab_cnt", led.ambientAvgCount);
  p.end();
  return true;
}

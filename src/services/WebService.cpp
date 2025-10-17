#include "WebService.hpp"
#include "../core/Log.hpp"
#include "../core/Config.hpp"
#include "../hal/HalDriver.hpp"
#include "../ui/WebUI.hpp"
#include "../services/TimeService.hpp"
#include <time.h>
#include <ctype.h>
#include <functional>

#ifdef ARDUINO_ARCH_ESP32
  #include <WiFi.h>
  #include <AsyncTCP.h>
  #include <ESPAsyncWebServer.h>
  #include <ESPmDNS.h>
#elif defined(ARDUINO_ARCH_ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
  #include <ESPAsyncWebServer.h>
  #include <ESP8266mDNS.h>
#endif

static AsyncWebServer server(80);
static bool apEnabled = false;
static String g_apSsid;
static String g_apPass;
static Config* g_cfgPtr = nullptr; // to access configuration in WiFi callbacks

// Verbose command logging helper: enabled only when compiled with -D VERBOSE
#ifdef VERBOSE
#define LOGV_CMD(...) LOGI(__VA_ARGS__)
#else
#define LOGV_CMD(...) do { } while(0)
#endif

#ifdef ARDUINO_ARCH_ESP32
static void onWifiEvent(arduino_event_id_t event, arduino_event_info_t /*info*/){
  LOGI("WiFi event: %d", (int)event);
  if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    if (apEnabled) {
      WiFi.softAPdisconnect(true);
      apEnabled = false;
      LOGI("AP disabled after STA connect. STA IP: %s", WiFi.localIP().toString().c_str());
    }
    // Start/Restart mDNS with current hostname
    MDNS.end();
    String host = (g_cfgPtr && g_cfgPtr->net.hostname.length()) ? g_cfgPtr->net.hostname : String("ezQlock");
    if (MDNS.begin(host.c_str())) {
      MDNS.addService("http", "tcp", 80);
      LOGI("mDNS started: %s.local", host.c_str());
    } else {
      LOGW("mDNS start failed");
    }
  } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    if (!apEnabled) {
      WiFi.softAP(g_apSsid.c_str(), g_apPass.c_str());
      apEnabled = true;
      LOGI("AP re-enabled: IP %s", WiFi.softAPIP().toString().c_str());
    }
  }
}
#endif

void WebService::begin(Config& cfg, HalDriver* hal) {
  g_cfgPtr = &cfg;

  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false); // improve responsiveness on C3
  // Set hostname early for STA
#ifdef ARDUINO_ARCH_ESP32
  if (!cfg.net.hostname.isEmpty()) {
    WiFi.setHostname(cfg.net.hostname.c_str());
  }
#else
  if (!cfg.net.hostname.isEmpty()) {
    WiFi.hostname(cfg.net.hostname);
  }
#endif
  // Optional: configure default AP IP/subnet to known 192.168.4.1
  WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
  g_apSsid = "EzClock-AP";
  g_apPass = "ezclock1234";
  WiFi.softAP(g_apSsid.c_str(), g_apPass.c_str());
  apEnabled = true;
  LOGI("AP started: %s  IP: %s", g_apSsid.c_str(), WiFi.softAPIP().toString().c_str());

  // If STA credentials exist, try to connect concurrently
  if (!cfg.wifi.ssid.isEmpty()) {
    WiFi.begin(cfg.wifi.ssid.c_str(), cfg.wifi.password.c_str());
  }

  // Start mDNS so device can be reached at ezclock.local
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
  if (!MDNS.begin("ezclock")) {
    LOGW("mDNS start failed; ezclock.local may not resolve");
  } else {
    MDNS.addService("http", "tcp", 80);
    LOGI("mDNS started: http://ezclock.local/");
  }
#endif

#ifdef ARDUINO_ARCH_ESP32
  WiFi.onEvent(std::function<void(arduino_event_id_t, arduino_event_info_t)>(onWifiEvent));
#endif

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req){
    LOGV_CMD("UI: GET /");
    req->send(200, "text/html", WEB_UI);
  });

  // Apply saved LED defaults (color and smoothing) at startup
  if (hal) {
    // smoothing
    hal->setSmoothing(cfg.led.fadeMs);
    // color
    String hex0 = cfg.led.colorHex;
    if (hex0.startsWith("#")) hex0.remove(0,1);
    if (hex0.length() == 6) {
      long v = strtol(hex0.c_str(), nullptr, 16);
      uint8_t r = (v >> 16) & 0xFF;
      uint8_t g = (v >> 8) & 0xFF;
      uint8_t b = (v >> 0) & 0xFF;
      hal->fill(r,g,b);
      hal->show();
    }
    // AutoHue
    hal->setAutoHue(cfg.led.autoHue, cfg.led.autoHueDegPerMin);
    // Ambient brightness control
    hal->setAmbientControl(cfg.led.ambientMinPct, cfg.led.ambientMaxPct, cfg.led.ambientFullPowerThreshold);
    // Ambient sampling parameters
    hal->setAmbientSampling(cfg.led.ambientSampleMs, cfg.led.ambientAvgCount);
  }

  server.on("/health", HTTP_GET, [](AsyncWebServerRequest* req){
    req->send(200, "text/plain", "OK");
  });

  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest* req){
    char buf[64];
    snprintf(buf, sizeof(buf), "heap=%u", (unsigned)ESP.getFreeHeap());
    req->send(200, "text/plain", buf);
  });

  server.on("/api/status", HTTP_GET, [&cfg](AsyncWebServerRequest* req){
    LOGV_CMD("UI: GET /api/status");
    const bool sta = (WiFi.status() == WL_CONNECTED);
    const String staIp = sta ? WiFi.localIP().toString() : String("");
    const String apIp = WiFi.softAPIP().toString();
    const int apClients = WiFi.softAPgetStationNum();
    const bool synced = TimeSvc::isSynced();
    time_t now = time(nullptr);
    struct tm tmv; localtime_r(&now, &tmv);
    char iso[32];
    snprintf(iso, sizeof(iso), "%04d-%02d-%02dT%02d:%02d:%02d",
             tmv.tm_year+1900, tmv.tm_mon+1, tmv.tm_mday, tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    String json;
    json.reserve(320);
    json += "{\"ok\":true,\"wifi\":{";
    json += "\"mode\":\"AP_STA\",";
    json += "\"ap_ip\":\"" + apIp + "\",";
    json += "\"ap_clients\":" + String(apClients) + ",";
    json += "\"sta_connected\":" + String(sta ? "true" : "false") + ",";
    json += "\"sta_ip\":\"" + staIp + "\"},";
  json += "\"time\":{";
    json += "\"synced\":" + String(synced ? "true" : "false") + ",";
    json += "\"epoch\":" + String((unsigned long)now) + ",";
    json += "\"iso\":\""; json += iso; json += "\"},";
  // NTP configuration (server + timezone)
  String tz = cfg.ntp.timezone; if (tz.length()==0) tz = "";
  String ntps = cfg.ntp.server; if (ntps.length()==0) ntps = "";
  json += "\"ntp\":{";
  json += "\"server\":\"" + ntps + "\",";
  json += "\"timezone\":\"" + tz + "\"},";
    // network info
    String host = cfg.net.hostname.length() ? cfg.net.hostname : String("ezQlock");
    json += "\"net\":{\"hostname\":\"" + host + "\"},";
    String hex = cfg.led.colorHex;
    if (!hex.startsWith("#")) hex = String("#") + hex;
    json += "\"led\":{";
    json += "\"hex\":\"" + hex + "\",";
  json += "\"fade\":" + String(cfg.led.fadeMs) + ",";
  json += "\"autoHue\":" + String(cfg.led.autoHue ? "true" : "false") + ",";
  json += "\"autoHueDegPerMin\":" + String((unsigned)cfg.led.autoHueDegPerMin) + ",";
  json += "\"ambientMinPct\":" + String((unsigned)cfg.led.ambientMinPct) + ",";
  json += "\"ambientMaxPct\":" + String((unsigned)cfg.led.ambientMaxPct) + ",";
  json += "\"ambientFullPowerThreshold\":" + String((unsigned)cfg.led.ambientFullPowerThreshold) + ",";
  json += "\"ambientSampleMs\":" + String((unsigned)cfg.led.ambientSampleMs) + ",";
  json += "\"ambientAvgCount\":" + String((unsigned)cfg.led.ambientAvgCount) + "}}";
    req->send(200, "application/json", json);
  });

  server.on("/api/color", HTTP_GET, [hal](AsyncWebServerRequest* req){
    if (!req->hasParam("hex")) { req->send(400, "text/plain", "missing hex"); return; }
    String hex = req->getParam("hex")->value();
    if (hex.startsWith("#")) hex.remove(0,1);
    if (hex.length() != 6) { req->send(400, "text/plain", "bad hex"); return; }
    long v = strtol(hex.c_str(), nullptr, 16);
    uint8_t r = (v >> 16) & 0xFF;
    uint8_t g = (v >> 8) & 0xFF;
    uint8_t b = (v >> 0) & 0xFF;
    LOGI("/api/color hex=#%s -> rgb(%u,%u,%u)", hex.c_str(), (unsigned)r, (unsigned)g, (unsigned)b);
    if (hal) { hal->fill(r,g,b); hal->show(); }
    req->send(200, "application/json", "{\"ok\":true}");
  });

  // Persist default color
  server.on("/api/color/default", HTTP_POST, [&cfg, hal](AsyncWebServerRequest* req){
    String hex = req->hasParam("hex", true) ? req->getParam("hex", true)->value() : String("");
    hex.trim();
    if (hex.startsWith("#")) hex.remove(0,1);
    if (hex.length() != 6) { req->send(400, "application/json", "{\"ok\":false,\"err\":\"bad hex\"}"); return; }
    LOGV_CMD("UI: POST /api/color/default hex=#%s", hex.c_str());
    cfg.led.colorHex = String("#") + hex;
    bool ok = cfg.save();
    if (hal) {
      long v = strtol(hex.c_str(), nullptr, 16);
      uint8_t r = (v >> 16) & 0xFF;
      uint8_t g = (v >> 8) & 0xFF;
      uint8_t b = (v >> 0) & 0xFF;
      hal->fill(r,g,b);
      hal->show();
    }
    req->send(ok ? 200 : 500, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
  });

  // Adjust smoothing time (ms)
  server.on("/api/fade", HTTP_POST, [&cfg, hal](AsyncWebServerRequest* req){
    String msStr = req->hasParam("ms", true) ? req->getParam("ms", true)->value() : String("0");
    uint16_t ms = (uint16_t) msStr.toInt();
    if (ms > 5000) ms = 5000; // clamp
    LOGV_CMD("UI: POST /api/fade ms=%u", (unsigned)ms);
    cfg.led.fadeMs = ms;
    bool ok = cfg.save();
    if (hal) hal->setSmoothing(ms);
    req->send(ok ? 200 : 500, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
  });

  // Update AutoHue settings
  server.on("/api/autohue", HTTP_POST, [&cfg, hal](AsyncWebServerRequest* req){
    String enStr = req->hasParam("enabled", true) ? req->getParam("enabled", true)->value() : String("false");
    String dpmStr = req->hasParam("degPerMin", true) ? req->getParam("degPerMin", true)->value() : String("2");
    enStr.trim(); dpmStr.trim();
    bool enabled = (enStr == "1" || enStr.equalsIgnoreCase("true") || enStr == "on");
    int dpm = dpmStr.toInt();
    if (dpm < 0) dpm = 0; if (dpm > 360) dpm = 360;
    LOGV_CMD("UI: POST /api/autohue enabled=%s degPerMin=%d", enabled?"true":"false", dpm);
    cfg.led.autoHue = enabled;
    cfg.led.autoHueDegPerMin = (uint16_t)dpm;
    bool ok = cfg.save();
    // Apply to HAL immediately
    if (hal) {
      hal->setAutoHue(cfg.led.autoHue, cfg.led.autoHueDegPerMin);
    }
    // If disabled, immediately apply persisted color frame
    if (!enabled && hal) {
      String hex = cfg.led.colorHex; String s = hex;
      if (s.startsWith("#")) s.remove(0,1);
      if (s.length() == 6) {
        long v = strtol(s.c_str(), nullptr, 16);
        uint8_t r = (v >> 16) & 0xFF;
        uint8_t g = (v >> 8) & 0xFF;
        uint8_t b = (v >> 0) & 0xFF;
        hal->fill(r,g,b); hal->show();
      }
    }
    String resp = String("{\"ok\":") + (ok?"true":"false") + 
                  ",\"enabled\":" + (cfg.led.autoHue?"true":"false") +
                  ",\"degPerMin\":" + String((unsigned)cfg.led.autoHueDegPerMin) + "}";
    req->send(ok ? 200 : 500, "application/json", resp);
  });

  // Update hostname (persist and apply). Also restarts mDNS if STA has IP.
  server.on("/api/hostname", HTTP_POST, [&cfg](AsyncWebServerRequest* req){
    String hn = req->hasParam("hostname", true) ? req->getParam("hostname", true)->value() : String("");
    hn.trim();
    // Basic validation: 1..23 chars, alnum & dash only (mDNS label rules)
    if (hn.length() == 0) hn = "ezQlock";
    if (hn.length() > 23) hn = hn.substring(0,23);
    for (size_t i=0;i<hn.length();++i){ char c = hn[i]; if (!(isalnum((unsigned char)c) || c=='-')) { hn.setCharAt(i, '-'); } }
  LOGV_CMD("UI: POST /api/hostname hostname=%s", hn.c_str());
    cfg.net.hostname = hn;
    bool ok = cfg.save();
#ifdef ARDUINO_ARCH_ESP32
    WiFi.setHostname(cfg.net.hostname.c_str());
#else
    WiFi.hostname(cfg.net.hostname);
#endif
    // If STA connected, restart mDNS
    if (WiFi.status() == WL_CONNECTED) {
      MDNS.end();
      if (MDNS.begin(cfg.net.hostname.c_str())) {
        MDNS.addService("http", "tcp", 80);
      }
    }
    req->send(ok ? 200 : 500, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
  });

  // Update ambient brightness control parameters
  server.on("/api/ambient", HTTP_POST, [&cfg, hal](AsyncWebServerRequest* req){
    String minStr = req->hasParam("minPct", true) ? req->getParam("minPct", true)->value() : String("10");
    String maxStr = req->hasParam("maxPct", true) ? req->getParam("maxPct", true)->value() : String("100");
    String thrStr = req->hasParam("threshold", true) ? req->getParam("threshold", true)->value() : String("1000");
    String perStr = req->hasParam("periodMs", true) ? req->getParam("periodMs", true)->value() : String("250");
    String cntStr = req->hasParam("avgCount", true) ? req->getParam("avgCount", true)->value() : String("20");
    int minPct = minStr.toInt();
    int maxPct = maxStr.toInt();
    int thr = thrStr.toInt();
    int per = perStr.toInt();
    int cnt = cntStr.toInt();
    if (minPct < 0) minPct = 0; if (minPct > 100) minPct = 100;
    if (maxPct < 0) maxPct = 0; if (maxPct > 100) maxPct = 100;
    if (maxPct < minPct) maxPct = minPct; // ensure ordering
    if (thr < 0) thr = 0; if (thr > 4095) thr = 4095;
    if (per < 50) per = 50; if (per > 5000) per = 5000;
    if (cnt < 1) cnt = 1; if (cnt > 60) cnt = 60;
    LOGV_CMD("UI: POST /api/ambient min=%d max=%d thr=%d periodMs=%d avgCount=%d", minPct, maxPct, thr, per, cnt);
    cfg.led.ambientMinPct = (uint8_t)minPct;
    cfg.led.ambientMaxPct = (uint8_t)maxPct;
    cfg.led.ambientFullPowerThreshold = (uint16_t)thr;
    cfg.led.ambientSampleMs = (uint16_t)per;
    cfg.led.ambientAvgCount = (uint8_t)cnt;
    bool ok = cfg.save();
    if (hal) {
      hal->setAmbientControl(cfg.led.ambientMinPct, cfg.led.ambientMaxPct, cfg.led.ambientFullPowerThreshold);
      hal->setAmbientSampling(cfg.led.ambientSampleMs, cfg.led.ambientAvgCount);
    }
    req->send(ok ? 200 : 500, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
  });

  server.on("/api/wifi", HTTP_POST, [&cfg](AsyncWebServerRequest* req){
    String ssid;
    String password;
    if (req->hasParam("ssid", true)) ssid = req->getParam("ssid", true)->value();
    if (req->hasParam("password", true)) password = req->getParam("password", true)->value();
    ssid.trim();
    LOGV_CMD("UI: POST /api/wifi ssid='%s' pwd.len=%u", ssid.c_str(), (unsigned)password.length());
    if (ssid.length() == 0) { req->send(400, "application/json", "{\"ok\":false,\"err\":\"empty ssid\"}"); return; }
    cfg.wifi.ssid = ssid;
    cfg.wifi.password = password;
    bool ok = cfg.save();
    LOGI("WiFi config saved. ssid='%s' len(pwd)=%u", ssid.c_str(), (unsigned)password.length());
    if (ok) {
      // Try to connect STA
      WiFi.begin(cfg.wifi.ssid.c_str(), cfg.wifi.password.c_str());
      req->send(200, "application/json", "{\"ok\":true}");
    } else {
      req->send(500, "application/json", "{\"ok\":false}");
    }
  });

  // Scan nearby Wi‑Fi networks
  server.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest* req){
    LOGV_CMD("UI: GET /api/wifi/scan");
    // Synchronous scan; typical duration ~1-3s
    int n = WiFi.scanNetworks(/*async=*/false, /*hidden=*/true);
    if (n < 0) n = 0;
    // Build JSON: {count:n, list:[{ssid:"...", rssi:-55, enc:WIFI_AUTH_*}, ...]}
    String json; json.reserve(64 + n * 48);
    json += "{\"count\":" + String(n) + ",\"list\":[";
    for (int i = 0; i < n; ++i) {
  if (i) json += ",";
  String ssid = WiFi.SSID(i);
  int32_t rssi = WiFi.RSSI(i);
#ifdef ARDUINO_ARCH_ESP32
  int enc = (int)WiFi.encryptionType(i);
#else
  int enc = (int)WiFi.encryptionType(i);
#endif
  // Escape quotes in SSID (rare)
  ssid.replace("\\", "\\\\");
  ssid.replace("\"", "\\\"");
  json += "{\"ssid\":\"" + ssid + "\",\"rssi\":" + String(rssi) + ",\"enc\":" + String(enc) + "}";
    }
    json += "]}";
    // Free scan results
    WiFi.scanDelete();
    req->send(200, "application/json", json);
  });

  server.on("/api/timezone", HTTP_POST, [&cfg](AsyncWebServerRequest* req){
    String tz;
    if (req->hasParam("tz", true)) tz = req->getParam("tz", true)->value();
    tz.trim();
    if (tz.isEmpty()) { req->send(400, "application/json", "{\"ok\":false,\"err\":\"empty tz\"}"); return; }
    LOGV_CMD("UI: POST /api/timezone tz=%s", tz.c_str());
    cfg.ntp.timezone = tz;
    bool ok = cfg.save();
    // Apply immediately and update TimeSvc cache so reconnects keep this TZ
    TimeSvc::applyNtpConfig(cfg.ntp.server, cfg.ntp.timezone);
    LOGI("Timezone set to %s", cfg.ntp.timezone.c_str());
    req->send(ok ? 200 : 500, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
  });

  // Read current ambient ADC value from the driver (if supported)
  server.on("/api/ambient/read", HTTP_GET, [hal](AsyncWebServerRequest* req){
    LOGV_CMD("UI: GET /api/ambient/read");
    uint16_t raw = 0, avg = 0;
    bool have = false;
    if (hal) {
      have = hal->getAmbientReading(raw, avg);
    }
    String json = String("{\"ok\":true,\"supported\":") + (have?"true":"false") + ",\"raw\":" + String(raw) + ",\"avg\":" + String(avg) + "}";
    req->send(200, "application/json", json);
  });

  // Register OTA routes before starting the server
  setupManualOTA();
  server.onNotFound([](AsyncWebServerRequest* req){ req->send(404, "text/plain", "Not found"); });
  server.begin();
}

void WebService::loop() {
  // Async web server runs in background
}

// Manual OTA update page and handler
#ifdef ARDUINO_ARCH_ESP32
#include <Update.h>
#endif

void WebService::setupManualOTA() {
#ifdef ARDUINO_ARCH_ESP32
  server.on("/ManualOTA", HTTP_GET, [](AsyncWebServerRequest* req){
    req->send(200, "text/html",
      "<h2>Manual OTA Update</h2>"
      "<form method='POST' action='/ManualOTA' enctype='multipart/form-data'>"
      "<input type='file' name='firmware'>"
      "<input type='submit' value='Update'>"
      "</form>"
    );
  });
  server.on("/ManualOTA", HTTP_POST,
    [](AsyncWebServerRequest* req){
      bool ok = !Update.hasError();
      req->send(200, "text/html", ok ? "Update Success. Rebooting..." : "Update Failed!");
      if (ok) {
        delay(1000);
        ESP.restart();
      }
    },
    [](AsyncWebServerRequest* req, String filename, size_t index, uint8_t *data, size_t len, bool final){
      if (!index) {
        Update.begin(UPDATE_SIZE_UNKNOWN);
      }
      Update.write(data, len);
      if (final) {
        Update.end(true);
      }
    }
  );
#endif
}

#include "WebService.hpp"
#include "../core/Log.hpp"
#include "../core/Config.hpp"
#include "../hal/HalDriver.hpp"
#include "../ui/WebUI.hpp"
#include "../services/TimeService.hpp"
#include <time.h>

#ifdef ARDUINO_ARCH_ESP32
  #include <WiFi.h>
  #include <AsyncTCP.h>
  #include <ESPAsyncWebServer.h>
#elif defined(ARDUINO_ARCH_ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
  #include <ESPAsyncWebServer.h>
#endif

static AsyncWebServer server(80);
static bool apEnabled = false;
static String g_apSsid;
static String g_apPass;

void WebService::begin(Config& cfg, HalDriver* hal) {

  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false); // improve responsiveness on C3
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

#ifdef ARDUINO_ARCH_ESP32
  WiFi.onEvent([](WiFiEvent_t e){
    LOGI("WiFi event: %d", (int)e);
    if (e == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
      if (apEnabled) {
        WiFi.softAPdisconnect(true);
        apEnabled = false;
        LOGI("AP disabled after STA connect. STA IP: %s", WiFi.localIP().toString().c_str());
      }
    } else if (e == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
      if (!apEnabled) {
        WiFi.softAP(g_apSsid.c_str(), g_apPass.c_str());
        apEnabled = true;
        LOGI("AP re-enabled: IP %s", WiFi.softAPIP().toString().c_str());
      }
    }
  });
#endif

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req){
    req->send_P(200, "text/html", WEB_UI);
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
    String hex = cfg.led.colorHex;
    if (!hex.startsWith("#")) hex = String("#") + hex;
    json += "\"led\":{";
    json += "\"hex\":\"" + hex + "\",";
    json += "\"fade\":" + String(cfg.led.fadeMs) + "}}";
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
    cfg.led.fadeMs = ms;
    bool ok = cfg.save();
    if (hal) hal->setSmoothing(ms);
    req->send(ok ? 200 : 500, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
  });

  server.on("/api/wifi", HTTP_POST, [&cfg](AsyncWebServerRequest* req){
    String ssid;
    String password;
    if (req->hasParam("ssid", true)) ssid = req->getParam("ssid", true)->value();
    if (req->hasParam("password", true)) password = req->getParam("password", true)->value();
    ssid.trim();
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

  server.on("/api/timezone", HTTP_POST, [&cfg](AsyncWebServerRequest* req){
    String tz;
    if (req->hasParam("tz", true)) tz = req->getParam("tz", true)->value();
    tz.trim();
    if (tz.isEmpty()) { req->send(400, "application/json", "{\"ok\":false,\"err\":\"empty tz\"}"); return; }
    cfg.ntp.timezone = tz;
    bool ok = cfg.save();
    setenv("TZ", cfg.ntp.timezone.c_str(), 1);
    tzset();
    LOGI("Timezone set to %s", cfg.ntp.timezone.c_str());
    req->send(ok ? 200 : 500, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
  });

  server.onNotFound([](AsyncWebServerRequest* req){ req->send(404, "text/plain", "Not found"); });
  server.begin();
}

void WebService::loop() {
  // Async web server runs in background
}

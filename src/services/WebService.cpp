#include "WebService.hpp"
#include "../core/Log.hpp"
#include "../core/Config.hpp"
#include "../hal/HalDriver.hpp"
#include "../ui/WebUI.hpp"

#if defined(USE_WEB)
  #ifdef ARDUINO_ARCH_ESP32
    #include <WiFi.h>
    #include <AsyncTCP.h>
    #include <ESPAsyncWebServer.h>
  #elif defined(ARDUINO_ARCH_ESP8266)
    #include <ESP8266WiFi.h>
    #include <ESPAsyncTCP.h>
    #include <ESPAsyncWebServer.h>
  #endif
#endif

void WebService::begin(Config& cfg, HalDriver* hal) {
#if defined(USE_WEB)
  static AsyncWebServer server(80);

  WiFi.mode(WIFI_AP);
  String apSsid = "EzClock-AP";
  String apPass = "ezclock1234";
  WiFi.softAP(apSsid.c_str(), apPass.c_str());
  LOGI("AP started: %s  IP: %s", apSsid.c_str(), WiFi.softAPIP().toString().c_str());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req){
    req->send_P(200, "text/html", WEB_UI);
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
    if (hal) { hal->fill(r,g,b); hal->show(); }
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.onNotFound([](AsyncWebServerRequest* req){ req->send(404, "text/plain", "Not found"); });
  server.begin();
#else
  (void)cfg; (void)hal;
  LOGI("WebService disabled (USE_WEB not defined)");
#endif
}

void WebService::loop() {
  // Async web server runs in background
}

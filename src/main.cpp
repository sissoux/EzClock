// Always-on Web-enabled app entry
#include <Arduino.h>
#include "core/Config.hpp"
#include "hal/HalDriver.hpp"
#include "services/WebService.hpp"
#include "services/TimeService.hpp"
#ifdef ARDUINO_ARCH_ESP32
#include <ArduinoOTA.h>
#include <WiFi.h>
#endif

static HalDriver* g_hal = nullptr;
static Config g_cfg;
static WebService g_web;

#ifdef ARDUINO_ARCH_ESP32
static bool g_otaReady = false;
static void startOTAOnce() {
    if (g_otaReady) return;
    ArduinoOTA.setHostname("ezclock");
    ArduinoOTA.setPort(3232);
    ArduinoOTA.onStart([](){ Serial.println("[OTA] Start"); });
    ArduinoOTA.onEnd([](){ Serial.println("[OTA] End"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total){
        static uint32_t last = 0; uint32_t now = millis();
        if (now - last > 250) { last = now; Serial.printf("[OTA] %u%%\n", (progress * 100) / total); }
    });
    ArduinoOTA.onError([](ota_error_t err){ Serial.printf("[OTA] Error %u\n", (unsigned)err); });
    ArduinoOTA.begin();
    g_otaReady = true;
    IPAddress ip = WiFi.localIP();
    Serial.printf("[OTA] Ready at %s:3232\n", ip.toString().c_str());
}
#endif

void setup() {
    Serial.begin(115200);
#ifdef ARDUINO_ARCH_ESP32
    Serial.setDebugOutput(true);
#endif
    // Give USB CDC a moment to enumerate so early logs are visible
    uint32_t _t0 = millis();
    while (!Serial && millis() - _t0 < 2000) {
        delay(10);
    }
    delay(100);
    Serial.println("\n[EzClock] Web mode starting...");

    g_hal = createDefaultDriver();
    g_hal->begin();

    g_cfg.load();
        Serial.printf("[EzClock] Loaded SSID='%s' TZ='%s' Hostname='%s'\n", g_cfg.wifi.ssid.c_str(), g_cfg.ntp.timezone.c_str(), g_cfg.net.hostname.c_str());
    TimeSvc::begin(g_cfg);
    g_web.begin(g_cfg, g_hal);

#ifdef ARDUINO_ARCH_ESP32
    // Start OTA after Wi-Fi STA gets an IP; also call once in case we're already up or AP-only
    WiFi.onEvent([](WiFiEvent_t e){
        if (e == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
            startOTAOnce();
        }
    });
    startOTAOnce();
#endif
}

void loop() {
    g_web.loop();
    TimeSvc::loop();
    if (g_hal) g_hal->loop();
#ifdef ARDUINO_ARCH_ESP32
    ArduinoOTA.handle();
#endif
    delay(10);
}

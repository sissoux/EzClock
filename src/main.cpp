// Always-on Web-enabled app entry
#include <Arduino.h>
#include "core/Config.hpp"
#include "hal/HalDriver.hpp"
#include "services/WebService.hpp"
#include "services/TimeService.hpp"

static HalDriver* g_hal = nullptr;
static Config g_cfg;
static WebService g_web;

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
}

void loop() {
    g_web.loop();
    TimeSvc::loop();
    if (g_hal) g_hal->loop();
    delay(10);
}

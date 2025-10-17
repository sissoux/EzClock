#include "TimeService.hpp"
#include "../core/Config.hpp"
#include "../core/Log.hpp"

#ifdef ARDUINO_ARCH_ESP32
  #include <WiFi.h>
#else
  #error "TimeService currently supports ESP32 only"
#endif

namespace TimeSvc {
  static bool wifiConnected = false;
  static bool synced = false;
  static uint32_t lastAttempt = 0;
  static uint32_t lastLog = 0; // periodic time log
  static String tzCached;
  static String ntpCached;

  static void ensureWifi(const Config& cfg) {
    if (wifiConnected) return;
    if (cfg.wifi.ssid.isEmpty()) {
      // AP mode; no STA connect attempt here
      return;
    }
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.wifi.ssid.c_str(), cfg.wifi.password.c_str());
    LOGI("WiFi connecting to %s...", cfg.wifi.ssid.c_str());
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
      delay(100);
    }
    wifiConnected = (WiFi.status() == WL_CONNECTED);
    if (wifiConnected) {
      LOGI("WiFi connected: %s", WiFi.localIP().toString().c_str());
    } else {
      LOGW("WiFi connect timeout");
    }
  }

  void begin(const Config& cfg) {
    wifiConnected = false;
    synced = false;
    lastAttempt = 0;
    lastLog = 0;
    tzCached = cfg.ntp.timezone;
    ntpCached = cfg.ntp.server;
    // Apply timezone immediately so localtime() uses it even before sync, and set NTP when Wi‑Fi is ready
    setenv("TZ", tzCached.c_str(), 1);
    tzset();
    ensureWifi(cfg);
    if (wifiConnected) {
      // Configure NTP and timezone together (Arduino helper)
      configTzTime(tzCached.c_str(), ntpCached.c_str());
    }
  }

  // (definition moved below as a qualified function)

  void loop() {
    #ifdef INHIBIT_TIME_SYNC
    // Keep reporting unsynced for debugging; don't ever mark synced
    // Still allow Wi‑Fi connection attempts so other features work
    if (!wifiConnected && WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      LOGI("WiFi connected: %s", WiFi.localIP().toString().c_str());
    }
    // Throttle log
    uint32_t msDbg = millis();
    if (msDbg - lastAttempt > 3000) {
      LOGI("[DBG] INHIBIT_TIME_SYNC active (simulating unsynced)");
      lastAttempt = msDbg;
    }
    return;
    #endif
    // Detect late WiFi connection and start NTP
    if (!wifiConnected && WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      LOGI("WiFi connected: %s", WiFi.localIP().toString().c_str());
      // Re-init NTP with cached config and timezone
      setenv("TZ", tzCached.c_str(), 1);
      tzset();
      configTzTime(tzCached.c_str(), ntpCached.c_str());
    }
    if (!wifiConnected) return;
    if (synced) {
      uint32_t msSync = millis();
      if (msSync - lastLog >= 10000) {
        lastLog = msSync;
        time_t t = time(nullptr);
        struct tm tmv; localtime_r(&t, &tmv);
        LOGI("Time: %04d-%02d-%02d %02d:%02d:%02d", tmv.tm_year+1900, tmv.tm_mon+1, tmv.tm_mday, tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
      }
      return;
    }
    const time_t now = time(nullptr);
    if (now > 1609459200) { // > 2021-01-01 means time is valid
      synced = true;
      struct tm tmv; localtime_r(&now, &tmv);
      LOGI("Time synced: %04d-%02d-%02d %02d:%02d:%02d", tmv.tm_year+1900, tmv.tm_mon+1, tmv.tm_mday, tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
      return;
    }
    // throttle logs/attempts
    uint32_t msThrottle = millis();
    if (msThrottle - lastAttempt > 3000) {
      LOGI("Waiting for NTP...");
      lastAttempt = msThrottle;
    }
  }

  bool isWifiConnected() { return wifiConnected; }
  bool isSynced() {
  #ifdef INHIBIT_TIME_SYNC
    return false;
  #else
    return synced;
  #endif
  }

  bool getLocal(struct tm& out) {
    time_t t = time(nullptr);
    if (t <= 0) return false;
    localtime_r(&t, &out);
    return true;
  }
}

// Allow WebService to update NTP/TZ dynamically
void TimeSvc::applyNtpConfig(const String& server, const String& timezone) {
  TimeSvc::ntpCached = server;
  TimeSvc::tzCached = timezone;
  // Apply TZ immediately for localtime()
  setenv("TZ", TimeSvc::tzCached.c_str(), 1);
  tzset();
  if (WiFi.status() == WL_CONNECTED) {
    // Reconfigure NTP and TZ jointly
    configTzTime(TimeSvc::tzCached.c_str(), TimeSvc::ntpCached.c_str());
  }
}

#pragma once
#include <Arduino.h>

struct WifiConfig {
  String ssid;
  String password;
};

struct NtpConfig {
  String server = "pool.ntp.org";
  String timezone = "UTC0"; // POSIX TZ string
};

struct MqttConfig {
  bool enabled = false;
  String host;
  uint16_t port = 1883;
  String user;
  String pass;
  String baseTopic = "ezclock";
};

struct NetConfig {
  // Device hostname used for mDNS (.local)
  String hostname = "ezQlock";
};

struct LedConfig {
  String colorHex = "#6633FF";
  uint8_t brightness = 64;
  uint16_t fadeMs = 300; // smoothing duration for digit transitions
  bool autoHue = false;           // enable hue auto-rotation
  uint16_t autoHueDegPerMin = 2;  // degrees per minute (0..360)
  // Ambient-based brightness control
  uint8_t ambientMinPct = 10;           // 0..100
  uint8_t ambientMaxPct = 100;          // 0..100
  uint16_t ambientFullPowerThreshold = 1000; // 0..4095 (ADC)
  // Ambient sampling settings
  uint16_t ambientSampleMs = 250; // sampling period in ms
  uint8_t ambientAvgCount = 20;   // number of samples in running average
};

struct Config {
  WifiConfig wifi;
  NtpConfig ntp;
  MqttConfig mqtt;
  NetConfig net;
  LedConfig led;

  bool load();
  bool save() const;
};

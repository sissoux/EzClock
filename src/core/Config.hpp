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

struct LedConfig {
  String colorHex = "#6633FF";
  uint8_t brightness = 64;
};

struct Config {
  WifiConfig wifi;
  NtpConfig ntp;
  MqttConfig mqtt;
  LedConfig led;

  bool load();
  bool save() const;
};

#pragma once
#include <Arduino.h>
#include <time.h>

struct Config;

namespace TimeSvc {
  void begin(const Config& cfg);
  void loop();
  bool isWifiConnected();
  bool isSynced();
  bool getLocal(struct tm& out);
}

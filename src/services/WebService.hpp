#pragma once
#include <Arduino.h>

class HalDriver;
struct Config;

class WebService {
public:
  void begin(Config& cfg, HalDriver* hal);
  void loop();
private:
  void setupManualOTA();
};

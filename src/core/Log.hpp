#pragma once
#include <Arduino.h>

#ifndef LOG_TAG
#define LOG_TAG "EzClock"
#endif

#define LOGI(...) do { Serial.printf("[I] %s: ", LOG_TAG); Serial.printf(__VA_ARGS__); Serial.println(); } while(0)
#define LOGW(...) do { Serial.printf("[W] %s: ", LOG_TAG); Serial.printf(__VA_ARGS__); Serial.println(); } while(0)
#define LOGE(...) do { Serial.printf("[E] %s: ", LOG_TAG); Serial.printf(__VA_ARGS__); Serial.println(); } while(0)

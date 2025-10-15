#pragma once
#include <Arduino.h>

class HalDriver {
public:
  virtual ~HalDriver() = default;
  virtual void begin() = 0;
  virtual void loop() = 0;
  virtual void setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b) = 0;
  virtual void fill(uint8_t r, uint8_t g, uint8_t b) = 0;
  virtual void clear() = 0;
  virtual void show() = 0;
  virtual uint16_t size() const = 0;

  // Optional: runtime smoothing/transition control; default no-op
  virtual void setSmoothing(uint16_t /*ms*/) {}

  // Optional: restart any driver-specific startup/demo animation
  virtual void restartAnimation() {}
};

HalDriver* createDefaultDriver();

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

  // Optional: AutoHue control (default no-op)
  virtual void setAutoHue(bool /*enabled*/, uint16_t /*degPerMin*/) {}

  // Optional: Ambient-based brightness control (default no-op)
  // minPct/maxPct in 0..100, threshold in 0..4095 (ADC full scale)
  virtual void setAmbientControl(uint8_t /*minPct*/, uint8_t /*maxPct*/, uint16_t /*threshold*/) {}

  // Optional: Ambient reading access (default not supported)
  // Returns true if supported and populates raw (last sample) and avg (rolling average)
  virtual bool getAmbientReading(uint16_t& /*raw*/, uint16_t& /*avg*/) { return false; }

  // Optional: Ambient sampling configuration (default no-op)
  virtual void setAmbientSampling(uint16_t /*periodMs*/, uint8_t /*avgCount*/) {}
};

HalDriver* createDefaultDriver();

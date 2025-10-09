#include "HalDriver.hpp"

class NoopDriver : public HalDriver {
public:
  void begin() override {}
  void loop() override {}
  void setPixel(uint16_t, uint8_t, uint8_t, uint8_t) override {}
  void fill(uint8_t, uint8_t, uint8_t) override {}
  void clear() override {}
  void show() override {}
  uint16_t size() const override { return 0; }
};

HalDriver* createDefaultDriver() {
  static NoopDriver d;
  return &d;
}

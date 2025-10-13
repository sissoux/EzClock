// Driver skeleton: use this as a template for your dedicated hardware driver.
//
// Responsibilities:
// - Initialize your hardware in begin()
// - Periodically update/refresh it in loop() (read time, animate, etc.)
// - React to color updates coming from the Web UI (fill()/show() are called by the server)
// - Implement setPixel/fill/clear/show to map to your device (LED strip, matrix, segments...)
//
// Time source:
// - The firmware configures timezone and NTP. You can:
//   a) Use TimeSvc::getLocal(struct tm&) for convenience, or
//   b) Use time() + localtime_r() directly (timezone already applied in TimeService).
//
// Color updates from Web UI:
// - The Web UI calls /api/color which PUSHES the color to the driver via hal->fill(r,g,b); hal->show();
// - So color changes are event-driven (push), not polled. You can store the color in fill() and apply it in show().
// - If you need to react in loop() (e.g., fade), track a "_dirty" flag set in fill() and clear it after show().

#include "HalDriver.hpp"
#include <time.h>
#include "../services/TimeService.hpp"

class TemplateDriver : public HalDriver {
public:
  void begin() override {
    // TODO: initialize your hardware here (GPIOs, LED library, display, etc.)
    // e.g., FastLED.addLeds<...>(...); or set pinMode(...)

    _lastMinute = 255; // invalid to force first update
    _dirty = true;     // ensure first show applies state
  }

  void loop() override {
    // Example: read current local time once per second and detect minute changes
    const uint32_t nowMs = millis();
    if (nowMs - _lastTimePollMs >= 1000) {
      _lastTimePollMs = nowMs;

      struct tm tmv{};
      // Preferred helper:
      if (!TimeSvc::getLocal(tmv)) {
        // Fallback to libc if needed
        time_t t = time(nullptr);
        localtime_r(&t, &tmv);
      }

      // React to time change here (e.g., recompute what to render)
      if (tmv.tm_min != _lastMinute) {
        _lastMinute = tmv.tm_min;
        // TODO: recompute your frame/buffer for the new minute
        _dirty = true;
      }
    }

    // If something changed (time or color), push updates to hardware
    if (_dirty) {
      applyToHardware();
      _dirty = false;
    }
  }

  // Pixel API: adapt as needed. For non-addressable hardware, you can ignore setPixel.
  void setPixel(uint16_t /*index*/, uint8_t r, uint8_t g, uint8_t b) override {
    _colorR = r; _colorG = g; _colorB = b; _dirty = true;
  }

  // Web UI pushes color via /api/color which calls hal->fill(r,g,b) then hal->show().
  void fill(uint8_t r, uint8_t g, uint8_t b) override {
    _colorR = r; _colorG = g; _colorB = b; _dirty = true;
  }

  void clear() override { _colorR = _colorG = _colorB = 0; _dirty = true; }

  // Called by Web UI after fill(), and by your loop() when _dirty.
  void show() override { applyToHardware(); _dirty = false; }

  // Return the number of addressable elements if applicable; 0/1 is fine for simple outputs.
  uint16_t size() const override { return 1; }

private:
  // Example internal state
  uint8_t _colorR{0}, _colorG{0}, _colorB{0};
  bool _dirty{false};
  uint8_t _lastMinute{255};
  uint32_t _lastTimePollMs{0};

  void applyToHardware() {
    // TODO: translate _colorR/_colorG/_colorB + current time info into hardware operations
    // - For LED strips: write to buffer and call FastLED.show()
    // - For GPIO LED: digitalWrite(...) based on brightness/color
    // - For displays: draw digits/segments and refresh
  }
};

// Factory: provide the default driver instance used by the app
#ifndef USE_7SEGSTRIP
HalDriver* createDefaultDriver() {
  static TemplateDriver d;
  return &d;
}
#endif

// Driver_Qlock3030: Word‑clock style driver for a 114‑LED matrix strip using FastLED
// Behavior:
// - Renders time using the Qlock mask/mapping approach adapted from Example/MyQlock.
// - Uses a linear 114‑LED strip wired to represent a 12x13 matrix (serpentine, with borders masked).
// - Color is controlled by hal->fill(r,g,b) from the Web UI; default color if none set.
// - Updates once per second; recomputes mask on minute changes (supports 5‑minute words + minute dots).
// - While not synced, LEDs remain off.

#ifdef USE_QLOCK3030

#include "HalDriver.hpp"
#include "../services/TimeService.hpp"
#include "../core/Log.hpp"
#include "../core/Config.hpp"
#include <FastLED.h>
#include <time.h>

#ifndef LED_PIN
#define LED_PIN 2
#endif

#ifndef QLOCK_LED_COUNT
#define QLOCK_LED_COUNT 114
#endif

// Matrix geometry (from Example/MyQlock)
#ifndef QLOCK_ROWS
#define QLOCK_ROWS 12
#endif
#ifndef QLOCK_COLS
#define QLOCK_COLS 13
#endif

class DriverQlock3030 : public HalDriver {
public:
  void begin() override {
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(_leds, QLOCK_LED_COUNT);
    FastLED.setBrightness(128);
    fill_solid(_leds, QLOCK_LED_COUNT, CRGB::Black);
    FastLED.show();
    _colorR = 255; _colorG = 255; _colorB = 255; // default white
    _lastMinute = 255; // force first render
    _dirty = true;
    _lastHueUpdateMs = millis();
  }

  void setAutoHue(bool enabled, uint16_t degPerMin) override {
    _autoHueEnabled = enabled;
    _autoHueDegPerMin = degPerMin;
  }

  void loop() override {
    const uint32_t nowMs = millis();
    if (nowMs - _lastPollMs < 200) {
      // Push pending color updates even if not polling time
      if (_dirty) { applyToHardware(); _dirty = false; }
      return;
    }
    _lastPollMs = nowMs;

    if (!TimeSvc::isSynced()) {
      // keep off until synced
      if (!_unsyncedShown) {
        fill_solid(_leds, QLOCK_LED_COUNT, CRGB::Black);
        FastLED.show();
        _unsyncedShown = true;
      }
      return;
    }
    _unsyncedShown = false;

    struct tm tmv{};
    if (!TimeSvc::getLocal(tmv)) {
      time_t t = time(nullptr);
      localtime_r(&t, &tmv);
    }
    const uint8_t hh = tmv.tm_hour;  // 0..23
    const uint8_t mm = tmv.tm_min;   // 0..59
    const uint8_t ss = tmv.tm_sec;   // 0..59

    // AutoHue: update base hue gradually if enabled
    if (_autoHueEnabled) {
      const uint32_t ms = nowMs;
      const uint32_t dt = ms - _lastHueUpdateMs;
      if (dt >= 1000) {
        _lastHueUpdateMs = ms;
        // Convert degrees per minute to degrees per second
        float dps = (float)_autoHueDegPerMin / 60.0f;
        _autoHueAccumDeg += dps; // per second step
        while (_autoHueAccumDeg >= 360.0f) _autoHueAccumDeg -= 360.0f;
        // Convert current RGB to HSV, replace H with accum, keep S/V
        CHSV hsv = rgb2hsv_approximate(CRGB(_colorR,_colorG,_colorB));
        hsv.h = (uint8_t)lroundf((_autoHueAccumDeg / 360.0f) * 255.0f);
        CRGB rgb; hsv2rgb_rainbow(hsv, rgb);
        _renderR = rgb.r; _renderG = rgb.g; _renderB = rgb.b;
        _dirty = true;
      }
    } else {
      _renderR = _colorR; _renderG = _colorG; _renderB = _colorB;
    }

    // Only recompute mask when minute changes or on first render
    if (mm != _lastMinute || _firstFrame) {
      _firstFrame = false;
      _lastMinute = mm;
      const uint32_t mask = timeMaskUpdate(hh, mm);
      pixelStateUpdate(mask);
      // Map state to LEDs buffer with current color
      renderFrame();
      FastLED.show();
    } else {
      // Optional: minute dots animation based on seconds could go here
    }
  }

  void setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b) override {
    if (index < QLOCK_LED_COUNT) { _leds[index].setRGB(r,g,b); }
  }

  void fill(uint8_t r, uint8_t g, uint8_t b) override {
    // Store color for rendering words; applied at next render/show
    _colorR = r; _colorG = g; _colorB = b; _dirty = true;
  }

  void clear() override { fill_solid(_leds, QLOCK_LED_COUNT, CRGB::Black); _dirty = true; }

  void show() override { applyToHardware(); _dirty = false; FastLED.show(); }

  uint16_t size() const override { return QLOCK_LED_COUNT; }

private:
  CRGB _leds[QLOCK_LED_COUNT];
  uint8_t _colorR{255}, _colorG{255}, _colorB{255};
  uint8_t _renderR{255}, _renderG{255}, _renderB{255};
  uint32_t _lastPollMs{0};
  bool _unsyncedShown{false};
  uint8_t _lastMinute{255};
  bool _dirty{false};
  bool _firstFrame{true};
  // AutoHue
  bool _autoHueEnabled{false};
  uint16_t _autoHueDegPerMin{2};
  float _autoHueAccumDeg{0.0f};
  uint32_t _lastHueUpdateMs{0};

  // Geometry and masks (adapted from Example/MyQlock.h)
  // Mapping converts matrix (row,col) to strip index (150=unused)
  const uint8_t Mapping[QLOCK_ROWS][QLOCK_COLS] = {
    {113,150,150,150,150,150,150,150,150,150,150,150,101},
    {150,112,111,110,109,108,107,106,105,104,103,102,150},
    {150,90,91,92,93,94,95,96,97,98,99,100,150},
    {150,89,88,87,86,85,84,83,82,81,80,79,150},
    {150,68,69,70,71,72,73,74,75,76,77,78,150},
    {150,67,66,65,64,63,62,61,60,59,58,57,150},
    {150,46,47,48,49,50,51,52,53,54,55,56,150},
    {150,45,44,43,42,41,40,39,38,37,36,35,150},
    {150,24,25,26,27,28,29,30,31,32,33,34,150},
    {150,23,22,21,20,19,18,17,16,15,14,13,150},
    {150,1,2,3,4,5,6,7,8,9,10,11,150},
    {0,150,150,150,150,150,150,150,150,150,150,150,12}
  };

  const uint32_t Mask[QLOCK_ROWS][QLOCK_COLS] = {
    {16,0,0,0,0,0,0,0,0,0,0,0,30},
    {0,1073610752,1073610752,0,1073610752,1073610752,1073610752,0,4194304,4194304,4194304,4194304,0},
    {0,2097152,2097152,2097152,2097152,2097152,2097152,1048576,1048576,1048576,1048576,1048576,0},
    {0,67108864,67108864,67108864,67108864,536870912,0,134217728,25165824,16777216,16777216,16777216,0},
    {0,262144,262144,262144,131072,537001984,131072,131072,142737408,131072,0,0,0},
    {0,268435456,268435456,268435456,268435456,537395200,524288,524288,8912896,134217728,0,0,0},
    {0,33554432,33554432,33554432,33554432,536870912,536608768,536608768,536608768,536608768,536608768,536346624,0},
    {0,126976,126976,126976,126976,126976,0,16384,16384,32896,32896,32896,0},
    {0,256,256,0,16640,16640,16640,16640,16640,0,0,0,0},
    {0,0,13824,13824,13824,13824,13824,5120,70720,70720,70720,70720,0},
    {0,2048,2048,0,2048,2048,2048,2048,2048,0,0,0,0},
    {24,0,0,0,0,0,0,0,0,0,0,0,28}
  };

  uint8_t AbsoluteOn[QLOCK_ROWS][QLOCK_COLS]{};

  static uint32_t timeMaskUpdate(uint8_t H, uint8_t M) {
    // Ported from Example/MyQlock.cpp
    uint8_t hours = H % 13 + (H - 1) / 12 + (M >= 35);
    uint8_t Minutes = M / 5;
    uint8_t minutes = M % 5;
    if (H == 23 && M >= 35) { hours = 0; }
    if (H == 12 && M >= 35) { hours = 1; }
    return (uint32_t)1 << (17 + hours) | (uint32_t)1 << (5 + Minutes) | (uint32_t)1 << minutes;
  }

  void pixelStateUpdate(uint32_t TimeMask) {
    for (uint8_t x = 0; x < QLOCK_COLS; x++) {
      for (uint8_t y = 0; y < QLOCK_ROWS; y++) {
        AbsoluteOn[y][x] = ((Mask[y][x] & TimeMask) > 0) ? 1 : 0;
      }
    }
  }

  void renderFrame() {
    // Map AbsoluteOn into the linear LED buffer with current RGB color
    for (uint8_t x = 0; x < QLOCK_COLS; x++) {
      for (uint8_t y = 0; y < QLOCK_ROWS; y++) {
        uint8_t idx = Mapping[y][x];
        if (idx >= QLOCK_LED_COUNT) continue; // 150 markers ignored
        if (AbsoluteOn[y][x]) _leds[idx].setRGB(_renderR, _renderG, _renderB);
        else _leds[idx] = CRGB::Black;
      }
    }
  }

  void applyToHardware() {
    // Re-apply current color to the last frame without recomputing time
    for (uint8_t x = 0; x < QLOCK_COLS; x++) {
      for (uint8_t y = 0; y < QLOCK_ROWS; y++) {
        uint8_t idx = Mapping[y][x];
        if (idx >= QLOCK_LED_COUNT) continue;
        if (AbsoluteOn[y][x]) _leds[idx].setRGB(_renderR, _renderG, _renderB);
        else _leds[idx] = CRGB::Black;
      }
    }
    FastLED.show();
  }
};

// Override default driver factory when enabled
HalDriver* createDefaultDriver() {
  static DriverQlock3030 d;
  return &d;
}

#endif // USE_QLOCK3030

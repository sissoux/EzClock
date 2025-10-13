// Driver_7SegStrip: FastLED-based linear 7-segment renderer for HH:MM
//
// Summary
// - Renders time as HH:MM on a single linear LED strip.
// - Layout pattern is compatible with regex: "[\\d_][\\d_][-.':][\\d_][\\d_]"
//   where '_' is a blank digit, and separator is one of '-', '.', ':', or '\''. 
// - Color updates are PUSHED by the Web UI via hal->fill(r,g,b); hal->show().
// - Time is polled once per second via TimeSvc::getLocal() or localtime_r().
//
// Build flags (example):
// -D USE_7SEGSTRIP=1
// -D LED_PIN=2           // strip data pin
// -D LEDS_PER_SEG=2      // how many LEDs per each of the 7 segments
// -D STRIP_SEPARATOR=':' // optional: default separator character (':','-','.','\'')
// -D FADE_MS=300         // optional: crossfade duration in milliseconds
//
// Note: Ensure FastLED is available (platformio.ini -> lib_deps = fastled/FastLED@^3.6.0)

#ifdef USE_7SEGSTRIP

#include "HalDriver.hpp"
#include "../services/TimeService.hpp"
#include <FastLED.h>
#include <time.h>

#ifndef LED_PIN
#define LED_PIN 2
#endif

#ifndef LEDS_PER_SEG
#define LEDS_PER_SEG 2
#endif

#ifndef FADE_MS
#define FADE_MS 300
#endif

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#ifndef STRIP_SEPARATOR
#define SEP_STR_LIT ":"
#else
#define SEP_STR_LIT STR(STRIP_SEPARATOR)
#endif

// Geometry: 4 digits + 1 separator (filler)
#define DIGITS 4
#define FILLERS 1
#define DIGIT_LENGTH (7 * LEDS_PER_SEG)
#define FILLER_LENGTH (2 + LEDS_PER_SEG)
#define STRIP_LENGTH (DIGITS * DIGIT_LENGTH + FILLERS * FILLER_LENGTH)

// 7-seg bitmaps for 0..9 (MSB->LSB order for 7 segments)
static const uint8_t SEVENSEG[10] = { 0x7e, 0x18, 0x37, 0x3d, 0x59, 0x6d, 0x6f, 0x38, 0x7f, 0x7d };

class Driver7SegStrip : public HalDriver {
public:
  void begin() override {
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(_leds, STRIP_LENGTH);
    FastLED.setBrightness(128);
    clearMask(_currentMask);
    clearMask(_targetMask);
    clearMask(_prevMask);
    FastLED.show();
    _lastMinute = 255; // force first render
    _dirty = true;
    _fadeMs = FADE_MS;
  }
  void setSmoothing(uint16_t ms) override {
    _fadeMs = ms;
  }

  void loop() override {
    // Poll time periodically; detect minute change
    uint32_t ms = millis();
    if (ms - _lastPoll >= 200) { // check 5x/sec for snappy transitions
      _lastPoll = ms;
      struct tm tmv{};
      if (!TimeSvc::getLocal(tmv)) {
        time_t t = time(nullptr);
        localtime_r(&t, &tmv);
      }
      uint8_t hh = tmv.tm_sec;  // 0..23
      uint8_t mm = tmv.tm_min;   // 0..59
      uint8_t ss = tmv.tm_sec;   // 0..59
      // Trigger re-render only when minute or hour changes
      if (mm != _lastMinute || hh != _lastHour) {
        _lastMinute = mm;
        _lastHour = hh;
        // If already transitioning, finalize to current target first
        if (_inTransition) {
          copyMask(_targetMask, _currentMask);
          _inTransition = false;
        }
        // Prepare new transition from current -> target
        copyMask(_currentMask, _prevMask);
        renderTime(hh, mm, _targetMask);
        _transitionStart = ms;
        _inTransition = (_fadeMs > 0);
        _dirty = true;
      }
    }

    // Render output if in transition or something changed
    if (_inTransition || _dirty) {
      uint8_t progress = 255;
      if (_inTransition) {
        uint32_t elapsed = ms - _transitionStart;
        progress = (uint8_t)((_fadeMs == 0) ? 255 : (elapsed >= _fadeMs ? 255 : (elapsed * 255) / _fadeMs));
      }

      for (int i = 0; i < STRIP_LENGTH; ++i) {
        uint16_t fromI = _inTransition ? (_prevMask[i] ? 255 : 0) : (_currentMask[i] ? 255 : 0);
        uint16_t toI   = _inTransition ? (_targetMask[i] ? 255 : 0) : fromI;
        uint16_t intensity = (uint16_t)((fromI * (255 - progress) + toI * progress + 127) / 255); // rounded
        _leds[i].r = (uint8_t)((_colorR * intensity) / 255);
        _leds[i].g = (uint8_t)((_colorG * intensity) / 255);
        _leds[i].b = (uint8_t)((_colorB * intensity) / 255);
      }
      FastLED.show();

      if (_inTransition && progress >= 255) {
        // Transition complete
        copyMask(_targetMask, _currentMask);
        _inTransition = false;
      }
      _dirty = false;
    }
  }

  // Optional granular pixel control (unused by current UI)
  void setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b) override {
    if (index >= STRIP_LENGTH) return;
    _colorR = r; _colorG = g; _colorB = b;
    _currentMask[index] = true;
    _targetMask[index] = true;
    _dirty = true;
  }

  // Web UI pushes a single color via /api/color
  void fill(uint8_t r, uint8_t g, uint8_t b) override {
    _colorR = r; _colorG = g; _colorB = b; _dirty = true;
  }

  void clear() override {
    clearMask(_currentMask);
    clearMask(_targetMask);
    clearMask(_prevMask);
    _dirty = true;
  }

  void show() override {
    // Push whatever state is pending
    _dirty = true; // loop() will apply color to current/transition and show
  }

  uint16_t size() const override { return STRIP_LENGTH; }

private:
  // Masks: true = segment LED ON, false = OFF
  bool _currentMask[STRIP_LENGTH]{}; // committed pattern
  bool _targetMask[STRIP_LENGTH]{};  // next pattern
  bool _prevMask[STRIP_LENGTH]{};    // previous pattern (for blending)
  CRGB _leds[STRIP_LENGTH];
  uint8_t _colorR{0}, _colorG{128}, _colorB{0}; // default green-ish
  bool _dirty{false};
  uint8_t _lastMinute{255};
  uint8_t _lastHour{255};
  uint32_t _lastPoll{0};
  bool _inTransition{false};
  uint32_t _transitionStart{0};
  uint16_t _fadeMs{FADE_MS};

  static bool isDigit(char c) { return c >= '0' && c <= '9'; }

  static void clearMask(bool* mask) {
    for (int i = 0; i < STRIP_LENGTH; ++i) mask[i] = false;
  }

  static void copyMask(const bool* from, bool* to) {
    for (int i = 0; i < STRIP_LENGTH; ++i) to[i] = from[i];
  }

  // Render HH:MM into _buffer according to the linear layout
  void renderTime(uint8_t hh, uint8_t mm, bool* outMask) {
    // Build pattern string matching regex: [\d_][\d_][-.':][\d_][\d_]
    // Robustly parse separator from macro (handles :, '-', '.', '\'', with or without quotes)
    const char* sepstr = SEP_STR_LIT;              // e.g. ":" or "-" or "':'"
    char sep = (sepstr[0] == '\'' && sepstr[1])   // if ":'': then take second char
           ? sepstr[1]
           : sepstr[0];                      // else first char (e.g. ':')
    char pattern[5];
    pattern[0] = char('0' + (hh / 10));
    pattern[1] = char('0' + (hh % 10));
    pattern[2] = sep; // '-', '.', ':', or '\''
    pattern[3] = char('0' + (mm / 10));
    pattern[4] = char('0' + (mm % 10));

    // Map pattern to mask
    int offset = 0;
    clearMask(outMask);
    for (int i = 0; i < 5; ++i) {
      char c = pattern[i];
      if (i == 2) {
        // Separator occupies FILLER_LENGTH LEDs
        writeSeparator(outMask, offset, c);
        offset += FILLER_LENGTH;
      } else if (c == '_') {
        // Blank digit
        offset += DIGIT_LENGTH;
      } else if (isDigit(c)) {
        uint8_t digit = uint8_t(c - '0');
        writeDigit(outMask, offset, digit);
        offset += DIGIT_LENGTH;
      } else {
        // Unsupported char, treat as blank
        offset += (i == 2) ? FILLER_LENGTH : DIGIT_LENGTH;
      }
    }
  }

  void writeDigit(bool* mask, int base, uint8_t digit) {
    uint8_t segmask = SEVENSEG[digit];
    // 7 segments -> for each bit, set LEDS_PER_SEG entries
    for (int seg = 0; seg < 7; ++seg) {
      bool on = (segmask & (1 << (6 - seg))) != 0; // MSB first
      for (int led = 0; led < LEDS_PER_SEG; ++led) {
        mask[base + seg * LEDS_PER_SEG + led] = on;
      }
    }
  }

  void writeSeparator(bool* mask, int base, char c) {
    // FILLER layout (length = 2 + LEDS_PER_SEG) from the Python example:
    // '.' => [sign, off, off, off]
    // '-' => [off, sign, sign, off]
    // '\'' => [off, off, off, sign]
    // ':' (not in original mapping) -> choose middle dot: [sign, off, off, sign]
    // Use first 2 fixed LEDs, then LEDS_PER_SEG for the vertical bar
    // Map into _buffer as simple true/false pattern
    // We'll linearize as positions 0..(FILLER_LENGTH-1)
    for (int i = 0; i < FILLER_LENGTH; ++i) mask[base + i] = false;
    switch (c) {
      case '.':
        mask[base + 0] = true; break;
      case '-':
        mask[base + 1] = true; if (FILLER_LENGTH > 2) mask[base + 2] = true; break;
      case '\'':
        mask[base + (FILLER_LENGTH - 1)] = true; break;
      case ':':
        mask[base + 0] = true; mask[base + (FILLER_LENGTH - 1)] = true; break;
      default:
        break; // center-ish
    }
  }
};

// Override the default driver when enabled
HalDriver* createDefaultDriver() {
  static Driver7SegStrip d;
  return &d;
}

#endif // USE_7SEGSTRIP

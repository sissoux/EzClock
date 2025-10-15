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
// -D STARTUP_ANIM_MS=3000 // optional: startup animation duration (ms)
// -D STARTUP_FLASH=1      // optional: initial white flash for 150 ms
// -D STARTUP_STEP_MS=200  // optional: time between animation steps (ms)
//
// Note: Ensure FastLED is available (platformio.ini -> lib_deps = fastled/FastLED@^3.6.0)

#ifdef USE_7SEGSTRIP

#include "HalDriver.hpp"
#include "../services/TimeService.hpp"
#include "../core/Log.hpp"
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

#ifndef STARTUP_ANIM_MS
#define STARTUP_ANIM_MS 2500
#endif

#ifndef STARTUP_STEP_MS
#define STARTUP_STEP_MS 200
#endif

#ifdef STARTUP_FLASH
#ifndef STARTUP_FLASH_MS
#define STARTUP_FLASH_MS 150
#endif
#else
#define STARTUP_FLASH_MS 0
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

#ifdef DRIVER_DEBUG
#define DLOG(fmt, ...) LOGI("[7SEG] " fmt, ##__VA_ARGS__)
#else
#define DLOG(fmt, ...)
#endif

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
    // Startup animation state
    #ifndef DISABLE_STARTUP_ANIM
      _animPhase = (STARTUP_FLASH_MS > 0) ? PHASE_FLASH : PHASE_SCROLL;
    #else
      _animPhase = PHASE_DONE;
    #endif
    _startupStart = millis();
    _lastStepTime = _startupStart;
    _scrollPrimed = false; // we'll generate first scroll frame immediately
    _useOverrideColor = false;
    _startupPrevBrightness = FastLED.getBrightness();
    FastLED.setBrightness(255); // ensure startup anim is clearly visible
    if (_animPhase == PHASE_FLASH) {
      for (int i=0;i<STRIP_LENGTH;++i) _leds[i] = CRGB::White;
      FastLED.show();
    }
    DLOG("begin: phase=%d", (int)_animPhase);
    _dirty = true; // request first frame
  }
  void restartAnimation() override {
    #ifdef DISABLE_STARTUP_ANIM
      return; // disabled
    #endif
    _animPhase = (STARTUP_FLASH_MS > 0) ? PHASE_FLASH : PHASE_SCROLL;
    _startupStart = millis();
    _lastStepTime = _startupStart;
    _inTransition = false;
    _useOverrideColor = false;
    DLOG("restartAnimation phase=%d", (int)_animPhase);
    _dirty = true;
  }
  void setSmoothing(uint16_t ms) override {
    _fadeMs = ms;
  }

  void loop() override {
    // Poll time periodically; detect minute change
    uint32_t ms = millis();
    // Handle startup animation first
    // Startup phase handling
    if (_animPhase == PHASE_FLASH) {
      if (ms - _startupStart >= STARTUP_FLASH_MS) {
        _animPhase = PHASE_SCROLL;
        _lastStepTime = ms;
        DLOG("flash -> scroll");
      } else {
            _scrollPrimed = false; // ensure immediate first frame after flash
        return; // remain white
      }
    }
    if (_animPhase == PHASE_SCROLL) {
      uint32_t elapsed = ms - _startupStart - STARTUP_FLASH_MS;
      if (elapsed >= STARTUP_ANIM_MS) {
        _animPhase = PHASE_DONE;
        _useOverrideColor = false;
        _lastMinute = 255; _lastHour = 255; _dirty = true; // force initial render
        FastLED.setBrightness(_startupPrevBrightness);
        DLOG("scroll complete -> done");
      } else if (ms - _lastStepTime >= STARTUP_STEP_MS) {
        _lastStepTime = ms;
        // Determine step index
            // Prime first frame immediately (when !_scrollPrimed) so animation is visible even with no flash
        uint16_t stepsTotal = STARTUP_ANIM_MS / STARTUP_STEP_MS;
            _scrollPrimed = true;
        if (!stepsTotal) stepsTotal = 1;
        uint16_t step = (elapsed / STARTUP_STEP_MS) % stepsTotal;
        // Map step to base digit (0..9) cycling
        uint8_t d = step % 10;
        uint8_t hue = (uint8_t)((elapsed * 255UL) / STARTUP_ANIM_MS);
        const char* sepstr = SEP_STR_LIT;
        char sep = (sepstr[0] == '\'' && sepstr[1]) ? sepstr[1] : sepstr[0];
        char pat[5];
        pat[0] = char('0' + d);
        pat[1] = char('0' + ((d + 1) % 10));
        pat[2] = sep;
        pat[3] = char('0' + ((d + 2) % 10));
        pat[4] = char('0' + ((d + 3) % 10));
        renderPattern(pat, _currentMask);
        _useOverrideColor = true;
        _overrideColor = CHSV(hue, 255, 255);
        _dirty = true;
      }
    }
    if (_animPhase == PHASE_DONE && (ms - _lastPoll >= 200)) { // skip time polling during startup anim
      _lastPoll = ms;
      struct tm tmv{};
      if (!TimeSvc::getLocal(tmv)) {
        time_t t = time(nullptr);
        localtime_r(&t, &tmv);
      }
      uint8_t hh = tmv.tm_hour;  // 0..23
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

    // While time isn't synced and startup is done, fade the separator on/off
  bool unsyncedMode = (_animPhase == PHASE_DONE && !TimeSvc::isSynced());
  if (unsyncedMode && !_reportedUnsynced) { DLOG("enter unsynced breathing"); _reportedUnsynced = true; }
    if (unsyncedMode) {
      // Build separator-only mask into current
      clearMask(_currentMask);
      const int SEP_BASE = DIGIT_LENGTH * 2; // after two digits
      const char* sepstr = SEP_STR_LIT; char sep = (sepstr[0] == '\'' && sepstr[1]) ? sepstr[1] : sepstr[0];
      writeSeparator(_currentMask, SEP_BASE, sep);
      _inTransition = false;
      _dirty = true;
    }

    // Render output if in transition or something changed
    if (_inTransition || _dirty || unsyncedMode) {
      uint8_t progress = 255;
      if (_inTransition) {
        uint32_t elapsed = ms - _transitionStart;
        progress = (uint8_t)((_fadeMs == 0) ? 255 : (elapsed >= _fadeMs ? 255 : (elapsed * 255) / _fadeMs));
      }

      uint8_t baseR = _useOverrideColor ? _overrideColor.r : _colorR;
      uint8_t baseG = _useOverrideColor ? _overrideColor.g : _colorG;
      uint8_t baseB = _useOverrideColor ? _overrideColor.b : _colorB;

      // When unsynced, apply a breathing effect on separator intensity
      uint8_t sepLevel = 255;
      if (unsyncedMode) {
        sepLevel = beatsin8(30, 40, 255); // 30 BPM, keep visibly on (min ~16%)
      }

      for (int i = 0; i < STRIP_LENGTH; ++i) {
        uint16_t intensity;
        if (unsyncedMode) {
          intensity = _currentMask[i] ? sepLevel : 0;
        } else {
          uint16_t fromI = _inTransition ? (_prevMask[i] ? 255 : 0) : (_currentMask[i] ? 255 : 0);
          uint16_t toI   = _inTransition ? (_targetMask[i] ? 255 : 0) : fromI;
          intensity = (uint16_t)((fromI * (255 - progress) + toI * progress + 127) / 255); // rounded
        }
        _leds[i].r = (uint8_t)((baseR * intensity) / 255);
        _leds[i].g = (uint8_t)((baseG * intensity) / 255);
        _leds[i].b = (uint8_t)((baseB * intensity) / 255);
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
  bool _useOverrideColor{false};
  CRGB _overrideColor{0,0,0};
  bool _dirty{false};
  uint8_t _lastMinute{255};
  uint8_t _lastHour{255};
  uint32_t _lastPoll{0};
  bool _inTransition{false};
  uint32_t _transitionStart{0};
  uint16_t _fadeMs{FADE_MS};
  enum AnimPhase : uint8_t { PHASE_FLASH=0, PHASE_SCROLL=1, PHASE_DONE=2 };
  AnimPhase _animPhase{PHASE_DONE};
  uint32_t _startupStart{0};
  uint8_t _startupPrevBrightness{128};
  uint32_t _lastStepTime{0};
  bool _reportedUnsynced{false};
  bool _scrollPrimed{false};

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

  void renderPattern(const char pattern[5], bool* outMask) {
    int offset = 0;
    clearMask(outMask);
    for (int i = 0; i < 5; ++i) {
      char c = pattern[i];
      if (i == 2) {
        writeSeparator(outMask, offset, c);
        offset += FILLER_LENGTH;
      } else if (c == '_') {
        offset += DIGIT_LENGTH;
      } else if (isDigit(c)) {
        uint8_t digit = uint8_t(c - '0');
        writeDigit(outMask, offset, digit);
        offset += DIGIT_LENGTH;
      } else {
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
    // '.' => [off, off, off, sign]
    // '-' => [off, sign, sign, off]
    // '\'' => [sign, off, off, off]
    // ':' (not in original mapping) -> choose middle dot: [sign, off, off, sign]
    // Use first 2 fixed LEDs, then LEDS_PER_SEG for the vertical bar
    // Map into _buffer as simple true/false pattern
    // We'll linearize as positions 0..(FILLER_LENGTH-1)
    for (int i = 0; i < FILLER_LENGTH; ++i) mask[base + i] = false;
    switch (c) {
      case '\'':
        mask[base + 0] = true; break;
      case '-':
        mask[base + 1] = true; if (FILLER_LENGTH > 2) mask[base + 2] = true; break;
      case '.':
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

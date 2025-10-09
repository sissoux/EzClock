// Minimal Blink example (default) or Web-enabled app (when USE_WEB is defined)
#include <Arduino.h>

#define USE_WEB

#if defined(USE_WEB)
	#include "core/Config.hpp"
	#include "hal/HalDriver.hpp"
	#include "services/WebService.hpp"
#else
	// Blink mode definitions
	#ifndef LED_PIN
		#define LED_PIN 2
	#endif
	#ifndef LED_ACTIVE_LOW
		#define LED_ACTIVE_LOW 0
	#endif
#endif

// -------------------------
// Web App mode
// -------------------------
#if defined(USE_WEB)
static HalDriver* g_hal = nullptr;
static Config g_cfg;
static WebService g_web;

void setup() {
	Serial.begin(115200);
	delay(100);
	Serial.println("\n[EzClock] Web mode starting...");

	g_hal = createDefaultDriver();
	g_hal->begin();

	g_cfg.load();
	g_web.begin(g_cfg, g_hal);
}

void loop() {
	g_web.loop();
	if (g_hal) g_hal->loop();
	delay(10);
}

// -------------------------
// Blink mode (default)
// -------------------------
#else

void setup() {
	pinMode(LED_PIN, OUTPUT);
	// Ensure LED starts OFF
	digitalWrite(LED_PIN, LED_ACTIVE_LOW ? HIGH : LOW);

	Serial.begin(115200);
	delay(100);
	Serial.println("\n[EzClock] Blink example starting...");
	Serial.print("LED pin: ");
	Serial.println(LED_PIN);
}

void loop() {
	static bool on = false;
	on = !on;

	if (LED_ACTIVE_LOW) {
		digitalWrite(LED_PIN, on ? LOW : HIGH);
	} else {
		digitalWrite(LED_PIN, on ? HIGH : LOW);
	}

	delay(200); // ~2 Hz blink
}

#endif

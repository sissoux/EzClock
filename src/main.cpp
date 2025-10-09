// Minimal Blink example for PlatformIO + Arduino
// Works on Seeed XIAO ESP32C3 using LED_BUILTIN if defined; falls back to GPIO 2.

#include <Arduino.h>

#define LED_PIN 2

// Some boards wire the LED as active-low. Leave 0 unless you know it's inverted.
#ifndef LED_ACTIVE_LOW
	#define LED_ACTIVE_LOW 0
#endif

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

	delay(200); // 1 Hz blink
}

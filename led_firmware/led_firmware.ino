#include <FastLED.h>

#define SERIAL_DEBUG     true

// LEDS
#define NUM_LEDS    25
#define DATA_PIN    11   // YELLOW
#define CLOCK_PIN   13   // GREEN
CRGB leds [NUM_LEDS];
int brightness = 120;

void setup() {
  if (SERIAL_DEBUG) {
    Serial.begin(115200);
  }

  // LEDs
  pinMode(DATA_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  FastLED.addLeds<WS2801, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);
	FastLED.setBrightness(brightness);
  fill_solid(&(leds[0]), NUM_LEDS, CRGB(0, 0, 0));
  FastLED.clear();

  // Master
  Serial1.begin(115200);
}

void loop() {
  fill_solid(&(leds[0]), NUM_LEDS, CRGB(255, 0, 0));
	FastLED.show();

  int incomingByte;
  if (Serial1.available() > 0) {
    incomingByte = Serial1.read();
    Serial.print(incomingByte, HEX);
    Serial.print(" :: ");
    Serial.println(incomingByte, DEC);
  }
}

/* === LEDs === */

void faderall() { for(int i = 0; i < NUM_LEDS; i++) { leds[i].nscale8(250); } }

void test() {
	static uint8_t hue = 0;
	// First slide the led in one direction
	for(int i = 0; i < NUM_LEDS; i++) {
		// Set the i'th led to red
		leds[i] = CHSV(hue++, 255, 255);
		// Show the leds
		FastLED.show();
		// now that we've shown the leds, reset the i'th led to black
		// leds[i] = CRGB::Black;
		faderall();
		// Wait a little bit before we loop around and do it again
		delay(40);
	}

	// Now go in the other direction.
	for(int i = (NUM_LEDS)-1; i >= 0; i--) {
		// Set the i'th led to red
		leds[i] = CHSV(hue++, 255, 255);
		// Show the leds
		FastLED.show();
		// now that we've shown the leds, reset the i'th led to black
		// leds[i] = CRGB::Black;
		faderall();
		// Wait a little bit before we loop around and do it again
		delay(40);
	}
}

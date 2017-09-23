#include <FastLED.h>
#include "led_slave.h"

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

  fill_solid(&(leds[0]), NUM_LEDS, CRGB(0, 0, 0));
  FastLED.show();
}

SLAVE_COMMAND mode = INVALID;
int byteNum = 0;

// note on command
int channel = -1;
int beat = -1;
int noteNum = -1;
int velocity = -1;

int idx = 0;

void loop() {
  // incoming commands
  byte bite;
  while (Serial1.available() > 0) {
    bite = Serial1.read();

    // handle command modes
    if (bite == BRIGHTNESS) {
      mode = BRIGHTNESS;
      byteNum = 0;
      Serial.print("BRIGHTNESS: ");
    } else if (bite == NOTE_ON) {
      mode = NOTE_ON;
      byteNum = 0;
    } else {
      if (mode == BRIGHTNESS) {

        FastLED.setBrightness(bite);
        FastLED.show();
        Serial.println(bite);

      } else if (mode == NOTE_ON) {
        if (byteNum == 0) {
          channel = bite;
        } else if (byteNum == 1) {
          beat = bite;
        } else if (byteNum == 2) {
          noteNum = bite;
        } else if (byteNum == 3) {
          velocity = bite;
          noteOn(channel, beat, noteNum, velocity);
        }
      }
      byteNum++;
    }
  }

  // fade and show updates to pixels every frame
  fadeAllPixels();
  // fill_solid(&(leds[0]), NUM_LEDS, CRGB(0, 0, 0));

  // setPixel(0, idx, CHSV(128, 255, 255));
  // setPixel(1, idx, CHSV(64, 255, 255));
  //
  FastLED.show();
  //
  // delay(500);

  idx++;
  if (idx >= 12) idx = 0;
}

/* === Note Events == */

void noteOn(int channel, int beat, int noteNum, int velocity) {
  Serial.print("NOTE ON: ");
  Serial.print(channel);
  Serial.print(" :: ");
  Serial.print(beat);
  Serial.print(" :: ");
  Serial.print(noteNum);
  Serial.print(" :: ");
  Serial.println(velocity);

  // translate note number to hue
  // int hue = map(noteNum, 64, 74, 0, 255);
  // 64 - 74
  int hue = channel == 0 ? 96 : 160;
  setPixel(channel, beat, CHSV(hue, 255, 255));

  // IDEAS:
  // + Circle is color wheel
  // + Each note spans the color wheel
  // + Rotating color wheel with bursts
}

/* === LEDs === */

int channel0[] = { 22, 8, 7, 6, 5, 4, 3, 2, 1, 0, 12, 23 };
int channel1[] = { 9, 21, 20, 19, 18, 17, 16, 15, 14, 13, 11, 10 };

void setPixel(int channel, int index, CHSV color) {
  int idx = channel == 0 ? channel0[index] : channel1[index];
  leds[idx] = color;
}

void fadeAllPixels() { for(int i = 0; i < NUM_LEDS; i++) { leds[i].nscale8(250); } }






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
		fadeAllPixels();
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
		fadeAllPixels();
		// Wait a little bit before we loop around and do it again
		delay(40);
	}
}

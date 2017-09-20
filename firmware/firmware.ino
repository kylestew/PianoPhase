#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <Bounce.h>
#include <FastLED.h>
#include <Metro.h>
#include <Encoder.h>
#include "piano_phase.h"


#define SERIAL_DEBUG     true

// GUItool: begin automatically generated code
AudioSynthKarplusStrong  mainVoice;        //xy=109,533
AudioFilterStateVariable filter1;        //xy=259,539
AudioEffectDelay         delay1;         //xy=422,687
AudioMixer4              mixer1;         //xy=660,683
AudioOutputI2S           i2s1;           //xy=851,601
AudioConnection          patchCord1(mainVoice, 0, filter1, 0);
AudioConnection          patchCord2(filter1, 0, delay1, 0);
AudioConnection          patchCord3(filter1, 0, mixer1, 0);
AudioConnection          patchCord4(delay1, 0, mixer1, 1);
AudioConnection          patchCord5(mixer1, 0, i2s1, 0);
AudioConnection          patchCord6(mixer1, 0, i2s1, 1);
AudioControlSGTL5000     sgtl5000_1;     //xy=700,488
// GUItool: end automatically generated code

// AUDIO SHIELD
// RESERVED PINS: 9, 11, 13, 18, 19, 22, 23

// LEDS
#define NUM_LEDS 25
#define DATA_PIN  2   // YELLOW
#define CLOCK_PIN 3   // GREEN
CRGB leds [NUM_LEDS];
const int DEFAULT_BRIGHT = 128;
const int MIN_BRIGHT = 10;
const int MAX_BRIGHT = 255;
int brightness = DEFAULT_BRIGHT;

// VOLUME POT
const int VOL_POT = A1; // PIN 15
int volume = 0;

// RING ENCODER
const int ENCA = 7; // All pins interrupt on Teensy
const int ENCB = 8; // swapped to encode other direction
long encPos = -999;
Encoder enc(ENCA, ENCB);
const int ENC_BUTTON = 10;
Bounce button = Bounce(ENC_BUTTON, 15);
enum ledColors {RED = 0, GREEN = 1, BLUE = 2, NONE = 3};
byte ledPins[] = { 4, 5, 6 }; // TODO: wire up BLUE 5

// INPUT SYSTEM
int controlMode = 0;    // 0 (BLU) == Phase
                        // 1 (GRN) == Tempo
                        // 2 (RED) == LED brightness

// MUSIC SETTINGS
#define mtof(N) ( (440 / 32.0) * pow(2,(N - 9) / 12.0) )
const int DEFAULT_PHASE = 0;
const int MIN_PHASE = 0;
const int MAX_PHASE = 1024;
const int DEFAULT_TEMPO = 60;
const int MIN_TEMPO = 0;
const int MAX_TEMPO = 100;
const int MIN_METRO = 20; // fastest BPM
const int MAX_METRO = 640; // slowest BPM
Metro metro = Metro(100);
int noteIdx = 0;
int phase = DEFAULT_PHASE;
int tempo = DEFAULT_TEMPO;


void setup() {
  if (SERIAL_DEBUG) {
    Serial.begin(115200);
  }

  // assign pins
  pinMode(VOL_POT, INPUT);
  pinMode(ENC_BUTTON, INPUT_PULLUP);
  pinMode(ledPins[RED], OUTPUT);
  analogWrite(ledPins[RED], 0);
  pinMode(ledPins[GREEN], OUTPUT);
  analogWrite(ledPins[GREEN], 0);
  pinMode(ledPins[BLUE], OUTPUT);
  analogWrite(ledPins[BLUE], 0);

  pinMode(DATA_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);

  // audio setup
  AudioMemory(120); // large memory needed for delay
  sgtl5000_1.enable();
  sgtl5000_1.volume(volume);

  // LEDs
  FastLED.addLeds<WS2801, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);
	FastLED.setBrightness(brightness);
  fill_solid(&(leds[0]), NUM_LEDS, CRGB(24, 128, 128));
  FastLED.clear();

  // start running!
  setMode(0);
  restartMusic();
}

void loop() {
  // volume control
  int n = analogRead(VOL_POT) / 32; // 0 - 32
  if (n != volume) {
    volume = n;
    sgtl5000_1.volume(map(n, 0, 32, 0, 1));
  }

  // mode switch
  button.update();
  if (button.fallingEdge()) {
    controlMode++;
    if (controlMode > 2) controlMode = 0;
    setMode(controlMode);
  }

  // mode input
  long pos = enc.read();
  if (pos != encPos) {
    encPos = updateMode(pos);
    enc.write(encPos); // write bounded value back to encoder
    Serial.println(encPos);
  }

  // music
  if (metro.check() == 1) {
    nextBeat();
  }

  // TEMP: make sure LEDs light
  // test();
}

/* === MODES === */
void setMode(int newMode) {
  controlMode = newMode;
  Serial.print("Mode: ");
  Serial.println(controlMode);

  if (controlMode == 0) {
    // (BLU) == Phase
    setKnobRGB(0, 0, 255);
    enc.write(phase);
  } else if (controlMode == 1) {
    // (GRN) == Tempo
    setKnobRGB(0, 255, 0);
    enc.write(tempo);
  } else {
    // (RED) == LED brightness
    setKnobRGB(255, 0, 0);
    enc.write(brightness);
  }
}

int updateMode(int val) {
  switch (controlMode) {
    case 0:
      phase = constrain(val, MIN_PHASE, MAX_PHASE);
      updatePhase();
      return phase;
    case 1:
      tempo = constrain(val, MIN_TEMPO, MAX_TEMPO);
      updateMetro();
      return tempo;
    case 2:
      brightness = constrain(val, MIN_BRIGHT, MAX_BRIGHT);
    	FastLED.setBrightness(brightness);
      return brightness;
    default:
      return 0;
  }
}

/* === Music Control === */

void restartMusic() {
  updateMetro();

  // TODO: shuffle play settings

}

void nextBeat() {
    // phrase1
    //
    // Serial.print(noteIdx);
    // Serial.print(" ");
    // noteIdx++;
    // if (noteIdx >= 12) {
    //   noteIdx = 0;
    //   Serial.println("");
    // }
}

void updatePhase() {

}

void updateMetro() {
  metro.interval(map(tempo, MIN_TEMPO, MAX_TEMPO, MAX_METRO, MIN_METRO));
}

void noteOn(byte aNote) {
  // note = aNote;
  // mainVoice.noteOn(mtof(note), 1.0);
}

void noteOff() {
  // mainVoice.noteOff(1.0);
}












/* === LEDs === */

void fadeall() { for(int i = 0; i < NUM_LEDS; i++) { leds[i].nscale8(250); } }

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
		fadeall();
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
		fadeall();
		// Wait a little bit before we loop around and do it again
		delay(40);
	}
}


/* === Knob LED === */
void setKnobRGB(int r, int g, int b) {
  // TODO: use true RGB
  // NOW: mix Green + Red for blue
  if (b > 0) {
    r = b;
    g = b;
  }

  analogWrite(ledPins[RED], r);
  analogWrite(ledPins[GREEN], g);
  analogWrite(ledPins[BLUE], b);
}

float map(float value, float istart, float istop, float ostart, float ostop) {
  return ostart + (ostop - ostart) * ((value - istart) / (istop - istart));
}

#include <Bounce.h>
#include <FastLED.h>
#include <Metro.h>
#define ENCODER_OPTIMIZE_INTERRUPTS
#include <Encoder.h>
#include "piano_phase.h"
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

#define SERIAL_DEBUG     true

// AUDIO CONNECTIONS
// RESERVED PINS: 9, 11, 13, 18, 19, 22, 23

// VOICE 1
AudioSynthKarplusStrong  stringVoice;

// OUTPUT
AudioControlSGTL5000     sgtl5000;
AudioOutputI2S           i2sOut;

// WIRING
AudioConnection          patchCord13(stringVoice, 0, i2sOut, 0);
AudioConnection          patchCord14(stringVoice, 0, i2sOut, 1);

// EFFECTS:
// CHORUS
// #define CHORUS_DELAY_LENGTH (16*AUDIO_BLOCK_SAMPLES)
// short delayline[CHORUS_DELAY_LENGTH];

// LEDS
#define NUM_LEDS    24
#define DATA_PIN    24   // YELLOW
#define CLOCK_PIN   25   // GREEN
CRGB leds [NUM_LEDS];
const int DEFAULT_BRIGHT = 128;
const int MIN_BRIGHT = 10;
const int MAX_BRIGHT = 220;
int brightness = DEFAULT_BRIGHT;

// VOLUME POT
// TODO: average input to filter
const int VOL_POT = A1; // PIN 15
int volume = 0;
const float MAX_VOLUME = 0.8;

// RING ENCODER
const int ENCA = 8; // All pins interrupt on Teensy
const int ENCB = 7;
long encPos = -999;
Encoder enc(ENCA, ENCB);
const int ENC_BUTTON = 1;
Bounce button = Bounce(ENC_BUTTON, 20);
enum ledColors {RED = 0, GREEN = 1, BLUE = 2, NONE = 3};
byte ledPins[] = { 4, 6, 5 };

// INPUT SYSTEM
int controlMode = 0;    // 0 (BLU) == Phase
                        // 1 (GRN) == Tempo
                        // 2 (RED) == LED brightness

// PHASE
// TODO: fix phase scratching
const int DEFAULT_PHASE = 0;
const int MIN_PHASE = 0;
const int MAX_PHASE = 512;
int phase = DEFAULT_PHASE;

// TEMPO
// TODO: update ranges for new 4x mode
const int DEFAULT_TEMPO = 214; // synced to source material
const int MIN_TEMPO = 0;
const int MAX_TEMPO = 256;
const int MIN_METRO = 60; // fastest BPM
const int MAX_METRO = 500; // slowest BPM
Metro metro = Metro(100);
int tempo = DEFAULT_TEMPO;
int interval = 0; // ms to achieve tempo

// MUSIC COORDINATOR
#define mtof(N) ( (440 / 32.0) * pow(2,(N - 9) / 12.0) )
const int NUM_VOICES = 4;
int voiceIdx = -1;
int noteIdx = 0;
const int PLAY_TIME = 90000; // 90 seconds
Metro playTimer = Metro(0);

void setup() {
  if (SERIAL_DEBUG) {
    Serial.begin(115200);
  }

  // Encoder LEDs sink current
  pinMode(ENC_BUTTON, INPUT);
  pinMode(ledPins[RED], OUTPUT);
  digitalWrite(ledPins[RED], 1); // high = off
  pinMode(ledPins[GREEN], OUTPUT);
  digitalWrite(ledPins[GREEN], 1); // high = off
  pinMode(ledPins[BLUE], OUTPUT);
  digitalWrite(ledPins[BLUE], 1); // high = off

  // audio setup
  AudioMemory(24); // TODO: calc delay length
  sgtl5000.enable();
  sgtl5000.volume(volume);
  pinMode(VOL_POT, INPUT);

  // LEDs
  // pinMode(DATA_PIN, OUTPUT);
  // pinMode(CLOCK_PIN, OUTPUT);
  // FastLED.addLeds<WS2801, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);
	// FastLED.setBrightness(brightness);
  // fill_solid(&(leds[0]), NUM_LEDS, CRGB(0, 0, 0));
  // FastLED.clear();

  // start running!
  setMode(0);
  restartMusic();
}

void loop() {
  // fill_solid(&(leds[0]), NUM_LEDS, CRGB(255, 0, 0));
	// FastLED.show();

  // volume control
  int n = analogRead(VOL_POT) / 32; // 0 - 32
  if (n != volume) {
    volume = n;

    Serial.print("VOL: ");
    Serial.println(volume);

    sgtl5000.volume(map((float)n, 0.0, 32.0, 0.0, MAX_VOLUME));
  }

  // mode switch
  button.update();
  if (button.risingEdge()) {
    controlMode++;
    if (controlMode > 2) controlMode = 0;
    setMode(controlMode);
  }

  // mode input
  long pos = enc.read();
  if (pos != encPos) {
    encPos = updateMode(pos);
    enc.write(encPos); // write bounded value back to encoder
  }

  // play the music
  if (playTimer.check() == 1) {
    // // stop playing
    // stopMusic();
    //
    // // stop taking input
    // setMode(-1);
    //
    // // visually take a break
    // interlude();
    //
    // // run again
    // setMode(0);
    // restartMusic();
  } else if (metro.check() == 1) {
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
  } else if (controlMode == 2) {
    // (RED) == LED brightness
    setKnobRGB(255, 0, 0);
    enc.write(brightness);
  } else {
    // shutdown input
    setKnobRGB(0,0,0);
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

/* === Inputs === */

void updatePhase() {
  // tempo is percentage of interval which is determined by tempo
  int delayMS = map(phase, MIN_PHASE, MAX_PHASE, 0, interval);

  Serial.print(phase);
  Serial.print(" :: ");
  Serial.print(interval);
  Serial.print(" :: ");
  Serial.println(delayMS);

  // phaseDelay.delay(0, delayMS);
}

void updateMetro() {
  interval = map(tempo, MIN_TEMPO, MAX_TEMPO, MAX_METRO, MIN_METRO);
  metro.interval(interval);
  updatePhase(); // product of tempo
}

/* === Music Coordinator === */

void stopMusic() {
  // fade.fadeOut(1000);
}

void restartMusic() {
  // AudioNoInterrupts();

  // reset
  playTimer.interval(PLAY_TIME);
  phase = DEFAULT_PHASE;
  tempo = DEFAULT_TEMPO;
  updateMetro();

  // increment voice
  voiceIdx++;
  if (voiceIdx >= NUM_VOICES) voiceIdx = 0;


  // TEMP: OVERRIDE:
  voiceIdx = 0;


  // deselect all voices
  // voiceSelect.gain(0,0);
  // voiceSelect.gain(1,0);
  // voiceSelect.gain(2,0);
  // voiceSelect.gain(3,0);

  // setup current voice
  switch (voiceIdx) {
    case 0: // strings
      // enable chorus
      // if (!chorusVoice1.begin(delayline, CHORUS_DELAY_LENGTH, 2)) {
      //   Serial.println("AudioEffectChorus - begin failed");
      // }
      // chorusVoice1.voices(2);

      // voiceSelect.gain(0, 1.0);

      break;

    case 1: // synth
    /*
      osc1.begin(WAVEFORM_SQUARE);
      osc1.pulseWidth(0.5);
      osc1.amplitude(0.8);

      osc2.begin(WAVEFORM_PULSE);
      osc2.pulseWidth(0.75);
      osc2.amplitude(0.8);

      oscmix.gain(0, 0.6);
      oscmix.gain(1, 0.4);

      filterEnv.attack(10);
      filterEnv.hold(10);
      filterEnv.decay(25);
      filterEnv.sustain(0.4);
      filterEnv.release(70);

      // filter2.octaveControl();
      filter2.resonance(0.6);

      voiceSelect.gain(1, 1.0);
      */

      break;

    case 2: // drums
      break;
    case 3: // PWM?
      break;
  }

  // turn audio on
  // fade.fadeIn(1000);

  // AudioInterrupts();
}

void nextBeat() {
  int note = phrase1[noteIdx];
  noteOn(note);

  // increment note index
  noteIdx++;
  if (noteIdx >= 12) {
    noteIdx = 0;
  }
}

void noteOn(byte note) {
  // AudioNoInterrupts();

  // drums
  /*
  drum1.frequency(mtof(note) / 3.0);
  drum1.length(400);
  drum1.secondMix(1.0);
  drum1.pitchMod(0.5);
  drum1.noteOn();
  drum2.frequency(mtof(note) / 2.0);
  drum2.length(400);
  drum2.secondMix(1.0);
  drum2.pitchMod(0.5);
  drum2.noteOn();
  */

  /*
  // synth
  filter2.frequency(mtof(note));
  osc1.frequency(mtof(note));
  osc2.frequency(mtof(note + 0.2));
  filterEnv.noteOn();
  */

  switch (voiceIdx) {
    case 0: // strings
      stringVoice.noteOn(mtof(note), 1.0);

//      Serial.print("note frequency: ");
//      Serial.println(mtof(note));


      break;

    case 1: // synth
    /*
      osc1.begin(WAVEFORM_PULSE);
      osc1.pulseWidth(0.5);
      osc1.amplitude(0.8);

      osc2.begin(WAVEFORM_SAWTOOTH);
      osc2.pulseWidth(0.5);
      osc2.amplitude(0.8);

      oscmix.gain(0, 0.6);
      oscmix.gain(1, 0.4);

      */
      break;

    case 2: // drums
      break;
    case 3: // drone?
      break;
  }

  // AudioInterrupts();
}


















/* === LEDs === */

void interlude() {
  // TODO: animate a 15second LED flourish
}

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


/* === Knob LED === */
void setKnobRGB(int r, int g, int b) {
  analogWrite(ledPins[RED], 255 - r);
  analogWrite(ledPins[GREEN], 255 - g);
  analogWrite(ledPins[BLUE], 255 - b);
}

float map(float value, float istart, float istop, float ostart, float ostop) {
  return ostart + (ostop - ostart) * ((value - istart) / (istop - istart));
}

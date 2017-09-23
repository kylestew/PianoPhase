#include <Bounce.h>
#include <Metro.h>
#define ENCODER_OPTIMIZE_INTERRUPTS
#include <Encoder.h>
#include "piano_phase.h"
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include "led_slave.h"

#define SERIAL_DEBUG     true

// AUDIO CONNECTIONS
// RESERVED PINS: 9, 11, 13, 18, 19, 22, 23

// VOICE 1
AudioSynthKarplusStrong  stringVoice;
AudioEffectChorus        chorusVoice1;
// AudioConnection          pathCord0(stringVoice, 0, chorusVoice1, 0);
#define CHORUS_DELAY_LENGTH (8*AUDIO_BLOCK_SAMPLES)
#define CHORUS_VOICES 4
short chorusDelayLine[CHORUS_DELAY_LENGTH];

// VOICE SELECTION
AudioMixer4              voiceSelect;
// AudioConnection          pathCord1(chorusVoice1, 0, voiceSelect, 0);
AudioConnection          pathCord1(stringVoice, 0, voiceSelect, 0);
// AudioConnection          pathCord2(stringVoice, 0, voiceSelect, 1);
// AudioConnection          pathCord3(stringVoice, 0, voiceSelect, 2);
// AudioConnection          pathCord4(stringVoice, 0, voiceSelect, 3);

// DELAY
AudioEffectDelay         phaseDelay;
AudioMixer4              phaseMix;
AudioConnection          pathCord5(voiceSelect, 0, phaseMix, 0);
AudioConnection          pathCord6(voiceSelect, phaseDelay);
AudioConnection          pathCord7(phaseDelay, 0, phaseMix, 1);

// POST PROCESSING
AudioEffectReverb        reverb;
AudioConnection          pathCord8(phaseMix, reverb);

// OUTPUT
AudioEffectFade          fade;
AudioControlSGTL5000     sgtl5000;
AudioOutputI2S           i2sOut;
// AudioConnection          pathCord9(reverb, fade);
AudioConnection          pathCord9(phaseMix, fade);
AudioConnection          pathCord10(fade, 0, i2sOut, 0);
AudioConnection          pathCord11(fade, 0, i2sOut, 1);


// LED SLAVE
int MIN_BRIGHT = 12;
int MAX_BRIGHT = 230;
int DEFAULT_BRIGHTNESS = 160;
int brightness = DEFAULT_BRIGHTNESS;

// VOLUME POT
// TODO: average input to filter
const int VOL_POT = A1; // PIN 15
int volume = 0;
const float MAX_VOLUME = 0.55;

// RING ENCODER
const int ENCA = 3; // All pins interrupt on Teensy
const int ENCB = 2;
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
const int DEFAULT_TEMPO = 440; // synced to source material
const int MIN_TEMPO = 0;
const int MAX_TEMPO = 512;
const int MIN_METRO = 80; // fastest BPM
const int MAX_METRO = 500; // slowest BPM
Metro metro = Metro(100);
Metro phaseMetro = Metro(0);
bool phaseFired = false;
int tempo = DEFAULT_TEMPO;
int interval = 0; // ms to achieve tempo

// MUSIC COORDINATOR
#define mtof(N) ( (440 / 32.0) * pow(2,(N - 9) / 12.0) )
const int NUM_VOICES = 4;
bool playing = false;
int voiceIdx = -1;
int noteIdx = 0;
int prevNoteIdx = 0;
byte note = 0;
byte velocity = 0;
const int PLAY_TIME = 150000;
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

  // connect to slave
  Serial1.setTX(26);
  Serial1.setRX(27);
  Serial1.begin(115200);

  // audio setup
  AudioMemory(96); // TODO: calc delay length
  sgtl5000.enable();
  sgtl5000.volume(volume);
  pinMode(VOL_POT, INPUT);

  // start running!
  setMode(0);
  restartMusic();
}

void loop() {
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

  if (playing == true ) {
    playLoop();
  }
}

void playLoop() {
  // play the music
  if (metro.check() == 1) {
    nextBeat();

    phaseMetro.reset(); // sync with beat
    phaseFired = false;
  }

  if (phaseFired == false && phaseMetro.check() == 1) {
    // fire second channel note
    // index is previous note played
    sendNoteOn(1, prevNoteIdx, note, velocity);
    phaseFired = true;
  }

  // reset music on timer
  if (playTimer.check() == 1) {
    // stop playing
    stopMusic();

    // stop taking input
    setMode(-1);

    // visually take a break
    delay(4000);
    // interlude();

    // run again
    restartMusic();
    setMode(0);
    playTimer.reset();
  }
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
      sendCommand(BRIGHTNESS, brightness);
      return brightness;
    default:
      return 0;
  }
}

/* === Inputs === */

void updatePhase() {
  // tempo is percentage of interval which is determined by tempo
  int delayMS = map(phase, MIN_PHASE, MAX_PHASE, 0, interval);

  // Serial.print(phase);
  // Serial.print(" :: ");
  // Serial.print(interval);
  // Serial.print(" :: ");
  // Serial.println(delayMS);

  phaseDelay.delay(0, delayMS);
  phaseMetro.interval(delayMS);
}

void updateMetro() {
  interval = map(tempo, MIN_TEMPO, MAX_TEMPO, MAX_METRO, MIN_METRO);
  metro.interval(interval);
  updatePhase(); // product of tempo
}

/* === Music Coordinator === */

void stopMusic() {
  playing = false;
  fade.fadeOut(2000);
}

void restartMusic() {
  // AudioNoInterrupts();

  // reset
  playTimer.interval(PLAY_TIME);
  phase = DEFAULT_PHASE;
  tempo = DEFAULT_TEMPO;
  noteIdx = 0;
  updatePhase();
  updateMetro();
  metro.reset();

  // next animation
  sendCommand(RESET);

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
      if (!chorusVoice1.begin(chorusDelayLine, CHORUS_DELAY_LENGTH, CHORUS_VOICES)) {
        Serial.println("AudioEffectChorus - begin failed");
      }
      chorusVoice1.voices(CHORUS_VOICES);

      // voiceSelect.gain(0, 1.0);
      reverb.reverbTime(0);

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
  playing = true;
  fade.fadeIn(2000);

  // AudioInterrupts();
}

void nextBeat() {
  // play note
  note = phrase[noteIdx];
  velocity = noteOn(note);

  // send to slave
  sendNoteOn(0, noteIdx, note, velocity);

  // increment note index
  prevNoteIdx = noteIdx;
  noteIdx++;
  if (noteIdx >= 12) {
    noteIdx = 0;
  }
}

byte noteOn(byte note) {
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

  return 120; // velocity we played at

  // AudioInterrupts();
}

/* === SLAVE LEDs === */

void sendCommand(SLAVE_COMMAND command) {
  Serial1.write(command);
}

void sendCommand(SLAVE_COMMAND command, byte value) {
  Serial1.write(command);
  Serial1.write(value);
}

void sendNoteOn(byte channel, byte beat, byte noteNum, byte velocity) {
  if (SERIAL_DEBUG) {
    Serial.print("Slave Note On: ");
    Serial.print(channel);
    Serial.print(" :: ");
    Serial.print(beat);
    Serial.print(" :: ");
    Serial.println(noteNum);
  }

  Serial1.write(NOTE_ON);
  Serial1.write(channel);
  Serial1.write(beat);
  Serial1.write(noteNum);
  Serial1.write(velocity);
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

#include "music.h"

const int AUDIOPIN = 7;
const int tsTimeDivisions = 8000;  // timeslices per second in the timestamps
const int timeDivisions = 31250; // timeslices per second in the arduino program itself
const uint8_t apuCyclesPerSample = 29;
const uint16_t noiseWavelengths[] = { 4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068 };
const uint8_t lengths[32] = { 10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14, 12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30 };

const uint8_t sq_duty[4][8] = { { 0, 1, 0, 0, 0, 0, 0, 0 },
                                { 0, 1, 1, 0, 0, 0, 0, 0 },
                                { 0, 1, 1, 1, 1, 0, 0, 0 },
                                { 1, 0, 0, 1, 1, 1, 1, 1 } };

const uint8_t tri_duty[16] = { 7, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7 };

unsigned long nextTS;
uint8_t nextReg;
uint8_t nextVal;
unsigned int eventIdx = 0;
unsigned long timeStamp = 0;
uint8_t nextSample = 0;

// Remaining frame ticks to play each of the channels
volatile uint8_t sq1LenCnt = 0;
volatile uint8_t sq2LenCnt = 0;
volatile uint8_t triLenCnt = 0;
volatile uint8_t noiseLenCnt = 0;

// Remaining NES APU ticks until we clock the duty cycles
int16_t sq1WavelengthCnt = 0;
int16_t sq2WavelengthCnt = 0;
int16_t triWavelengthCnt = 0;
int16_t noiseWavelengthCnt = 0;

// Wavelength counter reset values
uint16_t sq1WavelengthReset = 0;
uint16_t sq2WavelengthReset = 0;
uint16_t triWavelengthReset = 0;
uint16_t noiseWavelengthReset = 0;

// Index of current duty cycle
uint8_t sq1DutyIdx = 0;
uint8_t sq2DutyIdx = 0;
uint8_t triDutyIdx = 0;

const uint8_t squareDuties = 8;
const uint8_t triDuties = 16;

// Square channel duty cycle types
uint8_t sq1DutyCycle = 0;
uint8_t sq2DutyCycle = 0;

// Volumes
uint8_t sq1Vol = 0;
uint8_t sq2Vol = 0;
uint8_t noiseVol = 0;

// LFSR data for noise generation
uint16_t noiseLFSR = 1;
uint8_t noiseType = 0;

uint8_t noiseSample = 0;
uint8_t sq1Sample = 0;
uint8_t sq2Sample = 0;
uint8_t triSample = 0;

// length-clocking step
uint8_t step = 0;

volatile uint8_t *audioOut;
uint8_t setLow;
uint8_t setHigh;

bool nextEvent() {
  unsigned long nextEv = pgm_read_dword_near(music+eventIdx);
  nextTS = (nextEv & 0x000fffff)<<2;
  nextReg = nextEv >> 28;
  nextVal = ((nextEv >> 20) & 0xff);
  eventIdx++;
  return nextTS > 0;
}

void setup() {
  pinMode(AUDIOPIN, OUTPUT);
  pinMode(13, OUTPUT);
  nextEvent();

  TCCR1A = 0;           // Init Timer1 settings
  TCCR1B = B00000010;   // Set timer1 prescaler to 8x
  TCNT1 = 65535 - 64;  // Timer preload, where 64 0.5uS ticks will be 32uS, for a 31250Hz sample rate

  TCCR3A = 0;           // Init Timer3 settings
  TCCR3B = B00000000;   // Stop timer3

  TIMSK3 = B00000001;  // Enable timer3 overflow interrupt
  TIMSK1 = B00000001;  // Enable timer1 overflow interrupt

  // Stuff to avoid running digitalWrite for the audio output pin
  uint8_t audioPinTimer = digitalPinToTimer(AUDIOPIN);
	uint8_t audioPinBit = digitalPinToBitMask(AUDIOPIN);
	uint8_t audioPinPort = digitalPinToPort(AUDIOPIN);

	audioOut = portOutputRegister(audioPinPort);
  setLow = *audioOut & ~audioPinBit;
  setHigh = *audioOut | audioPinBit;
}

// Clock the note length counters at 96 and 192Hz
void loop() {
  if (step == 0 || step == 2) {  // 96 Hz step
    if (sq1LenCnt) sq1LenCnt--;
    if (sq2LenCnt) sq2LenCnt--;
    if (noiseLenCnt) noiseLenCnt--;
  }
  if (step != 3) {  // 192 Hz step
    if (triLenCnt) triLenCnt--;
  }
  step++;
  if (step == 5) step = 0;
  //delayMicroseconds(5208);
  delay(5);
}

// Process register writes, generate audio
ISR(TIMER1_OVF_vect) {
  if (nextSample > 0) {
    *audioOut = setHigh;               // Turn on pin, start timer to turn it off
    if(nextSample < 63) {
      TCNT3 = 65535 - (8 * nextSample);  // Leave the pin high for between 8 and 512 cycles, depending on sample
      TCCR3B = B00000001;                // Start timer 3, clocked at 1x of clock speed
    }
  }

  // Reset timer 1 to 64*8 cycles from now (31250Hz repeat rate)
  TCNT1 = 65535 - 64;
  timeStamp++;

  if (timeStamp >= nextTS) {
    switch (nextReg) {
      // SQ1
      case 0:
        sq1DutyCycle = nextVal >> 6;
        sq1Vol = nextVal & 0xf;
        break;
      case 2:
        sq1WavelengthReset &= 0xff00;
        sq1WavelengthReset |= nextVal;
        break;
      case 3:
        sq1WavelengthReset &= 0x00ff;
        sq1WavelengthReset |= (uint16_t(nextVal & 0x7) << 8);
        sq1LenCnt = lengths[nextVal >> 3];
        break;
      // SQ2
      case 4:
        sq2DutyCycle = nextVal >> 6;
        sq2Vol = nextVal & 0xf;
        break;
      case 6:
        sq2WavelengthReset &= 0xff00;
        sq2WavelengthReset |= nextVal;
        break;
      case 7:
        sq2WavelengthReset &= 0x00ff;
        sq2WavelengthReset |= (uint16_t(nextVal & 0x7) << 8);
        sq2LenCnt = lengths[nextVal >> 3];
        break;
      // TRI
      case 8: // Not used in Zelda's music
        //triLenCnt = nextVal & 0x7f;
        break;
      case 10:
        triWavelengthReset &= 0xff00;
        triWavelengthReset |= nextVal;
        break;
      case 11:
        triWavelengthReset &= 0x00ff;
        triWavelengthReset |= (uint16_t(nextVal & 0x07) << 8);
        triLenCnt = lengths[nextVal >> 3];
        break;
      // NOISE
      case 12:
        noiseVol = nextVal & 0xf;
        break;
      case 14:
        noiseType = nextVal>>7;
        noiseWavelengthReset = noiseWavelengths[nextVal & 0xf];
        break;
      case 15:
        noiseLenCnt = lengths[nextVal >> 3];
        break;
    }
    if (!nextEvent()) {  // Hit the last event. Let's restart!
      eventIdx = 0;
      timeStamp = 0;
      nextEvent();
    }
  }

  // Generate the samples!
  nextSample = 0;

  // Generate Square Channel 1
  if (sq1LenCnt > 0 && sq1Vol > 0 && sq1WavelengthReset > 7) {
    if (sq1WavelengthCnt <= 0) {
      // Clock the SQ1's duty cycle
      sq1WavelengthCnt += sq1WavelengthReset;
      sq1DutyIdx++;
      if (sq1DutyIdx >= squareDuties) sq1DutyIdx = 0;
      sq1Sample = ((sq_duty[sq1DutyCycle][sq1DutyIdx] * sq1Vol))<<3;
    }
    nextSample += sq1Sample;
    sq1WavelengthCnt -= apuCyclesPerSample;
  }

  // Generate Square Channel 2
  if (sq2LenCnt > 0 && sq2Vol > 0 && sq2WavelengthReset > 7) {
    if (sq2WavelengthCnt <= 0) {
      // Clock the noise SQ2's duty cycle
      sq2WavelengthCnt += sq2WavelengthReset;
      sq2DutyIdx++;
      if (sq2DutyIdx >= squareDuties) sq2DutyIdx = 0;
      sq2Sample = ((sq_duty[sq2DutyCycle][sq2DutyIdx] *sq2Vol))<<3;
    }
    nextSample += sq2Sample;
    sq2WavelengthCnt -= apuCyclesPerSample;
  }

  // Generate Triangle
  if (triWavelengthReset > 0 && triLenCnt > 0) {
    if (triWavelengthCnt <= 0) {
      // Clock the triangle's duty cycle
      triWavelengthCnt += triWavelengthReset;
      triDutyIdx++;
      if (triDutyIdx >= triDuties) triDutyIdx = 0;
      triSample = tri_duty[triDutyIdx]>>1;
    }
    nextSample += triSample;
    triWavelengthCnt -= apuCyclesPerSample;
  }

  // Generate Noise
  if (noiseWavelengthReset > 0 && noiseLenCnt > 0 && noiseVol > 0) {
    if (noiseWavelengthCnt <= 0) {
        // Clock the noise generator's duty cycle
        if (noiseType) {
          noiseSample = bitRead(noiseLFSR, 8) ^ bitRead(noiseLFSR, 14);
        } else {
        noiseSample = bitRead(noiseLFSR, 13) ^ bitRead(noiseLFSR, 14);
        }
        noiseLFSR <<= 1;
        noiseLFSR |= noiseSample;
        noiseWavelengthCnt += noiseWavelengthReset;
        noiseSample *= (noiseVol<<3);
    }
    nextSample += noiseSample;
    noiseWavelengthCnt -= apuCyclesPerSample;
  }
}

// Switch pin off when intended PWM length is reached
ISR(TIMER3_OVF_vect) {
  TCCR3B = B00000000;  // Stop timer3
  *audioOut = setLow;
}

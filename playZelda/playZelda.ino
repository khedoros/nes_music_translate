#include "music.h"

const int AUDIOPIN = 7;
const int timeDivisions = 8000;  // timeslices per second in my arduino program
const uint16_t noiseWavelengths[] = {4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068};
const uint8_t lengths[32] = {10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14, 12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30};

const uint8_t sq_duty[4][16] = {{0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0},
                                {0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0},
                                {0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0},
                                {1,1,0,0,0,0,1,1,1,1,1,1,1,1,1,1}};

const uint8_t tri_duty[32] = {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

unsigned long nextTS;
uint8_t nextReg;
uint8_t nextVal;
unsigned int eventIdx = 0;
unsigned long timeStamp = 0;
uint8_t nextSample = 0;

// Remaining frame ticks to play each of the channels
uint8_t sq1LenCnt = 0;
uint8_t sq2LenCnt = 0;
uint8_t triLenCnt = 0;
uint8_t noiseLenCnt = 0;

// Remaining 8KHz timer ticks until we clock the duty cycles
uint8_t sq1WavelengthCnt = 0;
uint8_t sq2WavelengthCnt = 0;
uint8_t triWavelengthCnt = 0;
uint8_t noiseWavelengthCnt = 0;

// Wavelength counter reset values
uint8_t sq1WavelengthReset = 0;
uint8_t sq2WavelengthReset = 0;
uint8_t triWavelengthReset = 0;
uint8_t noiseWavelengthReset = 0;

// Index of current duty cycle
uint8_t sq1DutyIdx = 0;
uint8_t sq2DutyIdx = 0;
uint8_t triDutyIdx = 0;

const uint8_t squareDuties = 16;
const uint8_t triDuties = 32;

// Square channel duty cycle types
uint8_t sq1DutyCycle = 0;
uint8_t sq2DutyCycle = 0;

// Volumes
uint8_t sq1Vol = 0;
uint8_t sq2Vol = 0;
uint8_t noiseVol = 0;

// LFSR data for noise generation
uint16_t noiseLFSR = 0;
bool noiseType;
uint8_t noiseSample;

// length-clocking step
uint8_t step = 0;

void clockNoise() {
    if(noiseType) {
      noiseSample = bitRead(noiseLFSR, 8) ^ bitRead(noiseLFSR, 14);
    } else {
      noiseSample = bitRead(noiseLFSR, 13) ^ bitRead(noiseLFSR, 14);
    }
    noiseLFSR <<= 1;
    noiseLFSR |= noiseSample;
}

bool nextEvent() {
  unsigned long nextEvent = pgm_read_dword(&music[eventIdx]);
  nextTS = nextEvent & 0x000fffff;
  nextReg = nextEvent >> 28;
  nextVal = ((nextEvent >> 20) & 0xff);
  eventIdx++;
  return nextTS > 0;
}

void setup() {
  pinMode(AUDIOPIN, OUTPUT);
  pinMode(13, OUTPUT);
  nextEvent();
  cli();
  TCCR1A = 0; // Init Timer1 settings
  TCCR1B = B00000010; // Set timer1 prescaler to 8x
  TCNT1 = 65535 - 250; // Timer preload, where 250 0.5uS ticks will be 125uS
  TIMSK1 |= B00000001; // Enable timer1 overflow interrupt

  TCCR3A = 0; // Init Timer3 settings
  TCCR3B = B00000000; // Stop timer3
  TIMSK3 |= B00000001; // Enable timer3 overflow interrupt
  sei();
}

// Clock the note length counters at 96 and 192Hz
void loop() {
  int t = micros();
  if(step == 0 || step == 2) { // 96 Hz step
    if(sq1LenCnt) sq1LenCnt--;
    if(sq2LenCnt) sq2LenCnt--;
    if(noiseLenCnt) noiseLenCnt--;
  }
  if(step != 3) { // 192 Hz step
    if(triLenCnt) triLenCnt--;
  }
  step++;
  if(step == 5) step = 0;
  t = micros() - t;
  delayMicroseconds(5208 - t);
}

// Process register writes, generate audio
ISR(TIMER1_OVF_vect) {
  // Reset timer
  TCNT1 = 65535 - 250;
  timeStamp++;
  if(nextSample > 0) {
    digitalWrite(AUDIOPIN, HIGH); // Turn on pin, start timer to turn it off
    TCNT3 = 65535 - (31 * nextSample); // Leave the pin high for between 31 and 1984 cycles, depending on sample
    TCCR3B = B00000001; // Clock timer 3 at 1x of clock speed
  }

  if(timeStamp == nextTS) {
    // nextTS = nextEvent & 0x000fffff;
    // nextReg = nextEvent >> 28;
    // nextVal = ((nextEvent >> 20) & 0xff);
    switch(nextReg) {
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
        sq1WavelengthReset |= ((nextVal & 0x7)<<8);
        sq1LenCnt = lengths[nextVal>>3];
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
        sq2WavelengthReset |= ((nextVal & 0x7)<<8);
        sq2LenCnt = lengths[nextVal>>3];
        break;
      // TRI
      case 8: break;
      case 10: break;
      case 11: break;
      // NOISE
      case 12: break;
      case 14: break;
      case 15: break;
    }
    if(!nextEvent()) { // Hit the last event. Let's restart!
      eventIdx = 0;
      timeStamp = 0;
      nextEvent();
    }
  }

  // Generate the samples!
  nextSample = 0;
  if(sq1LenCnt > 0 && sq1Vol > 0 && sq1WavelengthReset > 7) {
    digitalWrite(13,HIGH);
    if(sq1WavelengthCnt == 0) {
      sq1WavelengthCnt = sq1WavelengthReset;
      sq1DutyIdx++;
      if(sq1DutyIdx > squareDuties) sq1DutyIdx = 0;
    }
    nextSample += sq_duty[sq1DutyCycle][sq1DutyIdx] * sq1Vol;
    sq1WavelengthCnt--;
  } else {
    digitalWrite(13,LOW);
  }
  if(sq2LenCnt > 0 && sq2Vol > 0 && sq2WavelengthReset > 7) {
      if(sq2WavelengthCnt == 0) {
        sq2WavelengthCnt = sq2WavelengthReset;
        sq2DutyIdx++;
      if(sq2DutyIdx > squareDuties) sq2DutyIdx = 0;
    }
    nextSample += sq_duty[sq2DutyCycle][sq2DutyIdx] * sq2Vol;
    sq2WavelengthCnt--;
  }
  if(triWavelengthReset > 7 && triLenCnt > 0) {
    if(triWavelengthCnt == 0) {
      triWavelengthCnt = triWavelengthReset;
      triDutyIdx++;
      if(triDutyIdx > triDuties) triDutyIdx = 0;
    }
    nextSample += tri_duty[triDutyIdx];
    triWavelengthCnt--;
  }
  if(noiseWavelengthReset > 7 && noiseLenCnt > 0 && noiseVol > 0) {
    if(noiseWavelengthCnt == 0) {
      clockNoise();
      noiseWavelengthCnt = noiseWavelengthReset;     
    }
    nextSample += noiseSample * noiseVol;
    noiseWavelengthCnt--;
  }
}

ISR(TIMER3_OVF_vect) {
  TCCR3B = B00000000; // Stop timer3
  digitalWrite(AUDIOPIN, LOW);
}
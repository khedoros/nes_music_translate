#include "music.h"

const int timeDivisions = 8000;  // timeslices per second in my arduino program
uint16_t noiseWavelengths[] = {4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068};

struct musicEvent {
    unsigned long timeStamp;
    uint8_t reg;
    uint8_t val;
};

unsigned long nextTS;
uint8_t nextReg;
uint8_t nextVal;
unsigned int eventIdx;

bool nextEvent() {
  unsigned long nextEvent = pgm_read_dword(&music[eventIdx]);
  nextTS = nextEvent & 0x000fffff;
  nextReg = nextEvent >> 28;
  nextVal = ((nextEvent >> 20) & 0xff);
  eventIdx++;
  return nextTS > 0;
}

void setup() {
  // put your setup code here, to run once:
  eventIdx = 0;
  nextEvent();
}

void loop() {
  // put your main code here, to run repeatedly:
}

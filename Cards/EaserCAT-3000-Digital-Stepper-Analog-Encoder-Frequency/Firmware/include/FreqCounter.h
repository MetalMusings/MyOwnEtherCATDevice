#ifndef FREQCOUNTER_H
#define FREQCOUNTER_H

#include <stdint.h>

// Frequency input on PA2 (THCAD connector J5 through the MAX3485 receiver).
//
// TIM5 runs free at the timer clock and captures the time of every rising edge
// on channel 3; DMA1 stream 0 copies each capture into a ring buffer. There is
// no interrupt per edge, so the step generator is not disturbed whatever the
// input frequency. read() takes the edges since the previous call and returns
// edges / time between the first and the last of them (reciprocal counting):
// the resolution is one timer clock (12 ns) over the cycle, at any frequency.
class FreqCounter {
 public:
  void begin();
  // Frequency in Hz. Call once per cycle. 0 when no edge for ZERO_TIMEOUT_US.
  float read(uint32_t nowMicros);

  static const uint32_t BUF_LEN = 1024;           // Edges kept; at 1 ms up to 1 MHz
  static const uint32_t MARGIN = 64;              // Entries the DMA may overwrite while reading
  static const uint32_t ZERO_TIMEOUT_US = 100000; // Below 10 Hz reads as 0

 private:
  volatile uint32_t buf[BUF_LEN];
  uint32_t lastPos = 0;
  uint32_t valid = 0;          // Captured edges in buf since the last restart
  uint32_t lastEdgeMicros = 0;
  float clkHz = 0;
  float freq = 0;
};

#endif

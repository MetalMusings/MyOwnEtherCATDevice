#ifndef EXTEND32TO64
#define EXTEND32TO54

#include <Arduino.h>

class extend32to64
{
public:
    int64_t previousTimeValue = 0;
    const uint64_t ONE_PERIOD = 4294967296;  // almost UINT32_MAX;
    const uint64_t HALF_PERIOD = 2147483648; // Half of that
    int64_t extendTime(uint32_t in);
};
#endif

#include "extend32to64.h"

// Extend from 32-bit to 64-bit precision
int64_t extend32to64::extendTime(uint32_t in)
{
    int64_t c64 = (int64_t)in - HALF_PERIOD; // remove half period to determine (+/-) sign of the wrap
    int64_t dif = (c64 - previousTimeValue); // core concept: prev + (current - prev) = current

    // wrap difference from -HALF_PERIOD to HALF_PERIOD. modulo prevents differences after the wrap from having an incorrect result
    int64_t mod_dif = ((dif + HALF_PERIOD) % ONE_PERIOD) - HALF_PERIOD;
    if (dif < int64_t(-HALF_PERIOD))
        mod_dif += ONE_PERIOD; // account for mod of negative number behavior in C

    int64_t unwrapped = previousTimeValue + mod_dif;
    previousTimeValue = unwrapped; // load previous value

    return unwrapped + HALF_PERIOD; // remove the shift we applied at the beginning, and return
}
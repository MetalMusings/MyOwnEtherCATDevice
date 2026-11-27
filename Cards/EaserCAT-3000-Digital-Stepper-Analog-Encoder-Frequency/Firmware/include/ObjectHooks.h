#ifndef OBJECTHOOKS_H
#define OBJECTHOOKS_H

#include <stddef.h>
#include <stdint.h>

// Step timing 0x2010..0x2013 (ns): length and space at least STEP_PULSE_MIN_NS, all at
// most STEP_TIMING_MAX_NS (the timer entries hold a pulse of up to 204 us)
#define STEP_PULSE_MIN_NS 1000
#define STEP_TIMING_MAX_NS 200000

// SOES pre_object_download_hook. Returns 0 or an SDO abort code.
uint32_t ObjectHooks_preDownload(uint16_t index, uint8_t subindex, void *data, size_t size,
                                 uint16_t flags);

#endif

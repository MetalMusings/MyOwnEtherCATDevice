#ifndef OBJECTHOOKS_H
#define OBJECTHOOKS_H

#include <stddef.h>
#include <stdint.h>

// Allowed range of the step generator base period, object 0x2000 (ns)
#define BASE_PERIOD_MIN_NS 5000
#define BASE_PERIOD_MAX_NS 200000

// SOES pre_object_download_hook. Returns 0 or an SDO abort code.
uint32_t ObjectHooks_preDownload(uint16_t index, uint8_t subindex, void *data, size_t size,
                                 uint16_t flags);

#endif

// SDO download validation (SOES pre_object_download_hook).
//
// - PDO mapping objects 0x16xx/0x1Axx and PDO assignment 0x1C12/0x1C13 are checked
//   according to ETG.1020 chapter 10 "Change of Mapping Objects".
// - CiA402 option codes and other parameters are range checked.

#include "ObjectHooks.h"

extern "C" {
#include "ecat_slv.h"
#include "esc_coe.h"
#include "utypes.h"
}

namespace {

uint32_t readValue(const void *data, size_t size) {
  const uint8_t *p = (const uint8_t *)data;
  uint32_t v = 0;
  for (size_t i = 0; i < size && i < 4; i++) v |= (uint32_t)p[i] << (8 * i);
  return v;
}

// Check one mapping entry (index << 16 | subindex << 8 | bitlength)
bool mappingEntryValid(uint32_t entry, bool rx) {
  uint16_t index = entry >> 16;
  uint8_t sub = (entry >> 8) & 0xFF;
  uint8_t bits = entry & 0xFF;
  if (index == 0 && sub == 0) return bits > 0;  // padding entry
  int32_t nidx = SDO_findobject(index);
  if (nidx < 0) return false;
  int16_t nsub = SDO_findsubindex(nidx, sub);
  if (nsub < 0) return false;
  const _objd *o = &SDOobjects[nidx].objdesc[nsub];
  if (o->bitlength != bits) return false;
  return (o->flags & (rx ? ATYPE_RXPDO : ATYPE_TXPDO)) != 0;
}

bool pdoIndexValid(uint16_t pdo, bool rx) {
  if (rx) return pdo >= 0x1600 && pdo < 0x1600 + N_RXPDO;
  return pdo >= 0x1A00 && pdo < 0x1A00 + N_TXPDO;
}

uint32_t checkMappingObject(PdoMap_t *map, bool rx, uint8_t sub, uint32_t value) {
  if (sub == 0) {
    if (value > MAX_PDO_ENTRIES) return ABORT_VALUE_EXCEEDED;
    for (uint32_t i = 0; i < value; i++)
      if (!mappingEntryValid(map->Entry[i], rx)) return ABORT_MAPPING_OBJECT_ERROR;
    return 0;
  }
  // ETG.1020: entries may only be changed while subindex 0 is 0
  if (map->Count != 0 && map->Entry[sub - 1] != value) return ABORT_SUBINDEX0_NOT_ZERO;
  if (!mappingEntryValid(value, rx)) return ABORT_MAPPING_OBJECT_ERROR;
  return 0;
}

uint32_t checkAssignObject(PdoAssign_t *assign, bool rx, uint8_t sub, uint32_t value) {
  uint32_t capacity = rx ? N_RXPDO : N_TXPDO;
  if (sub == 0) {
    if (value > capacity) return ABORT_VALUE_EXCEEDED;
    for (uint32_t i = 0; i < value; i++)
      if (!pdoIndexValid(assign->Pdo[i], rx)) return ABORT_VALUE_EXCEEDED;
    return 0;
  }
  if (assign->Count != 0 && assign->Pdo[sub - 1] != value) return ABORT_SUBINDEX0_NOT_ZERO;
  if (!pdoIndexValid((uint16_t)value, rx)) return ABORT_VALUE_EXCEEDED;
  return 0;
}

uint32_t checkAxisObject(uint16_t offset, uint32_t value) {
  int32_t sv16 = (int16_t)(value & 0xFFFF);
  int32_t sv8 = (int8_t)(value & 0xFF);
  switch (offset) {
    case 0x007:  // Abort connection option code
      return (sv16 >= 0 && sv16 <= 3) ? 0 : ABORT_VALUE_EXCEEDED;
    case 0x05A:  // Quick stop option code
      return (sv16 == 0 || sv16 == 1 || sv16 == 2 || sv16 == 5 || sv16 == 6)
                 ? 0
                 : ABORT_VALUE_EXCEEDED;
    case 0x05E:  // Fault reaction option code
      return (sv16 >= 0 && sv16 <= 2) ? 0 : ABORT_VALUE_EXCEEDED;
    case 0x060:  // Modes of operation
      return (sv8 == 0 || sv8 == 6 || sv8 == 8 || sv8 == 9) ? 0 : ABORT_VALUE_EXCEEDED;
    case 0x098:  // Homing method: home switch without index (19..22), current position (35, 37)
      return ((sv8 >= 19 && sv8 <= 22) || sv8 == 35 || sv8 == 37) ? 0 : ABORT_VALUE_EXCEEDED;
    case 0x07E:  // Polarity, only bit 6 (velocity) and 7 (position) are supported
      return (value & ~0xC0u & 0xFF) == 0 ? 0 : ABORT_VALUE_EXCEEDED;
    default:
      return 0;
  }
}

}  // namespace

uint32_t ObjectHooks_preDownload(uint16_t index, uint8_t subindex, void *data, size_t size,
                                 uint16_t flags) {
  (void)flags;
  uint32_t value = readValue(data, size);

  if (index >= 0x1600 && index < 0x1600 + N_RXPDO)
    return checkMappingObject(&Obj.RxPdoMap[index - 0x1600], true, subindex, value);
  if (index >= 0x1A00 && index < 0x1A00 + N_TXPDO)
    return checkMappingObject(&Obj.TxPdoMap[index - 0x1A00], false, subindex, value);
  if (index == 0x1C12) return checkAssignObject(&Obj.Sm2Assign, true, subindex, value);
  if (index == 0x1C13) return checkAssignObject(&Obj.Sm3Assign, false, subindex, value);

  if (index == 0x2000)  // Base period ns
    return (value >= BASE_PERIOD_MIN_NS && value <= BASE_PERIOD_MAX_NS) ? 0
                                                                        : ABORT_VALUE_EXCEEDED;

  if (index >= 0x2020 && index < 0x2020 + N_AXES && subindex >= 1 && subindex <= 3)
    return value <= 8 ? 0 : ABORT_VALUE_EXCEEDED;  // switch input: 0 = none, 1..8 = IN1..IN8

  if (index >= 0x6000 && index < 0x6000 + 0x800 * N_AXES)
    return checkAxisObject((index - 0x6000) % 0x800, value);

  return 0;
}

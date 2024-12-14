#include "esc_coe.h"
#include "utypes.h"
#include <stddef.h>


static const char acName1000[] = "Device Type";
static const char acName1008[] = "Device Name";
static const char acName1009[] = "Hardware Version";
static const char acName100A[] = "Software Version";
static const char acName1018[] = "Identity Object";
static const char acName1018_00[] = "Max SubIndex";
static const char acName1018_01[] = "Vendor ID";
static const char acName1018_02[] = "Product Code";
static const char acName1018_03[] = "Revision Number";
static const char acName1018_04[] = "Serial Number";
static const char acName1600[] = "Output";
static const char acName1600_00[] = "Max SubIndex";
static const char acName1600_01[] = "New array subitem";
static const char acName1600_02[] = "New array subitem";
static const char acName1600_03[] = "New array subitem";
static const char acName1600_04[] = "New array subitem";
static const char acName1A00[] = "Input";
static const char acName1A00_00[] = "Max SubIndex";
static const char acName1A00_01[] = "New array subitem";
static const char acName1A00_02[] = "New array subitem";
static const char acName1A00_03[] = "New array subitem";
static const char acName1A00_04[] = "New array subitem";
static const char acName1A00_05[] = "New array subitem";
static const char acName1A00_06[] = "New array subitem";
static const char acName1A00_07[] = "New array subitem";
static const char acName1A00_08[] = "New array subitem";
static const char acName1A00_09[] = "New array subitem";
static const char acName1A00_0a[] = "New array subitem";
static const char acName1A00_0b[] = "New array subitem";
static const char acName1A00_0c[] = "New array subitem";
static const char acName1A01[] = "Velocity";
static const char acName1A01_00[] = "Max SubIndex";
static const char acName1A01_01[] = "Velocity";
static const char acName1C00[] = "Sync Manager Communication Type";
static const char acName1C00_00[] = "Max SubIndex";
static const char acName1C00_01[] = "Communications Type SM0";
static const char acName1C00_02[] = "Communications Type SM1";
static const char acName1C00_03[] = "Communications Type SM2";
static const char acName1C00_04[] = "Communications Type SM3";
static const char acName1C12[] = "Sync Manager 2 PDO Assignment";
static const char acName1C12_00[] = "Max SubIndex";
static const char acName1C12_01[] = "PDO Mapping";
static const char acName1C13[] = "Sync Manager 3 PDO Assignment";
static const char acName1C13_00[] = "Max SubIndex";
static const char acName1C13_01[] = "PDO Mapping";
static const char acName1C13_02[] = "PDO Mapping";
static const char acName2000[] = "VelocityScale";
static const char acName6000[] = "Input";
static const char acName6000_00[] = "Max SubIndex";
static const char acName6000_01[] = "New array subitem";
static const char acName6000_02[] = "New array subitem";
static const char acName6000_03[] = "New array subitem";
static const char acName6000_04[] = "New array subitem";
static const char acName6000_05[] = "New array subitem";
static const char acName6000_06[] = "New array subitem";
static const char acName6000_07[] = "New array subitem";
static const char acName6000_08[] = "New array subitem";
static const char acName6000_09[] = "New array subitem";
static const char acName6000_0a[] = "New array subitem";
static const char acName6000_0b[] = "New array subitem";
static const char acName6000_0c[] = "New array subitem";
static const char acName6001[] = "Velocity";
static const char acName7000[] = "Output";
static const char acName7000_00[] = "Max SubIndex";
static const char acName7000_01[] = "New array subitem";
static const char acName7000_02[] = "New array subitem";
static const char acName7000_03[] = "New array subitem";
static const char acName7000_04[] = "New array subitem";

const _objd SDO1000[] =
{
  {0x0, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1000, 5001, NULL},
};
const _objd SDO1008[] =
{
  {0x0, DTYPE_VISIBLE_STRING, 208, ATYPE_RO, acName1008, 0, "MetalMusings EaserCAT 6000"},
};
const _objd SDO1009[] =
{
  {0x0, DTYPE_VISIBLE_STRING, 40, ATYPE_RO, acName1009, 0, "0.0.1"},
};
const _objd SDO100A[] =
{
  {0x0, DTYPE_VISIBLE_STRING, 40, ATYPE_RO, acName100A, 0, "0.0.1"},
};
const _objd SDO1018[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1018_00, 4, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1018_01, 2730, NULL},
  {0x02, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1018_02, 13294767, NULL},
  {0x03, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1018_03, 3, NULL},
  {0x04, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1018_04, 1, &Obj.serial},
};
const _objd SDO1600[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1600_00, 4, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1600_01, 0x70000101, NULL},
  {0x02, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1600_02, 0x70000201, NULL},
  {0x03, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1600_03, 0x70000301, NULL},
  {0x04, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1600_04, 0x70000401, NULL},
};
const _objd SDO1A00[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1A00_00, 12, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A00_01, 0x60000101, NULL},
  {0x02, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A00_02, 0x60000201, NULL},
  {0x03, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A00_03, 0x60000301, NULL},
  {0x04, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A00_04, 0x60000401, NULL},
  {0x05, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A00_05, 0x60000501, NULL},
  {0x06, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A00_06, 0x60000601, NULL},
  {0x07, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A00_07, 0x60000701, NULL},
  {0x08, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A00_08, 0x60000801, NULL},
  {0x09, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A00_09, 0x60000901, NULL},
  {0x0a, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A00_0a, 0x60000a01, NULL},
  {0x0b, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A00_0b, 0x60000b01, NULL},
  {0x0c, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A00_0c, 0x60000c01, NULL},
};
const _objd SDO1A01[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1A01_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A01_01, 0x60010020, NULL},
};
const _objd SDO1C00[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1C00_00, 4, NULL},
  {0x01, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1C00_01, 1, NULL},
  {0x02, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1C00_02, 2, NULL},
  {0x03, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1C00_03, 3, NULL},
  {0x04, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1C00_04, 4, NULL},
};
const _objd SDO1C12[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1C12_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C12_01, 0x1600, NULL},
};
const _objd SDO1C13[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1C13_00, 2, NULL},
  {0x01, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C13_01, 0x1A00, NULL},
  {0x02, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C13_02, 0x1A01, NULL},
};
const _objd SDO2000[] =
{
  {0x0, DTYPE_REAL32, 32, ATYPE_RO, acName2000, 0x00000000, &Obj.VelocityScale},
};
const _objd SDO6000[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName6000_00, 12, NULL},
  {0x01, DTYPE_BOOLEAN, 1, ATYPE_RO | ATYPE_TXPDO, acName6000_01, 0, &Obj.Input[0]},
  {0x02, DTYPE_BOOLEAN, 1, ATYPE_RO | ATYPE_TXPDO, acName6000_02, 0, &Obj.Input[1]},
  {0x03, DTYPE_BOOLEAN, 1, ATYPE_RO | ATYPE_TXPDO, acName6000_03, 0, &Obj.Input[2]},
  {0x04, DTYPE_BOOLEAN, 1, ATYPE_RO | ATYPE_TXPDO, acName6000_04, 0, &Obj.Input[3]},
  {0x05, DTYPE_BOOLEAN, 1, ATYPE_RO | ATYPE_TXPDO, acName6000_05, 0, &Obj.Input[4]},
  {0x06, DTYPE_BOOLEAN, 1, ATYPE_RO | ATYPE_TXPDO, acName6000_06, 0, &Obj.Input[5]},
  {0x07, DTYPE_BOOLEAN, 1, ATYPE_RO | ATYPE_TXPDO, acName6000_07, 0, &Obj.Input[6]},
  {0x08, DTYPE_BOOLEAN, 1, ATYPE_RO | ATYPE_TXPDO, acName6000_08, 0, &Obj.Input[7]},
  {0x09, DTYPE_BOOLEAN, 1, ATYPE_RO | ATYPE_TXPDO, acName6000_09, 0, &Obj.Input[8]},
  {0x0a, DTYPE_BOOLEAN, 1, ATYPE_RO | ATYPE_TXPDO, acName6000_0a, 0, &Obj.Input[9]},
  {0x0b, DTYPE_BOOLEAN, 1, ATYPE_RO | ATYPE_TXPDO, acName6000_0b, 0, &Obj.Input[10]},
  {0x0c, DTYPE_BOOLEAN, 1, ATYPE_RO | ATYPE_TXPDO, acName6000_0c, 0, &Obj.Input[11]},
};
const _objd SDO6001[] =
{
  {0x0, DTYPE_REAL32, 32, ATYPE_RO | ATYPE_TXPDO, acName6001, 0x00000000, &Obj.Velocity},
};
const _objd SDO7000[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName7000_00, 4, NULL},
  {0x01, DTYPE_BOOLEAN, 1, ATYPE_RO | ATYPE_RXPDO, acName7000_01, 0, &Obj.Output[0]},
  {0x02, DTYPE_BOOLEAN, 1, ATYPE_RO | ATYPE_RXPDO, acName7000_02, 0, &Obj.Output[1]},
  {0x03, DTYPE_BOOLEAN, 1, ATYPE_RO | ATYPE_RXPDO, acName7000_03, 0, &Obj.Output[2]},
  {0x04, DTYPE_BOOLEAN, 1, ATYPE_RO | ATYPE_RXPDO, acName7000_04, 0, &Obj.Output[3]},
};

const _objectlist SDOobjects[] =
{
  {0x1000, OTYPE_VAR, 0, 0, acName1000, SDO1000},
  {0x1008, OTYPE_VAR, 0, 0, acName1008, SDO1008},
  {0x1009, OTYPE_VAR, 0, 0, acName1009, SDO1009},
  {0x100A, OTYPE_VAR, 0, 0, acName100A, SDO100A},
  {0x1018, OTYPE_RECORD, 4, 0, acName1018, SDO1018},
  {0x1600, OTYPE_RECORD, 4, 0, acName1600, SDO1600},
  {0x1A00, OTYPE_RECORD, 12, 0, acName1A00, SDO1A00},
  {0x1A01, OTYPE_RECORD, 1, 0, acName1A01, SDO1A01},
  {0x1C00, OTYPE_ARRAY, 4, 0, acName1C00, SDO1C00},
  {0x1C12, OTYPE_ARRAY, 1, 0, acName1C12, SDO1C12},
  {0x1C13, OTYPE_ARRAY, 2, 0, acName1C13, SDO1C13},
  {0x2000, OTYPE_VAR, 0, 0, acName2000, SDO2000},
  {0x6000, OTYPE_ARRAY, 12, 0, acName6000, SDO6000},
  {0x6001, OTYPE_VAR, 0, 0, acName6001, SDO6001},
  {0x7000, OTYPE_ARRAY, 4, 0, acName7000, SDO7000},
  {0xffff, 0xff, 0xff, 0xff, NULL, NULL}
};

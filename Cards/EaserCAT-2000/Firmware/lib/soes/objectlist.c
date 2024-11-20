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
static const char acName1600[] = "IndexLatchEnable";
static const char acName1600_00[] = "Max SubIndex";
static const char acName1600_01[] = "IndexLatchEnable";
static const char acName1601[] = "CommandedPosition1";
static const char acName1601_00[] = "Max SubIndex";
static const char acName1601_01[] = "CommandedPosition1";
static const char acName1602[] = "CommandedPosition2";
static const char acName1602_00[] = "Max SubIndex";
static const char acName1602_01[] = "CommandedPosition2";
static const char acName1603[] = "StepsPerMM1";
static const char acName1603_00[] = "Max SubIndex";
static const char acName1603_01[] = "StepsPerMM1";
static const char acName1604[] = "StepsPerMM2";
static const char acName1604_00[] = "Max SubIndex";
static const char acName1604_01[] = "StepsPerMM2";
static const char acName1A00[] = "EncPos";
static const char acName1A00_00[] = "Max SubIndex";
static const char acName1A00_01[] = "EncPos";
static const char acName1A01[] = "EncFrequency";
static const char acName1A01_00[] = "Max SubIndex";
static const char acName1A01_01[] = "EncFrequency";
static const char acName1A02[] = "DiffT";
static const char acName1A02_00[] = "Max SubIndex";
static const char acName1A02_01[] = "DiffT";
static const char acName1A03[] = "IndexByte";
static const char acName1A03_00[] = "Max SubIndex";
static const char acName1A03_01[] = "IndexByte";
static const char acName1A04[] = "IndexStatus";
static const char acName1A04_00[] = "Max SubIndex";
static const char acName1A04_01[] = "IndexStatus";
static const char acName1A05[] = "ActualPosition1";
static const char acName1A05_00[] = "Max SubIndex";
static const char acName1A05_01[] = "ActualPosition1";
static const char acName1A06[] = "ActualPosition2";
static const char acName1A06_00[] = "Max SubIndex";
static const char acName1A06_01[] = "ActualPosition2";
static const char acName1A07[] = "D1";
static const char acName1A07_00[] = "Max SubIndex";
static const char acName1A07_01[] = "D1";
static const char acName1A08[] = "D2";
static const char acName1A08_00[] = "Max SubIndex";
static const char acName1A08_01[] = "D2";
static const char acName1A09[] = "D3";
static const char acName1A09_00[] = "Max SubIndex";
static const char acName1A09_01[] = "D3";
static const char acName1A0A[] = "D4";
static const char acName1A0A_00[] = "Max SubIndex";
static const char acName1A0A_01[] = "D4";
static const char acName1C00[] = "Sync Manager Communication Type";
static const char acName1C00_00[] = "Max SubIndex";
static const char acName1C00_01[] = "Communications Type SM0";
static const char acName1C00_02[] = "Communications Type SM1";
static const char acName1C00_03[] = "Communications Type SM2";
static const char acName1C00_04[] = "Communications Type SM3";
static const char acName1C12[] = "Sync Manager 2 PDO Assignment";
static const char acName1C12_00[] = "Max SubIndex";
static const char acName1C12_01[] = "PDO Mapping";
static const char acName1C12_02[] = "PDO Mapping";
static const char acName1C12_03[] = "PDO Mapping";
static const char acName1C12_04[] = "PDO Mapping";
static const char acName1C12_05[] = "PDO Mapping";
static const char acName1C13[] = "Sync Manager 3 PDO Assignment";
static const char acName1C13_00[] = "Max SubIndex";
static const char acName1C13_01[] = "PDO Mapping";
static const char acName1C13_02[] = "PDO Mapping";
static const char acName1C13_03[] = "PDO Mapping";
static const char acName1C13_04[] = "PDO Mapping";
static const char acName1C13_05[] = "PDO Mapping";
static const char acName1C13_06[] = "PDO Mapping";
static const char acName1C13_07[] = "PDO Mapping";
static const char acName1C13_08[] = "PDO Mapping";
static const char acName1C13_09[] = "PDO Mapping";
static const char acName1C13_10[] = "PDO Mapping";
static const char acName1C13_11[] = "PDO Mapping";
static const char acName6000[] = "EncPos";
static const char acName6001[] = "EncFrequency";
static const char acName6002[] = "DiffT";
static const char acName6003[] = "IndexByte";
static const char acName6004[] = "IndexStatus";
static const char acName6005[] = "ActualPosition1";
static const char acName6006[] = "ActualPosition2";
static const char acName6007[] = "D1";
static const char acName6008[] = "D2";
static const char acName6009[] = "D3";
static const char acName600A[] = "D4";
static const char acName7000[] = "IndexLatchEnable";
static const char acName7001[] = "CommandedPosition1";
static const char acName7002[] = "CommandedPosition2";
static const char acName7003[] = "StepsPerMM1";
static const char acName7004[] = "StepsPerMM2";

const _objd SDO1000[] =
{
  {0x0, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1000, 5001, NULL},
};
const _objd SDO1008[] =
{
  {0x0, DTYPE_VISIBLE_STRING, 208, ATYPE_RO, acName1008, 0, "MetalMusings EaserCAT 2000"},
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
  {0x02, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1018_02, 12303564, NULL},
  {0x03, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1018_03, 2, NULL},
  {0x04, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1018_04, 1, &Obj.serial},
};
const _objd SDO1600[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1600_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1600_01, 0x70000020, NULL},
};
const _objd SDO1601[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1601_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1601_01, 0x70010020, NULL},
};
const _objd SDO1602[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1602_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1602_01, 0x70020020, NULL},
};
const _objd SDO1603[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1603_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1603_01, 0x70030010, NULL},
};
const _objd SDO1604[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1604_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1604_01, 0x70040010, NULL},
};
const _objd SDO1A00[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1A00_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A00_01, 0x60000020, NULL},
};
const _objd SDO1A01[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1A01_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A01_01, 0x60010020, NULL},
};
const _objd SDO1A02[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1A02_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A02_01, 0x60020010, NULL},
};
const _objd SDO1A03[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1A03_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A03_01, 0x60030020, NULL},
};
const _objd SDO1A04[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1A04_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A04_01, 0x60040020, NULL},
};
const _objd SDO1A05[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1A05_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A05_01, 0x60050020, NULL},
};
const _objd SDO1A06[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1A06_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A06_01, 0x60060020, NULL},
};
const _objd SDO1A07[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1A07_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A07_01, 0x60070010, NULL},
};
const _objd SDO1A08[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1A08_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A08_01, 0x60080010, NULL},
};
const _objd SDO1A09[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1A09_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A09_01, 0x60090010, NULL},
};
const _objd SDO1A0A[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1A0A_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A0A_01, 0x600A0010, NULL},
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
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1C12_00, 5, NULL},
  {0x01, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C12_01, 0x1600, NULL},
  {0x02, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C12_02, 0x1601, NULL},
  {0x03, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C12_03, 0x1602, NULL},
  {0x04, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C12_04, 0x1603, NULL},
  {0x05, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C12_05, 0x1604, NULL},
};
const _objd SDO1C13[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1C13_00, 11, NULL},
  {0x01, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C13_01, 0x1A00, NULL},
  {0x02, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C13_02, 0x1A01, NULL},
  {0x03, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C13_03, 0x1A02, NULL},
  {0x04, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C13_04, 0x1A03, NULL},
  {0x05, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C13_05, 0x1A04, NULL},
  {0x06, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C13_06, 0x1A05, NULL},
  {0x07, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C13_07, 0x1A06, NULL},
  {0x08, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C13_08, 0x1A07, NULL},
  {0x09, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C13_09, 0x1A08, NULL},
  {0x0a, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C13_10, 0x1A09, NULL},
  {0x0b, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C13_11, 0x1A0A, NULL},
};
const _objd SDO6000[] =
{
  {0x0, DTYPE_REAL32, 32, ATYPE_RO | ATYPE_TXPDO, acName6000, 0x00000000, &Obj.EncPos},
};
const _objd SDO6001[] =
{
  {0x0, DTYPE_REAL32, 32, ATYPE_RO | ATYPE_TXPDO, acName6001, 0x00000000, &Obj.EncFrequency},
};
const _objd SDO6002[] =
{
  {0x0, DTYPE_UNSIGNED16, 16, ATYPE_RO | ATYPE_TXPDO, acName6002, 0, &Obj.DiffT},
};
const _objd SDO6003[] =
{
  {0x0, DTYPE_UNSIGNED32, 32, ATYPE_RO | ATYPE_TXPDO, acName6003, 0, &Obj.IndexByte},
};
const _objd SDO6004[] =
{
  {0x0, DTYPE_UNSIGNED32, 32, ATYPE_RO | ATYPE_TXPDO, acName6004, 0, &Obj.IndexStatus},
};
const _objd SDO6005[] =
{
  {0x0, DTYPE_REAL32, 32, ATYPE_RO | ATYPE_TXPDO, acName6005, 0x00000000, &Obj.ActualPosition1},
};
const _objd SDO6006[] =
{
  {0x0, DTYPE_REAL32, 32, ATYPE_RO | ATYPE_TXPDO, acName6006, 0x00000000, &Obj.ActualPosition2},
};
const _objd SDO6007[] =
{
  {0x0, DTYPE_INTEGER16, 16, ATYPE_RO | ATYPE_TXPDO, acName6007, 0, &Obj.D1},
};
const _objd SDO6008[] =
{
  {0x0, DTYPE_INTEGER16, 16, ATYPE_RO | ATYPE_TXPDO, acName6008, 0, &Obj.D2},
};
const _objd SDO6009[] =
{
  {0x0, DTYPE_INTEGER16, 16, ATYPE_RO | ATYPE_TXPDO, acName6009, 0, &Obj.D3},
};
const _objd SDO600A[] =
{
  {0x0, DTYPE_INTEGER16, 16, ATYPE_RO | ATYPE_TXPDO, acName600A, 0, &Obj.D4},
};
const _objd SDO7000[] =
{
  {0x0, DTYPE_UNSIGNED32, 32, ATYPE_RO | ATYPE_RXPDO, acName7000, 0, &Obj.IndexLatchEnable},
};
const _objd SDO7001[] =
{
  {0x0, DTYPE_REAL32, 32, ATYPE_RO | ATYPE_RXPDO, acName7001, 0x00000000, &Obj.CommandedPosition1},
};
const _objd SDO7002[] =
{
  {0x0, DTYPE_REAL32, 32, ATYPE_RO | ATYPE_RXPDO, acName7002, 0x00000000, &Obj.CommandedPosition2},
};
const _objd SDO7003[] =
{
  {0x0, DTYPE_INTEGER16, 16, ATYPE_RO | ATYPE_RXPDO, acName7003, 0, &Obj.StepsPerMM1},
};
const _objd SDO7004[] =
{
  {0x0, DTYPE_INTEGER16, 16, ATYPE_RO | ATYPE_RXPDO, acName7004, 0, &Obj.StepsPerMM2},
};

const _objectlist SDOobjects[] =
{
  {0x1000, OTYPE_VAR, 0, 0, acName1000, SDO1000},
  {0x1008, OTYPE_VAR, 0, 0, acName1008, SDO1008},
  {0x1009, OTYPE_VAR, 0, 0, acName1009, SDO1009},
  {0x100A, OTYPE_VAR, 0, 0, acName100A, SDO100A},
  {0x1018, OTYPE_RECORD, 4, 0, acName1018, SDO1018},
  {0x1600, OTYPE_RECORD, 1, 0, acName1600, SDO1600},
  {0x1601, OTYPE_RECORD, 1, 0, acName1601, SDO1601},
  {0x1602, OTYPE_RECORD, 1, 0, acName1602, SDO1602},
  {0x1603, OTYPE_RECORD, 1, 0, acName1603, SDO1603},
  {0x1604, OTYPE_RECORD, 1, 0, acName1604, SDO1604},
  {0x1A00, OTYPE_RECORD, 1, 0, acName1A00, SDO1A00},
  {0x1A01, OTYPE_RECORD, 1, 0, acName1A01, SDO1A01},
  {0x1A02, OTYPE_RECORD, 1, 0, acName1A02, SDO1A02},
  {0x1A03, OTYPE_RECORD, 1, 0, acName1A03, SDO1A03},
  {0x1A04, OTYPE_RECORD, 1, 0, acName1A04, SDO1A04},
  {0x1A05, OTYPE_RECORD, 1, 0, acName1A05, SDO1A05},
  {0x1A06, OTYPE_RECORD, 1, 0, acName1A06, SDO1A06},
  {0x1A07, OTYPE_RECORD, 1, 0, acName1A07, SDO1A07},
  {0x1A08, OTYPE_RECORD, 1, 0, acName1A08, SDO1A08},
  {0x1A09, OTYPE_RECORD, 1, 0, acName1A09, SDO1A09},
  {0x1A0A, OTYPE_RECORD, 1, 0, acName1A0A, SDO1A0A},
  {0x1C00, OTYPE_ARRAY, 4, 0, acName1C00, SDO1C00},
  {0x1C12, OTYPE_ARRAY, 5, 0, acName1C12, SDO1C12},
  {0x1C13, OTYPE_ARRAY, 11, 0, acName1C13, SDO1C13},
  {0x6000, OTYPE_VAR, 0, 0, acName6000, SDO6000},
  {0x6001, OTYPE_VAR, 0, 0, acName6001, SDO6001},
  {0x6002, OTYPE_VAR, 0, 0, acName6002, SDO6002},
  {0x6003, OTYPE_VAR, 0, 0, acName6003, SDO6003},
  {0x6004, OTYPE_VAR, 0, 0, acName6004, SDO6004},
  {0x6005, OTYPE_VAR, 0, 0, acName6005, SDO6005},
  {0x6006, OTYPE_VAR, 0, 0, acName6006, SDO6006},
  {0x6007, OTYPE_VAR, 0, 0, acName6007, SDO6007},
  {0x6008, OTYPE_VAR, 0, 0, acName6008, SDO6008},
  {0x6009, OTYPE_VAR, 0, 0, acName6009, SDO6009},
  {0x600A, OTYPE_VAR, 0, 0, acName600A, SDO600A},
  {0x7000, OTYPE_VAR, 0, 0, acName7000, SDO7000},
  {0x7001, OTYPE_VAR, 0, 0, acName7001, SDO7001},
  {0x7002, OTYPE_VAR, 0, 0, acName7002, SDO7002},
  {0x7003, OTYPE_VAR, 0, 0, acName7003, SDO7003},
  {0x7004, OTYPE_VAR, 0, 0, acName7004, SDO7004},
  {0xffff, 0xff, 0xff, 0xff, NULL, NULL}
};

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
static const char acName1600[] = "Scale";
static const char acName1600_00[] = "Max SubIndex";
static const char acName1600_01[] = "Scale";
static const char acName1601[] = "Outputs";
static const char acName1601_00[] = "Max SubIndex";
static const char acName1601_01[] = "OUT1";
static const char acName1601_02[] = "OUT2";
static const char acName1601_03[] = "OUT3";
static const char acName1601_04[] = "OUT4";
static const char acName1602[] = "CommandedPosition1";
static const char acName1602_00[] = "Max SubIndex";
static const char acName1602_01[] = "CommandedPosition1";
static const char acName1603[] = "CommandedPosition2";
static const char acName1603_00[] = "Max SubIndex";
static const char acName1603_01[] = "CommandedPosition2";
static const char acName1604[] = "CommandedPosition3";
static const char acName1604_00[] = "Max SubIndex";
static const char acName1604_01[] = "CommandedPosition3";
static const char acName1605[] = "CommandedPosition4";
static const char acName1605_00[] = "Max SubIndex";
static const char acName1605_01[] = "CommandedPosition4";
static const char acName1606[] = "StepsPerMM1";
static const char acName1606_00[] = "Max SubIndex";
static const char acName1606_01[] = "StepsPerMM1";
static const char acName1607[] = "StepsPerMM2";
static const char acName1607_00[] = "Max SubIndex";
static const char acName1607_01[] = "StepsPerMM2";
static const char acName1608[] = "StepsPerMM3";
static const char acName1608_00[] = "Max SubIndex";
static const char acName1608_01[] = "StepsPerMM3";
static const char acName1609[] = "StepsPerMM4";
static const char acName1609_00[] = "Max SubIndex";
static const char acName1609_01[] = "StepsPerMM4";
static const char acName1A00[] = "Velocity";
static const char acName1A00_00[] = "Max SubIndex";
static const char acName1A00_01[] = "Velocity";
static const char acName1A01[] = "Inputs";
static const char acName1A01_00[] = "Max SubIndex";
static const char acName1A01_01[] = "IN1";
static const char acName1A01_02[] = "IN2";
static const char acName1A01_03[] = "IN3";
static const char acName1A01_04[] = "IN4";
static const char acName1A01_05[] = "IN5";
static const char acName1A01_06[] = "IN6";
static const char acName1A01_07[] = "IN7";
static const char acName1A01_08[] = "IN8";
static const char acName1A02[] = "Frequency";
static const char acName1A02_00[] = "Max SubIndex";
static const char acName1A02_01[] = "Frequency";
static const char acName1A03[] = "ActualPosition1";
static const char acName1A03_00[] = "Max SubIndex";
static const char acName1A03_01[] = "ActualPosition1";
static const char acName1A04[] = "ActualPosition2";
static const char acName1A04_00[] = "Max SubIndex";
static const char acName1A04_01[] = "ActualPosition2";
static const char acName1A05[] = "ActualPosition3";
static const char acName1A05_00[] = "Max SubIndex";
static const char acName1A05_01[] = "ActualPosition3";
static const char acName1A06[] = "ActualPosition4";
static const char acName1A06_00[] = "Max SubIndex";
static const char acName1A06_01[] = "ActualPosition4";
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
static const char acName1C12_06[] = "PDO Mapping";
static const char acName1C12_07[] = "PDO Mapping";
static const char acName1C12_08[] = "PDO Mapping";
static const char acName1C12_09[] = "PDO Mapping";
static const char acName1C12_0a[] = "PDO Mapping";
static const char acName1C13[] = "Sync Manager 3 PDO Assignment";
static const char acName1C13_00[] = "Max SubIndex";
static const char acName1C13_01[] = "PDO Mapping";
static const char acName1C13_02[] = "PDO Mapping";
static const char acName1C13_03[] = "PDO Mapping";
static const char acName1C13_04[] = "PDO Mapping";
static const char acName1C13_05[] = "PDO Mapping";
static const char acName1C13_06[] = "PDO Mapping";
static const char acName1C13_07[] = "PDO Mapping";
static const char acName6000[] = "Velocity";
static const char acName6001[] = "Inputs";
static const char acName6001_00[] = "Max SubIndex";
static const char acName6001_01[] = "IN1";
static const char acName6001_02[] = "IN2";
static const char acName6001_03[] = "IN3";
static const char acName6001_04[] = "IN4";
static const char acName6001_05[] = "IN5";
static const char acName6001_06[] = "IN6";
static const char acName6001_07[] = "IN7";
static const char acName6001_08[] = "IN8";
static const char acName6002[] = "Frequency";
static const char acName6003[] = "ActualPosition1";
static const char acName6004[] = "ActualPosition2";
static const char acName6005[] = "ActualPosition3";
static const char acName6006[] = "ActualPosition4";
static const char acName7000[] = "Scale";
static const char acName7001[] = "Outputs";
static const char acName7001_00[] = "Max SubIndex";
static const char acName7001_01[] = "OUT1";
static const char acName7001_02[] = "OUT2";
static const char acName7001_03[] = "OUT3";
static const char acName7001_04[] = "OUT4";
static const char acName7002[] = "CommandedPosition1";
static const char acName7003[] = "CommandedPosition2";
static const char acName7004[] = "CommandedPosition3";
static const char acName7005[] = "CommandedPosition4";
static const char acName7006[] = "StepsPerMM1";
static const char acName7007[] = "StepsPerMM2";
static const char acName7008[] = "StepsPerMM3";
static const char acName7009[] = "StepsPerMM4";

const _objd SDO1000[] =
{
  {0x0, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1000, 5001, NULL},
};
const _objd SDO1008[] =
{
  {0x0, DTYPE_VISIBLE_STRING, 408, ATYPE_RO, acName1008, 0, "MetalMusings EaserCAT 3000 test plasma torch reader"},
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
  {0x02, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1018_02, 13294766, NULL},
  {0x03, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1018_03, 1, NULL},
  {0x04, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1018_04, 1, &Obj.serial},
};
const _objd SDO1600[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1600_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1600_01, 0x70000020, NULL},
};
const _objd SDO1601[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1601_00, 4, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1601_01, 0x70010108, NULL},
  {0x02, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1601_02, 0x70010208, NULL},
  {0x03, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1601_03, 0x70010308, NULL},
  {0x04, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1601_04, 0x70010408, NULL},
};
const _objd SDO1602[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1602_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1602_01, 0x70020020, NULL},
};
const _objd SDO1603[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1603_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1603_01, 0x70030020, NULL},
};
const _objd SDO1604[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1604_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1604_01, 0x70040020, NULL},
};
const _objd SDO1605[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1605_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1605_01, 0x70050020, NULL},
};
const _objd SDO1606[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1606_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1606_01, 0x70060020, NULL},
};
const _objd SDO1607[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1607_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1607_01, 0x70070020, NULL},
};
const _objd SDO1608[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1608_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1608_01, 0x70080020, NULL},
};
const _objd SDO1609[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1609_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1609_01, 0x70090020, NULL},
};
const _objd SDO1A00[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1A00_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A00_01, 0x60000020, NULL},
};
const _objd SDO1A01[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1A01_00, 8, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A01_01, 0x60010108, NULL},
  {0x02, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A01_02, 0x60010208, NULL},
  {0x03, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A01_03, 0x60010308, NULL},
  {0x04, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A01_04, 0x60010408, NULL},
  {0x05, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A01_05, 0x60010508, NULL},
  {0x06, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A01_06, 0x60010608, NULL},
  {0x07, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A01_07, 0x60010708, NULL},
  {0x08, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A01_08, 0x60010808, NULL},
};
const _objd SDO1A02[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1A02_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A02_01, 0x60020020, NULL},
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
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1C12_00, 10, NULL},
  {0x01, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C12_01, 0x1600, NULL},
  {0x02, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C12_02, 0x1601, NULL},
  {0x03, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C12_03, 0x1602, NULL},
  {0x04, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C12_04, 0x1603, NULL},
  {0x05, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C12_05, 0x1604, NULL},
  {0x06, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C12_06, 0x1605, NULL},
  {0x07, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C12_07, 0x1606, NULL},
  {0x08, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C12_08, 0x1607, NULL},
  {0x09, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C12_09, 0x1608, NULL},
  {0x0a, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C12_0a, 0x1609, NULL},
};
const _objd SDO1C13[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1C13_00, 7, NULL},
  {0x01, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C13_01, 0x1A00, NULL},
  {0x02, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C13_02, 0x1A01, NULL},
  {0x03, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C13_03, 0x1A02, NULL},
  {0x04, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C13_04, 0x1A03, NULL},
  {0x05, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C13_05, 0x1A04, NULL},
  {0x06, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C13_06, 0x1A05, NULL},
  {0x07, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C13_07, 0x1A06, NULL},
};
const _objd SDO6000[] =
{
  {0x0, DTYPE_REAL32, 32, ATYPE_RO | ATYPE_TXPDO, acName6000, 0x00000000, &Obj.Velocity},
};
const _objd SDO6001[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName6001_00, 8, NULL},
  {0x01, DTYPE_UNSIGNED8, 8, ATYPE_RO | ATYPE_TXPDO, acName6001_01, 0, &Obj.Inputs[0]},
  {0x02, DTYPE_UNSIGNED8, 8, ATYPE_RO | ATYPE_TXPDO, acName6001_02, 0, &Obj.Inputs[1]},
  {0x03, DTYPE_UNSIGNED8, 8, ATYPE_RO | ATYPE_TXPDO, acName6001_03, 0, &Obj.Inputs[2]},
  {0x04, DTYPE_UNSIGNED8, 8, ATYPE_RO | ATYPE_TXPDO, acName6001_04, 0, &Obj.Inputs[3]},
  {0x05, DTYPE_UNSIGNED8, 8, ATYPE_RO | ATYPE_TXPDO, acName6001_05, 0, &Obj.Inputs[4]},
  {0x06, DTYPE_UNSIGNED8, 8, ATYPE_RO | ATYPE_TXPDO, acName6001_06, 0, &Obj.Inputs[5]},
  {0x07, DTYPE_UNSIGNED8, 8, ATYPE_RO | ATYPE_TXPDO, acName6001_07, 0, &Obj.Inputs[6]},
  {0x08, DTYPE_UNSIGNED8, 8, ATYPE_RO | ATYPE_TXPDO, acName6001_08, 0, &Obj.Inputs[7]},
};
const _objd SDO6002[] =
{
  {0x0, DTYPE_REAL32, 32, ATYPE_RO | ATYPE_TXPDO, acName6002, 0x00000000, &Obj.Frequency},
};
const _objd SDO6003[] =
{
  {0x0, DTYPE_REAL32, 32, ATYPE_RO | ATYPE_TXPDO, acName6003, 0x00000000, &Obj.ActualPosition1},
};
const _objd SDO6004[] =
{
  {0x0, DTYPE_REAL32, 32, ATYPE_RO | ATYPE_TXPDO, acName6004, 0x00000000, &Obj.ActualPosition2},
};
const _objd SDO6005[] =
{
  {0x0, DTYPE_REAL32, 32, ATYPE_RO | ATYPE_TXPDO, acName6005, 0x00000000, &Obj.ActualPosition3},
};
const _objd SDO6006[] =
{
  {0x0, DTYPE_REAL32, 32, ATYPE_RO | ATYPE_TXPDO, acName6006, 0x00000000, &Obj.ActualPosition4},
};
const _objd SDO7000[] =
{
  {0x0, DTYPE_REAL32, 32, ATYPE_RO | ATYPE_RXPDO, acName7000, 0x00000000, &Obj.Scale},
};
const _objd SDO7001[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName7001_00, 4, NULL},
  {0x01, DTYPE_UNSIGNED8, 8, ATYPE_RO | ATYPE_RXPDO, acName7001_01, 0, &Obj.Outputs[0]},
  {0x02, DTYPE_UNSIGNED8, 8, ATYPE_RO | ATYPE_RXPDO, acName7001_02, 0, &Obj.Outputs[1]},
  {0x03, DTYPE_UNSIGNED8, 8, ATYPE_RO | ATYPE_RXPDO, acName7001_03, 0, &Obj.Outputs[2]},
  {0x04, DTYPE_UNSIGNED8, 8, ATYPE_RO | ATYPE_RXPDO, acName7001_04, 0, &Obj.Outputs[3]},
};
const _objd SDO7002[] =
{
  {0x0, DTYPE_REAL32, 32, ATYPE_RO | ATYPE_RXPDO, acName7002, 0x00000000, &Obj.CommandedPosition1},
};
const _objd SDO7003[] =
{
  {0x0, DTYPE_REAL32, 32, ATYPE_RO | ATYPE_RXPDO, acName7003, 0x00000000, &Obj.CommandedPosition2},
};
const _objd SDO7004[] =
{
  {0x0, DTYPE_REAL32, 32, ATYPE_RO | ATYPE_RXPDO, acName7004, 0x00000000, &Obj.CommandedPosition3},
};
const _objd SDO7005[] =
{
  {0x0, DTYPE_REAL32, 32, ATYPE_RO | ATYPE_RXPDO, acName7005, 0x00000000, &Obj.CommandedPosition4},
};
const _objd SDO7006[] =
{
  {0x0, DTYPE_INTEGER32, 32, ATYPE_RO | ATYPE_RXPDO, acName7006, 0, &Obj.StepsPerMM1},
};
const _objd SDO7007[] =
{
  {0x0, DTYPE_INTEGER32, 32, ATYPE_RO | ATYPE_RXPDO, acName7007, 0, &Obj.StepsPerMM2},
};
const _objd SDO7008[] =
{
  {0x0, DTYPE_INTEGER32, 32, ATYPE_RO | ATYPE_RXPDO, acName7008, 0, &Obj.StepsPerMM3},
};
const _objd SDO7009[] =
{
  {0x0, DTYPE_INTEGER32, 32, ATYPE_RO | ATYPE_RXPDO, acName7009, 0, &Obj.StepsPerMM4},
};

const _objectlist SDOobjects[] =
{
  {0x1000, OTYPE_VAR, 0, 0, acName1000, SDO1000},
  {0x1008, OTYPE_VAR, 0, 0, acName1008, SDO1008},
  {0x1009, OTYPE_VAR, 0, 0, acName1009, SDO1009},
  {0x100A, OTYPE_VAR, 0, 0, acName100A, SDO100A},
  {0x1018, OTYPE_RECORD, 4, 0, acName1018, SDO1018},
  {0x1600, OTYPE_RECORD, 1, 0, acName1600, SDO1600},
  {0x1601, OTYPE_RECORD, 4, 0, acName1601, SDO1601},
  {0x1602, OTYPE_RECORD, 1, 0, acName1602, SDO1602},
  {0x1603, OTYPE_RECORD, 1, 0, acName1603, SDO1603},
  {0x1604, OTYPE_RECORD, 1, 0, acName1604, SDO1604},
  {0x1605, OTYPE_RECORD, 1, 0, acName1605, SDO1605},
  {0x1606, OTYPE_RECORD, 1, 0, acName1606, SDO1606},
  {0x1607, OTYPE_RECORD, 1, 0, acName1607, SDO1607},
  {0x1608, OTYPE_RECORD, 1, 0, acName1608, SDO1608},
  {0x1609, OTYPE_RECORD, 1, 0, acName1609, SDO1609},
  {0x1A00, OTYPE_RECORD, 1, 0, acName1A00, SDO1A00},
  {0x1A01, OTYPE_RECORD, 8, 0, acName1A01, SDO1A01},
  {0x1A02, OTYPE_RECORD, 1, 0, acName1A02, SDO1A02},
  {0x1A03, OTYPE_RECORD, 1, 0, acName1A03, SDO1A03},
  {0x1A04, OTYPE_RECORD, 1, 0, acName1A04, SDO1A04},
  {0x1A05, OTYPE_RECORD, 1, 0, acName1A05, SDO1A05},
  {0x1A06, OTYPE_RECORD, 1, 0, acName1A06, SDO1A06},
  {0x1C00, OTYPE_ARRAY, 4, 0, acName1C00, SDO1C00},
  {0x1C12, OTYPE_ARRAY, 10, 0, acName1C12, SDO1C12},
  {0x1C13, OTYPE_ARRAY, 7, 0, acName1C13, SDO1C13},
  {0x6000, OTYPE_VAR, 0, 0, acName6000, SDO6000},
  {0x6001, OTYPE_ARRAY, 8, 0, acName6001, SDO6001},
  {0x6002, OTYPE_VAR, 0, 0, acName6002, SDO6002},
  {0x6003, OTYPE_VAR, 0, 0, acName6003, SDO6003},
  {0x6004, OTYPE_VAR, 0, 0, acName6004, SDO6004},
  {0x6005, OTYPE_VAR, 0, 0, acName6005, SDO6005},
  {0x6006, OTYPE_VAR, 0, 0, acName6006, SDO6006},
  {0x7000, OTYPE_VAR, 0, 0, acName7000, SDO7000},
  {0x7001, OTYPE_ARRAY, 4, 0, acName7001, SDO7001},
  {0x7002, OTYPE_VAR, 0, 0, acName7002, SDO7002},
  {0x7003, OTYPE_VAR, 0, 0, acName7003, SDO7003},
  {0x7004, OTYPE_VAR, 0, 0, acName7004, SDO7004},
  {0x7005, OTYPE_VAR, 0, 0, acName7005, SDO7005},
  {0x7006, OTYPE_VAR, 0, 0, acName7006, SDO7006},
  {0x7007, OTYPE_VAR, 0, 0, acName7007, SDO7007},
  {0x7008, OTYPE_VAR, 0, 0, acName7008, SDO7008},
  {0x7009, OTYPE_VAR, 0, 0, acName7009, SDO7009},
  {0xffff, 0xff, 0xff, 0xff, NULL, NULL}
};
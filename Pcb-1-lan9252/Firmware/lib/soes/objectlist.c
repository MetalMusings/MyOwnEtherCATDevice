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
static const char acName1600[] = "StatusWord";
static const char acName1600_00[] = "Max SubIndex";
static const char acName1600_01[] = "StatusWord";
static const char acName1601[] = "OpModeDisplay";
static const char acName1601_00[] = "Max SubIndex";
static const char acName1601_01[] = "OpModeDisplay";
static const char acName1602[] = "ActualPosition";
static const char acName1602_00[] = "Max SubIndex";
static const char acName1602_01[] = "ActualPosition";
static const char acName1603[] = "ActualVelocity";
static const char acName1603_00[] = "Max SubIndex";
static const char acName1603_01[] = "ActualVelocity";
static const char acName1604[] = "ActualTorque";
static const char acName1604_00[] = "Max SubIndex";
static const char acName1604_01[] = "ActualTorque";
static const char acName1605[] = "EncoderIn";
static const char acName1605_00[] = "Max SubIndex";
static const char acName1605_01[] = "IndexEnable";
static const char acName1605_02[] = "Reset";
static const char acName1A00[] = "EncoderOut";
static const char acName1A00_00[] = "Max SubIndex";
static const char acName1A00_01[] = "ECount";
static const char acName1A00_02[] = "IndexTriggered";
static const char acName1A01[] = "ControlWord";
static const char acName1A01_00[] = "Max SubIndex";
static const char acName1A01_01[] = "ControlWord";
static const char acName1A02[] = "OpMode";
static const char acName1A02_00[] = "Max SubIndex";
static const char acName1A02_01[] = "OpMode";
static const char acName1A03[] = "TargetPosition";
static const char acName1A03_00[] = "Max SubIndex";
static const char acName1A03_01[] = "TargetPosition";
static const char acName1A04[] = "TargetVelocity";
static const char acName1A04_00[] = "Max SubIndex";
static const char acName1A04_01[] = "TargetVelocity";
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
static const char acName1C13[] = "Sync Manager 3 PDO Assignment";
static const char acName1C13_00[] = "Max SubIndex";
static const char acName1C13_01[] = "PDO Mapping";
static const char acName1C13_02[] = "PDO Mapping";
static const char acName1C13_03[] = "PDO Mapping";
static const char acName1C13_04[] = "PDO Mapping";
static const char acName1C13_05[] = "PDO Mapping";
static const char acName2000[] = "StepperData";
static const char acName2000_00[] = "Max SubIndex";
static const char acName2000_01[] = "Period";
static const char acName2000_02[] = "Resolution";
static const char acName6000[] = "EncoderOut";
static const char acName6000_00[] = "Max SubIndex";
static const char acName6000_01[] = "ECount";
static const char acName6000_02[] = "IndexTriggered";
static const char acName6040[] = "ControlWord";
static const char acName6041[] = "StatusWord";
static const char acName6060[] = "OpMode";
static const char acName6061[] = "OpModeDisplay";
static const char acName6064[] = "ActualPosition";
static const char acName606C[] = "ActualVelocity";
static const char acName6077[] = "ActualTorque";
static const char acName607A[] = "TargetPosition";
static const char acName60FF[] = "TargetVelocity";
static const char acName7000[] = "EncoderIn";
static const char acName7000_00[] = "Max SubIndex";
static const char acName7000_01[] = "IndexEnable";
static const char acName7000_02[] = "Reset";

const _objd SDO1000[] =
{
  {0x0, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1000, 5001, NULL},
};
const _objd SDO1008[] =
{
  {0x0, DTYPE_VISIBLE_STRING, 272, ATYPE_RO, acName1008, 0, "MetalMusings EaserCAT 2000 encoder"},
};
const _objd SDO1009[] =
{
  {0x0, DTYPE_VISIBLE_STRING, 8, ATYPE_RO, acName1009, 0, "3"},
};
const _objd SDO100A[] =
{
  {0x0, DTYPE_VISIBLE_STRING, 8, ATYPE_RO, acName100A, 0, "4"},
};
const _objd SDO1018[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1018_00, 4, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1018_01, 4321, NULL},
  {0x02, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1018_02, 1234, NULL},
  {0x03, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1018_03, 1, NULL},
  {0x04, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1018_04, 2, &Obj.serial},
};
const _objd SDO1600[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1600_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1600_01, 0x60410010, NULL},
};
const _objd SDO1601[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1601_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1601_01, 0x60610008, NULL},
};
const _objd SDO1602[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1602_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1602_01, 0x60640020, NULL},
};
const _objd SDO1603[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1603_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1603_01, 0x606C0020, NULL},
};
const _objd SDO1604[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1604_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1604_01, 0x60770020, NULL},
};
const _objd SDO1605[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1605_00, 2, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1605_01, 0x70000108, NULL},
  {0x02, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1605_02, 0x70000208, NULL},
};
const _objd SDO1A00[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1A00_00, 2, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A00_01, 0x60000120, NULL},
  {0x02, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A00_02, 0x60000208, NULL},
};
const _objd SDO1A01[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1A01_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A01_01, 0x60400010, NULL},
};
const _objd SDO1A02[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1A02_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A02_01, 0x60600008, NULL},
};
const _objd SDO1A03[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1A03_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A03_01, 0x607A0020, NULL},
};
const _objd SDO1A04[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1A04_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A04_01, 0x60FF0020, NULL},
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
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1C12_00, 6, NULL},
  {0x01, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C12_01, 0x1600, NULL},
  {0x02, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C12_02, 0x1601, NULL},
  {0x03, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C12_03, 0x1602, NULL},
  {0x04, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C12_04, 0x1603, NULL},
  {0x05, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C12_05, 0x1604, NULL},
  {0x06, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C12_06, 0x1605, NULL},
};
const _objd SDO1C13[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1C13_00, 5, NULL},
  {0x01, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C13_01, 0x1A00, NULL},
  {0x02, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C13_02, 0x1A01, NULL},
  {0x03, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C13_03, 0x1A02, NULL},
  {0x04, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C13_04, 0x1A03, NULL},
  {0x05, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C13_05, 0x1A04, NULL},
};
const _objd SDO2000[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName2000_00, 2, NULL},
  {0x01, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName2000_01, 1000, &Obj.StepperData.Period},
  {0x02, DTYPE_REAL32, 32, ATYPE_RO, acName2000_02, 0x40200000, &Obj.StepperData.Resolution},
};
const _objd SDO6000[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName6000_00, 2, NULL},
  {0x01, DTYPE_INTEGER32, 32, ATYPE_RO, acName6000_01, 0, &Obj.EncoderOut.ECount},
  {0x02, DTYPE_UNSIGNED8, 8, ATYPE_RW, acName6000_02, 0, &Obj.EncoderOut.IndexTriggered},
};
const _objd SDO6040[] =
{
  {0x0, DTYPE_UNSIGNED16, 16, ATYPE_RO | ATYPE_TXPDO, acName6040, 0, &Obj.ControlWord},
};
const _objd SDO6041[] =
{
  {0x0, DTYPE_UNSIGNED16, 16, ATYPE_RO | ATYPE_RXPDO, acName6041, 0, &Obj.StatusWord},
};
const _objd SDO6060[] =
{
  {0x0, DTYPE_UNSIGNED8, 8, ATYPE_RO | ATYPE_TXPDO, acName6060, 0, &Obj.OpMode},
};
const _objd SDO6061[] =
{
  {0x0, DTYPE_UNSIGNED8, 8, ATYPE_RO | ATYPE_RXPDO, acName6061, 0, &Obj.OpModeDisplay},
};
const _objd SDO6064[] =
{
  {0x0, DTYPE_INTEGER32, 32, ATYPE_RO | ATYPE_RXPDO, acName6064, 0, &Obj.ActualPosition},
};
const _objd SDO606C[] =
{
  {0x0, DTYPE_INTEGER32, 32, ATYPE_RO | ATYPE_RXPDO, acName606C, 0, &Obj.ActualVelocity},
};
const _objd SDO6077[] =
{
  {0x0, DTYPE_INTEGER32, 32, ATYPE_RO | ATYPE_RXPDO, acName6077, 0, &Obj.ActualTorque},
};
const _objd SDO607A[] =
{
  {0x0, DTYPE_INTEGER32, 32, ATYPE_RO | ATYPE_TXPDO, acName607A, 0, &Obj.TargetPosition},
};
const _objd SDO60FF[] =
{
  {0x0, DTYPE_INTEGER32, 32, ATYPE_RO | ATYPE_TXPDO, acName60FF, 0, &Obj.TargetVelocity},
};
const _objd SDO7000[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName7000_00, 2, NULL},
  {0x01, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName7000_01, 0, &Obj.EncoderIn.IndexEnable},
  {0x02, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName7000_02, 0, &Obj.EncoderIn.Reset},
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
  {0x1605, OTYPE_RECORD, 2, 0, acName1605, SDO1605},
  {0x1A00, OTYPE_RECORD, 2, 0, acName1A00, SDO1A00},
  {0x1A01, OTYPE_RECORD, 1, 0, acName1A01, SDO1A01},
  {0x1A02, OTYPE_RECORD, 1, 0, acName1A02, SDO1A02},
  {0x1A03, OTYPE_RECORD, 1, 0, acName1A03, SDO1A03},
  {0x1A04, OTYPE_RECORD, 1, 0, acName1A04, SDO1A04},
  {0x1C00, OTYPE_ARRAY, 4, 0, acName1C00, SDO1C00},
  {0x1C12, OTYPE_ARRAY, 6, 0, acName1C12, SDO1C12},
  {0x1C13, OTYPE_ARRAY, 5, 0, acName1C13, SDO1C13},
  {0x2000, OTYPE_RECORD, 2, 0, acName2000, SDO2000},
  {0x6000, OTYPE_RECORD, 2, 0, acName6000, SDO6000},
  {0x6040, OTYPE_VAR, 0, 0, acName6040, SDO6040},
  {0x6041, OTYPE_VAR, 0, 0, acName6041, SDO6041},
  {0x6060, OTYPE_VAR, 0, 0, acName6060, SDO6060},
  {0x6061, OTYPE_VAR, 0, 0, acName6061, SDO6061},
  {0x6064, OTYPE_VAR, 0, 0, acName6064, SDO6064},
  {0x606C, OTYPE_VAR, 0, 0, acName606C, SDO606C},
  {0x6077, OTYPE_VAR, 0, 0, acName6077, SDO6077},
  {0x607A, OTYPE_VAR, 0, 0, acName607A, SDO607A},
  {0x60FF, OTYPE_VAR, 0, 0, acName60FF, SDO60FF},
  {0x7000, OTYPE_RECORD, 2, 0, acName7000, SDO7000},
  {0xffff, 0xff, 0xff, 0xff, NULL, NULL}
};

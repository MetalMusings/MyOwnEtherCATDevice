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
static const char acName1600[] = "Output4";
static const char acName1600_00[] = "Max SubIndex";
static const char acName1600_01[] = "Output4";
static const char acName1601[] = "In_Unit1";
static const char acName1601_00[] = "Max SubIndex";
static const char acName1601_01[] = "VoltageScale";
static const char acName1601_02[] = "VoltageOffset";
static const char acName1601_03[] = "LowPassFilterThresholdVoltage";
static const char acName1601_04[] = "EnableOhmicSensing";
static const char acName1601_05[] = "OhmicSensingSetupTime";
static const char acName1601_06[] = "OhmicSensingVoltageLimit";
static const char acName1602[] = "In_Unit2";
static const char acName1602_00[] = "Max SubIndex";
static const char acName1602_01[] = "VoltageScale";
static const char acName1602_02[] = "VoltageOffset";
static const char acName1602_03[] = "LowPassFilterThresholdVoltage";
static const char acName1602_04[] = "EnableOhmicSensing";
static const char acName1602_05[] = "OhmicSensingSetupTime";
static const char acName1602_06[] = "OhmicSensingVoltageLimit";
static const char acName1A00[] = "Input12";
static const char acName1A00_00[] = "Max SubIndex";
static const char acName1A00_01[] = "Input12";
static const char acName1A01[] = "Out_Unit1";
static const char acName1A01_00[] = "Max SubIndex";
static const char acName1A01_01[] = "RawData";
static const char acName1A01_02[] = "CalculatedVoltage";
static const char acName1A01_03[] = "Status";
static const char acName1A01_04[] = "LowpassFilteredVoltage";
static const char acName1A01_05[] = "OhmicSensingSensed";
static const char acName1A02[] = "Out_Unit2";
static const char acName1A02_00[] = "Max SubIndex";
static const char acName1A02_01[] = "RawData";
static const char acName1A02_02[] = "CalculatedVoltage";
static const char acName1A02_03[] = "Status";
static const char acName1A02_04[] = "LowpassFilteredVoltage";
static const char acName1A02_05[] = "OhmicSensingSensed";
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
static const char acName1C13[] = "Sync Manager 3 PDO Assignment";
static const char acName1C13_00[] = "Max SubIndex";
static const char acName1C13_01[] = "PDO Mapping";
static const char acName1C13_02[] = "PDO Mapping";
static const char acName1C13_03[] = "PDO Mapping";
static const char acName2000[] = "Settings_Unit1";
static const char acName2000_00[] = "Max SubIndex";
static const char acName2000_01[] = "I2C_devicetype";
static const char acName2000_02[] = "I2C_address";
static const char acName2000_03[] = "LowpassFilterPoleFrequency";
static const char acName2001[] = "Settings_Unit2";
static const char acName2001_00[] = "Max SubIndex";
static const char acName2001_01[] = "I2C_devicetype";
static const char acName2001_02[] = "I2C_address";
static const char acName2001_03[] = "LowpassFilterPoleFrequency";
static const char acName6000[] = "Input12";
static const char acName6001[] = "Out_Unit1";
static const char acName6001_00[] = "Max SubIndex";
static const char acName6001_01[] = "RawData";
static const char acName6001_02[] = "CalculatedVoltage";
static const char acName6001_03[] = "Status";
static const char acName6001_04[] = "LowpassFilteredVoltage";
static const char acName6001_05[] = "OhmicSensingSensed";
static const char acName6002[] = "Out_Unit2";
static const char acName6002_00[] = "Max SubIndex";
static const char acName6002_01[] = "RawData";
static const char acName6002_02[] = "CalculatedVoltage";
static const char acName6002_03[] = "Status";
static const char acName6002_04[] = "LowpassFilteredVoltage";
static const char acName6002_05[] = "OhmicSensingSensed";
static const char acName7000[] = "Output4";
static const char acName7001[] = "In_Unit1";
static const char acName7001_00[] = "Max SubIndex";
static const char acName7001_01[] = "VoltageScale";
static const char acName7001_02[] = "VoltageOffset";
static const char acName7001_03[] = "LowPassFilterThresholdVoltage";
static const char acName7001_04[] = "EnableOhmicSensing";
static const char acName7001_05[] = "OhmicSensingSetupTime";
static const char acName7001_06[] = "OhmicSensingVoltageLimit";
static const char acName7002[] = "In_Unit2";
static const char acName7002_00[] = "Max SubIndex";
static const char acName7002_01[] = "VoltageScale";
static const char acName7002_02[] = "VoltageOffset";
static const char acName7002_03[] = "LowPassFilterThresholdVoltage";
static const char acName7002_04[] = "EnableOhmicSensing";
static const char acName7002_05[] = "OhmicSensingSetupTime";
static const char acName7002_06[] = "OhmicSensingVoltageLimit";

const _objd SDO1000[] =
{
  {0x0, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1000, 5001, NULL},
};
const _objd SDO1008[] =
{
  {0x0, DTYPE_VISIBLE_STRING, 208, ATYPE_RO, acName1008, 0, "MetalMusings EaserCAT 7000"},
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
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1018_01, 3502166480, NULL},
  {0x02, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1018_02, 28672, NULL},
  {0x03, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1018_03, 2, NULL},
  {0x04, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1018_04, 1, &Obj.serial},
};
const _objd SDO1600[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1600_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1600_01, 0x70000008, NULL},
};
const _objd SDO1601[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1601_00, 6, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1601_01, 0x70010120, NULL},
  {0x02, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1601_02, 0x70010220, NULL},
  {0x03, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1601_03, 0x70010320, NULL},
  {0x04, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1601_04, 0x70010408, NULL},
  {0x05, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1601_05, 0x70010520, NULL},
  {0x06, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1601_06, 0x70010620, NULL},
};
const _objd SDO1602[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1602_00, 6, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1602_01, 0x70020120, NULL},
  {0x02, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1602_02, 0x70020220, NULL},
  {0x03, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1602_03, 0x70020320, NULL},
  {0x04, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1602_04, 0x70020408, NULL},
  {0x05, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1602_05, 0x70020520, NULL},
  {0x06, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1602_06, 0x70020620, NULL},
};
const _objd SDO1A00[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1A00_00, 1, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A00_01, 0x60000020, NULL},
};
const _objd SDO1A01[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1A01_00, 5, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A01_01, 0x60010120, NULL},
  {0x02, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A01_02, 0x60010220, NULL},
  {0x03, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A01_03, 0x60010320, NULL},
  {0x04, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A01_04, 0x60010420, NULL},
  {0x05, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A01_05, 0x60010508, NULL},
};
const _objd SDO1A02[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1A02_00, 5, NULL},
  {0x01, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A02_01, 0x60020120, NULL},
  {0x02, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A02_02, 0x60020220, NULL},
  {0x03, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A02_03, 0x60020320, NULL},
  {0x04, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A02_04, 0x60020420, NULL},
  {0x05, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName1A02_05, 0x60020508, NULL},
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
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1C12_00, 3, NULL},
  {0x01, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C12_01, 0x1600, NULL},
  {0x02, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C12_02, 0x1601, NULL},
  {0x03, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C12_03, 0x1602, NULL},
};
const _objd SDO1C13[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName1C13_00, 3, NULL},
  {0x01, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C13_01, 0x1A00, NULL},
  {0x02, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C13_02, 0x1A01, NULL},
  {0x03, DTYPE_UNSIGNED16, 16, ATYPE_RO, acName1C13_03, 0x1A02, NULL},
};
const _objd SDO2000[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName2000_00, 3, NULL},
  {0x01, DTYPE_UNSIGNED8, 8, ATYPE_RW, acName2000_01, 0, &Obj.Settings_Unit1.I2C_devicetype},
  {0x02, DTYPE_UNSIGNED8, 8, ATYPE_RW, acName2000_02, 0, &Obj.Settings_Unit1.I2C_address},
  {0x03, DTYPE_UNSIGNED32, 32, ATYPE_RW, acName2000_03, 0, &Obj.Settings_Unit1.LowpassFilterPoleFrequency},
};
const _objd SDO2001[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName2001_00, 3, NULL},
  {0x01, DTYPE_UNSIGNED8, 8, ATYPE_RW, acName2001_01, 0, &Obj.Settings_Unit2.I2C_devicetype},
  {0x02, DTYPE_UNSIGNED8, 8, ATYPE_RW, acName2001_02, 0, &Obj.Settings_Unit2.I2C_address},
  {0x03, DTYPE_UNSIGNED32, 32, ATYPE_RW, acName2001_03, 0, &Obj.Settings_Unit2.LowpassFilterPoleFrequency},
};
const _objd SDO6000[] =
{
  {0x0, DTYPE_INTEGER32, 32, ATYPE_RO | ATYPE_TXPDO, acName6000, 0, &Obj.Input12},
};
const _objd SDO6001[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName6001_00, 5, NULL},
  {0x01, DTYPE_INTEGER32, 32, ATYPE_RO, acName6001_01, 0, &Obj.Out_Unit1.RawData},
  {0x02, DTYPE_REAL32, 32, ATYPE_RO, acName6001_02, 0x00000000, &Obj.Out_Unit1.CalculatedVoltage},
  {0x03, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName6001_03, 0, &Obj.Out_Unit1.Status},
  {0x04, DTYPE_REAL32, 32, ATYPE_RO, acName6001_04, 0x00000000, &Obj.Out_Unit1.LowpassFilteredVoltage},
  {0x05, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName6001_05, 0, &Obj.Out_Unit1.OhmicSensingSensed},
};
const _objd SDO6002[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName6002_00, 5, NULL},
  {0x01, DTYPE_INTEGER32, 32, ATYPE_RO, acName6002_01, 0, &Obj.Out_Unit2.RawData},
  {0x02, DTYPE_REAL32, 32, ATYPE_RO, acName6002_02, 0x00000000, &Obj.Out_Unit2.CalculatedVoltage},
  {0x03, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName6002_03, 0, &Obj.Out_Unit2.Status},
  {0x04, DTYPE_REAL32, 32, ATYPE_RO, acName6002_04, 0x00000000, &Obj.Out_Unit2.LowpassFilteredVoltage},
  {0x05, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName6002_05, 0, &Obj.Out_Unit2.OhmicSensingSensed},
};
const _objd SDO7000[] =
{
  {0x0, DTYPE_UNSIGNED8, 8, ATYPE_RO | ATYPE_RXPDO, acName7000, 0, &Obj.Output4},
};
const _objd SDO7001[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName7001_00, 6, NULL},
  {0x01, DTYPE_REAL32, 32, ATYPE_RO, acName7001_01, 0x00000000, &Obj.In_Unit1.VoltageScale},
  {0x02, DTYPE_REAL32, 32, ATYPE_RO, acName7001_02, 0x00000000, &Obj.In_Unit1.VoltageOffset},
  {0x03, DTYPE_REAL32, 32, ATYPE_RO, acName7001_03, 0x00000000, &Obj.In_Unit1.LowPassFilterThresholdVoltage},
  {0x04, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName7001_04, 0, &Obj.In_Unit1.EnableOhmicSensing},
  {0x05, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName7001_05, 0, &Obj.In_Unit1.OhmicSensingSetupTime},
  {0x06, DTYPE_REAL32, 32, ATYPE_RO, acName7001_06, 0x00000000, &Obj.In_Unit1.OhmicSensingVoltageLimit},
};
const _objd SDO7002[] =
{
  {0x00, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName7002_00, 6, NULL},
  {0x01, DTYPE_REAL32, 32, ATYPE_RO, acName7002_01, 0x00000000, &Obj.In_Unit2.VoltageScale},
  {0x02, DTYPE_REAL32, 32, ATYPE_RO, acName7002_02, 0x00000000, &Obj.In_Unit2.VoltageOffset},
  {0x03, DTYPE_REAL32, 32, ATYPE_RO, acName7002_03, 0x00000000, &Obj.In_Unit2.LowPassFilterThresholdVoltage},
  {0x04, DTYPE_UNSIGNED8, 8, ATYPE_RO, acName7002_04, 0, &Obj.In_Unit2.EnableOhmicSensing},
  {0x05, DTYPE_UNSIGNED32, 32, ATYPE_RO, acName7002_05, 0, &Obj.In_Unit2.OhmicSensingSetupTime},
  {0x06, DTYPE_REAL32, 32, ATYPE_RO, acName7002_06, 0x00000000, &Obj.In_Unit2.OhmicSensingVoltageLimit},
};

const _objectlist SDOobjects[] =
{
  {0x1000, OTYPE_VAR, 0, 0, acName1000, SDO1000},
  {0x1008, OTYPE_VAR, 0, 0, acName1008, SDO1008},
  {0x1009, OTYPE_VAR, 0, 0, acName1009, SDO1009},
  {0x100A, OTYPE_VAR, 0, 0, acName100A, SDO100A},
  {0x1018, OTYPE_RECORD, 4, 0, acName1018, SDO1018},
  {0x1600, OTYPE_RECORD, 1, 0, acName1600, SDO1600},
  {0x1601, OTYPE_RECORD, 6, 0, acName1601, SDO1601},
  {0x1602, OTYPE_RECORD, 6, 0, acName1602, SDO1602},
  {0x1A00, OTYPE_RECORD, 1, 0, acName1A00, SDO1A00},
  {0x1A01, OTYPE_RECORD, 5, 0, acName1A01, SDO1A01},
  {0x1A02, OTYPE_RECORD, 5, 0, acName1A02, SDO1A02},
  {0x1C00, OTYPE_ARRAY, 4, 0, acName1C00, SDO1C00},
  {0x1C12, OTYPE_ARRAY, 3, 0, acName1C12, SDO1C12},
  {0x1C13, OTYPE_ARRAY, 3, 0, acName1C13, SDO1C13},
  {0x2000, OTYPE_RECORD, 3, 0, acName2000, SDO2000},
  {0x2001, OTYPE_RECORD, 3, 0, acName2001, SDO2001},
  {0x6000, OTYPE_VAR, 0, 0, acName6000, SDO6000},
  {0x6001, OTYPE_RECORD, 5, 0, acName6001, SDO6001},
  {0x6002, OTYPE_RECORD, 5, 0, acName6002, SDO6002},
  {0x7000, OTYPE_VAR, 0, 0, acName7000, SDO7000},
  {0x7001, OTYPE_RECORD, 6, 0, acName7001, SDO7001},
  {0x7002, OTYPE_RECORD, 6, 0, acName7002, SDO7002},
  {0xffff, 0xff, 0xff, 0xff, NULL, NULL}
};

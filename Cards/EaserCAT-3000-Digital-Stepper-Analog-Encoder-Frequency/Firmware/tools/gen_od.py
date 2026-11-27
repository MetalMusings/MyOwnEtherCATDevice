#!/usr/bin/env python3
"""
Object dictionary generator for the EaserCAT 3000 CiA402 firmware.

This script is the single source of truth for the object dictionary. It
generates, from the tables below:

  lib/soes-esi/objectlist.c                       SOES object dictionary
  lib/soes-esi/utypes.h                           SOES object storage (struct _Objects)
  lib/soes-esi/ecat_options.h                     SOES sizes / sync manager setup
  lib/soes-esi/MetalMusings_EaserCAT_3000_CiA402.xml   ESI file (ETG.2000)
  lib/soes-esi/eeprom.bin                         SII EEPROM image (ETG.2010)

Profile: CiA402 / ETG.6010, four axes (multi axis device, object range
0x6000..0x67FF shifted by 0x800 per axis), modes csp (8) and csv (9).

Usage:  python3 tools/gen_od.py          (run from the Firmware directory or anywhere)
"""

import os
import struct
import sys
from xml.sax.saxutils import escape

# ---------------------------------------------------------------------------
# Device identity
# ---------------------------------------------------------------------------
VENDOR_ID = 0xD0BED1D0
VENDOR_NAME = "MetalMusings"
PRODUCT_CODE = 0x3402          # New product code: CiA402 variant of the EaserCAT 3000
REVISION = 0x00000001
SERIAL = 0x00000001
DEVICE_NAME = "MetalMusings EaserCAT 3000 CiA402"
ORDER_TEXT = "EaserCAT 3000 CiA402"
GROUP_TYPE = "MachineControl"
GROUP_NAME = "CNC control"
HW_VERSION = "0.0.1"
SW_VERSION = "1.0.0"

N_AXES = 4
AXIS_OFFSET = 0x800                      # ETG.6010 chapter 16
DEVICE_TYPE = 0x00040192                 # ETG.6010 table 84: stepper drive, profile 402
SUPPORTED_MODES = (1 << 5) | (1 << 7) | (1 << 8)    # homing + csp + csv

MAX_PDO_ENTRIES = 8                      # Capacity of each PDO mapping object
EEPROM_BYTES = 2048

# Mailbox / SM layout (must match ESC configuration in the EEPROM)
MBX_SIZE = 512
MBX_OUT = 0x1000
MBX_IN = 0x1200
SM2_START = 0x1600
SM3_START = 0x1A00

# EEPROM ConfigData words 0..6 (PDI control, PDI config, sync impulse...).
# Same values as the previously used, verified EaserCAT 3000 EEPROM (AX58100, SPI mode 3,
# SYNC0/SYNC1 outputs enabled, 1 us sync impulse).
CONFIG_WORDS = [0x0605, 0x4403, 0x0064, 0x0000, 0x0000, 0x001A, 0x0000]

# ---------------------------------------------------------------------------
# Data types
# ---------------------------------------------------------------------------
#            name           SOES DTYPE                bits  ESI name  SII code
DTYPES = {
    "BOOLEAN":    ("DTYPE_BOOLEAN", 1, "BOOL", 0x01),
    "INTEGER8":   ("DTYPE_INTEGER8", 8, "SINT", 0x02),
    "INTEGER16":  ("DTYPE_INTEGER16", 16, "INT", 0x03),
    "INTEGER32":  ("DTYPE_INTEGER32", 32, "DINT", 0x04),
    "UNSIGNED8":  ("DTYPE_UNSIGNED8", 8, "USINT", 0x05),
    "UNSIGNED16": ("DTYPE_UNSIGNED16", 16, "UINT", 0x06),
    "UNSIGNED32": ("DTYPE_UNSIGNED32", 32, "UDINT", 0x07),
    "REAL32":     ("DTYPE_REAL32", 32, "REAL", 0x08),
    "VISIBLE_STRING": ("DTYPE_VISIBLE_STRING", 0, "STRING", 0x09),
}
C_TYPES = {
    "BOOLEAN": "uint8_t", "INTEGER8": "int8_t", "INTEGER16": "int16_t",
    "INTEGER32": "int32_t", "UNSIGNED8": "uint8_t", "UNSIGNED16": "uint16_t",
    "UNSIGNED32": "uint32_t", "REAL32": "float",
}

# Access codes: RO, RW, RWpre (writable in PREOP only, ETG.1020 ch. 10 for mapping objects)
SOES_ACCESS = {"RO": "ATYPE_RO", "RW": "ATYPE_RW", "RWpre": "ATYPE_RWpre"}


class Entry:
    def __init__(self, sub, name, dtype, access="RO", pdo=None, default=0, field=None,
                 bits=None, const=False):
        self.sub = sub
        self.name = name
        self.dtype = dtype
        self.access = access
        self.pdo = pdo          # None, "R" (RxPDO) or "T" (TxPDO)
        self.default = default
        self.field = field      # C expression of storage (without &), None for constants
        self.bits = bits if bits is not None else DTYPES[dtype][1]
        self.const = const


class Obj:
    def __init__(self, index, name, otype, entries, max_sub=None):
        self.index = index
        self.name = name
        self.otype = otype      # VAR, ARRAY, RECORD
        self.entries = entries
        self.max_sub = max_sub if max_sub is not None else (
            0 if otype == "VAR" else max(e.sub for e in entries))


def var(index, name, dtype, access="RO", pdo=None, default=0, field=None, bits=None):
    return Obj(index, name, "VAR", [Entry(0, name, dtype, access, pdo, default, field, bits)])


def f2u(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]


# ---------------------------------------------------------------------------
# Process data layout
# ---------------------------------------------------------------------------
def axis_base(n):
    return 0x6000 + AXIS_OFFSET * n


def map_val(index, sub, bits):
    return (index << 16) | (sub << 8) | bits


IO_RXPDO = 0x1600 + N_AXES
IO_TXPDO = 0x1A00 + N_AXES


def default_rx_mapping(n):
    b = axis_base(n)
    m = [map_val(b + 0x40, 0, 16),   # Controlword
         map_val(b + 0x60, 0, 8),    # Modes of operation
         map_val(b + 0x7A, 0, 32),   # Target position
         map_val(b + 0xFF, 0, 32)]   # Target velocity
    return m


def default_tx_mapping(n):
    b = axis_base(n)
    m = [map_val(b + 0x41, 0, 16),   # Statusword
         map_val(b + 0x61, 0, 8),    # Modes of operation display
         map_val(b + 0x64, 0, 32),   # Position actual value
         map_val(b + 0x6C, 0, 32),   # Velocity actual value
         map_val(b + 0xF4, 0, 32)]   # Following error actual value
    return m


# The board's own I/O: encoder, analog output, THCAD frequency and the digital
# inputs/outputs (bit 0 = terminal 1). The digital I/O can also be mapped as
# CiA402 0x60FD / 0x60FE (bits 16..), for masters that expect it there.
IO_RX_MAPPING = [map_val(0x2101, 1, 8), map_val(0x2101, 2, 32), map_val(0x2102, 0, 32),
                 map_val(0x2105, 0, 8)]
IO_TX_MAPPING = [map_val(0x2100, 1, 8), map_val(0x2100, 2, 8), map_val(0x2100, 3, 32),
                 map_val(0x2100, 4, 32), map_val(0x2103, 0, 32), map_val(0x2104, 0, 8)]

RX_PDOS = [(0x1600 + n, f"Axis {n + 1} RxPDO", default_rx_mapping(n)) for n in range(N_AXES)]
RX_PDOS.append((IO_RXPDO, "IO RxPDO", IO_RX_MAPPING))
TX_PDOS = [(0x1A00 + n, f"Axis {n + 1} TxPDO", default_tx_mapping(n)) for n in range(N_AXES)]
TX_PDOS.append((IO_TXPDO, "IO TxPDO", IO_TX_MAPPING))


# ---------------------------------------------------------------------------
# Object dictionary
# ---------------------------------------------------------------------------
def build_od():
    od = []
    od.append(var(0x1000, "Device Type", "UNSIGNED32", default=DEVICE_TYPE))
    od.append(var(0x1001, "Error Register", "UNSIGNED8", pdo="T", field="Obj.ErrorRegister"))
    od.append(Obj(0x1008, "Device Name", "VAR",
                  [Entry(0, "Device Name", "VISIBLE_STRING", default=DEVICE_NAME,
                         bits=8 * len(DEVICE_NAME))]))
    od.append(Obj(0x1009, "Hardware Version", "VAR",
                  [Entry(0, "Hardware Version", "VISIBLE_STRING", default=HW_VERSION,
                         bits=8 * len(HW_VERSION))]))
    od.append(Obj(0x100A, "Software Version", "VAR",
                  [Entry(0, "Software Version", "VISIBLE_STRING", default=SW_VERSION,
                         bits=8 * len(SW_VERSION))]))
    od.append(Obj(0x1018, "Identity Object", "RECORD", [
        Entry(0, "Max SubIndex", "UNSIGNED8", default=4),
        Entry(1, "Vendor ID", "UNSIGNED32", default=VENDOR_ID),
        Entry(2, "Product Code", "UNSIGNED32", default=PRODUCT_CODE),
        Entry(3, "Revision Number", "UNSIGNED32", default=REVISION),
        Entry(4, "Serial Number", "UNSIGNED32", default=SERIAL, field="Obj.serial"),
    ]))

    # PDO mapping objects, writable in PREOP (ETG.1020 chapter 10)
    for i, (idx, name, mapping) in enumerate(RX_PDOS):
        od.append(mapping_obj(idx, name, mapping, f"Obj.RxPdoMap[{i}]"))
    for i, (idx, name, mapping) in enumerate(TX_PDOS):
        od.append(mapping_obj(idx, name, mapping, f"Obj.TxPdoMap[{i}]"))

    od.append(Obj(0x1C00, "Sync Manager Communication Type", "ARRAY", [
        Entry(0, "Max SubIndex", "UNSIGNED8", default=4),
        Entry(1, "Communications Type SM0", "UNSIGNED8", default=1),
        Entry(2, "Communications Type SM1", "UNSIGNED8", default=2),
        Entry(3, "Communications Type SM2", "UNSIGNED8", default=3),
        Entry(4, "Communications Type SM3", "UNSIGNED8", default=4),
    ]))
    od.append(assign_obj(0x1C12, "Sync Manager 2 PDO Assignment", RX_PDOS, "Obj.Sm2Assign"))
    od.append(assign_obj(0x1C13, "Sync Manager 3 PDO Assignment", TX_PDOS, "Obj.Sm3Assign"))
    od.append(sm_param_obj(0x1C32, "Output SyncManager Parameter", "Obj.SmOutParam"))
    od.append(sm_param_obj(0x1C33, "Input SyncManager Parameter", "Obj.SmInParam"))

    # Manufacturer specific objects
    # Cycle diagnostics: is the cycle time / step rate asked for possible?
    # Read by SDO or mapped into a TxPDO. Maxima and counters are cleared by 0x1C32:08.
    d = "Obj.CycleDiag."
    od.append(Obj(0x2001, "Cycle diagnostics", "RECORD", [
        Entry(0, "Max SubIndex", "UNSIGNED8", default=10),
        Entry(1, "Max step frequency Hz", "UNSIGNED32", pdo="T", field=d + "MaxStepFrequency"),
        Entry(2, "Step interrupt time max ns", "UNSIGNED32", pdo="T", field=d + "IsrTimeMax"),
        Entry(3, "Step generation load 0.1%", "UNSIGNED16", pdo="T", field=d + "StepLoad"),
        Entry(4, "Step generation load max 0.1%", "UNSIGNED16", pdo="T", field=d + "StepLoadMax"),
        Entry(5, "Cycle time used 0.1%", "UNSIGNED16", pdo="T", field=d + "CycleUsed"),
        Entry(6, "Cycle time used max 0.1%", "UNSIGNED16", pdo="T", field=d + "CycleUsedMax"),
        Entry(7, "Cycle distance max ns", "UNSIGNED32", pdo="T", field=d + "CycleDistanceMax"),
        Entry(8, "Late steps", "UNSIGNED32", pdo="T", field=d + "LateSteps"),
        Entry(9, "Frequency limited axes", "UNSIGNED8", pdo="T", field=d + "FrequencyLimited"),
        Entry(10, "Step queue lead min us", "UNSIGNED32", pdo="T", field=d + "QueueLeadMin"),
    ]))
    for n in range(N_AXES):
        f = f"Obj.StepTiming[{n}]"
        od.append(Obj(0x2010 + n, f"Axis {n + 1} step timing", "RECORD", [
            Entry(0, "Max SubIndex", "UNSIGNED8", default=4),
            Entry(1, "Step length ns", "UNSIGNED32", "RW", default=5000, field=f + ".StepLength"),
            Entry(2, "Step space ns", "UNSIGNED32", "RW", default=5000, field=f + ".StepSpace"),
            Entry(3, "Direction setup ns", "UNSIGNED32", "RW", default=5000, field=f + ".DirSetup"),
            Entry(4, "Direction hold ns", "UNSIGNED32", "RW", default=5000, field=f + ".DirHold"),
        ]))
    od.append(Obj(0x2100, "Encoder inputs", "RECORD", [
        Entry(0, "Max SubIndex", "UNSIGNED8", default=4),
        Entry(1, "Index status", "UNSIGNED8", pdo="T", field="Obj.EncoderIn.IndexStatus"),
        Entry(2, "Index pin", "UNSIGNED8", pdo="T", field="Obj.EncoderIn.IndexPin"),
        Entry(3, "Position", "REAL32", pdo="T", field="Obj.EncoderIn.Position"),
        Entry(4, "Velocity", "REAL32", pdo="T", field="Obj.EncoderIn.Velocity"),
    ]))
    od.append(Obj(0x2101, "Encoder outputs", "RECORD", [
        Entry(0, "Max SubIndex", "UNSIGNED8", default=2),
        Entry(1, "Index latch enable", "UNSIGNED8", "RW", pdo="R",
              field="Obj.EncoderOut.IndexLatchEnable"),
        Entry(2, "Scale", "REAL32", "RW", pdo="R", default=f2u(1.0),
              field="Obj.EncoderOut.Scale"),
    ]))
    od.append(var(0x2102, "Analog output V", "REAL32", "RW", pdo="R", field="Obj.AnalogOut"))
    od.append(var(0x2103, "Frequency input Hz", "REAL32", pdo="T", field="Obj.FrequencyIn"))
    od.append(var(0x2104, "Digital inputs", "UNSIGNED8", pdo="T", field="Obj.DigitalIn"))
    od.append(var(0x2105, "Digital outputs", "UNSIGNED8", "RW", pdo="R", field="Obj.DigitalOut"))
    od.append(var(0x2106, "Input polarity", "UNSIGNED8", "RW", default=0xFF,
                  field="Obj.InputPolarity"))
    od.append(var(0x2107, "Input filter time us", "UNSIGNED16", "RW", default=1000,
                  field="Obj.InputFilterTime"))
    for n in range(N_AXES):
        a = f"Obj.Axis[{n}]"
        od.append(Obj(0x2020 + n, f"Axis {n + 1} switches", "RECORD", [
            Entry(0, "Max SubIndex", "UNSIGNED8", default=4),
            Entry(1, "Home switch input", "UNSIGNED8", "RW", default=n + 1,
                  field=a + ".HomeInput"),
            Entry(2, "Positive limit switch input", "UNSIGNED8", "RW", default=0,
                  field=a + ".PosLimitInput"),
            Entry(3, "Negative limit switch input", "UNSIGNED8", "RW", default=0,
                  field=a + ".NegLimitInput"),
            Entry(4, "Homing time limit ms", "UNSIGNED32", "RW", default=60000,
                  field=a + ".HomingTimeLimit"),
        ]))

    # CiA402 axis objects
    for n in range(N_AXES):
        b = axis_base(n)
        a = f"Obj.Axis[{n}]"
        od.append(var(b + 0x007, "Abort connection option code", "INTEGER16", "RW", default=1,
                      field=a + ".AbortConnectionOption"))
        od.append(var(b + 0x03F, "Error code", "UNSIGNED16", pdo="T", field=a + ".ErrorCode"))
        od.append(var(b + 0x040, "Controlword", "UNSIGNED16", "RW", pdo="R",
                      field=a + ".Controlword"))
        od.append(var(b + 0x041, "Statusword", "UNSIGNED16", pdo="T", field=a + ".Statusword"))
        od.append(var(b + 0x05A, "Quick stop option code", "INTEGER16", "RW", default=2,
                      field=a + ".QuickStopOption"))
        od.append(var(b + 0x05E, "Fault reaction option code", "INTEGER16", "RW", default=2,
                      field=a + ".FaultReactionOption"))
        od.append(var(b + 0x060, "Modes of operation", "INTEGER8", "RW", pdo="R", default=8,
                      field=a + ".ModesOfOperation"))
        od.append(var(b + 0x061, "Modes of operation display", "INTEGER8", pdo="T",
                      field=a + ".ModesOfOperationDisplay"))
        od.append(var(b + 0x062, "Position demand value", "INTEGER32", pdo="T",
                      field=a + ".PositionDemand"))
        od.append(var(b + 0x064, "Position actual value", "INTEGER32", pdo="T",
                      field=a + ".PositionActual"))
        od.append(var(b + 0x065, "Following error window", "UNSIGNED32", "RW",
                      default=0xFFFFFFFF, field=a + ".FollowingErrorWindow"))
        od.append(var(b + 0x066, "Following error time out", "UNSIGNED16", "RW", default=10,
                      field=a + ".FollowingErrorTimeout"))
        od.append(var(b + 0x06C, "Velocity actual value", "INTEGER32", pdo="T",
                      field=a + ".VelocityActual"))
        od.append(var(b + 0x07A, "Target position", "INTEGER32", "RW", pdo="R",
                      field=a + ".TargetPosition"))
        od.append(var(b + 0x07C, "Home offset", "INTEGER32", "RW", field=a + ".HomeOffset"))
        od.append(var(b + 0x07E, "Polarity", "UNSIGNED8", "RW", field=a + ".Polarity"))
        od.append(var(b + 0x07F, "Max profile velocity", "UNSIGNED32", "RW", default=0,
                      field=a + ".MaxProfileVelocity"))
        od.append(var(b + 0x085, "Quick stop deceleration", "UNSIGNED32", "RW", default=100000,
                      field=a + ".QuickStopDeceleration"))
        od.append(var(b + 0x098, "Homing method", "INTEGER8", "RW", default=37,
                      field=a + ".HomingMethod"))
        od.append(Obj(b + 0x099, "Homing speeds", "RECORD", [
            Entry(0, "Max SubIndex", "UNSIGNED8", default=2),
            Entry(1, "Speed during search for switch", "UNSIGNED32", "RW", default=10000,
                  field=a + ".HomingSpeedSwitch"),
            Entry(2, "Speed during search for zero", "UNSIGNED32", "RW", default=2000,
                  field=a + ".HomingSpeedZero"),
        ]))
        od.append(var(b + 0x09A, "Homing acceleration", "UNSIGNED32", "RW", default=100000,
                      field=a + ".HomingAcceleration"))
        od.append(Obj(b + 0x0C2, "Interpolation time period", "RECORD", [
            Entry(0, "Max SubIndex", "UNSIGNED8", default=2),
            Entry(1, "Interpolation time period value", "UNSIGNED8", default=1,
                  field=a + ".InterpolationTimeValue"),
            Entry(2, "Interpolation time index", "INTEGER8", default=0xFD,
                  field=a + ".InterpolationTimeIndex"),
        ]))
        od.append(var(b + 0x0C5, "Max acceleration", "UNSIGNED32", "RW", default=0,
                      field=a + ".MaxAcceleration"))
        od.append(var(b + 0x0F4, "Following error actual value", "INTEGER32", pdo="T",
                      field=a + ".FollowingErrorActual"))
        od.append(var(b + 0x0FD, "Digital inputs", "UNSIGNED32", pdo="T",
                      field=a + ".DigitalInputs"))
        od.append(Obj(b + 0x0FE, "Digital outputs", "RECORD", [
            Entry(0, "Max SubIndex", "UNSIGNED8", default=2),
            Entry(1, "Physical outputs", "UNSIGNED32", "RW", pdo="R",
                  field=a + ".DigitalOutputs"),
            Entry(2, "Bit mask", "UNSIGNED32", "RW", default=0x000F0000,
                  field=a + ".DigitalOutputsMask"),
        ]))
        od.append(var(b + 0x0FF, "Target velocity", "INTEGER32", "RW", pdo="R",
                      field=a + ".TargetVelocity"))
        od.append(var(b + 0x502, "Supported drive modes", "UNSIGNED32", default=SUPPORTED_MODES))
        od.append(var(b + 0x7FF, "Device type", "UNSIGNED32", default=DEVICE_TYPE))

    od.sort(key=lambda o: o.index)
    return od


def mapping_obj(idx, name, mapping, field):
    entries = [Entry(0, "Max SubIndex", "UNSIGNED8", "RWpre", default=len(mapping),
                     field=field + ".Count")]
    for i in range(MAX_PDO_ENTRIES):
        dv = mapping[i] if i < len(mapping) else 0
        entries.append(Entry(i + 1, f"Mapped object {i + 1}", "UNSIGNED32", "RWpre", default=dv,
                             field=f"{field}.Entry[{i}]"))
    return Obj(idx, name, "RECORD", entries)


def assign_obj(idx, name, pdos, field):
    entries = [Entry(0, "Max SubIndex", "UNSIGNED8", "RWpre", default=len(pdos),
                     field=field + ".Count")]
    for i, (pdo_idx, _, _) in enumerate(pdos):
        entries.append(Entry(i + 1, f"PDO Mapping {i + 1}", "UNSIGNED16", "RWpre",
                             default=pdo_idx, field=f"{field}.Pdo[{i}]"))
    return Obj(idx, name, "ARRAY", entries)


def sm_param_obj(idx, name, field):
    # ETG.1020 table 80/81. :05 is worked out from the step rates (CycleModel.h), :06
    # measured; :08 = 1 starts a new measurement (clears :06 and the maxima in 0x2001),
    # 2 also clears the counters :0B, :0C and 0x2001:09/:0A.
    return Obj(idx, name, "RECORD", [
        Entry(0, "Max SubIndex", "UNSIGNED8", default=32),
        Entry(1, "Sync mode", "UNSIGNED16", field=field + ".SyncType"),
        Entry(2, "Cycle time", "UNSIGNED32", field=field + ".CycleTime"),
        Entry(3, "Shift time", "UNSIGNED32", field=field + ".ShiftTime"),
        Entry(4, "Sync modes supported", "UNSIGNED16", default=0x0006),
        Entry(5, "Minimum cycle time", "UNSIGNED32", default=250000,
              field=field + ".MinCycleTime"),
        Entry(6, "Calc and copy time", "UNSIGNED32", field=field + ".CalcCopyTime"),
        Entry(8, "Get cycle time", "UNSIGNED16", "RW", field=field + ".GetCycleTime"),
        Entry(9, "Delay time", "UNSIGNED32", default=0),
        Entry(10, "Sync0 cycle time", "UNSIGNED32", field=field + ".Sync0CycleTime"),
        Entry(11, "SM event missed", "UNSIGNED16", field=field + ".SmEventMissed"),
        Entry(12, "Cycle time too small", "UNSIGNED16", field=field + ".CycleTimeTooSmall"),
        Entry(13, "Shift time too short", "UNSIGNED16", default=0),
        Entry(32, "Sync error", "BOOLEAN", field=field + ".SyncError"),
    ])


# ---------------------------------------------------------------------------
# C generation
# ---------------------------------------------------------------------------
def cname(index, sub=None, single=False):
    if sub is None or single:
        return f"acName{index:04X}"
    return f"acName{index:04X}_{sub:02X}"


def entry_flags(obj, e):
    fl = SOES_ACCESS[e.access]
    if e.pdo == "R":
        fl += " | ATYPE_RXPDO"
    elif e.pdo == "T":
        fl += " | ATYPE_TXPDO"
    return fl


def gen_objectlist(od):
    out = ['/* Generated by tools/gen_od.py - do not edit by hand */',
           '#include "esc_coe.h"', '#include "utypes.h"', '#include <stddef.h>', '']
    for o in od:
        out.append(f'static const char {cname(o.index)}[] = "{o.name}";')
        if o.otype != "VAR":
            for e in o.entries:
                out.append(f'static const char {cname(o.index, e.sub)}[] = "{e.name}";')
    out.append('')
    for o in od:
        out.append(f'const _objd SDO{o.index:04X}[] =')
        out.append('{')
        for e in o.entries:
            dt = DTYPES[e.dtype][0]
            nm = cname(o.index) if o.otype == "VAR" else cname(o.index, e.sub)
            if e.dtype == "VISIBLE_STRING":
                out.append(f'  {{0x{e.sub:02x}, {dt}, {e.bits}, {entry_flags(o, e)}, {nm}, 0, '
                           f'"{e.default}"}},')
                continue
            val = e.default & 0xFFFFFFFF
            data = f"&{e.field}" if e.field else "NULL"
            out.append(f'  {{0x{e.sub:02x}, {dt}, {e.bits}, {entry_flags(o, e)}, {nm}, '
                       f'0x{val:08X}, {data}}},')
        out.append('};')
    out.append('')
    out.append('const _objectlist SDOobjects[] =')
    out.append('{')
    for o in od:
        out.append(f'  {{0x{o.index:04X}, OTYPE_{o.otype}, {o.max_sub}, 0, {cname(o.index)}, '
                   f'SDO{o.index:04X}}},')
    out.append('  {0xffff, 0xff, 0xff, 0xff, NULL, NULL}')
    out.append('};')
    return "\n".join(out) + "\n"


def gen_utypes():
    return f"""/* Generated by tools/gen_od.py - do not edit by hand */
#ifndef __UTYPES_H__
#define __UTYPES_H__

#include "cc.h"

#define N_AXES {N_AXES}
#define N_RXPDO {len(RX_PDOS)}
#define N_TXPDO {len(TX_PDOS)}
#define MAX_PDO_ENTRIES {MAX_PDO_ENTRIES}

typedef struct
{{
   uint8_t Count;
   uint32_t Entry[MAX_PDO_ENTRIES];
}} PdoMap_t;

typedef struct
{{
   uint8_t Count;
   uint16_t Pdo[N_RXPDO > N_TXPDO ? N_RXPDO : N_TXPDO];
}} PdoAssign_t;

/* ETG.1020 0x1C32 / 0x1C33 */
typedef struct
{{
   uint16_t SyncType;
   uint32_t CycleTime;
   uint32_t ShiftTime;
   uint32_t MinCycleTime;
   uint32_t CalcCopyTime;
   uint16_t GetCycleTime;
   uint32_t Sync0CycleTime;
   uint16_t SmEventMissed;
   uint16_t CycleTimeTooSmall;
   uint8_t SyncError;
}} SmParam_t;

typedef struct
{{
   uint32_t StepLength;
   uint32_t StepSpace;
   uint32_t DirSetup;
   uint32_t DirHold;
}} StepTiming_t;

/* CiA402 objects of one axis (index = 0x6000 + 0x800 * axis + offset) */
typedef struct
{{
   int16_t AbortConnectionOption;     /* 0x6007 */
   uint16_t ErrorCode;                /* 0x603F */
   uint16_t Controlword;              /* 0x6040 */
   uint16_t Statusword;               /* 0x6041 */
   int16_t QuickStopOption;           /* 0x605A */
   int16_t FaultReactionOption;       /* 0x605E */
   int8_t ModesOfOperation;           /* 0x6060 */
   int8_t ModesOfOperationDisplay;    /* 0x6061 */
   int32_t PositionDemand;            /* 0x6062 */
   int32_t PositionActual;            /* 0x6064 */
   uint32_t FollowingErrorWindow;     /* 0x6065 */
   uint16_t FollowingErrorTimeout;    /* 0x6066 */
   int32_t VelocityActual;            /* 0x606C */
   int32_t TargetPosition;            /* 0x607A */
   int32_t HomeOffset;                /* 0x607C */
   uint8_t Polarity;                  /* 0x607E */
   uint32_t MaxProfileVelocity;       /* 0x607F */
   uint32_t QuickStopDeceleration;    /* 0x6085 */
   int8_t HomingMethod;               /* 0x6098 */
   uint32_t HomingSpeedSwitch;        /* 0x6099:01 */
   uint32_t HomingSpeedZero;          /* 0x6099:02 */
   uint32_t HomingAcceleration;       /* 0x609A */
   uint8_t InterpolationTimeValue;    /* 0x60C2:01 */
   int8_t InterpolationTimeIndex;     /* 0x60C2:02 */
   uint32_t MaxAcceleration;          /* 0x60C5 */
   int32_t FollowingErrorActual;      /* 0x60F4 */
   uint32_t DigitalInputs;            /* 0x60FD */
   uint32_t DigitalOutputs;           /* 0x60FE:01 */
   uint32_t DigitalOutputsMask;       /* 0x60FE:02 */
   int32_t TargetVelocity;            /* 0x60FF */
   /* Manufacturer specific, 0x2020 + axis */
   uint8_t HomeInput;                 /* :01, 0 = none, 1..8 = IN1..IN8 */
   uint8_t PosLimitInput;             /* :02 */
   uint8_t NegLimitInput;             /* :03 */
   uint32_t HomingTimeLimit;          /* :04, ms, 0 = none */
}} Cia402Axis_t;

typedef struct
{{
   /* Identity */
   uint32_t serial;

   /* Communication area */
   uint8_t ErrorRegister;
   PdoMap_t RxPdoMap[N_RXPDO];
   PdoMap_t TxPdoMap[N_TXPDO];
   PdoAssign_t Sm2Assign;
   PdoAssign_t Sm3Assign;
   SmParam_t SmOutParam;
   SmParam_t SmInParam;

   /* Manufacturer specific */
   struct
   {{
      uint32_t MaxStepFrequency;     /* :01 Hz, the lowest of the axes */
      uint32_t IsrTimeMax;           /* :02 ns, longest step timer interrupt */
      uint16_t StepLoad;             /* :03 0.1 % of the CPU: interrupts and planning */
      uint16_t StepLoadMax;          /* :04 */
      uint16_t CycleUsed;            /* :05 0.1 % of the cycle time, last cycle */
      uint16_t CycleUsedMax;         /* :06 */
      uint32_t CycleDistanceMax;     /* :07 ns between cycle starts */
      uint32_t LateSteps;            /* :08 the step queue ran empty while moving */
      uint8_t FrequencyLimited;      /* :09 bit n: axis n+1 was held at its step rate limit */
      uint32_t QueueLeadMin;         /* :0A us, shortest time the queue was ahead */
   }} CycleDiag;
   StepTiming_t StepTiming[N_AXES];
   struct
   {{
      uint8_t IndexStatus;
      uint8_t IndexPin;
      float Position;
      float Velocity;
   }} EncoderIn;
   struct
   {{
      uint8_t IndexLatchEnable;
      float Scale;
   }} EncoderOut;
   float AnalogOut;
   float FrequencyIn;
   uint8_t DigitalIn;
   uint8_t DigitalOut;
   uint8_t InputPolarity;
   uint16_t InputFilterTime;

   /* CiA402 */
   Cia402Axis_t Axis[N_AXES];
}} _Objects;

extern _Objects Obj;

#endif /* __UTYPES_H__ */
"""


def gen_ecat_options():
    max_entries = MAX_PDO_ENTRIES * max(len(RX_PDOS), len(TX_PDOS))
    return f"""/* Generated by tools/gen_od.py - do not edit by hand */
#ifndef __ECAT_OPTIONS_H__
#define __ECAT_OPTIONS_H__

#define USE_FOE          0
#define USE_EOE          0

#define MBXSIZE          {MBX_SIZE}
#define MBXSIZEBOOT      {MBX_SIZE}
#define MBXBUFFERS       3

#define MBX0_sma         0x{MBX_OUT:04X}
#define MBX0_sml         MBXSIZE
#define MBX0_sme         MBX0_sma+MBX0_sml-1
#define MBX0_smc         0x26
#define MBX1_sma         MBX0_sma+MBX0_sml
#define MBX1_sml         MBXSIZE
#define MBX1_sme         MBX1_sma+MBX1_sml-1
#define MBX1_smc         0x22

#define MBX0_sma_b       0x{MBX_OUT:04X}
#define MBX0_sml_b       MBXSIZEBOOT
#define MBX0_sme_b       MBX0_sma_b+MBX0_sml_b-1
#define MBX0_smc_b       0x26
#define MBX1_sma_b       MBX0_sma_b+MBX0_sml_b
#define MBX1_sml_b       MBXSIZEBOOT
#define MBX1_sme_b       MBX1_sma_b+MBX1_sml_b-1
#define MBX1_smc_b       0x22

#define SM2_sma          0x{SM2_START:04X}
#define SM2_smc          0x24
#define SM2_act          1
#define SM3_sma          0x{SM3_START:04X}
#define SM3_smc          0x20
#define SM3_act          1

/* Max number of mapped entries per sync manager (all PDOs fully used) */
#define MAX_MAPPINGS_SM2 {max_entries}
#define MAX_MAPPINGS_SM3 {max_entries}

#define MAX_RXPDO_SIZE   512
#define MAX_TXPDO_SIZE   512

#endif /* __ECAT_OPTIONS_H__ */
"""


# ---------------------------------------------------------------------------
# ESI generation (ETG.2000)
# ---------------------------------------------------------------------------
def esi_access(e):
    if e.access == "RO":
        return "<Access>ro</Access>"
    if e.access == "RWpre":
        return '<Access WriteRestrictions="PreOP">rw</Access>'
    return "<Access>rw</Access>"


def esi_type(e):
    if e.dtype == "VISIBLE_STRING":
        return f"STRING({e.bits // 8})"
    return DTYPES[e.dtype][2]


def esi_default(e):
    if e.dtype == "VISIBLE_STRING":
        return f"<DefaultString>{escape(e.default)}</DefaultString>"
    bytes_ = max(1, e.bits // 8)
    v = e.default & ((1 << (8 * bytes_)) - 1)
    # DefaultData is little endian hex
    data = v.to_bytes(bytes_, "little").hex()
    return f"<DefaultData>{data}</DefaultData>"


def obj_bitsize(o):
    if o.otype == "VAR":
        return o.entries[0].bits
    # Record layout: sub0 in 16 bits, then entries packed, 8 bit entries take 8 bits
    off = 16
    for e in o.entries[1:]:
        off += e.bits
    return off


def gen_esi(od):
    L = []
    w = L.append
    w('<?xml version="1.0" encoding="UTF-8"?>')
    w('<!-- Generated by tools/gen_od.py - do not edit by hand -->')
    w('<EtherCATInfo xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" '
      'xsi:noNamespaceSchemaLocation="EtherCATInfo.xsd" Version="1.6">')
    w('  <Vendor>')
    w(f'    <Id>#x{VENDOR_ID:08X}</Id>')
    w(f'    <Name LcId="1033">{VENDOR_NAME}</Name>')
    w('  </Vendor>')
    w('  <Descriptions>')
    w('    <Groups>')
    w('      <Group>')
    w(f'        <Type>{GROUP_TYPE}</Type>')
    w(f'        <Name LcId="1033">{GROUP_NAME}</Name>')
    w('      </Group>')
    w('    </Groups>')
    w('    <Devices>')
    w('      <Device Physics="YY">')
    w(f'        <Type ProductCode="#x{PRODUCT_CODE:08X}" RevisionNo="#x{REVISION:08X}">'
      f'{ORDER_TEXT}</Type>')
    w(f'        <Name LcId="1033">{DEVICE_NAME}</Name>')
    w(f'        <GroupType>{GROUP_TYPE}</GroupType>')
    w('        <Profile>')
    w(f'          <ChannelCount>{N_AXES}</ChannelCount>')
    for n in range(N_AXES):
        w('          <ChannelInfo>')
        w('            <ProfileNo>402</ProfileNo>')
        w(f'            <AddInfo>{(DEVICE_TYPE >> 16) & 0xFF}</AddInfo>')
        w(f'            <DisplayName LcId="1033">Axis {n + 1}</DisplayName>')
        w('          </ChannelInfo>')
    w('          <Dictionary>')
    w('            <DataTypes>')
    used = set()
    for o in od:
        for e in o.entries:
            used.add((esi_type(e), e.bits))
    for t, bits in sorted(used):
        w('              <DataType>')
        w(f'                <Name>{t}</Name>')
        w(f'                <BitSize>{bits}</BitSize>')
        w('              </DataType>')
    for o in od:
        if o.otype == "VAR":
            continue
        dtn = f"DT{o.index:04X}"
        if o.otype == "ARRAY":
            base = o.entries[1]
            n = len(o.entries) - 1
            w('              <DataType>')
            w(f'                <Name>{dtn}ARR</Name>')
            w(f'                <BaseType>{esi_type(base)}</BaseType>')
            w(f'                <BitSize>{base.bits * n}</BitSize>')
            w('                <ArrayInfo>')
            w('                  <LBound>1</LBound>')
            w(f'                  <Elements>{n}</Elements>')
            w('                </ArrayInfo>')
            w('              </DataType>')
            w('              <DataType>')
            w(f'                <Name>{dtn}</Name>')
            w(f'                <BitSize>{16 + base.bits * n}</BitSize>')
            w('                <SubItem>')
            w('                  <SubIdx>0</SubIdx>')
            w('                  <Name>Max SubIndex</Name>')
            w('                  <Type>USINT</Type>')
            w('                  <BitSize>8</BitSize>')
            w('                  <BitOffs>0</BitOffs>')
            w(f'                  <Flags>{esi_access(o.entries[0])}</Flags>')
            w('                </SubItem>')
            w('                <SubItem>')
            w('                  <Name>Elements</Name>')
            w(f'                  <Type>{dtn}ARR</Type>')
            w(f'                  <BitSize>{base.bits * n}</BitSize>')
            w('                  <BitOffs>16</BitOffs>')
            w(f'                  <Flags>{esi_access(base)}</Flags>')
            w('                </SubItem>')
            w('              </DataType>')
            continue
        w('              <DataType>')
        w(f'                <Name>{dtn}</Name>')
        w(f'                <BitSize>{obj_bitsize(o)}</BitSize>')
        off = 0
        for e in o.entries:
            w('                <SubItem>')
            w(f'                  <SubIdx>{e.sub}</SubIdx>')
            w(f'                  <Name>{escape(e.name)}</Name>')
            w(f'                  <Type>{esi_type(e)}</Type>')
            w(f'                  <BitSize>{e.bits}</BitSize>')
            w(f'                  <BitOffs>{off}</BitOffs>')
            flags = esi_access(e)
            if e.pdo:
                flags += f"<PdoMapping>{e.pdo}</PdoMapping>"
            w(f'                  <Flags>{flags}</Flags>')
            w('                </SubItem>')
            off = 16 if e.sub == 0 else off + e.bits
        w('              </DataType>')
    w('            </DataTypes>')
    w('            <Objects>')
    for o in od:
        w('              <Object>')
        w(f'                <Index>#x{o.index:04X}</Index>')
        w(f'                <Name>{escape(o.name)}</Name>')
        if o.otype == "VAR":
            e = o.entries[0]
            w(f'                <Type>{esi_type(e)}</Type>')
            w(f'                <BitSize>{e.bits}</BitSize>')
            w(f'                <Info>{esi_default(e)}</Info>')
            flags = esi_access(e)
            if e.pdo:
                flags += f"<PdoMapping>{e.pdo}</PdoMapping>"
            w(f'                <Flags>{flags}</Flags>')
        else:
            w(f'                <Type>DT{o.index:04X}</Type>')
            w(f'                <BitSize>{obj_bitsize(o) if o.otype == "RECORD" else 16 + o.entries[1].bits * (len(o.entries) - 1)}</BitSize>')
            w('                <Info>')
            for e in o.entries:
                w('                  <SubItem>')
                w(f'                    <Name>{escape(e.name)}</Name>')
                w(f'                    <Info>{esi_default(e)}</Info>')
                w('                  </SubItem>')
            w('                </Info>')
            w(f'                <Flags>{esi_access(o.entries[0])}</Flags>')
        w('              </Object>')
    w('            </Objects>')
    w('          </Dictionary>')
    w('        </Profile>')
    w('        <Fmmu>Outputs</Fmmu>')
    w('        <Fmmu>Inputs</Fmmu>')
    w('        <Fmmu>MBoxState</Fmmu>')
    w(f'        <Sm DefaultSize="{MBX_SIZE}" StartAddress="#x{MBX_OUT:04X}" ControlByte="#x26" '
      f'Enable="1">MBoxOut</Sm>')
    w(f'        <Sm DefaultSize="{MBX_SIZE}" StartAddress="#x{MBX_IN:04X}" ControlByte="#x22" '
      f'Enable="1">MBoxIn</Sm>')
    w(f'        <Sm StartAddress="#x{SM2_START:04X}" ControlByte="#x24" Enable="1">Outputs</Sm>')
    w(f'        <Sm StartAddress="#x{SM3_START:04X}" ControlByte="#x20" Enable="1">Inputs</Sm>')
    entry_lookup = {}
    for o in od:
        for e in o.entries:
            entry_lookup[(o.index, e.sub)] = (o, e)
    for tag, pdos, sm in (("RxPdo", RX_PDOS, 2), ("TxPdo", TX_PDOS, 3)):
        for idx, name, mapping in pdos:
            w(f'        <{tag} Sm="{sm}">')
            w(f'          <Index>#x{idx:04X}</Index>')
            w(f'          <Name>{escape(name)}</Name>')
            for m in mapping:
                oi, si, bl = m >> 16, (m >> 8) & 0xFF, m & 0xFF
                o, e = entry_lookup[(oi, si)]
                ename = o.name if o.otype == "VAR" else e.name
                w('          <Entry>')
                w(f'            <Index>#x{oi:04X}</Index>')
                w(f'            <SubIndex>{si}</SubIndex>')
                w(f'            <BitLen>{bl}</BitLen>')
                w(f'            <Name>{escape(ename)}</Name>')
                w(f'            <DataType>{esi_type(e)}</DataType>')
                w('          </Entry>')
            w(f'        </{tag}>')
    w('        <Mailbox DataLinkLayer="true">')
    w('          <CoE SdoInfo="true" PdoAssign="true" PdoConfig="true" PdoUpload="true" '
      'CompleteAccess="false"/>')
    w('        </Mailbox>')
    w('        <Dc>')
    w('          <OpMode>')
    w('            <Name>DC</Name>')
    w('            <Desc>DC-Synchron (SYNC0)</Desc>')
    w('            <AssignActivate>#x0300</AssignActivate>')
    w('            <CycleTimeSync0 Factor="1">0</CycleTimeSync0>')
    w('            <ShiftTimeSync0>0</ShiftTimeSync0>')
    w('          </OpMode>')
    w('          <OpMode>')
    w('            <Name>SM</Name>')
    w('            <Desc>SM-Synchron</Desc>')
    w('            <AssignActivate>#x0000</AssignActivate>')
    w('          </OpMode>')
    w('        </Dc>')
    w('        <Eeprom>')
    w(f'          <ByteSize>{EEPROM_BYTES}</ByteSize>')
    cfg = b"".join(struct.pack("<H", x) for x in CONFIG_WORDS)
    w(f'          <ConfigData>{cfg.hex().upper()}</ConfigData>')
    w('        </Eeprom>')
    w('      </Device>')
    w('    </Devices>')
    w('  </Descriptions>')
    w('</EtherCATInfo>')
    return "\n".join(L) + "\n"


# ---------------------------------------------------------------------------
# SII EEPROM generation (ETG.2010)
# ---------------------------------------------------------------------------
def crc8_sii(data):
    crc = 0xFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


class Strings:
    def __init__(self):
        self.items = []

    def idx(self, s):
        if not s:
            return 0
        if s not in self.items:
            self.items.append(s)
        return self.items.index(s) + 1


def category(ctype, payload):
    if len(payload) % 2:
        payload += b"\x00"
    return struct.pack("<HH", ctype, len(payload) // 2) + payload


def gen_eeprom(od):
    strs = Strings()
    i_group = strs.idx(GROUP_TYPE)
    i_order = strs.idx(ORDER_TEXT)
    i_name = strs.idx(DEVICE_NAME)

    entry_lookup = {(o.index, e.sub): (o, e) for o in od for e in o.entries}

    def pdo_cat(pdos, sm):
        p = b""
        for idx, name, mapping in pdos:
            p += struct.pack("<HBBBBH", idx, len(mapping), sm, 0, strs.idx(name), 0x0002)
            for m in mapping:
                oi, si, bl = m >> 16, (m >> 8) & 0xFF, m & 0xFF
                _, e = entry_lookup[(oi, si)]
                p += struct.pack("<HBBBBH", oi, si, 0, DTYPES[e.dtype][3], bl, 0)
        return p

    rx_cat = pdo_cat(RX_PDOS, 2)
    tx_cat = pdo_cat(TX_PDOS, 3)

    rx_size = sum(m & 0xFF for _, _, mp in RX_PDOS for m in mp) // 8
    tx_size = sum(m & 0xFF for _, _, mp in TX_PDOS for m in mp) // 8

    coe_details = 0x01 | 0x02 | 0x04 | 0x08 | 0x10   # SDO, SDOInfo, PdoAssign, PdoConfig, upload
    flags = 0x04                                     # MboxDataLinkLayer
    general = struct.pack("<BBBBBBBBBBBBhBBHH", i_group, 0, i_order, i_name, 0, coe_details,
                          0, 0, 0, 0, 0, flags, 0, i_group, 0, 0x0011, 0) + bytes(12)
    fmmu = bytes([0x01, 0x02, 0x03, 0xFF])
    syncm = b"".join([
        struct.pack("<HHBBBB", MBX_OUT, MBX_SIZE, 0x26, 0, 1, 1),
        struct.pack("<HHBBBB", MBX_IN, MBX_SIZE, 0x22, 0, 1, 2),
        struct.pack("<HHBBBB", SM2_START, rx_size, 0x24, 0, 1, 3),
        struct.pack("<HHBBBB", SM3_START, tx_size, 0x20, 0, 1, 4),
    ])
    i_dc = strs.idx("DC")
    i_dcdesc = strs.idx("DC-Synchron (SYNC0)")
    i_sm = strs.idx("SM")
    i_smdesc = strs.idx("SM-Synchron")
    dc = (struct.pack("<IIIhHhBB", 0, 0, 0, 0, 0x0300, 1, i_dc, i_dcdesc) + bytes(4) +
          struct.pack("<IIIhHhBB", 0, 0, 0, 0, 0x0000, 0, i_sm, i_smdesc) + bytes(4))

    s = bytes([len(strs.items)])
    for st in strs.items:
        b = st.encode("ascii")
        s += bytes([len(b)]) + b

    cats = (category(10, s) + category(30, general) + category(40, fmmu) +
            category(41, syncm) + category(51, rx_cat) + category(50, tx_cat) +
            category(60, dc) + struct.pack("<H", 0xFFFF))

    hdr = b"".join(struct.pack("<H", x) for x in CONFIG_WORDS)
    hdr += struct.pack("<H", crc8_sii(hdr))
    hdr += struct.pack("<IIII", VENDOR_ID, PRODUCT_CODE, REVISION, SERIAL)
    hdr += bytes(8)
    hdr += bytes(8)                                                    # no bootstrap mailbox
    hdr += struct.pack("<HHHH", MBX_OUT, MBX_SIZE, MBX_IN, MBX_SIZE)   # standard mailbox
    hdr += struct.pack("<H", 0x0004)                                   # CoE
    hdr += bytes(66)
    hdr += struct.pack("<HH", EEPROM_BYTES * 8 // 1024 - 1, 1)
    assert len(hdr) == 128, len(hdr)

    img = hdr + cats
    if len(img) > EEPROM_BYTES:
        sys.exit(f"EEPROM image too large: {len(img)} > {EEPROM_BYTES}")
    return img + b"\xFF" * (EEPROM_BYTES - len(img)), len(img), rx_size, tx_size


# ---------------------------------------------------------------------------
def main():
    here = os.path.dirname(os.path.abspath(__file__))
    outdir = os.path.join(here, "..", "lib", "soes-esi")
    od = build_od()

    # Sanity checks
    for idx, _, mapping in RX_PDOS + TX_PDOS:
        assert len(mapping) <= MAX_PDO_ENTRIES, hex(idx)
    lookup = {(o.index, e.sub): e for o in od for e in o.entries}
    for pdos, kind in ((RX_PDOS, "R"), (TX_PDOS, "T")):
        for idx, _, mapping in pdos:
            for m in mapping:
                e = lookup[(m >> 16, (m >> 8) & 0xFF)]
                assert e.pdo == kind and e.bits == (m & 0xFF), (hex(idx), hex(m))

    files = {
        "objectlist.c": gen_objectlist(od),
        "utypes.h": gen_utypes(),
        "ecat_options.h": gen_ecat_options(),
        "MetalMusings_EaserCAT_3000_CiA402.xml": gen_esi(od),
    }
    for name, text in files.items():
        with open(os.path.join(outdir, name), "w", newline="\n") as f:
            f.write(text)
    img, used, rx, tx = gen_eeprom(od)
    with open(os.path.join(outdir, "eeprom.bin"), "wb") as f:
        f.write(img)
    print(f"Generated {len(files) + 1} files in {os.path.normpath(outdir)}")
    print(f"  objects: {len(od)}, EEPROM used: {used}/{EEPROM_BYTES} bytes, "
          f"default RxPDO {rx} bytes, TxPDO {tx} bytes")


if __name__ == "__main__":
    main()

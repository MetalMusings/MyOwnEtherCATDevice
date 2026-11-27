# EaserCAT 3000 CiA402 in LinuxCNC (lcec, IgH kernel master)

A three-axis milling machine (X, Y, Z) with the AXIS GUI, in mm, on the
EaserCAT 3000 with its CiA402 firmware (product code `0x3402`,
[Firmware/CiA402.md](../../../Firmware/CiA402.md)), for LinuxCNC with
[linuxcnc-ethercat](https://github.com/linuxcnc-ethercat/linuxcnc-ethercat)
(lcec) and the IgH EtherCAT kernel master.

| File | |
|---|---|
| `axis.ini` | The machine. `[JOINT_n]SCALE` is steps per mm. |
| `e3000.hal` | lcec, and a `cia402` component per axis that runs its CiA402 state machine (csp) and scales mm to steps. |
| `ethercat-conf.xml` | The board's process data for lcec: the firmware's default PDO mapping, DC SYNC0 every servo cycle. |
| `tool.tbl` | A tool table. |

LinuxCNC writes `linuxcnc.var` and `position.txt` here while it runs
(`.gitignore`).

## What is needed

- LinuxCNC 2.9 or later.
- The IgH EtherCAT master as a kernel module (`ethercat.service`, the card in
  `/etc/ethercat.conf`), and lcec with `lcec_conf`.
- The `cia402` HAL component (hal-cia402, `cia402.comp`), installed with
  `sudo halcompile --install cia402.comp`.
- The board flashed with the CiA402 firmware and its EEPROM written
  (`ethercat sii_write -p <position> Firmware/lib/soes-esi/eeprom.bin`).

## Before the first start

1. `idx` of the slave in `ethercat-conf.xml` is the board's position on the
   bus (`ethercat slaves`). It is 4 here, on a test bus with other slaves in
   front of it.
2. `[JOINT_n]SCALE` in `axis.ini`: steps per mm of each axis. The board's step
   rate is at most 1 / (step length + step space) (0x2010..0x2013, default
   5 us each, down to 1 us), so `MAX_VELOCITY * SCALE` must stay below that and
   below what the drives take.
3. Directions: a negative `SCALE`, or the axis' polarity object (example in
   `ethercat-conf.xml`).
4. Homing: the board does it (CiA402 homing mode). LinuxCNC starts it as
   "home on index" through `cia402.N.home`. With the board's default method 37
   a joint is homed where it stands. For home switches (IN1..IN3 for X, Y, Z),
   set the method and speeds with the commented SDOs in `ethercat-conf.xml`.
   The switch edge becomes `HOME_OFFSET`.
5. Acceleration: LinuxCNC limits it, and the board does not by default (0x60C5
   = 0). Set 0x60C5 of each axis to 1.5-2 x `MAX_ACCELERATION` x `SCALE`
   steps/s² (example in `ethercat-conf.xml`), so a late or repeated frame from
   the master does not jerk the axis.
6. E-stop: `e3000.hal` loops the GUI's enable back; wire a real E-stop to
   `iocontrol.0.emc-enable-in`, for example through a board input.

Then `linuxcnc axis.ini`.

## Pins

lcec makes `lcec.0.E3000.*`: per board axis `axN-controlword`, `axN-opmode`,
`axN-target-position`, `axN-target-velocity`, `axN-statusword`,
`axN-opmode-display`, `axN-actual-position`, `axN-actual-velocity`,
`axN-following-error`; the board's `in-1`..`in-8` and `out-1`..`out-4`
(numbered as the terminals); the spindle encoder (`enc-*`); the THCAD
frequency `thcad-frequency` in Hz; and the analog output `analog-out-v`
(volts at the DAC pin, 0..3.3: a VFD's 0..10 V input needs an external
amplifier). Examples for the I/O are at the end of `e3000.hal`.

## Notes

- The DC reference clock is the first slave on the bus with DC; this lcec
  cannot choose another. With other DC slaves in front of the board that the
  configuration leaves out, that slave stays in PREOP with the master's error
  flag (and `lcec.0.all-op` stays false). The board itself runs.
- Tried 2026-11-25 on a bus with three ECT60 drives and an EaserCAT 7000 in
  front of the board (LinuxCNC 2.9.4, HAL loaded with halrun, no axis
  enabled): the board went to OP with DC SYNC0 and no sync error, and the
  `cia402` components read its status words. Not yet run with the AXIS GUI and
  motion.
- The same board works with EtherCAT Setup and the `ecat` component (userspace
  master) as well: there the CiA402 state machine is in the component.

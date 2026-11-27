// Host unit tests for the CiA402 state machine and the generated object dictionary.
// Build and run with test/host/run_tests.sh (needs only g++/gcc).

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "Cia402Axis.h"
#include "CycleModel.h"
#include "StepPlanner.h"
#include "ObjectHooks.h"
#include "StepGenChannel.h"
#include "StepGenLoop.h"

extern "C" {
#include "ecat_slv.h"
#include "esc_coe.h"
#include "utypes.h"
}

_Objects Obj;

// SOES hardware abstraction stubs
extern "C" {
void ESC_read(uint16_t, void *buf, uint16_t len) { memset(buf, 0, len); }
void ESC_write(uint16_t, void *, uint16_t) {}
void ESC_init(const esc_cfg_t *) {}
void ESC_reset(void) {}
void cb_get_inputs(void) {}
void cb_set_outputs(void) {}
}

static int failures = 0;
static int checks = 0;
#define CHECK(cond)                                                     \
  do {                                                                  \
    checks++;                                                           \
    if (!(cond)) {                                                      \
      failures++;                                                       \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);            \
    }                                                                   \
  } while (0)
#define CHECK_EQ(a, b)                                                                  \
  do {                                                                                  \
    checks++;                                                                           \
    long long _a = (long long)(a), _b = (long long)(b);                                 \
    if (_a != _b) {                                                                     \
      failures++;                                                                       \
      printf("FAIL %s:%d: %s == %s (0x%llx != 0x%llx)\n", __FILE__, __LINE__, #a, #b, \
             _a, _b);                                                                   \
    }                                                                                   \
  } while (0)

static const double DT = 0.001;

// Minimal plant: integrates the command like the step generator would do
struct Plant {
  double pos = 0, vel = 0;
  void apply(const MotionCommand &c) {
    if (!c.enable) {
      vel = 0;
      return;
    }
    double v;
    if (c.positionMode) {
      v = ((double)c.position / (double)STEPGEN_FX_ONE - pos) / DT;
    } else {
      v = c.velocity;
    }
    if (c.maxAccel > 0) {
      double dv = c.maxAccel * DT;
      if (v > vel + dv) v = vel + dv;
      if (v < vel - dv) v = vel - dv;
    }
    vel = v;
    pos += vel * DT;
  }
  MotionFeedback fb() const {
    return {(int64_t)llround(pos * (double)STEPGEN_FX_ONE), (float)vel};
  }
};

static void resetAxisOd(Cia402Axis_t &od) {
  memset(&od, 0, sizeof(od));
  od.AbortConnectionOption = 1;
  od.QuickStopOption = 2;
  od.FaultReactionOption = 2;
  od.ModesOfOperation = CIA402_MODE_CSP;
  od.FollowingErrorWindow = 0xFFFFFFFF;
  od.FollowingErrorTimeout = 10;
  od.QuickStopDeceleration = 100000;
  od.DigitalOutputsMask = 0x000F0000;
}

static MotionCommand step(Cia402Axis &ax, Cia402Axis_t &od, Plant &p, uint16_t cw,
                          bool valid = true, bool fresh = true) {
  od.Controlword = cw;
  MotionCommand c = ax.cycle(p.fb(), DT, valid, fresh);
  p.apply(c);
  return c;
}

static void enableAxis(Cia402Axis &ax, Cia402Axis_t &od, Plant &p) {
  step(ax, od, p, 0x0006);
  step(ax, od, p, 0x0007);
  step(ax, od, p, 0x000F);
}

static void testStateMachine() {
  Cia402Axis_t od;
  resetAxisOd(od);
  Cia402Axis ax;
  Plant p;
  ax.init(&od);
  CHECK(ax.state() == Cia402State::SwitchOnDisabled);
  CHECK_EQ(od.Statusword & 0x006F, 0x0040);

  MotionCommand c = step(ax, od, p, 0x0006);
  CHECK(ax.state() == Cia402State::ReadyToSwitchOn);
  CHECK_EQ(od.Statusword & 0x006F, 0x0021);
  CHECK(!c.enable);

  c = step(ax, od, p, 0x0007);
  CHECK(ax.state() == Cia402State::SwitchedOn);
  CHECK_EQ(od.Statusword & 0x006F, 0x0023);
  CHECK(!c.enable);

  od.TargetPosition = 0;
  c = step(ax, od, p, 0x000F);
  CHECK(ax.state() == Cia402State::OperationEnabled);
  CHECK_EQ(od.Statusword & 0x006F, 0x0027);
  CHECK(od.Statusword & SW_FOLLOWS_COMMAND);
  CHECK(c.enable && c.positionMode && c.resync);
  CHECK_EQ(od.ModesOfOperationDisplay, CIA402_MODE_CSP);

  // Disable operation -> Switched on (transition 5)
  step(ax, od, p, 0x0007);
  CHECK(ax.state() == Cia402State::SwitchedOn);
  // Enable again, then shutdown -> Ready (8), disable voltage -> SOD
  step(ax, od, p, 0x000F);
  step(ax, od, p, 0x0006);
  CHECK(ax.state() == Cia402State::ReadyToSwitchOn);
  step(ax, od, p, 0x0000);
  CHECK(ax.state() == Cia402State::SwitchOnDisabled);

  // Ready to switch on -> Operation enabled with one command (0x0F)
  step(ax, od, p, 0x0006);
  step(ax, od, p, 0x000F);
  CHECK(ax.state() == Cia402State::OperationEnabled);

  // Controlword ignored while outputs are not valid
  step(ax, od, p, 0x0000, false, false);
  CHECK(ax.state() == Cia402State::OperationEnabled);
}

static void testCspFollowsAndWraps() {
  Cia402Axis_t od;
  resetAxisOd(od);
  Cia402Axis ax;
  Plant p;
  ax.init(&od);
  enableAxis(ax, od, p);
  for (int i = 1; i <= 100; i++) {
    od.TargetPosition = i * 10;
    step(ax, od, p, 0x000F);
  }
  step(ax, od, p, 0x000F);  // actual value is sampled at the start of a cycle
  CHECK_EQ(od.PositionActual, 1000);
  CHECK_EQ(od.PositionDemand, 1000);
  CHECK_EQ(od.FollowingErrorActual, 0);

  // Wrap around of the 32 bit target position must give a continuous motion
  Cia402Axis_t od2;
  resetAxisOd(od2);
  Cia402Axis ax2;
  Plant p2;
  p2.pos = 2147483600.0;
  ax2.init(&od2);
  od2.TargetPosition = 2147483600;
  enableAxis(ax2, od2, p2);
  int32_t t = 2147483600;
  for (int i = 0; i < 20; i++) {
    t = (int32_t)((uint32_t)t + 10u);
    od2.TargetPosition = t;
    step(ax2, od2, p2, 0x000F);
  }
  CHECK(fabs(p2.pos - (2147483600.0 + 200.0)) < 0.5);
  CHECK(fabs(p2.vel - 10000.0) < 1.0);
  step(ax2, od2, p2, 0x000F);
  CHECK_EQ(od2.PositionActual, t);
}

static void testMissedFrameExtrapolation() {
  Cia402Axis_t od;
  resetAxisOd(od);
  Cia402Axis ax;
  Plant p;
  ax.init(&od);
  enableAxis(ax, od, p);
  for (int i = 1; i <= 10; i++) {
    od.TargetPosition = i * 5;
    step(ax, od, p, 0x000F);
  }
  // Frame missed: target not updated, but motion continues with the last delta
  MotionCommand c = step(ax, od, p, 0x000F, true, false);
  CHECK_EQ(c.position, stepgenFxFromSteps(55));
  // Next real frame jumps to the real target, which is consistent again
  od.TargetPosition = 60;
  c = step(ax, od, p, 0x000F);
  CHECK_EQ(c.position, stepgenFxFromSteps(60));
}

static void testCsvAndPolarity() {
  Cia402Axis_t od;
  resetAxisOd(od);
  od.ModesOfOperation = CIA402_MODE_CSV;
  od.MaxProfileVelocity = 5000;
  od.Polarity = 0xC0;  // invert position and velocity
  Cia402Axis ax;
  Plant p;
  ax.init(&od);
  enableAxis(ax, od, p);
  od.TargetVelocity = 8000;  // will be limited to 5000
  MotionCommand c = step(ax, od, p, 0x000F);
  CHECK(!c.positionMode);
  CHECK(fabs(c.velocity + 5000.0) < 1e-9);  // inverted and clamped
  for (int i = 0; i < 10; i++) step(ax, od, p, 0x000F);
  CHECK_EQ(od.VelocityActual, 5000);  // reported in external direction
  CHECK(od.PositionActual > 0);
  CHECK(p.pos < 0);
  CHECK_EQ(od.ModesOfOperationDisplay, CIA402_MODE_CSV);
}

static void testQuickStop() {
  Cia402Axis_t od;
  resetAxisOd(od);
  od.ModesOfOperation = CIA402_MODE_CSV;
  od.QuickStopDeceleration = 100000;  // 100 steps/s per 1 ms cycle
  Cia402Axis ax;
  Plant p;
  ax.init(&od);
  enableAxis(ax, od, p);
  od.TargetVelocity = 1000;
  for (int i = 0; i < 5; i++) step(ax, od, p, 0x000F);
  CHECK(fabs(p.vel - 1000) < 1e-6);

  step(ax, od, p, 0x000B);  // quick stop (bit 2 = 0)
  CHECK(ax.state() == Cia402State::QuickStopActive);
  CHECK_EQ(od.Statusword & 0x006F, 0x0007);
  int cycles = 0;
  while (ax.state() == Cia402State::QuickStopActive && cycles < 100) {
    step(ax, od, p, 0x000B);
    cycles++;
  }
  CHECK(ax.state() == Cia402State::SwitchOnDisabled);
  CHECK(cycles >= 9 && cycles <= 12);  // 1000 steps/s at 100000 steps/s^2 = 10 ms
  CHECK(fabs(p.vel) < 1e-6);
}

static void testFollowingErrorFaultAndReset() {
  Cia402Axis_t od;
  resetAxisOd(od);
  od.FollowingErrorWindow = 100;
  od.FollowingErrorTimeout = 5;  // ms
  od.FaultReactionOption = 0;    // disable immediately
  Cia402Axis ax;
  Plant p;
  ax.init(&od);
  enableAxis(ax, od, p);
  // Plant that does not move: simulate a stalled axis by feeding a fixed feedback
  od.TargetPosition = 1000;
  MotionFeedback stalled = {0, 0.0f};
  int i;
  for (i = 0; i < 20 && !ax.faulted(); i++) {
    od.Controlword = 0x000F;
    ax.cycle(stalled, DT, true, true);
    if (i == 0) CHECK(od.Statusword & SW_FOLLOWING_ERROR);
  }
  CHECK(ax.faulted());
  CHECK(i >= 5 && i <= 7);
  CHECK_EQ(od.ErrorCode, CIA402_ERR_FOLLOWING_ERROR);
  od.Controlword = 0x000F;
  MotionCommand c = ax.cycle(stalled, DT, true, true);
  CHECK(ax.state() == Cia402State::Fault);
  CHECK(!c.enable);
  CHECK_EQ(od.Statusword & 0x004F, 0x0008);

  // Fault reset needs a rising edge of bit 7
  od.Controlword = 0x0080;
  ax.cycle(stalled, DT, true, true);
  CHECK(ax.state() == Cia402State::SwitchOnDisabled);
  CHECK_EQ(od.ErrorCode, 0);
  od.Controlword = 0x0080;  // holding the bit does nothing more
  ax.cycle(stalled, DT, true, true);
  CHECK(ax.state() == Cia402State::SwitchOnDisabled);
}

static void testCommunicationLoss() {
  // Default option 1: fault, but only for an axis with power enabled
  Cia402Axis_t od;
  resetAxisOd(od);
  Cia402Axis ax;
  Plant p;
  ax.init(&od);
  ax.communicationLost();
  step(ax, od, p, 0x0000, false, false);
  CHECK(ax.state() == Cia402State::SwitchOnDisabled);

  enableAxis(ax, od, p);
  ax.communicationLost();
  step(ax, od, p, 0x000F, false, false);
  // Standing still: the fault reaction ramp ends at once
  step(ax, od, p, 0x000F, false, false);
  CHECK(ax.state() == Cia402State::Fault);
  CHECK_EQ(od.ErrorCode, CIA402_ERR_COMMUNICATION);

  // Option 2: disable voltage
  Cia402Axis_t od2;
  resetAxisOd(od2);
  od2.AbortConnectionOption = 2;
  Cia402Axis ax2;
  Plant p2;
  ax2.init(&od2);
  enableAxis(ax2, od2, p2);
  ax2.communicationLost();
  MotionCommand c = step(ax2, od2, p2, 0x000F, false, false);
  CHECK(ax2.state() == Cia402State::SwitchOnDisabled);
  CHECK(!c.enable);
}

static void testUnsupportedMode() {
  Cia402Axis_t od;
  resetAxisOd(od);
  od.ModesOfOperation = 1;  // profile position, not supported
  Cia402Axis ax;
  Plant p;
  ax.init(&od);
  enableAxis(ax, od, p);
  CHECK_EQ(od.ModesOfOperationDisplay, 0);
  CHECK(!(od.Statusword & SW_FOLLOWS_COMMAND));
}

// ---------------------------------------------------------------------------
// Object dictionary
// ---------------------------------------------------------------------------
static uint32_t sdoWrite(uint16_t index, uint8_t sub, uint32_t value, size_t size) {
  int32_t nidx = SDO_findobject(index);
  if (nidx < 0) return ABORT_NOOBJECT;
  int16_t nsub = SDO_findsubindex(nidx, sub);
  if (nsub < 0) return ABORT_NOSUBINDEX;
  // As SOES: written only if writable (in PREOP here), then the hook
  if (!(SDOobjects[nidx].objdesc[nsub].flags & ATYPE_Wpre)) return ABORT_READONLY;
  uint32_t abort = ObjectHooks_preDownload(index, sub, &value, size, 0);
  if (abort) return abort;
  memcpy(SDOobjects[nidx].objdesc[nsub].data, &value, size);
  return 0;
}

static void testObjectDictionary() {
  COE_initDefaultValues();
  _SMmap map[64];
  int n = 0;
  CHECK_EQ(sizeOfPDO(RX_PDO_OBJIDX, &n, map, MAX_MAPPINGS_SM2), 54);
  CHECK_EQ(n, 4 * 4 + 4);
  CHECK_EQ(sizeOfPDO(TX_PDO_OBJIDX, &n, map, MAX_MAPPINGS_SM3), 75);
  CHECK_EQ(n, 4 * 5 + 6);

  // Default values
  CHECK_EQ(Obj.Axis[2].QuickStopOption, 2);
  CHECK_EQ(Obj.Axis[3].ModesOfOperation, 8);
  CHECK_EQ(Obj.Axis[1].InterpolationTimeIndex, -3);
  CHECK_EQ(Obj.Axis[0].FollowingErrorWindow, 0xFFFFFFFF);
  CHECK(fabs(Obj.EncoderOut.Scale - 1.0f) < 1e-9);

  // Axis objects are at 0x6000 + 0x800 * axis
  int32_t nidx = SDO_findobject(0x7840);  // Controlword axis 4
  CHECK(nidx >= 0);
  CHECK(SDOobjects[nidx].objdesc[0].data == &Obj.Axis[3].Controlword);
  nidx = SDO_findobject(0x6FFF);  // Device type of axis 2
  CHECK(nidx >= 0);
  CHECK_EQ(SDOobjects[nidx].objdesc[0].value, 0x00040192);
  nidx = SDO_findobject(0x6502);
  CHECK_EQ(SDOobjects[nidx].objdesc[0].value, 0x1A0);
  CHECK_EQ(Obj.Axis[0].HomingMethod, 37);
  CHECK_EQ(Obj.Axis[2].HomeInput, 3);
  CHECK_EQ(Obj.InputPolarity, 0xFF);

  // ETG.1020 chapter 10: entries only changeable while subindex 0 = 0
  CHECK_EQ(sdoWrite(0x1A00, 6, 0x60770010, 4), ABORT_SUBINDEX0_NOT_ZERO);
  CHECK_EQ(sdoWrite(0x1A00, 1, Obj.TxPdoMap[0].Entry[0], 4), 0);  // same value is ok
  CHECK_EQ(sdoWrite(0x1A00, 0, 0, 1), 0);
  CHECK_EQ(sdoWrite(0x1A00, 6, 0x60770010, 4), ABORT_MAPPING_OBJECT_ERROR);  // no 0x6077
  CHECK_EQ(sdoWrite(0x1A00, 6, 0x607A0020, 4), ABORT_MAPPING_OBJECT_ERROR);  // RxPDO object
  CHECK_EQ(sdoWrite(0x1A00, 6, 0x60640010, 4), ABORT_MAPPING_OBJECT_ERROR);  // wrong length
  CHECK_EQ(sdoWrite(0x1A00, 6, 0x60620020, 4), 0);  // position demand value
  CHECK_EQ(sdoWrite(0x1A00, 0, 9, 1), ABORT_VALUE_EXCEEDED);
  CHECK_EQ(sdoWrite(0x1A00, 0, 6, 1), 0);
  CHECK_EQ(sizeOfPDO(TX_PDO_OBJIDX, &n, map, MAX_MAPPINGS_SM3), 75 + 4);
  // The board inputs can also be mapped the CiA402 way
  CHECK_EQ(sdoWrite(0x1A00, 0, 0, 1), 0);
  CHECK_EQ(sdoWrite(0x1A00, 7, 0x60FD0020, 4), 0);
  CHECK_EQ(sdoWrite(0x1A00, 0, 7, 1), 0);
  CHECK_EQ(sizeOfPDO(TX_PDO_OBJIDX, &n, map, MAX_MAPPINGS_SM3), 75 + 4 + 4);

  // Remap like linuxcnc-ethercat does: only some PDOs assigned
  CHECK_EQ(sdoWrite(0x1C13, 1, 0x1A01, 2), ABORT_SUBINDEX0_NOT_ZERO);
  CHECK_EQ(sdoWrite(0x1C13, 0, 0, 1), 0);
  CHECK_EQ(sdoWrite(0x1C13, 1, 0x1A01, 2), 0);
  CHECK_EQ(sdoWrite(0x1C13, 2, 0x1600, 2), ABORT_VALUE_EXCEEDED);
  CHECK_EQ(sdoWrite(0x1C13, 0, 1, 1), 0);
  CHECK_EQ(sizeOfPDO(TX_PDO_OBJIDX, &n, map, MAX_MAPPINGS_SM3), 15);

  // Parameter range checks
  CHECK_EQ(sdoWrite(0x605A, 0, 3, 2), ABORT_VALUE_EXCEEDED);
  CHECK_EQ(sdoWrite(0x705A, 0, 6, 2), 0);
  CHECK_EQ(sdoWrite(0x6860, 0, 1, 1), ABORT_VALUE_EXCEEDED);
  CHECK_EQ(sdoWrite(0x6860, 0, 9, 1), 0);
  CHECK_EQ(sdoWrite(0x6860, 0, 6, 1), 0);
  CHECK_EQ(sdoWrite(0x6098, 0, 21, 1), 0);
  CHECK_EQ(sdoWrite(0x6098, 0, 3, 1), ABORT_VALUE_EXCEEDED);  // needs an index pulse
  CHECK_EQ(sdoWrite(0x2021, 1, 8, 1), 0);
  CHECK_EQ(sdoWrite(0x2021, 2, 9, 1), ABORT_VALUE_EXCEEDED);
  CHECK_EQ(sdoWrite(0x607E, 0, 0x80, 1), 0);
  CHECK_EQ(sdoWrite(0x607E, 0, 0x01, 1), ABORT_VALUE_EXCEEDED);
  CHECK_EQ(sdoWrite(0x2000, 0, 20000, 4), ABORT_READONLY);  // diagnostics, read only
}

// ---------------------------------------------------------------------------
// Homing and limit switches
// ---------------------------------------------------------------------------

// A home switch that is active on one side of an edge, like a cam on the axis
struct Switch {
  double edge = 1000;   // steps
  bool positiveSide = true;
  bool last = false;
  bool active(double pos) const { return positiveSide ? pos >= edge : pos <= edge; }
};

static MotionCommand stepIn(Cia402Axis &ax, Cia402Axis_t &od, Plant &p, uint16_t cw,
                            Switch &sw, bool posLim = false, bool negLim = false) {
  od.Controlword = cw;
  AxisInputs in;
  in.home = sw.active(p.pos);
  in.homeChanged = in.home != sw.last;
  in.homeChangePosition = stepgenFxFromSteps((int64_t)sw.edge);  // the latch: to one step
  sw.last = in.home;
  in.posLimit = posLim;
  in.negLimit = negLim;
  MotionCommand c = ax.cycle(p.fb(), DT, true, true, in);
  p.apply(c);
  return c;
}

static void homingOd(Cia402Axis_t &od, int8_t method) {
  resetAxisOd(od);
  od.ModesOfOperation = CIA402_MODE_HOMING;
  od.HomingMethod = method;
  od.HomingSpeedSwitch = 10000;
  od.HomingSpeedZero = 1000;
  od.HomingAcceleration = 100000;
  od.QuickStopDeceleration = 200000;
  od.HomeOffset = 500;
  od.HomeInput = 1;
  od.HomingTimeLimit = 60000;
}

// Run homing to the end; returns the cycles used, or -1
static int runHoming(Cia402Axis &ax, Cia402Axis_t &od, Plant &p, Switch &sw,
                     int maxCycles = 20000) {
  sw.last = sw.active(p.pos);
  for (int i = 0; i < 3; i++) stepIn(ax, od, p, 0x000F, sw);  // enabled, mode 6
  for (int i = 0; i < maxCycles; i++) {
    stepIn(ax, od, p, 0x001F, sw);
    if (od.Statusword & SW_TARGET_REACHED) return i;
  }
  return -1;
}

static void testHomingMethods() {
  struct Case {
    int8_t method;
    bool positiveSide;
    double start;
  } cases[] = {
      {19, true, 0},    {19, true, 2000},  // switch inactive / active at the start
      {20, true, 0},    {20, true, 2000},  {21, false, 2000}, {21, false, 0},
      {22, false, 2000}, {22, false, 0},
  };
  for (const Case &k : cases) {
    Cia402Axis_t od;
    homingOd(od, k.method);
    Cia402Axis ax;
    Plant p;
    p.pos = k.start;
    ax.init(&od);
    step(ax, od, p, 0x0006);
    step(ax, od, p, 0x0007);
    Switch sw;
    sw.positiveSide = k.positiveSide;
    int n = runHoming(ax, od, p, sw);
    CHECK(n > 0);
    CHECK_EQ(od.ModesOfOperationDisplay, CIA402_MODE_HOMING);
    CHECK_EQ(od.Statusword & (SW_TARGET_REACHED | SW_HOMING_ATTAINED | SW_HOMING_ERROR),
             SW_TARGET_REACHED | SW_HOMING_ATTAINED);
    CHECK(ax.homingAttained());
    // The edge is the home offset; the axis is homed where it stopped
    CHECK_EQ(od.PositionActual, 500 + llround(p.pos) - (long long)sw.edge);
    // It stopped close to the edge (slow final approach), on the side the method says
    bool endActive = sw.active(p.pos);
    bool wantActive = (k.method == 20 || k.method == 22);
    CHECK(endActive == wantActive);
    CHECK(fabs(p.pos - sw.edge) < 20);
  }
}

static void testHomingCurrentPosition() {
  Cia402Axis_t od;
  homingOd(od, 37);
  Cia402Axis ax;
  Plant p;
  p.pos = 1234;
  ax.init(&od);
  step(ax, od, p, 0x0006);
  step(ax, od, p, 0x0007);
  Switch sw;
  int n = runHoming(ax, od, p, sw);
  CHECK_EQ(n, 0);
  CHECK_EQ(od.PositionActual, 500);
  CHECK(od.Statusword & SW_HOMING_ATTAINED);

  // Back to csp with a target still in the old frame (as hal-cia402 sends for one
  // cycle): the axis holds until the target is near the actual position
  od.ModesOfOperation = CIA402_MODE_CSP;
  od.TargetPosition = 1234;
  for (int i = 0; i < 5; i++) stepIn(ax, od, p, 0x000F, sw);
  CHECK(fabs(p.pos - 1234) < 1e-6);
  CHECK_EQ(od.Statusword & SW_FOLLOWS_COMMAND, 0);
  od.TargetPosition = 500;
  stepIn(ax, od, p, 0x000F, sw);
  stepIn(ax, od, p, 0x000F, sw);
  CHECK(od.Statusword & SW_FOLLOWS_COMMAND);
  CHECK_EQ(od.PositionActual, 500);
  CHECK(fabs(p.pos - 1234) < 1e-6);
  od.TargetPosition = 600;
  stepIn(ax, od, p, 0x000F, sw);
  stepIn(ax, od, p, 0x000F, sw);  // the position actual value is from the cycle's start
  CHECK_EQ(od.PositionActual, 600);
  CHECK(fabs(p.pos - 1334) < 1e-6);
}

static void testHomingErrors() {
  // A limit switch during homing: error, stop
  {
    Cia402Axis_t od;
    homingOd(od, 19);
    od.PosLimitInput = 2;
    Cia402Axis ax;
    Plant p;
    ax.init(&od);
    step(ax, od, p, 0x0006);
    step(ax, od, p, 0x0007);
    Switch sw;
    for (int i = 0; i < 3; i++) stepIn(ax, od, p, 0x000F, sw);
    for (int i = 0; i < 20; i++) stepIn(ax, od, p, 0x001F, sw);
    CHECK(p.vel > 0);
    for (int i = 0; i < 200; i++) stepIn(ax, od, p, 0x001F, sw, true);
    CHECK(fabs(p.vel) < 1);
    CHECK_EQ(od.Statusword & (SW_TARGET_REACHED | SW_HOMING_ATTAINED | SW_HOMING_ERROR),
             SW_TARGET_REACHED | SW_HOMING_ERROR);
    CHECK(od.Statusword & SW_INTERNAL_LIMIT);
    CHECK(ax.state() == Cia402State::OperationEnabled);
    // A new start clears the error
    stepIn(ax, od, p, 0x000F, sw);
    stepIn(ax, od, p, 0x001F, sw);
    CHECK_EQ(od.Statusword & (SW_TARGET_REACHED | SW_HOMING_ERROR), 0);
  }
  // The home switch never found: time limit
  {
    Cia402Axis_t od;
    homingOd(od, 21);
    od.HomingTimeLimit = 100;
    Cia402Axis ax;
    Plant p;
    ax.init(&od);
    step(ax, od, p, 0x0006);
    step(ax, od, p, 0x0007);
    Switch sw;
    sw.edge = -1e9;
    sw.positiveSide = false;
    int n = runHoming(ax, od, p, sw, 1000);
    CHECK(n > 100 && n < 200);
    CHECK(od.Statusword & SW_HOMING_ERROR);
  }
  // Bit 4 back to 0: interrupted, not homed
  {
    Cia402Axis_t od;
    homingOd(od, 19);
    Cia402Axis ax;
    Plant p;
    ax.init(&od);
    step(ax, od, p, 0x0006);
    step(ax, od, p, 0x0007);
    Switch sw;
    for (int i = 0; i < 3; i++) stepIn(ax, od, p, 0x000F, sw);
    for (int i = 0; i < 20; i++) stepIn(ax, od, p, 0x001F, sw);
    CHECK_EQ(od.Statusword & SW_TARGET_REACHED, 0);
    for (int i = 0; i < 300; i++) stepIn(ax, od, p, 0x000F, sw);
    CHECK(fabs(p.vel) < 1);
    CHECK_EQ(od.Statusword & (SW_TARGET_REACHED | SW_HOMING_ATTAINED | SW_HOMING_ERROR),
             SW_TARGET_REACHED);
  }
  // A method that is not supported: error at once
  {
    Cia402Axis_t od;
    homingOd(od, 19);
    od.HomingSpeedZero = 0;
    Cia402Axis ax;
    Plant p;
    ax.init(&od);
    step(ax, od, p, 0x0006);
    step(ax, od, p, 0x0007);
    Switch sw;
    CHECK_EQ(runHoming(ax, od, p, sw), 0);
    CHECK(od.Statusword & SW_HOMING_ERROR);
  }
}

static void testLimitSwitches() {
  // csp: the positive limit stops the axis; it follows again when the target comes back
  Cia402Axis_t od;
  resetAxisOd(od);
  od.QuickStopDeceleration = 1000000;
  Cia402Axis ax;
  Plant p;
  ax.init(&od);
  Switch sw;
  sw.edge = 1e9;
  od.TargetPosition = 0;
  enableAxis(ax, od, p);
  int32_t t = 0;
  for (int i = 0; i < 100; i++) {
    t += 10;
    od.TargetPosition = t;
    stepIn(ax, od, p, 0x000F, sw);
  }
  CHECK(fabs(p.pos - t) < 1e-6);
  for (int i = 0; i < 100; i++) {
    t += 10;
    od.TargetPosition = t;
    stepIn(ax, od, p, 0x000F, sw, true);
  }
  double stopped = p.pos;
  CHECK(fabs(p.vel) < 1);
  CHECK(stopped < t - 500);
  CHECK(od.Statusword & SW_INTERNAL_LIMIT);
  CHECK_EQ(od.Statusword & SW_FOLLOWS_COMMAND, 0);
  // The target comes back below where the axis stopped: follows again
  t = (int32_t)stopped - 50;
  od.TargetPosition = t;
  stepIn(ax, od, p, 0x000F, sw, true);
  stepIn(ax, od, p, 0x000F, sw, true);
  CHECK(od.Statusword & SW_FOLLOWS_COMMAND);
  CHECK(fabs(p.pos - t) < 1e-6);

  // csv: motion towards the negative limit is blocked, away from it allowed
  od.ModesOfOperation = CIA402_MODE_CSV;
  od.TargetVelocity = -1000;
  for (int i = 0; i < 10; i++) stepIn(ax, od, p, 0x000F, sw, false, true);
  CHECK(fabs(p.vel) < 1);
  od.TargetVelocity = 1000;
  for (int i = 0; i < 10; i++) stepIn(ax, od, p, 0x000F, sw, false, true);
  CHECK(fabs(p.vel - 1000) < 1);
}

// ---------------------------------------------------------------------------
// PDO packing: the byte-aligned fast path and the bit slicing path
// ---------------------------------------------------------------------------
static void testPdoPacking() {
  COE_initDefaultValues();
  _SMmap map[64];
  int n = 0;
  int bytes = sizeOfPDO(TX_PDO_OBJIDX, &n, map, MAX_MAPPINGS_SM3);
  CHECK_EQ(bytes, 75);
  Obj.Axis[0].Statusword = 0x1237;
  Obj.Axis[0].ModesOfOperationDisplay = -3;
  Obj.Axis[0].PositionActual = -123456789;
  Obj.Axis[3].FollowingErrorActual = 0x01020304;
  Obj.FrequencyIn = 1.5f;
  Obj.DigitalIn = 0xA5;
  alignas(8) uint8_t buf[80];
  memset(buf, 0xEE, sizeof(buf));
  COE_pdoPack(buf, n, map);
  uint16_t sw;
  memcpy(&sw, &buf[0], 2);
  CHECK_EQ(sw, 0x1237);
  CHECK_EQ((int8_t)buf[2], -3);
  int32_t pos;
  memcpy(&pos, &buf[3], 4);
  CHECK_EQ(pos, -123456789);
  int32_t fe;
  memcpy(&fe, &buf[3 * 15 + 11], 4);  // axis 4: 3 x 15 bytes before it, then 2+1+4+4
  CHECK_EQ(fe, 0x01020304);
  float fq;
  memcpy(&fq, &buf[4 * 15 + 1 + 1 + 4 + 4], 4);  // after the encoder inputs
  CHECK(fq == 1.5f);
  CHECK_EQ(buf[74], 0xA5);

  // Unpack the RxPDO
  bytes = sizeOfPDO(RX_PDO_OBJIDX, &n, map, MAX_MAPPINGS_SM2);
  CHECK_EQ(bytes, 54);
  memset(buf, 0, sizeof(buf));
  uint16_t cw = 0x001F;
  memcpy(&buf[11], &cw, 2);  // axis 2 controlword: after axis 1's 2+1+4+4
  int32_t tp = -42;
  memcpy(&buf[11 + 3], &tp, 4);
  buf[53] = 0x0B;  // digital outputs, the last entry
  COE_pdoUnpack(buf, n, map);
  CHECK_EQ(Obj.Axis[1].Controlword, 0x001F);
  CHECK_EQ(Obj.Axis[1].TargetPosition, -42);
  CHECK_EQ(Obj.DigitalOut, 0x0B);

  // An entry that is not byte aligned still takes the bit slicing path
  _SMmap m1 = map[0];
  m1.offset = 4;  // 16 bit controlword at bit 4
  alignas(8) uint8_t b2[8] = {0};
  Obj.Axis[0].Controlword = 0xABCD;
  COE_pdoPack(b2, 1, &m1);
  CHECK_EQ(b2[0], 0xD0);
  CHECK_EQ(b2[1], 0xBC);
  CHECK_EQ(b2[2], 0x0A);
  Obj.Axis[0].Controlword = 0;
  COE_pdoUnpack(b2, 1, &m1);
  CHECK_EQ(Obj.Axis[0].Controlword, 0xABCD);
}

// ---------------------------------------------------------------------------
// An ideal step generator for the loop tests: over one cycle the velocity ramps from
// v0 to v1 at the acceleration limit acc, then stays; the position (fixed point, as
// stepgen_t's accum) moves by the area, rounded to the fixed point.
// ---------------------------------------------------------------------------
static void idealRamp(long long &accum, double v0, double v1, double acc, double dt) {
  double tr = (acc > 0) ? fabs(v1 - v0) / acc : 0.0;
  if (tr > dt) tr = dt;
  const double dist = (v0 + v1) * 0.5 * tr + v1 * (dt - tr);
  accum += llround(dist * (double)(1LL << PICKOFF));
}

static long long idealSteps(long long accum) { return accum >> PICKOFF; }

// ---------------------------------------------------------------------------
// Position loop: stepgenUpdateFreq() (fixed point + float) against LinuxCNC's
// update_freq() in double, both driving the ideal step generator
// ---------------------------------------------------------------------------
struct RefLoop {
  double pos_cmd = 0, old_pos_cmd = 0, vel_cmd = 0, freq = 0, maxvel = 0, maxaccel = 0;
  int pos_mode = 1;
};

static double refUpdateFreq(stepgen_t *sg, RefLoop &r, double dt) {
  double recip_dt = 1.0 / dt;
  double pos_cmd, vel_cmd, curr_pos, curr_vel, avg_v, max_freq, max_ac;
  double match_ac, match_time, est_out, est_cmd, est_err, dp, dv, new_vel;
  if (sg->enable == 0) {
    if (r.pos_mode) r.old_pos_cmd = r.pos_cmd;
    r.freq = 0;
    return 0;
  }
  long min_step_period = sg->step_len + sg->step_space;
  max_freq = 1.0 / (min_step_period * 0.000000001);
  if (r.maxvel <= 0.0) r.maxvel = 0.0;
  else if (r.maxvel > max_freq) r.maxvel = max_freq;
  else max_freq = r.maxvel;
  max_ac = max_freq * recip_dt;
  if (r.maxaccel <= 0.0) r.maxaccel = 0.0;
  else if (r.maxaccel > max_ac) r.maxaccel = max_ac;
  else max_ac = r.maxaccel;
  if (r.pos_mode) {
    pos_cmd = r.pos_cmd;
    vel_cmd = (pos_cmd - r.old_pos_cmd) * recip_dt;
    r.old_pos_cmd = pos_cmd;
    curr_pos = (sg->accum - (1 << (PICKOFF - 1))) * (1.0 / (1L << PICKOFF));
    curr_vel = r.freq;
    match_ac = (vel_cmd > curr_vel) ? max_ac : -max_ac;
    match_time = (vel_cmd - curr_vel) / match_ac;
    avg_v = (vel_cmd + curr_vel) * 0.5;
    est_out = curr_pos + avg_v * match_time;
    est_cmd = pos_cmd + vel_cmd * (match_time - 1.5 * dt);
    est_err = est_out - est_cmd;
    if (match_time < dt) {
      if (fabs(est_err) < 0.0001) {
        new_vel = vel_cmd;
      } else {
        new_vel = vel_cmd - 0.5 * est_err * recip_dt;
        if (new_vel > (curr_vel + max_ac * dt)) new_vel = curr_vel + max_ac * dt;
        else if (new_vel < (curr_vel - max_ac * dt)) new_vel = curr_vel - max_ac * dt;
      }
    } else {
      dv = -2.0 * match_ac * dt;
      dp = dv * match_time;
      if (fabs(est_err + dp * 2.0) < fabs(est_err)) match_ac = -match_ac;
      new_vel = curr_vel + match_ac * dt;
    }
    if (new_vel > max_freq) new_vel = max_freq;
    else if (new_vel < -max_freq) new_vel = -max_freq;
  } else {
    vel_cmd = r.vel_cmd;
    if (vel_cmd > max_freq) vel_cmd = max_freq;
    else if (vel_cmd < -max_freq) vel_cmd = -max_freq;
    dv = max_ac * dt;
    if (vel_cmd > (r.freq + dv)) new_vel = r.freq + dv;
    else if (vel_cmd < (r.freq - dv)) new_vel = r.freq - dv;
    else new_vel = vel_cmd;
  }
  r.freq = new_vel;
  return max_ac;
}

static void testPositionLoop() {
  const double dt = 0.001;    // servo cycle
  StepLoopTiming k = {(float)dt, (float)(1.0 / dt)};
  unsigned rnd = 777;
  auto next = [&rnd]() { rnd = rnd * 1103515245u + 12345u; return (rnd >> 8) & 0xFFFFFF; };

  long long maxDiff = 0, maxLagNew = 0, maxLagRef = 0, cycles = 0;
  for (int run = 0; run < 12; run++) {
    const long long start = (run % 2) ? 50000000LL : -3000LL;  // far from zero too
    stepgen_t a, b;
    memset((void *)&a, 0, sizeof(a));
    a.accum = (start << PICKOFF) + (1LL << (PICKOFF - 1));
    a.enable = 1;
    a.pos_mode = 1;
    a.step_len = 10000;
    a.step_space = 10000;
    memcpy((void *)&b, (const void *)&a, sizeof(a));
    RefLoop r;
    r.pos_cmd = r.old_pos_cmd = (double)start;
    b.pos_cmd = b.old_pos_cmd = stepgenFxFromSteps(start);
    double maxAcc = (run % 3 == 0) ? 0 : 20000.0 + (double)(next() % 2000000);
    double maxVel = (run % 4 == 0) ? 0 : 5000.0 + (double)(next() % 40000);
    r.maxaccel = maxAcc;
    r.maxvel = maxVel;
    double cmd = (double)start, vel = 0;
    for (int c = 0; c < 3000; c++) {
      // Command: smooth moves, sometimes a jump (with resync), csv segments
      int phase = (c / 500) % 3;
      if (c % 100 == 0) vel = (double)((long)(next() % 60000) - 30000);  // steps/s
      if (phase == 2) {
        r.pos_mode = b.pos_mode = 0;
        r.vel_cmd = vel;
        b.vel_cmd = (float)vel;
      } else {
        if (!r.pos_mode) {  // back to position mode: resync, as setChannel() does
          cmd = floor((double)stepgenFxRound(stepgenPosition(&b)));
          r.old_pos_cmd = cmd;
          b.old_pos_cmd = stepgenFxFromSteps((long long)cmd);
        }
        r.pos_mode = b.pos_mode = 1;
        cmd += floor(vel * dt);
        r.pos_cmd = cmd;
        b.pos_cmd = stepgenFxFromSteps((long long)cmd);
      }
      b.maxvel = (float)r.maxvel;
      b.maxaccel = (float)r.maxaccel;
      r.maxvel = maxVel;
      r.maxaccel = maxAcc;
      {
        const double va = r.freq, vb = b.freq;
        const double accA = refUpdateFreq(&a, r, dt);
        stepgenUpdateFreq(&b, k);
        idealRamp(a.accum, va, r.freq, accA, dt);
        idealRamp(b.accum, vb, b.freq, b.accel, dt);
      }
      long long d = llabs(idealSteps(a.accum) - idealSteps(b.accum));
      if (d > maxDiff) maxDiff = d;
      if (r.pos_mode && maxAcc == 0) {  // no accel limit: both should follow closely
        long long lagA = llabs(idealSteps(a.accum) - (long long)cmd);
        long long lagB = llabs(idealSteps(b.accum) - (long long)cmd);
        if (c % 500 > 20) {  // not right after a mode change
          if (lagA > maxLagRef) maxLagRef = lagA;
          if (lagB > maxLagNew) maxLagNew = lagB;
        }
      }
      cycles++;
    }
    // At the end: command still, both settle on it
    for (int c = 0; c < 2000; c++) {
      r.pos_mode = b.pos_mode = 1;
      r.pos_cmd = cmd;
      b.pos_cmd = stepgenFxFromSteps((long long)cmd);
      const double va = r.freq, vb = b.freq;
      const double accA = refUpdateFreq(&a, r, dt);
      stepgenUpdateFreq(&b, k);
      idealRamp(a.accum, va, r.freq, accA, dt);
      idealRamp(b.accum, vb, b.freq, b.accel, dt);
    }
    CHECK_EQ(stepgenFxRound(stepgenPosition(&b)), (long long)cmd);
    CHECK_EQ(stepgenFxRound(stepgenPosition(&a)), (long long)cmd);
  }
  printf("  position loop: %lld cycles, max difference to the double loop %lld steps,"
         " max lag %lld (double) / %lld (new) steps\n",
         cycles, maxDiff, maxLagRef, maxLagNew);
  CHECK(maxDiff <= 1);
  CHECK(maxLagNew <= maxLagRef + 1);
}

// ---------------------------------------------------------------------------
// Cycle time model, diagnostics objects and the frequency limit flag
// ---------------------------------------------------------------------------
static void testCycleDiagnostics() {
  // The model: cycle work plus CPU per step, over all axes
  CHECK_EQ(cycleModelStepLoad(0), 0u);
  CHECK_EQ(cycleModelStepLoad(400000), (uint32_t)(400000ull * CYCLE_STEP_NS / 1000000u));
  CHECK(cycleModelMinCycle(0) >= CYCLE_WORK_NS && cycleModelMinCycle(0) <= 2 * CYCLE_WORK_NS);
  CHECK(cycleModelMinCycle(400000) > cycleModelMinCycle(0));       // 4 x 100 kHz
  CHECK(cycleModelMinCycle(400000) <= 500000);
  CHECK(cycleModelMinCycle(1000000) <= 1000000 || cycleModelMinCycle(1000000) == CYCLE_IMPOSSIBLE);
  CHECK_EQ(cycleModelMinCycle(4000000), CYCLE_IMPOSSIBLE);
  CHECK_EQ(cycleModelMinCycle(0) % 10000, 0u);

  // Objects: 0x1C32/0x1C33 :05, :08, :0A, :0D; 0x2000 (the board) and 0x2010+n :05..:08
  COE_initDefaultValues();
  int32_t nidx = SDO_findobject(0x1C32);
  CHECK(nidx >= 0);
  CHECK(SDO_findsubindex(nidx, 5) >= 0);
  CHECK(SDO_findsubindex(nidx, 8) >= 0);
  CHECK(SDO_findsubindex(nidx, 10) >= 0);
  CHECK(SDO_findsubindex(nidx, 13) >= 0);
  CHECK(SDOobjects[nidx].objdesc[SDO_findsubindex(nidx, 5)].data == &Obj.SmOutParam.MinCycleTime);
  CHECK(SDO_findobject(0x2001) < 0);
  nidx = SDO_findobject(0x2000);
  CHECK(nidx >= 0);
  CHECK_EQ(SDOobjects[nidx].objdesc[0].value, 6);
  int16_t sub = SDO_findsubindex(nidx, 3);
  CHECK(sub >= 0 && SDOobjects[nidx].objdesc[sub].data == &Obj.CycleDiag.CycleUsed);
  CHECK((SDOobjects[nidx].objdesc[SDO_findsubindex(nidx, 1)].flags & ATYPE_TXPDO) != 0);
  CHECK_EQ(sdoWrite(0x2000, 1, 5, 2), ABORT_READONLY);
  nidx = SDO_findobject(0x2012);  // axis 3
  CHECK(nidx >= 0);
  CHECK_EQ(SDOobjects[nidx].objdesc[0].value, 8);
  sub = SDO_findsubindex(nidx, 6);
  CHECK(sub >= 0 && SDOobjects[nidx].objdesc[sub].data == &Obj.StepTiming[2].FrequencyLimited);
  sub = SDO_findsubindex(nidx, 7);
  CHECK(sub >= 0 && SDOobjects[nidx].objdesc[sub].data == &Obj.StepTiming[2].LateSteps);
  CHECK((SDOobjects[nidx].objdesc[SDO_findsubindex(nidx, 5)].flags & ATYPE_TXPDO) != 0);
  // and they can be mapped
  CHECK_EQ(sdoWrite(0x1A04, 0, 0, 1), 0);
  CHECK_EQ(sdoWrite(0x1A04, 7, 0x20000310, 4), 0);  // cycle time used, 16 bit
  CHECK_EQ(sdoWrite(0x1A04, 8, 0x20120708, 4), ABORT_MAPPING_OBJECT_ERROR);  // 32 bit object
  CHECK_EQ(sdoWrite(0x1A04, 8, 0x20120720, 4), 0);  // axis 3 late steps
  CHECK_EQ(sdoWrite(0x1A04, 0, 8, 1), 0);
  _SMmap map[64];
  int n = 0;
  CHECK_EQ(sizeOfPDO(TX_PDO_OBJIDX, &n, map, MAX_MAPPINGS_SM3), 75 + 2 + 4);
  COE_initDefaultValues();

  // Get cycle time: 0..2
  CHECK_EQ(sdoWrite(0x1C32, 8, 2, 2), 0);
  CHECK_EQ(sdoWrite(0x1C33, 8, 1, 2), 0);
  CHECK_EQ(sdoWrite(0x1C32, 8, 3, 2), ABORT_VALUE_EXCEEDED);

  // Step timing: 0 or 1 us .. 200 us
  CHECK_EQ(sdoWrite(0x2010, 1, 0, 4), 0);
  CHECK_EQ(sdoWrite(0x2010, 1, 999, 4), ABORT_VALUE_TOO_LOW);
  CHECK_EQ(sdoWrite(0x2011, 2, 1000, 4), 0);
  CHECK_EQ(sdoWrite(0x2012, 3, 0, 4), 0);
  CHECK_EQ(sdoWrite(0x2012, 3, 10, 4), 0);  // direction setup may be short
  CHECK_EQ(sdoWrite(0x2013, 4, 200001, 4), ABORT_VALUE_TOO_HIGH);
  CHECK_EQ(Obj.StepTiming[0].StepLength, 0u);
  COE_initDefaultValues();
  CHECK_EQ(Obj.StepTiming[3].DirHold, 5000u);

  // The frequency limit flag: set when the command is faster than the step timing
  StepLoopTiming k = {0.001f, 1000.0f};
  stepgen_t g;
  memset((void *)&g, 0, sizeof(g));
  g.enable = 1;
  g.step_len = 10000;
  g.step_space = 10000;  // 50 kHz
  g.vel_cmd = 40000;
  stepgenUpdateFreq(&g, k);
  CHECK_EQ(g.freq_limited, 0);
  g.vel_cmd = -60000;
  stepgenUpdateFreq(&g, k);
  CHECK_EQ(g.freq_limited, 1);
  stepgenUpdateFreq(&g, k);  // the ramp: at most 50 kHz per cycle here
  CHECK(fabsf(g.freq + 50000.0f) < 1.0f);
  // position mode: a command moving faster than 50 kHz
  memset((void *)&g, 0, sizeof(g));
  g.enable = 1;
  g.pos_mode = 1;
  g.step_len = 10000;
  g.step_space = 10000;
  g.accum = 1LL << (PICKOFF - 1);
  // the axis moves at the loop's output, as the step timers make it
  auto cycleAt = [&](int64_t steps) {
    g.pos_cmd = stepgenFxFromSteps(steps);
    stepgenUpdateFreq(&g, k);
    g.accum += (int64_t)((double)g.freq * 0.001 * (double)(1LL << PICKOFF));
  };
  for (int c = 1; c <= 200; c++) cycleAt(40 * c);  // 40 kHz
  CHECK_EQ(g.freq_limited, 0);
  for (int c = 1; c <= 3; c++) cycleAt(8000 + 70 * c);  // 70 kHz: held at 50 kHz
  CHECK_EQ(g.freq_limited, 1);
  CHECK(fabsf(g.freq - 50000.0f) < 1.0f);
  // a single doubled step of the command (a repeated frame) with an acceleration
  // limit: the loop takes it, no flag
  memset((void *)&g, 0, sizeof(g));
  g.enable = 1;
  g.pos_mode = 1;
  g.step_len = 10000;
  g.step_space = 10000;
  g.maxaccel = 2e6f;
  g.accum = 1LL << (PICKOFF - 1);
  int64_t p = 0;
  for (int c = 1; c <= 300; c++) {
    int64_t v = c < 40 ? c : 40;  // ramps to 40 kHz (1e6 steps/s^2)
    p += (c == 200) ? 0 : (c == 201 ? 2 * v : v);  // one repeated frame
    cycleAt(p);
  }
  CHECK_EQ(g.freq_limited, 0);
}

// ---------------------------------------------------------------------------
// Step planner and the timer queue: a simulated timer runs the queue as the hardware
// does; every pulse and direction change is checked against the step timing
// ---------------------------------------------------------------------------
struct TimerSim {
  StepQueue &q;
  const StepTimingTicks &tm;
  int64_t nextUpdate;
  int64_t lastRise = -1000000000, lastDir = -1000000000;
  int32_t dir = 1;
  bool pendingDirCheck = false;
  int64_t dirTime = 0;
  long rises = 0, dirChanges = 0, badPeriods = 0, badSpacing = 0, badHold = 0, badSetup = 0;
  std::vector<int64_t> riseTimes;
  bool record = false;
  TimerSim(StepQueue &qq, const StepTimingTicks &t, uint32_t idle) : q(qq), tm(t), nextUpdate(idle) {}
  void runUntil(int64_t now) {
    while (nextUpdate <= now) {
      const uint32_t act = q.preload;  // becomes active at this update
      uint32_t da;
      stepQueueAdvance(q, &da);
      const int64_t st = nextUpdate;
      const uint32_t len = stepEntryTicks(act), pulse = stepEntryPulse(act);
      if (len < STEP_MIN_PERIOD_TICKS || len > STEP_MAX_PERIOD_TICKS || (pulse && pulse >= len))
        badPeriods++;
      if (da) {
        if (st - lastRise < (int64_t)(tm.len + tm.hold)) badHold++;
        dir = (da == 1) ? 1 : -1;
        dirTime = st;
        pendingDirCheck = true;
        dirChanges++;
      }
      if (pulse) {
        if (st - lastRise < (int64_t)(tm.len + tm.space)) badSpacing++;
        if (pendingDirCheck && st - dirTime < (int64_t)tm.setup) badSetup++;
        pendingDirCheck = false;
        lastRise = st;
        rises++;
        if (record) riseTimes.push_back(st);
      }
      nextUpdate = st + len;
    }
  }
};

static void testStepPlanner() {
  static uint32_t buf[STEP_QUEUE_LEN];
  unsigned rnd = 4242;
  auto next = [&rnd]() { rnd = rnd * 1103515245u + 12345u; return (rnd >> 8) & 0xFFFFFF; };
  long totalSteps = 0, totalRev = 0;
  for (int run = 0; run < 10; run++) {
    // 5 us / 5 us (the default), or 2 / 2 us; direction setup and hold
    StepTimingTicks tm = (run % 2) ? StepTimingTicks{160, 160, 240, 160}
                                   : StepTimingTicks{400, 400, 400, 400};
    const float maxF = (float)STEP_TICK_HZ / (float)(tm.len + tm.space);
    const uint32_t cycles[3] = {80000, 20000, 320000};  // 1 ms, 250 us, 4 ms
    const uint32_t cyc = cycles[run % 3];
    StepQueue q;
    stepQueueInit(q, buf, 8000);
    StepPlanState st;
    stepPlanInit(st, q, 1LL << 27);
    TimerSim sim(q, tm, 8000);
    int64_t now = 20000;
    uint32_t underrunsAfterStart = 0;
    float v = 0;
    for (int c = 0; c < 1500; c++) {
      sim.runUntil(now);
      uint32_t u = q.underrunTicks;
      q.underrunTicks = 0;
      if (c > 2 && u) underrunsAfterStart++;
      stepPlanTakeUnderrun(st, u);
      if (c % (run >= 6 ? 3 : 40) == 0) {  // the last runs: reversals all the time
        int r = (int)(next() % 10);
        v = r == 0 ? 0.0f : ((float)((int)(next() % 2001) - 1000) / 1000.0f) * 0.95f * maxF;
        if (r == 1) v = 3.0f;  // very slow
      }
      float accel = (c % 80 < 40) ? 2e6f : (float)(1e5 + next() % 50000000);
      stepPlanWindow(st, q, tm, v, accel, now + cyc + STEP_LEAD_TICKS);
      now += cyc + (int64_t)(next() % 1600) - 800;  // +-10 us of cycle jitter
    }
    // Stop and drain
    for (int c = 0; c < 400; c++) {
      sim.runUntil(now);
      uint32_t u = q.underrunTicks;
      q.underrunTicks = 0;
      stepPlanTakeUnderrun(st, u);
      stepPlanWindow(st, q, tm, 0.0f, 2e6f, now + cyc + STEP_LEAD_TICKS);
      now += cyc;
    }
    sim.runUntil(now + 10 * (int64_t)cyc);
    CHECK_EQ(sim.badPeriods, 0);
    CHECK_EQ(sim.badSpacing, 0);
    CHECK_EQ(sim.badHold, 0);
    CHECK_EQ(sim.badSetup, 0);
    CHECK_EQ(underrunsAfterStart, 0u);
    CHECK_EQ((int64_t)q.emitted, st.accum >> 28);  // every planned step came out
    CHECK(fabsf(st.vel) < 1.0f);
    totalSteps += sim.rises;
    totalRev += sim.dirChanges;
  }
  printf("  step planner: %ld steps, %ld direction changes\n", totalSteps, totalRev);
  CHECK(totalSteps > 100000);
  CHECK(totalRev > 300);

  // Constant speed: every interval within a tick of the ideal, across window boundaries
  for (int k = 0; k < 3; k++) {
    StepTimingTicks tm = {400, 400, 400, 400};
    const float v = k == 0 ? 33333.3f : (k == 1 ? -77777.0f : 1234.5f);
    StepQueue q;
    stepQueueInit(q, buf, 8000);
    StepPlanState st;
    stepPlanInit(st, q, 1LL << 27);
    TimerSim sim(q, tm, 8000);
    int64_t now = 20000;
    for (int c = 0; c < 300; c++) {
      sim.record = c > 100;
      sim.runUntil(now);
      stepPlanWindow(st, q, tm, v, 1e12f, now + 80000 + STEP_LEAD_TICKS);
      now += 80000 + (int64_t)(next() % 1600) - 800;
    }
    const double ideal = (double)STEP_TICK_HZ / fabs((double)v);
    double worst = 0;
    for (size_t i = 1; i < sim.riseTimes.size(); i++) {
      double d = fabs((double)(sim.riseTimes[i] - sim.riseTimes[i - 1]) - ideal);
      if (d > worst) worst = d;
    }
    printf("  constant %.1f steps/s: %zu steps, worst interval error %.2f ticks\n", v,
           sim.riseTimes.size(), worst);
    CHECK(sim.riseTimes.size() > 100);
    CHECK(worst <= 1.5);
  }

  // A late cycle: the queue runs empty, the interrupt idles, nothing is lost
  {
    StepTimingTicks tm = {400, 400, 400, 400};
    StepQueue q;
    stepQueueInit(q, buf, 8000);
    StepPlanState st;
    stepPlanInit(st, q, 1LL << 27);
    TimerSim sim(q, tm, 8000);
    int64_t now = 20000;
    for (int c = 0; c < 200; c++) {
      sim.runUntil(now);
      uint32_t u = q.underrunTicks;
      q.underrunTicks = 0;
      stepPlanTakeUnderrun(st, u);
      stepPlanWindow(st, q, tm, c < 150 ? 50000.0f : 0.0f, 5e6f, now + 80000 + STEP_LEAD_TICKS);
      now += (c == 60) ? 400000 : 80000;  // one cycle 4 ms late
    }
    sim.runUntil(now + 1000000);
    CHECK(q.underruns > 0);
    CHECK_EQ((int64_t)q.emitted, st.accum >> 28);
    CHECK_EQ(sim.badSpacing, 0);
  }

  // Flush: stops after the preload period, the plan continues from there
  {
    StepTimingTicks tm = {400, 400, 400, 400};
    StepQueue q;
    stepQueueInit(q, buf, 8000);
    StepPlanState st;
    stepPlanInit(st, q, 1LL << 27);
    TimerSim sim(q, tm, 8000);
    int64_t now = 20000;
    for (int c = 0; c < 50; c++) {
      sim.runUntil(now);
      stepPlanWindow(st, q, tm, 60000.0f, 1e12f, now + 80000 + STEP_LEAD_TICKS);
      now += 80000;
    }
    sim.runUntil(now);
    stepQueueFlush(q, st);
    long before = sim.rises;
    sim.runUntil(now + 80000);
    CHECK(sim.rises - before <= 1);
    CHECK_EQ((int64_t)q.emitted, st.accum >> 28);
    for (int c = 0; c < 20; c++) {
      now += 80000;
      sim.runUntil(now);
      uint32_t u = q.underrunTicks;
      q.underrunTicks = 0;
      stepPlanTakeUnderrun(st, u);
      stepPlanWindow(st, q, tm, -20000.0f, 1e12f, now + 80000 + STEP_LEAD_TICKS);
    }
    for (int c = 0; c < 20; c++) {
      now += 80000;
      sim.runUntil(now);
      stepPlanWindow(st, q, tm, 0.0f, 1e12f, now + 80000 + STEP_LEAD_TICKS);
    }
    sim.runUntil(now + 1000000);
    CHECK_EQ((int64_t)q.emitted, st.accum >> 28);
    CHECK_EQ(sim.badSpacing + sim.badHold + sim.badSetup + sim.badPeriods, 0);
  }
}

// ---------------------------------------------------------------------------
// The whole chain on the host: position loop, extrapolation, planner and the
// simulated timer, in csp and csv, as StepTimers::update() runs it
// ---------------------------------------------------------------------------
static void testStepAxisLoop() {
  static uint32_t buf[STEP_QUEUE_LEN];
  unsigned rnd = 999;
  auto next = [&rnd]() { rnd = rnd * 1103515245u + 12345u; return (rnd >> 8) & 0xFFFFFF; };
  long long worstNew = 0, worstOld = 0, worstExtra = 0;
  for (int run = 0; run < 6; run++) {
    const int64_t cycleTicks = (run % 2) ? 40000 : 80000;  // 0.5 / 1 ms
    const float dt = (float)cycleTicks / (float)STEP_TICK_HZ;
    const StepLoopTiming k = {dt, 1.0f / dt};
    StepTimingTicks tm = {400, 400, 400, 400};  // 5 us each, the default
    StepQueue q;
    stepQueueInit(q, buf, 8000);
    StepPlanState ps;
    stepPlanInit(ps, q, 1LL << 27);
    TimerSim sim(q, tm, 8000);
    stepgen_t g;
    memset((void *)&g, 0, sizeof(g));
    g.accum = 1LL << 27;
    g.step_len = 5000;
    g.step_space = 5000;
    StepAxisState ax;
    // For comparison: the same loop driving the ideal step generator, without the lead
    stepgen_t o;
    memcpy((void *)&o, (const void *)&g, sizeof(g));
    // Command: a trajectory with an acceleration limit below the step generators'
    const double trajAcc = 1e6, genAcc = (run < 2) ? 0.0 : 2e6;
    double cmd = 0, v = 0, vel = 0;
    int64_t now = 20000;
    for (int c = 0; c < 4000; c++) {
      sim.runUntil(now);
      uint32_t u = q.underrunTicks;
      q.underrunTicks = 0;
      stepPlanTakeUnderrun(ps, u);
      if (c > 5 && u) CHECK_EQ(u, 0u);
      if (c % 50 == 0) vel = (double)((long)(next() % 160000) - 80000);  // up to 80 kHz
      if (c % 400 > 300) vel = 0;                                         // and stand still
      if (vel > v + trajAcc * dt) v += trajAcc * dt;
      else if (vel < v - trajAcc * dt) v -= trajAcc * dt;
      else v = vel;
      cmd += v * dt;
      const int64_t pc = stepgenFxFromSteps((long long)floor(cmd));
      g.pos_mode = o.pos_mode = 1;
      if (c == 3) g.old_pos_cmd = o.old_pos_cmd = pc;
      g.pos_cmd = o.pos_cmd = pc;
      g.maxaccel = o.maxaccel = (float)genAcc;
      g.enable = o.enable = c >= 3;
      // Lag behind the command, both at this cycle's start (the ideal one moves below)
      if (c > 10) {
        long long lagNew = llabs((long long)q.emitted - (long long)floor(cmd));
        long long lagOld = llabs(idealSteps(o.accum) - (long long)floor(cmd));
        if (lagNew > worstNew) worstNew = lagNew;
        if (lagOld > worstOld) worstOld = lagOld;
        long long extra = llabs((long long)q.emitted - idealSteps(o.accum));
        if (extra > worstExtra) worstExtra = extra;
      }
      stepAxisCycle(&g, ax, ps, q, tm, k, now, cycleTicks, 250000.0f);
      {
        const double v0 = o.freq;
        stepgenUpdateFreq(&o, k);
        idealRamp(o.accum, v0, o.freq, o.accel, dt);
      }
      now += cycleTicks + (int64_t)(next() % 1600) - 800;
    }
    // Stand still: the axis ends exactly on the command
    long long target = (long long)floor(cmd);
    for (int c = 0; c < 1500; c++) {
      sim.runUntil(now);
      uint32_t u = q.underrunTicks;
      q.underrunTicks = 0;
      stepPlanTakeUnderrun(ps, u);
      g.pos_cmd = stepgenFxFromSteps(target);
      stepAxisCycle(&g, ax, ps, q, tm, k, now, cycleTicks, 250000.0f);
      now += cycleTicks;
    }
    sim.runUntil(now + 10 * cycleTicks);
    CHECK_EQ((long long)q.emitted, target);
    CHECK_EQ((int64_t)q.emitted, ps.accum >> 28);
    CHECK_EQ(sim.badSpacing + sim.badHold + sim.badSetup + sim.badPeriods, 0);
  }
  printf("  step axis loop: worst lag %lld steps (ideal %lld), worst difference to the ideal %lld\n",
         worstNew, worstOld, worstExtra);
  CHECK(worstNew <= worstOld + 20);
}

int main() {
  testStepPlanner();
  testStepAxisLoop();
  testPositionLoop();
  testPdoPacking();
  testHomingMethods();
  testHomingCurrentPosition();
  testHomingErrors();
  testLimitSwitches();
  testStateMachine();
  testCspFollowsAndWraps();
  testMissedFrameExtrapolation();
  testCsvAndPolarity();
  testQuickStop();
  testFollowingErrorFaultAndReset();
  testCommunicationLoss();
  testUnsupportedMode();
  testObjectDictionary();
  testCycleDiagnostics();
  printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}

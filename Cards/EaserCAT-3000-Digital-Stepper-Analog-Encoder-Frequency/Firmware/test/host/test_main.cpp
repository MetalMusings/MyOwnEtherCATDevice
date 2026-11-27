// Host unit tests for the CiA402 state machine and the generated object dictionary.
// Build and run with test/host/run_tests.sh (needs only g++/gcc).

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "Cia402Axis.h"
#include "CycleModel.h"
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
  uint32_t abort = ObjectHooks_preDownload(index, sub, &value, size, 0);
  if (abort) return abort;
  int32_t nidx = SDO_findobject(index);
  if (nidx < 0) return ABORT_NOOBJECT;
  int16_t nsub = SDO_findsubindex(nidx, sub);
  if (nsub < 0) return ABORT_NOSUBINDEX;
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
  CHECK_EQ(Obj.BasePeriod, 10000);
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
  CHECK_EQ(sdoWrite(0x2000, 0, 1000, 4), ABORT_VALUE_EXCEEDED);
  CHECK_EQ(sdoWrite(0x2000, 0, 20000, 4), 0);
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
// Step pulses: stepgenPulseStepDir() against the original make_pulses() algorithm
// ---------------------------------------------------------------------------
static void refPulse(stepgen_t *stepgen, long periodns) {
  long old_addval, target_addval, new_addval, step_now;
  if (stepgen->timer1 > 0) {
    if (stepgen->timer1 > (unsigned)periodns) stepgen->timer1 -= periodns;
    else stepgen->timer1 = 0;
  }
  if (stepgen->timer2 > 0) {
    if (stepgen->timer2 > (unsigned)periodns) stepgen->timer2 -= periodns;
    else stepgen->timer2 = 0;
  }
  if (stepgen->timer3 > 0) {
    if (stepgen->timer3 > (unsigned)periodns) stepgen->timer3 -= periodns;
    else {
      stepgen->timer3 = 0;
      stepgen->hold_dds = 0;
    }
  }
  if (!stepgen->hold_dds && stepgen->enable) {
    old_addval = stepgen->addval;
    target_addval = stepgen->target_addval;
    if (stepgen->deltalim != 0) {
      if (target_addval > (old_addval + stepgen->deltalim)) new_addval = old_addval + stepgen->deltalim;
      else if (target_addval < (old_addval - stepgen->deltalim)) new_addval = old_addval - stepgen->deltalim;
      else new_addval = target_addval;
    } else {
      new_addval = target_addval;
    }
    stepgen->addval = new_addval;
    if (((new_addval >= 0) && (old_addval < 0)) || ((new_addval < 0) && (old_addval >= 0))) {
      if (stepgen->timer3 != 0) stepgen->hold_dds = 1;
    }
  }
  if (!stepgen->hold_dds && stepgen->enable) {
    step_now = stepgen->accum;
    stepgen->accum += stepgen->addval;
    step_now ^= stepgen->accum;
    step_now &= (1L << PICKOFF);
    stepgen->rawcount = stepgen->accum >> PICKOFF;
  } else {
    step_now = 0;
  }
  if (stepgen->timer2 == 0) {
    if (stepgen->addval > 0) stepgen->curr_dir = 1;
    else if (stepgen->addval < 0) stepgen->curr_dir = -1;
  }
  if (step_now) {
    stepgen->timer1 = stepgen->step_len;
    stepgen->timer2 = stepgen->timer1 + stepgen->dir_hold_dly;
    stepgen->timer3 = stepgen->timer2 + stepgen->dir_setup;
  }
  stepgen->phase[STEP_PIN] = stepgen->timer1 != 0;
  stepgen->phase[DIR_PIN] = stepgen->curr_dir < 0;
}

static bool sameChannel(const stepgen_t &a, const stepgen_t &b) {
  return a.timer1 == b.timer1 && a.timer2 == b.timer2 && a.timer3 == b.timer3 &&
         a.hold_dds == b.hold_dds && a.addval == b.addval && a.accum == b.accum &&
         a.rawcount == b.rawcount && a.curr_dir == b.curr_dir &&
         a.phase[STEP_PIN] == b.phase[STEP_PIN] && a.phase[DIR_PIN] == b.phase[DIR_PIN];
}

static void testStepPulses() {
  const long period = 10000;
  unsigned rnd = 12345;
  auto next = [&rnd]() { rnd = rnd * 1103515245u + 12345u; return (rnd >> 8) & 0xFFFFFF; };
  int mismatches = 0, steps = 0, reversals = 0;
  for (int run = 0; run < 20; run++) {
    stepgen_t a, b;
    memset((void *)&a, 0, sizeof(a));
    a.accum = 1 << (PICKOFF - 1);
    a.enable = 1;
    a.step_len = 20000;      // 2 base periods
    a.dir_hold_dly = 10000;  // and other step timing, to exercise the hold logic
    a.dir_setup = 30000;
    a.deltalim = (run % 3 == 0) ? 0 : (long)(next() % 2000000);
    memcpy((void *)&b, (const void *)&a, sizeof(a));
    for (int k = 0; k < 20000; k++) {
      if (k % 100 == 0) {  // a new frequency command each "servo cycle"
        long f = (next() & 1) ? (long)(next() % 16000000) - 8000000    // slow, many reversals
                               : (long)next() * 8 - 67108864;          // up to 0.25 steps/period
        if (next() % 7 == 0) f = 0;
        a.target_addval = b.target_addval = f;
        if (next() % 50 == 0) a.enable = b.enable = !a.enable;
      }
      int oldDir = a.curr_dir;
      refPulse(&a, period);
      stepgenPulseStepDir(&b, period);
      if (!sameChannel(a, b)) mismatches++;
      if (a.timer1 == (unsigned)a.step_len) steps++;
      if (a.curr_dir != oldDir) reversals++;
    }
  }
  CHECK_EQ(mismatches, 0);
  CHECK(steps > 1000);
  printf("  step pulses: %d steps, %d direction changes\n", steps, reversals);
  CHECK(reversals > 100);
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
// Position loop: stepgenUpdateFreq() (fixed point + float) against LinuxCNC's
// update_freq() in double, both driving the pulse core
// ---------------------------------------------------------------------------
struct RefLoop {
  double pos_cmd = 0, old_pos_cmd = 0, vel_cmd = 0, freq = 0, maxvel = 0, maxaccel = 0;
  int pos_mode = 1;
};

static void refUpdateFreq(stepgen_t *sg, RefLoop &r, double dt, double freqscale,
                          double accelscale) {
  double recip_dt = 1.0 / dt;
  double pos_cmd, vel_cmd, curr_pos, curr_vel, avg_v, max_freq, max_ac;
  double match_ac, match_time, est_out, est_cmd, est_err, dp, dv, new_vel;
  if (sg->enable == 0) {
    if (r.pos_mode) r.old_pos_cmd = r.pos_cmd;
    r.freq = 0;
    sg->addval = 0;
    sg->target_addval = 0;
    return;
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
  sg->target_addval = r.freq * freqscale;
  sg->deltalim = max_ac * accelscale;
}

static void testPositionLoop() {
  const long period = 10000;  // base period, ns
  const double dt = 0.001;    // servo cycle
  const int pulsesPerCycle = 100;
  const double freqscale = (1L << PICKOFF) * (period * 1e-9);
  const double accelscale = freqscale * (period * 1e-9);
  StepLoopTiming k = {(float)dt, (float)(1.0 / dt), (float)freqscale, (float)accelscale};
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
    a.dir_hold_dly = 10000;
    a.dir_setup = 10000;
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
      refUpdateFreq(&a, r, dt, freqscale, accelscale);
      stepgenUpdateFreq(&b, k);
      for (int i = 0; i < pulsesPerCycle; i++) {
        stepgenPulseStepDir(&a, period);
        stepgenPulseStepDir(&b, period);
      }
      long long d = llabs((long long)a.rawcount - (long long)b.rawcount);
      if (d > maxDiff) maxDiff = d;
      if (r.pos_mode && maxAcc == 0) {  // no accel limit: both should follow closely
        long long lagA = llabs((long long)a.rawcount - (long long)cmd);
        long long lagB = llabs((long long)b.rawcount - (long long)cmd);
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
      refUpdateFreq(&a, r, dt, freqscale, accelscale);
      stepgenUpdateFreq(&b, k);
      for (int i = 0; i < pulsesPerCycle; i++) {
        stepgenPulseStepDir(&a, period);
        stepgenPulseStepDir(&b, period);
      }
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
  // The model against the OP measurements (four axes, 160 MHz): 10 us -> 231 us measured,
  // 7 us -> 380 us; 7 us fits 0.5 ms, 6 us fits 1 ms but not 0.5 ms, 5.5 us nothing
  CHECK(cycleModelMinCycle(10000) >= 231000 && cycleModelMinCycle(10000) <= 300000);
  CHECK(cycleModelMinCycle(7000) >= 380000 && cycleModelMinCycle(7000) <= 500000);
  CHECK(cycleModelMinCycle(6000) > 500000 && cycleModelMinCycle(6000) <= 1000000);
  CHECK_EQ(cycleModelMinCycle(5500), CYCLE_IMPOSSIBLE);
  CHECK_EQ(cycleModelMinCycle(5000), CYCLE_IMPOSSIBLE);
  CHECK(cycleModelMinCycle(200000) == 400000);  // at least two base periods
  CHECK(cycleModelMinBasePeriod() > 5500 && cycleModelMinBasePeriod() <= 6000);
  CHECK_EQ(cycleModelIsrLoad(9800), 500);
  // Step rate: length and space at least one base period each, rounded up
  CHECK_EQ(cycleModelMaxStepFrequency(10000, 0, 0), 50000);
  CHECK_EQ(cycleModelMaxStepFrequency(10000, 2500, 0), 50000);
  CHECK_EQ(cycleModelMaxStepFrequency(10000, 15000, 5000), 33333);
  CHECK_EQ(cycleModelMaxStepFrequency(5000, 5000, 5000), 100000);

  // Objects: 0x1C32/0x1C33 :05, :08, :0A, :0D, and 0x2001
  COE_initDefaultValues();
  int32_t nidx = SDO_findobject(0x1C32);
  CHECK(nidx >= 0);
  CHECK(SDO_findsubindex(nidx, 5) >= 0);
  CHECK(SDO_findsubindex(nidx, 8) >= 0);
  CHECK(SDO_findsubindex(nidx, 10) >= 0);
  CHECK(SDO_findsubindex(nidx, 13) >= 0);
  CHECK(SDOobjects[nidx].objdesc[SDO_findsubindex(nidx, 5)].data == &Obj.SmOutParam.MinCycleTime);
  nidx = SDO_findobject(0x2001);
  CHECK(nidx >= 0);
  CHECK_EQ(SDOobjects[nidx].objdesc[0].value, 10);
  int16_t sub = SDO_findsubindex(nidx, 10);
  CHECK(sub >= 0 && SDOobjects[nidx].objdesc[sub].data == &Obj.CycleDiag.FrequencyLimited);
  CHECK((SDOobjects[nidx].objdesc[SDO_findsubindex(nidx, 4)].flags & ATYPE_TXPDO) != 0);
  // and it can be mapped
  CHECK_EQ(sdoWrite(0x1A04, 0, 0, 1), 0);
  CHECK_EQ(sdoWrite(0x1A04, 7, 0x20010610, 4), 0);  // cycle time used, 16 bit
  CHECK_EQ(sdoWrite(0x1A04, 0, 7, 1), 0);
  _SMmap map[64];
  int n = 0;
  CHECK_EQ(sizeOfPDO(TX_PDO_OBJIDX, &n, map, MAX_MAPPINGS_SM3), 75 + 2);
  COE_initDefaultValues();

  // Get cycle time: 0..2
  CHECK_EQ(sdoWrite(0x1C32, 8, 2, 2), 0);
  CHECK_EQ(sdoWrite(0x1C33, 8, 1, 2), 0);
  CHECK_EQ(sdoWrite(0x1C32, 8, 3, 2), ABORT_VALUE_EXCEEDED);

  // Base period: range always; on SYNC0 not shorter than the cycle time allows
  Obj.SmOutParam.SyncType = 1;
  CHECK_EQ(sdoWrite(0x2000, 0, 6000, 4), 0);
  CHECK_EQ(sdoWrite(0x2000, 0, 4000, 4), ABORT_VALUE_EXCEEDED);
  Obj.SmOutParam.SyncType = 2;
  Obj.SmOutParam.CycleTime = 500000;
  ESCvar.ALstatus = ESCpreop;  // back in PREOP the old values do not count
  CHECK_EQ(sdoWrite(0x2000, 0, 6000, 4), 0);
  ESCvar.ALstatus = ESCop;
  CHECK_EQ(sdoWrite(0x2000, 0, 7000, 4), 0);
  CHECK_EQ(sdoWrite(0x2000, 0, 6000, 4), ABORT_VALUE_TOO_LOW);
  Obj.SmOutParam.CycleTime = 1000000;
  CHECK_EQ(sdoWrite(0x2000, 0, 6000, 4), 0);
  CHECK_EQ(sdoWrite(0x2000, 0, 5500, 4), ABORT_VALUE_TOO_LOW);
  Obj.SmOutParam.SyncType = 0;
  Obj.SmOutParam.CycleTime = 0;
  ESCvar.ALstatus = 0;
  COE_initDefaultValues();

  // The frequency limit flag: set when the command is faster than the step timing
  StepLoopTiming k = {0.001f, 1000.0f, (float)((1L << PICKOFF) * 1e-5),
                      (float)((1L << PICKOFF) * 1e-10)};
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
  for (int c = 1; c <= 3; c++) {
    g.pos_cmd = stepgenFxFromSteps(40 * c);  // 40 kHz
    stepgenUpdateFreq(&g, k);
  }
  CHECK_EQ(g.freq_limited, 0);
  g.pos_cmd = stepgenFxFromSteps(120 + 70);  // 70 kHz
  stepgenUpdateFreq(&g, k);
  CHECK_EQ(g.freq_limited, 1);
}

int main() {
  testPositionLoop();
  testStepPulses();
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

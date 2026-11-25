// Host unit tests for the CiA402 state machine and the generated object dictionary.
// Build and run with test/host/run_tests.sh (needs only g++/gcc).

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "Cia402Axis.h"
#include "ObjectHooks.h"

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
      v = (c.position - pos) / DT;
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
  MotionFeedback fb() const { return {pos, vel}; }
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
  CHECK(fabs(c.position - 55.0) < 1e-9);
  // Next real frame jumps to the real target, which is consistent again
  od.TargetPosition = 60;
  c = step(ax, od, p, 0x000F);
  CHECK(fabs(c.position - 60.0) < 1e-9);
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
  MotionFeedback stalled = {0.0, 0.0};
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
  CHECK_EQ(sizeOfPDO(RX_PDO_OBJIDX, &n, map, MAX_MAPPINGS_SM2), 57);
  CHECK_EQ(n, 4 * 4 + 1 + 3);
  CHECK_EQ(sizeOfPDO(TX_PDO_OBJIDX, &n, map, MAX_MAPPINGS_SM3), 74);
  CHECK_EQ(n, 4 * 5 + 1 + 4);

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
  CHECK_EQ(SDOobjects[nidx].objdesc[0].value, 0x180);

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
  CHECK_EQ(sizeOfPDO(TX_PDO_OBJIDX, &n, map, MAX_MAPPINGS_SM3), 74 - 4 + 4);

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
  CHECK_EQ(sdoWrite(0x607E, 0, 0x80, 1), 0);
  CHECK_EQ(sdoWrite(0x607E, 0, 0x01, 1), ABORT_VALUE_EXCEEDED);
  CHECK_EQ(sdoWrite(0x2000, 0, 1000, 4), ABORT_VALUE_EXCEEDED);
  CHECK_EQ(sdoWrite(0x2000, 0, 20000, 4), 0);
}

int main() {
  testStateMachine();
  testCspFollowsAndWraps();
  testMissedFrameExtrapolation();
  testCsvAndPolarity();
  testQuickStop();
  testFollowingErrorFaultAndReset();
  testCommunicationLoss();
  testUnsupportedMode();
  testObjectDictionary();
  printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}

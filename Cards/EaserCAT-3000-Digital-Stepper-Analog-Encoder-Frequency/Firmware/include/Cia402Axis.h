// CiA402 drive state machine (FSA) and cyclic synchronous modes (csp, csv) for one
// axis, implemented according to ETG.6010 (CiA402 Implementation Directive).
//
// The class is hardware independent: it reads/writes the CiA402 objects of one axis
// (struct Cia402Axis_t from utypes.h) and produces a MotionCommand for the step
// generator. This makes it possible to unit test it on a host computer.
//
// Units (no factor group, ETG.6010 table 56): position in steps (inc), velocity in
// steps/s, acceleration in steps/s^2.

#ifndef CIA402AXIS_H
#define CIA402AXIS_H

#include <stdint.h>
extern "C" {
#include "utypes.h"
}

enum class Cia402State : uint8_t {
  NotReadyToSwitchOn,
  SwitchOnDisabled,
  ReadyToSwitchOn,
  SwitchedOn,
  OperationEnabled,
  QuickStopActive,
  FaultReactionActive,
  Fault
};

// Modes of operation (object 0x6060)
static const int8_t CIA402_MODE_NONE = 0;
static const int8_t CIA402_MODE_CSP = 8;
static const int8_t CIA402_MODE_CSV = 9;

// Error codes (object 0x603F)
static const uint16_t CIA402_ERR_NONE = 0x0000;
static const uint16_t CIA402_ERR_FOLLOWING_ERROR = 0x8611;
static const uint16_t CIA402_ERR_COMMUNICATION = 0x8100;

// Statusword bits (object 0x6041)
static const uint16_t SW_READY_TO_SWITCH_ON = 1u << 0;
static const uint16_t SW_SWITCHED_ON = 1u << 1;
static const uint16_t SW_OPERATION_ENABLED = 1u << 2;
static const uint16_t SW_FAULT = 1u << 3;
static const uint16_t SW_VOLTAGE_ENABLED = 1u << 4;
static const uint16_t SW_QUICK_STOP = 1u << 5;
static const uint16_t SW_SWITCH_ON_DISABLED = 1u << 6;
static const uint16_t SW_WARNING = 1u << 7;
static const uint16_t SW_REMOTE = 1u << 9;
static const uint16_t SW_FOLLOWS_COMMAND = 1u << 12;     // csp/csv mode specific
static const uint16_t SW_FOLLOWING_ERROR = 1u << 13;     // csp mode specific

// Position feedback from the step generator, in internal (motor) direction.
struct MotionFeedback {
  double position;  // steps
  double velocity;  // steps/s
};

// What the step generator shall do during the next cycle, internal direction.
struct MotionCommand {
  bool enable;          // false: no step pulses at all
  bool positionMode;    // true: follow position, false: follow velocity
  double position;      // steps, used when positionMode
  double velocity;      // steps/s, used when !positionMode
  double maxVelocity;   // steps/s, 0 = only limited by step timing
  double maxAccel;      // steps/s^2, 0 = unlimited
  bool resync;          // drop velocity feed forward this cycle (mode change, enable)
};

class Cia402Axis {
 public:
  void init(Cia402Axis_t *od);

  // Call once per cycle.
  //   fb           : step generator feedback
  //   dtSeconds    : cycle time
  //   outputsValid : the master's outputs (RxPDO) are valid, i.e. ESM is in OP
  //   newOutputs   : the RxPDO has been updated during this cycle
  MotionCommand cycle(const MotionFeedback &fb, double dtSeconds, bool outputsValid,
                      bool newOutputs);

  // ESM left OP (watchdog, master request, error). Reacts according to 0x6007.
  void communicationLost();

  Cia402State state() const { return fsaState; }
  bool faulted() const {
    return fsaState == Cia402State::Fault || fsaState == Cia402State::FaultReactionActive;
  }

 private:
  enum class StopKind : uint8_t { None, QuickStop, FaultReaction };

  void processControlword(uint16_t cw);
  void enterFault(uint16_t errorCode);
  void startStop(StopKind kind, int16_t optionCode);
  void enterOperationEnabled();
  void updateStatusword(bool following, bool followingErrorActive);
  double rampDeceleration(int16_t optionCode) const;
  int64_t unwrapTarget(int32_t raw) const;

  Cia402Axis_t *od = nullptr;
  Cia402State fsaState = Cia402State::NotReadyToSwitchOn;
  StopKind stopKind = StopKind::None;
  double stopDecel = 0.0;          // deceleration used by the active stop ramp
  bool stopDisableAtEnd = true;    // quick stop: go to Switch on disabled when stopped
  bool lastFaultResetBit = false;
  bool pendingCommLost = false;
  int8_t activeMode = CIA402_MODE_NONE;
  int64_t actual = 0;              // position actual value, external direction, unwrapped
  int64_t target = 0;              // target position, external direction, unwrapped
  int64_t receivedTarget = 0;      // last target position received from the master
  int32_t lastRawTarget = 0;
  int64_t lastTargetDelta = 0;     // for extrapolation when a frame is missed
  uint8_t missedFrames = 0;
  bool resyncNext = true;
  double followingErrorTime = 0.0; // seconds the following error has been outside window
};

#endif

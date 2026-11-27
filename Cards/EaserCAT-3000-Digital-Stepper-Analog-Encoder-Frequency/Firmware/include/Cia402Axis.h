// CiA402 drive state machine (FSA), cyclic synchronous modes (csp, csv) and homing
// mode (hm, methods 19..22, 35, 37) for one axis, implemented according to ETG.6010
// (CiA402 Implementation Directive) and IEC 61800-7-201.
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

#include "StepGenLoop.h"  // fixed point positions
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
static const int8_t CIA402_MODE_HOMING = 6;
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
static const uint16_t SW_TARGET_REACHED = 1u << 10;      // homing: stopped (done, not started or interrupted)
static const uint16_t SW_INTERNAL_LIMIT = 1u << 11;      // a limit switch is active
static const uint16_t SW_FOLLOWS_COMMAND = 1u << 12;     // csp/csv mode specific
static const uint16_t SW_HOMING_ATTAINED = 1u << 12;     // homing mode specific
static const uint16_t SW_FOLLOWING_ERROR = 1u << 13;     // csp mode specific
static const uint16_t SW_HOMING_ERROR = 1u << 13;        // homing mode specific

// Controlword bits (object 0x6040), besides the state machine commands
static const uint16_t CW_HOMING_START = 1u << 4;         // homing mode: rising edge starts
static const uint16_t CW_HALT = 1u << 8;

// The axis' switches this cycle, after the input polarity (0x2106) and filter (0x2107).
struct AxisInputs {
  bool home = false;               // home switch active
  bool posLimit = false;           // positive limit switch active
  bool negLimit = false;           // negative limit switch active
  bool homeChanged = false;        // the home switch changed state since the last cycle
  int64_t homeChangePosition = 0;  // step generator position latched at that change (fixed point)
};

// Position feedback from the step generator, in internal (motor) direction.
struct MotionFeedback {
  int64_t position;  // fixed point steps (StepGenLoop.h)
  float velocity;    // steps/s
};

// What the step generator shall do during the next cycle, internal direction.
struct MotionCommand {
  bool enable;          // false: no step pulses at all
  bool positionMode;    // true: follow position, false: follow velocity
  int64_t position;     // fixed point steps, used when positionMode
  float velocity;       // steps/s, used when !positionMode
  float maxVelocity;    // steps/s, 0 = only limited by step timing
  float maxAccel;       // steps/s^2, 0 = unlimited
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
  //   in           : the axis' switches
  MotionCommand cycle(const MotionFeedback &fb, float dtSeconds, bool outputsValid,
                      bool newOutputs, const AxisInputs &in = AxisInputs());

  // ESM left OP (watchdog, master request, error). Reacts according to 0x6007.
  void communicationLost();

  Cia402State state() const { return fsaState; }
  bool faulted() const {
    return fsaState == Cia402State::Fault || fsaState == Cia402State::FaultReactionActive;
  }
  bool homingAttained() const { return homed; }

 private:
  enum class StopKind : uint8_t { None, QuickStop, FaultReaction };

  void processControlword(uint16_t cw);
  void enterFault(uint16_t errorCode);
  void startStop(StopKind kind, int16_t optionCode);
  void enterOperationEnabled();
  void updateStatusword(bool following, bool followingErrorActive, uint16_t extraBits);
  // Homing mode
  enum class HomingPhase : uint8_t { Idle, Moving, Braking, Done, Failed };
  enum class BrakeReason : uint8_t { NextMove, Home, Interrupt, Error };
  struct HomingMove {
    int8_t dir;         // +1 / -1, in position (external) direction
    bool fast;          // 0x6099:01, else 0x6099:02
    bool untilActive;   // move until the home switch is active, else until inactive
  };
  void startHoming(const AxisInputs &in, int posSign, const MotionFeedback &fb);
  void brakeHoming(BrakeReason why);
  void homingCycle(const MotionFeedback &fb, float dt, const AxisInputs &in,
                   int posSign, bool startBit, bool halt, MotionCommand &cmd);
  bool homingActive() const {
    return homingPhase == HomingPhase::Moving || homingPhase == HomingPhase::Braking;
  }
  float rampDeceleration(int16_t optionCode) const;
  int64_t unwrapTarget(int32_t raw) const;

  Cia402Axis_t *od = nullptr;
  Cia402State fsaState = Cia402State::NotReadyToSwitchOn;
  StopKind stopKind = StopKind::None;
  float stopDecel = 0.0f;          // deceleration used by the active stop ramp
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
  float followingErrorTime = 0.0f; // seconds the following error has been outside window
  int64_t homeShift = 0;           // added to the step position to give the position actual value
  bool homed = false;              // homing attained (kept until the next homing starts)
  bool homingFailed = false;       // homing error
  bool lastHomingStart = false;    // controlword bit 4 last cycle
  HomingPhase homingPhase = HomingPhase::Idle;
  BrakeReason brakeReason = BrakeReason::Interrupt;
  HomingMove moves[3] = {};
  uint8_t nMoves = 0, moveIdx = 0;
  int64_t homeEdge = 0;            // step position of the home switch edge (fixed point)
  float homingElapsed = 0.0f;      // seconds since homing started
  bool cspSyncWait = false;        // csp after a mode change: wait until the target is near
  bool limitHold = false;          // csp: stopped by a limit switch, waiting for the target to come back
  int8_t holdDir = 0;
};

#endif

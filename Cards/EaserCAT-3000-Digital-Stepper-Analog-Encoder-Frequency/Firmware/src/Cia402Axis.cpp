// CiA402 drive state machine for one axis. See Cia402Axis.h and ETG.6010.

#include "Cia402Axis.h"

#include <math.h>

namespace {

// Controlword command decoding (CiA402 / IEC 61800-7-201, bits 7,3,2,1,0)
inline bool cmdShutdown(uint16_t cw) { return (cw & 0x0087) == 0x0006; }
inline bool cmdSwitchOn(uint16_t cw) { return (cw & 0x008F) == 0x0007; }
inline bool cmdSwitchOnEnable(uint16_t cw) { return (cw & 0x008F) == 0x000F; }
inline bool cmdDisableVoltage(uint16_t cw) { return (cw & 0x0082) == 0x0000; }
inline bool cmdQuickStop(uint16_t cw) { return (cw & 0x0086) == 0x0002; }
inline bool cwFaultReset(uint16_t cw) { return (cw & 0x0080) != 0; }

const double STOPPED_VELOCITY = 0.5;  // steps/s, below this the axis is considered stopped
// csp after a mode change (e.g. the end of homing, where the position has just been
// re-referenced) follows the target only once it is this close to the actual position,
// so that a target still in the old frame does not make the axis jump.
const int64_t CSP_SYNC_STEPS = 10;

inline int32_t clampToInt32(int64_t v) {
  if (v > INT32_MAX) return INT32_MAX;
  if (v < INT32_MIN) return INT32_MIN;
  return (int32_t)v;
}

}  // namespace

void Cia402Axis::init(Cia402Axis_t *axisOd) {
  od = axisOd;
  // The "Not ready to switch on" state is left automatically after initialisation.
  fsaState = Cia402State::SwitchOnDisabled;
  stopKind = StopKind::None;
  activeMode = CIA402_MODE_NONE;
  resyncNext = true;
  od->ErrorCode = CIA402_ERR_NONE;
  od->ModesOfOperationDisplay = CIA402_MODE_NONE;
  homingPhase = HomingPhase::Idle;
  homed = false;
  homingFailed = false;
  homeShift = 0;
  limitHold = false;
  updateStatusword(false, false, 0);
}

// ---------------------------------------------------------------------------
// Homing (mode 6). Methods 19..22: home switch without index pulse, as in
// IEC 61800-7-201 and the A6/ECT manuals; 35 and 37: the current position.
//   19: switch active on the positive side, home = edge where it goes off moving negative
//   20: switch active on the positive side, home = edge where it comes on moving positive
//   21, 22: the same with the switch on the negative side
// The search for the switch runs at 0x6099:01, every move that ends at the home edge
// at 0x6099:02. The axis brakes at the switch with the quick stop deceleration
// (0x6085), as the ECT drives do, accelerates with 0x609A, and is homed where it
// stands: the edge gets the position 0x607C.
// ---------------------------------------------------------------------------

void Cia402Axis::startHoming(const AxisInputs &in, double posSign, const MotionFeedback &fb) {
  homed = false;
  homingFailed = false;
  homingElapsed = 0.0;
  moveIdx = 0;
  nMoves = 0;
  int8_t method = od->HomingMethod;

  if (method == 35 || method == 37) {  // the current position is home
    homeShift = (int64_t)od->HomeOffset - (int64_t)llround(posSign * fb.position);
    homed = true;
    homingPhase = HomingPhase::Done;
    return;
  }
  if (method < 19 || method > 22 || od->HomingSpeedSwitch == 0 || od->HomingSpeedZero == 0) {
    homingFailed = true;
    homingPhase = HomingPhase::Failed;
    return;
  }
  int8_t toSwitch = (method == 19 || method == 20) ? 1 : -1;  // where the switch is active
  bool edgeComingOn = (method == 20 || method == 22);
  if (!edgeComingOn) {
    // 19, 21: into the switch (fast), out of it (slow): home where it goes off
    if (!in.home) moves[nMoves++] = {toSwitch, true, true};
    moves[nMoves++] = {(int8_t)-toSwitch, false, false};
  } else {
    // 20, 22: into the switch (fast), back out (slow), in again (slow): home where it comes on
    if (!in.home) moves[nMoves++] = {toSwitch, true, true};
    moves[nMoves++] = {(int8_t)-toSwitch, false, false};
    moves[nMoves++] = {toSwitch, false, true};
  }
  homingPhase = HomingPhase::Moving;
}

void Cia402Axis::brakeHoming(BrakeReason why) {
  brakeReason = why;
  homingPhase = HomingPhase::Braking;
}

void Cia402Axis::homingCycle(const MotionFeedback &fb, double dt, const AxisInputs &in,
                             double posSign, bool startBit, bool halt, MotionCommand &cmd) {
  bool startEdge = startBit && !lastHomingStart;
  if (startEdge && !halt && !homingActive()) startHoming(in, posSign, fb);
  else if (homingActive() && (!startBit || halt) && homingPhase == HomingPhase::Moving)
    brakeHoming(BrakeReason::Interrupt);  // bit 4 back to 0, or halt: decelerate and stop

  double accel = (double)od->HomingAcceleration;
  double quick = od->QuickStopDeceleration ? (double)od->QuickStopDeceleration : accel;

  if (homingPhase == HomingPhase::Moving) {
    homingElapsed += dt;
    // A limit switch ends homing with an error, unless it is the home switch itself
    bool posLim = in.posLimit && od->PosLimitInput != od->HomeInput;
    bool negLim = in.negLimit && od->NegLimitInput != od->HomeInput;
    const HomingMove &m = moves[moveIdx];
    if (od->HomingTimeLimit != 0 && homingElapsed * 1000.0 > (double)od->HomingTimeLimit) {
      homingFailed = true;
      brakeHoming(BrakeReason::Error);
    } else if (posLim || negLim) {
      homingFailed = true;
      brakeHoming(BrakeReason::Error);
    } else if (in.home == m.untilActive) {
      if (moveIdx + 1 >= nMoves) {
        homeEdge = in.homeChanged ? in.homeChangePosition : fb.position;
        brakeHoming(BrakeReason::Home);
      } else {
        brakeHoming(BrakeReason::NextMove);
      }
    } else {
      double speed = (double)(m.fast ? od->HomingSpeedSwitch : od->HomingSpeedZero);
      if (cmd.maxVelocity > 0.0 && speed > cmd.maxVelocity) speed = cmd.maxVelocity;
      cmd.velocity = posSign * m.dir * speed;
      cmd.maxAccel = accel;
      return;
    }
  }

  if (homingPhase == HomingPhase::Braking) {
    cmd.velocity = 0.0;
    cmd.maxAccel = (brakeReason == BrakeReason::Interrupt) ? accel : quick;
    if (cmd.maxAccel <= 0.0 || fabs(fb.velocity) < STOPPED_VELOCITY) {
      switch (brakeReason) {
        case BrakeReason::NextMove:
          moveIdx++;
          homingPhase = HomingPhase::Moving;
          break;
        case BrakeReason::Home:
          homeShift = (int64_t)od->HomeOffset - (int64_t)llround(posSign * homeEdge);
          homed = true;
          homingPhase = HomingPhase::Done;
          break;
        case BrakeReason::Interrupt:
          homingPhase = HomingPhase::Idle;
          break;
        case BrakeReason::Error:
          homingPhase = HomingPhase::Failed;
          break;
      }
    }
    return;
  }

  // Not homing: stand still
  cmd.velocity = 0.0;
  cmd.maxAccel = accel;
}

void Cia402Axis::communicationLost() { pendingCommLost = true; }

int64_t Cia402Axis::unwrapTarget(int32_t raw) const {
  // Choose the 64 bit target closest to the actual position. Makes the 32 bit target
  // position wrap around correctly ("endless" operation).
  return actual + (int32_t)((uint32_t)raw - (uint32_t)actual);
}

double Cia402Axis::rampDeceleration(int16_t optionCode) const {
  // 1, 5: slow down ramp (max acceleration, falls back to quick stop deceleration)
  // 2, 6: quick stop ramp (0x6085)
  if ((optionCode == 1 || optionCode == 5) && od->MaxAcceleration != 0)
    return (double)od->MaxAcceleration;
  return (double)od->QuickStopDeceleration;
}

void Cia402Axis::startStop(StopKind kind, int16_t optionCode) {
  stopKind = kind;
  if (kind == StopKind::QuickStop) {
    fsaState = Cia402State::QuickStopActive;
    stopDisableAtEnd = !(optionCode == 5 || optionCode == 6);
  } else {
    fsaState = Cia402State::FaultReactionActive;
    stopDisableAtEnd = true;
  }
  // Option code 0 (and an unset deceleration) means: disable drive immediately.
  stopDecel = (optionCode == 0) ? 0.0 : rampDeceleration(optionCode);
}

void Cia402Axis::enterFault(uint16_t errorCode) {
  if (faulted()) return;
  od->ErrorCode = errorCode;
  if (fsaState == Cia402State::OperationEnabled || fsaState == Cia402State::QuickStopActive) {
    startStop(StopKind::FaultReaction, od->FaultReactionOption);
  } else {
    fsaState = Cia402State::Fault;
    stopKind = StopKind::None;
  }
}

void Cia402Axis::enterOperationEnabled() {
  fsaState = Cia402State::OperationEnabled;
  cspSyncWait = false;
  stopKind = StopKind::None;
  resyncNext = true;
  followingErrorTime = 0.0;
}

void Cia402Axis::processControlword(uint16_t cw) {
  bool resetEdge = cwFaultReset(cw) && !lastFaultResetBit;
  lastFaultResetBit = cwFaultReset(cw);

  switch (fsaState) {
    case Cia402State::NotReadyToSwitchOn:
      fsaState = Cia402State::SwitchOnDisabled;  // transition 1, automatic
      break;
    case Cia402State::SwitchOnDisabled:
      if (cmdShutdown(cw)) fsaState = Cia402State::ReadyToSwitchOn;  // 2
      break;
    case Cia402State::ReadyToSwitchOn:
      if (cmdDisableVoltage(cw) || cmdQuickStop(cw))
        fsaState = Cia402State::SwitchOnDisabled;  // 7
      else if (cmdSwitchOn(cw))
        fsaState = Cia402State::SwitchedOn;  // 3
      else if (cmdSwitchOnEnable(cw))
        enterOperationEnabled();  // 3 + 4 in one command (ETG.6010 5.1)
      break;
    case Cia402State::SwitchedOn:
      if (cmdDisableVoltage(cw) || cmdQuickStop(cw))
        fsaState = Cia402State::SwitchOnDisabled;  // 10
      else if (cmdShutdown(cw))
        fsaState = Cia402State::ReadyToSwitchOn;  // 6
      else if (cmdSwitchOnEnable(cw))
        enterOperationEnabled();  // 4
      break;
    case Cia402State::OperationEnabled:
      if (cmdDisableVoltage(cw))
        fsaState = Cia402State::SwitchOnDisabled;  // 9
      else if (cmdQuickStop(cw))
        startStop(StopKind::QuickStop, od->QuickStopOption);  // 11
      else if (cmdShutdown(cw))
        fsaState = Cia402State::ReadyToSwitchOn;  // 8
      else if (cmdSwitchOn(cw))
        fsaState = Cia402State::SwitchedOn;  // 5
      break;
    case Cia402State::QuickStopActive:
      if (cmdDisableVoltage(cw)) {
        fsaState = Cia402State::SwitchOnDisabled;  // 12
        stopKind = StopKind::None;
      } else if (!stopDisableAtEnd && cmdSwitchOnEnable(cw)) {
        enterOperationEnabled();  // 16, only for quick stop option codes 5..8
      }
      break;
    case Cia402State::FaultReactionActive:
      break;  // transition 14 is taken when the stop ramp has finished
    case Cia402State::Fault:
      if (resetEdge) {  // 15
        fsaState = Cia402State::SwitchOnDisabled;
        od->ErrorCode = CIA402_ERR_NONE;
      }
      break;
  }
}

MotionCommand Cia402Axis::cycle(const MotionFeedback &fb, double dt, bool outputsValid,
                                bool newOutputs, const AxisInputs &in) {
  const double posSign = (od->Polarity & 0x80) ? -1.0 : 1.0;  // 0x607E bit 7
  const double velSign = (od->Polarity & 0x40) ? -1.0 : 1.0;  // 0x607E bit 6

  actual = (int64_t)llround(posSign * fb.position) + homeShift;

  if (pendingCommLost) {
    pendingCommLost = false;
    // Only an axis with power enabled reacts. An axis already disabled by the master
    // (e.g. normal shutdown, or at power up) stays where it is.
    bool powered = fsaState == Cia402State::SwitchedOn ||
                   fsaState == Cia402State::OperationEnabled ||
                   fsaState == Cia402State::QuickStopActive;
    switch (powered ? od->AbortConnectionOption : 0) {
      case 0:  // No action
        break;
      case 2:  // Disable voltage
        if (!faulted()) {
          fsaState = Cia402State::SwitchOnDisabled;
          stopKind = StopKind::None;
        }
        break;
      case 3:  // Quick stop
        if (fsaState == Cia402State::OperationEnabled)
          startStop(StopKind::QuickStop, od->QuickStopOption);
        else if (!faulted())
          fsaState = Cia402State::SwitchOnDisabled;
        break;
      case 1:  // Fault signal (default)
      default:
        enterFault(CIA402_ERR_COMMUNICATION);
        break;
    }
  }

  // The controlword is only evaluated while the master's outputs are valid.
  if (outputsValid) processControlword(od->Controlword);
  bool homingStartBit = outputsValid && (od->Controlword & CW_HOMING_START);
  bool haltBit = outputsValid && (od->Controlword & CW_HALT);

  // Modes of operation: only supported modes are accepted, otherwise "no mode".
  int8_t requested = od->ModesOfOperation;
  int8_t newMode = (requested == CIA402_MODE_CSP || requested == CIA402_MODE_CSV ||
                    requested == CIA402_MODE_HOMING)
                       ? requested
                       : CIA402_MODE_NONE;
  if (newMode != activeMode) {
    cspSyncWait = newMode == CIA402_MODE_CSP && fsaState == Cia402State::OperationEnabled;
    activeMode = newMode;
    resyncNext = true;
    limitHold = false;
  }
  // Homing stops when the mode changes or the axis leaves "Operation enabled"
  if (homingActive() &&
      (activeMode != CIA402_MODE_HOMING || fsaState != Cia402State::OperationEnabled))
    homingPhase = HomingPhase::Idle;
  od->ModesOfOperationDisplay = activeMode;

  MotionCommand cmd = {};
  cmd.maxVelocity = (double)od->MaxProfileVelocity;
  cmd.maxAccel = (double)od->MaxAcceleration;
  cmd.position = fb.position;
  cmd.positionMode = false;

  bool following = false;
  bool followingErrorActive = false;
  uint16_t extraBits = 0;
  if (in.posLimit || in.negLimit) extraBits |= SW_INTERNAL_LIMIT;
  int64_t followingError = 0;
  int64_t demand = actual;

  switch (fsaState) {
    case Cia402State::OperationEnabled: {
      cmd.enable = true;
      if (activeMode == CIA402_MODE_CSP && cspSyncWait) {
        int64_t t = unwrapTarget(od->TargetPosition);
        followingError = t - actual;
        if (llabs(t - actual) <= CSP_SYNC_STEPS) {
          cspSyncWait = false;  // follows from the next cycle, resynchronised
        }
        cmd.velocity = 0.0;  // hold
        break;               // resyncNext stays set
      }
      if (activeMode == CIA402_MODE_CSP) {
        int32_t raw = od->TargetPosition;
        if (resyncNext) {
          receivedTarget = unwrapTarget(raw);
          lastTargetDelta = 0;
          missedFrames = 0;
          target = receivedTarget;
        } else if (newOutputs || !outputsValid) {
          lastTargetDelta = (int64_t)(int32_t)((uint32_t)raw - (uint32_t)lastRawTarget);
          receivedTarget += lastTargetDelta;
          missedFrames = 0;
          target = receivedTarget;
        } else if (missedFrames < 3) {
          // Missed frame: extrapolate with the last delta to avoid a hard stop. The next
          // received target is used as is again.
          missedFrames++;
          target = receivedTarget + lastTargetDelta * missedFrames;
        }
        lastRawTarget = raw;
        followingError = target - actual;

        // A limit switch stops the axis (quick stop deceleration) when the target
        // moves further towards it. It follows again once the target is back on the
        // free side of where it stopped.
        if (!limitHold && ((in.posLimit && target > actual) || (in.negLimit && target < actual))) {
          limitHold = true;
          holdDir = target > actual ? 1 : -1;
        } else if (limitHold && ((holdDir > 0 && target <= actual) ||
                                 (holdDir < 0 && target >= actual))) {
          limitHold = false;
          resyncNext = true;
        }
        if (limitHold) {
          cmd.positionMode = false;
          cmd.velocity = 0.0;
          if (od->QuickStopDeceleration) cmd.maxAccel = (double)od->QuickStopDeceleration;
          followingErrorTime = 0.0;
          break;  // resyncNext stays as it is: it is set on release
        }

        cmd.positionMode = true;
        cmd.position = posSign * (double)(target - homeShift);
        cmd.resync = resyncNext;
        demand = target;
        following = true;

        // Following error supervision (0x6065, 0x6066). 0xFFFFFFFF switches it off.
        uint32_t window = od->FollowingErrorWindow;
        if (window != 0xFFFFFFFFu && (uint64_t)llabs(followingError) > window) {
          followingErrorActive = true;
          followingErrorTime += dt;
          if (followingErrorTime * 1000.0 > (double)od->FollowingErrorTimeout)
            enterFault(CIA402_ERR_FOLLOWING_ERROR);
        } else {
          followingErrorTime = 0.0;
        }
      } else if (activeMode == CIA402_MODE_CSV) {
        double v = velSign * (double)od->TargetVelocity;
        if (cmd.maxVelocity > 0.0) {
          if (v > cmd.maxVelocity) v = cmd.maxVelocity;
          if (v < -cmd.maxVelocity) v = -cmd.maxVelocity;
        }
        // A limit switch blocks motion towards it (quick stop deceleration)
        if ((in.posLimit && posSign * v > 0.0) || (in.negLimit && posSign * v < 0.0)) {
          v = 0.0;
          if (od->QuickStopDeceleration) cmd.maxAccel = (double)od->QuickStopDeceleration;
        }
        cmd.velocity = v;
        cmd.resync = resyncNext;
        following = true;
      } else if (activeMode == CIA402_MODE_HOMING) {
        homingCycle(fb, dt, in, posSign, homingStartBit, haltBit, cmd);
        actual = (int64_t)llround(posSign * fb.position) + homeShift;  // homeShift may be new
        demand = actual;
      } else {
        // No valid mode: hold the axis (decelerate to standstill).
        cmd.velocity = 0.0;
      }
      resyncNext = false;
      break;
    }
    case Cia402State::QuickStopActive:
    case Cia402State::FaultReactionActive: {
      if (stopDecel <= 0.0 || fabs(fb.velocity) < STOPPED_VELOCITY) {
        // Ramp finished (or immediate stop requested)
        if (fsaState == Cia402State::FaultReactionActive) {
          fsaState = Cia402State::Fault;  // 14
          stopKind = StopKind::None;
          cmd.enable = false;
        } else if (stopDisableAtEnd) {
          fsaState = Cia402State::SwitchOnDisabled;  // 12
          stopKind = StopKind::None;
          cmd.enable = false;
        } else {
          cmd.enable = true;  // option 5/6: stay in quick stop active, holding position
          cmd.velocity = 0.0;
          cmd.maxAccel = stopDecel;
        }
      } else {
        cmd.enable = true;
        cmd.velocity = 0.0;
        cmd.maxAccel = stopDecel;
      }
      resyncNext = true;
      break;
    }
    default:
      cmd.enable = false;  // no torque: no step pulses
      resyncNext = true;
      followingErrorTime = 0.0;
      break;
  }

  if (activeMode == CIA402_MODE_HOMING) {
    if (!homingActive()) extraBits |= SW_TARGET_REACHED;
    if (homed) extraBits |= SW_HOMING_ATTAINED;
    if (homingFailed) extraBits |= SW_HOMING_ERROR;
  }
  lastHomingStart = homingStartBit;

  od->PositionActual = (int32_t)(uint32_t)(uint64_t)actual;
  od->PositionDemand = (int32_t)(uint32_t)(uint64_t)demand;
  od->VelocityActual = (int32_t)lround(velSign * fb.velocity);
  od->FollowingErrorActual = clampToInt32(followingError);
  updateStatusword(following && fsaState == Cia402State::OperationEnabled,
                   followingErrorActive, extraBits);
  return cmd;
}

void Cia402Axis::updateStatusword(bool following, bool followingErrorActive,
                                  uint16_t extraBits) {
  uint16_t sw = SW_REMOTE | extraBits;
  switch (fsaState) {
    case Cia402State::NotReadyToSwitchOn:
      break;
    case Cia402State::SwitchOnDisabled:
      sw |= SW_SWITCH_ON_DISABLED;
      break;
    case Cia402State::ReadyToSwitchOn:
      sw |= SW_READY_TO_SWITCH_ON | SW_QUICK_STOP | SW_VOLTAGE_ENABLED;
      break;
    case Cia402State::SwitchedOn:
      sw |= SW_READY_TO_SWITCH_ON | SW_SWITCHED_ON | SW_QUICK_STOP | SW_VOLTAGE_ENABLED;
      break;
    case Cia402State::OperationEnabled:
      sw |= SW_READY_TO_SWITCH_ON | SW_SWITCHED_ON | SW_OPERATION_ENABLED | SW_QUICK_STOP |
            SW_VOLTAGE_ENABLED;
      break;
    case Cia402State::QuickStopActive:
      sw |= SW_READY_TO_SWITCH_ON | SW_SWITCHED_ON | SW_OPERATION_ENABLED | SW_VOLTAGE_ENABLED;
      break;
    case Cia402State::FaultReactionActive:
      sw |= SW_READY_TO_SWITCH_ON | SW_SWITCHED_ON | SW_OPERATION_ENABLED | SW_FAULT |
            SW_VOLTAGE_ENABLED;
      break;
    case Cia402State::Fault:
      sw |= SW_FAULT;
      break;
  }
  if (following) sw |= SW_FOLLOWS_COMMAND;
  if (followingErrorActive) sw |= SW_FOLLOWING_ERROR | SW_WARNING;
  od->Statusword = sw;
}

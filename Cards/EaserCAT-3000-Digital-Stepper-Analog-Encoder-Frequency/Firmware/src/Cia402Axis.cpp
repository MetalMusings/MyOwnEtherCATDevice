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
  updateStatusword(false, false);
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
                                bool newOutputs) {
  const double posSign = (od->Polarity & 0x80) ? -1.0 : 1.0;  // 0x607E bit 7
  const double velSign = (od->Polarity & 0x40) ? -1.0 : 1.0;  // 0x607E bit 6

  actual = (int64_t)llround(posSign * fb.position);

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

  // Modes of operation: only supported modes are accepted, otherwise "no mode".
  int8_t requested = od->ModesOfOperation;
  int8_t newMode =
      (requested == CIA402_MODE_CSP || requested == CIA402_MODE_CSV) ? requested
                                                                       : CIA402_MODE_NONE;
  if (newMode != activeMode) {
    activeMode = newMode;
    resyncNext = true;
  }
  od->ModesOfOperationDisplay = activeMode;

  MotionCommand cmd = {};
  cmd.maxVelocity = (double)od->MaxProfileVelocity;
  cmd.maxAccel = (double)od->MaxAcceleration;
  cmd.position = fb.position;
  cmd.positionMode = false;

  bool following = false;
  bool followingErrorActive = false;
  int64_t followingError = 0;
  int64_t demand = actual;

  switch (fsaState) {
    case Cia402State::OperationEnabled: {
      cmd.enable = true;
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
        cmd.positionMode = true;
        cmd.position = posSign * (double)target;
        cmd.resync = resyncNext;
        demand = target;
        followingError = target - actual;
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
        cmd.velocity = v;
        cmd.resync = resyncNext;
        following = true;
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

  od->PositionActual = (int32_t)(uint32_t)(uint64_t)actual;
  od->PositionDemand = (int32_t)(uint32_t)(uint64_t)demand;
  od->VelocityActual = (int32_t)lround(velSign * fb.velocity);
  od->FollowingErrorActual = clampToInt32(followingError);
  updateStatusword(following && fsaState == Cia402State::OperationEnabled,
                   followingErrorActive);
  return cmd;
}

void Cia402Axis::updateStatusword(bool following, bool followingErrorActive) {
  uint16_t sw = SW_REMOTE;
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

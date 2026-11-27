// The state of one step generator channel's position/velocity loop (LinuxCNC stepgen,
// StepGenLoop.h). Kept free of Arduino headers so it can be tested on the host.
#ifndef STEPGENCHANNEL_H
#define STEPGENCHANNEL_H

#define PICKOFF 28 /* fraction bits of a position */

#include <stdint.h>

// Positions are fixed point in steps with PICKOFF fraction bits, velocities and
// accelerations float.

typedef struct
{
    long long accum;           /* position where the queue ends: steps << PICKOFF, plus half a step */
    int enable;                /* the axis is enabled */
    int pos_mode;              /* 1 = position mode, 0 = velocity mode */
    int step_len;              /* step pulse length, ns */
    unsigned int step_space;   /* min step pulse spacing, ns */
    int64_t old_pos_cmd;       /* previous position command (fixed point steps) */
    /* no scale: positions are in steps, the EtherCAT master scales */
    float vel_cmd;             /* velocity command (steps/sec) */
    int64_t pos_cmd;           /* position command (fixed point steps) */
    float freq;                /* frequency command (steps/sec) */
    float maxvel;              /* max velocity (steps/sec), 0 = step timing only */
    float maxaccel;            /* max accel (steps/sec^2), 0 = unlimited */
    int freq_limited;          /* set when held at the step rate limit; cleared by the user */
    float freq_cap;            /* further limit of the step rate (steps/sec), 0 = none */
    float accel;               /* the acceleration limit used in the last cycle (steps/sec^2) */
} stepgen_t;

#endif

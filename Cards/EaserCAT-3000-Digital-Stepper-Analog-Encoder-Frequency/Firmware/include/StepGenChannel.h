// One step generator channel (LinuxCNC stepgen) and its pulse function for step/dir
// output. Kept free of Arduino headers so it can be tested on the host.
#ifndef STEPGENCHANNEL_H
#define STEPGENCHANNEL_H

#define STEP_PIN 0 /* output phase used for STEP signal */
#define DIR_PIN 1  /* output phase used for DIR signal */
#define PICKOFF 28 /* bit location in DDS accum */

#include <stdint.h>

// Positions are fixed point in steps with PICKOFF fraction bits (the accumulator's
// format without its half step offset), velocities and accelerations float.

typedef struct
{
    /* stuff that is both read and written by makepulses */
    volatile unsigned int timer1; /* times out when step pulse should end */
    volatile unsigned int timer2; /* times out when safe to change dir */
    volatile unsigned int timer3; /* times out when safe to step in new dir */
    volatile int hold_dds;        /* prevents accumulator from updating */
    volatile long addval;         /* actual frequency generator add value */
    volatile long long accum;     /* frequency generator accumulator */
    volatile int rawcount;        /* param: position feedback in counts */
    volatile int curr_dir;        /* current direction */
    volatile int state;           /* current position in state table */
    /* stuff that is read but not written by makepulses */
    volatile int enable;                /* pin for enable stepgen */
    volatile int target_addval;         /* desired freq generator add value */
    volatile long deltalim;             /* max allowed change per period */
    volatile int step_len;              /* parameter: step pulse length */
    volatile unsigned int dir_hold_dly; /* param: direction hold time or delay */
    volatile unsigned int dir_setup;    /* param: direction setup time */
    volatile int step_type;             /* stepping type - see list above */
    volatile int cycle_max;             /* cycle length for step types 2 and up */
    volatile int num_phases;            /* number of phases for types 2 and up */
    volatile int phase[5];              /* pins for output signals */
    volatile const unsigned char *lut;  /* pointer to state lookup table */
                                        /* stuff that is not accessed by makepulses */
    int pos_mode;                       /* 1 = position mode, 0 = velocity mode */
    unsigned int step_space;            /* parameter: min step pulse spacing */
    int64_t old_pos_cmd;                /* previous position command (fixed point steps) */
    int count;                          /* pin: captured feedback in counts */
    /* no scale: positions are in steps, the EtherCAT master scales */
    float vel_cmd;             /* velocity command (steps/sec) */
    int64_t pos_cmd;           /* position command (fixed point steps) */
    int64_t pos_fb;            /* position feedback (fixed point steps) */
    float freq;                /* frequency command (steps/sec) */
    float maxvel;              /* max velocity (steps/sec), 0 = step timing only */
    float maxaccel;            /* max accel (steps/sec^2), 0 = unlimited */
    int freq_limited;          /* set when held at the step timing limit; cleared by the user */
    float freq_cap;            /* further limit of the step rate (steps/sec), 0 = none */
    float accel;               /* the acceleration limit used in the last cycle (steps/sec^2) */
    unsigned int old_step_len; /* used to detect parameter changes */
    unsigned int old_step_space;
    unsigned int old_dir_hold_dly;
    unsigned int old_dir_setup;
    int printed_error; /* flag to avoid repeated printing */
} stepgen_t;

// One base period of a step/dir channel (step type 0). The same algorithm as
// StepGen3::make_pulses(), but on local copies: the volatile fields are read and
// written once each instead of at every use, and the other step types are left out.
static inline void stepgenPulseStepDir(stepgen_t *sg, unsigned int periodns)
{
    unsigned int t1 = sg->timer1, t2 = sg->timer2, t3 = sg->timer3;
    int hold = sg->hold_dds;
    const int enable = sg->enable;
    long addval = sg->addval;
    bool stepNow = false;

    /* decrement the timing constraint timers */
    if (t1) t1 = t1 > periodns ? t1 - periodns : 0;
    if (t2) t2 = t2 > periodns ? t2 - periodns : 0;
    if (t3)
    {
        if (t3 > periodns)
            t3 -= periodns;
        else
        {
            t3 = 0;
            hold = 0; /* last timer timed out, cancel hold */
        }
    }
    if (!hold && enable)
    {
        /* update addval (ramping) */
        const long oldAddval = addval;
        const long target = sg->target_addval;
        const long dl = sg->deltalim;
        long nv = target;
        if (dl != 0)
        {
            if (target > oldAddval + dl)
                nv = oldAddval + dl;
            else if (target < oldAddval - dl)
                nv = oldAddval - dl;
        }
        addval = nv;
        /* direction reversal: hold everything until the delays have timed out */
        if (((nv >= 0) && (oldAddval < 0)) || ((nv < 0) && (oldAddval >= 0)))
        {
            if (t3 != 0)
                hold = 1;
        }
    }
    if (!hold && enable)
    {
        /* update DDS, a step when the pickoff bit toggles */
        const long long acc = sg->accum;
        const long long na = acc + addval;
        stepNow = ((acc ^ na) & (1LL << PICKOFF)) != 0;
        sg->accum = na;
        sg->rawcount = (int)(na >> PICKOFF);
    }
    int dir = sg->curr_dir;
    if (t2 == 0)
    {
        /* update direction - do not change if addval = 0 */
        if (addval > 0)
            dir = 1;
        else if (addval < 0)
            dir = -1;
    }
    if (stepNow)
    {
        t1 = sg->step_len;           /* time till end of step pulse */
        t2 = t1 + sg->dir_hold_dly;  /* time till allowed to change dir pin */
        t3 = t2 + sg->dir_setup;     /* time till allowed to step the other way */
    }
    sg->timer1 = t1;
    sg->timer2 = t2;
    sg->timer3 = t3;
    sg->hold_dds = hold;
    sg->addval = addval;
    sg->curr_dir = dir;
    sg->phase[STEP_PIN] = t1 != 0;
    sg->phase[DIR_PIN] = dir < 0;
}

#endif

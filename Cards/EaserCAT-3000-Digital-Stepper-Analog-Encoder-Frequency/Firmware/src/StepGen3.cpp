#include <Arduino.h>
#include <stdio.h>
#include "StepGen3.h"
#include "extend32to64.h"

extern "C"
{
#include "esc.h"
}
extern HardwareSerial Serial1;
////////////////////////////////////////////////
/********************************************************************
 * Description:  stepgen.c
 *               This file, 'stepgen.c', is a HAL component that
 *               provides software based step pulse generation.
 *
 * Author: John Kasunich
 * License: GPL Version 2
 *
 * Copyright (c) 2003-2007 All rights reserved.
 *
 * Last change:
 ********************************************************************/
/** This file, 'stepgen.c', is a HAL component that provides software
    based step pulse generation.  The maximum step rate will depend
    on the speed of the PC, but is expected to exceed 5KHz for even
    the slowest computers, and may reach 25KHz on fast ones.  It is
    a realtime component.

    It supports up to 16 pulse generators.  Each generator can produce
    several types of outputs in addition to step/dir, including
    quadrature, half- and full-step unipolar and bipolar, three phase,
    and five phase.  A 32 bit feedback value is provided indicating
    the current position of the motor in counts (assuming no lost
    steps), and a floating point feedback in user specified position
    units is also provided.

    The number of step generators and type of outputs is determined
    by the insmod command line parameter 'step_type'.  It accepts
    a comma separated (no spaces) list of up to 16 stepping types
    to configure up to 16 channels.  A second command line parameter
    "ctrl_type", selects between position and velocity control modes
    for each step generator.  (ctrl_type is optional, the default
    control type is position.)

    So a command line like this:

    insmod stepgen step_type=0,0,1,2  ctrl_type=p,p,v,p

    will install four step generators, two using stepping type 0,
    one using type 1, and one using type 2.  The first two and
    the last one will be running in position mode, and the third
    one will be running in velocity mode.

    The driver exports three functions.  'stepgen.make-pulses', is
    responsible for actually generating the step pulses.  It must
    be executed in a fast thread to reduce pulse jitter.  The other
    two functions are normally called from a much slower thread.
    'stepgen.update-freq' reads the position or frequency command
    and sets internal variables used by 'stepgen.make-pulses'.
    'stepgen.capture-position' captures and scales the current
    values of the position feedback counters.  Both 'update-freq' and
    'capture-position' use floating point, 'make-pulses' does not.

    Polarity:

    All signals from this module have fixed polarity (active high
    in most cases).  If the driver needs the opposite polarity,
    the signals can be inverted using parameters exported by the
    hardware driver(s) such as ParPort.

    Timing parameters:

    There are five timing parameters which control the output waveform.
    No step type uses all five, and only those which will be used are
    exported to HAL.  The values of these parameters are in nano-seconds,
    so no recalculation is needed when changing thread periods.  In
    the timing diagrams that follow, they are identified by the
    following numbers:

    (1): 'stepgen.n.steplen' = length of the step pulse
    (2): 'stepgen.n.stepspace' = minimum space between step pulses
      (actual space depends on frequency command, and is infinite
      if the frequency command is zero)
    (3): 'stepgen.n.dirhold' = minimum delay after a step pulse before
      a direction change - may be longer
    (4): 'stepgen.n.dirsetup' = minimum delay after a direction change
      and before the next step - may be longer
    (5): 'stepgen.n.dirdelay' = minimum delay after a step before a
     step in the opposite direction - may be longer

    Stepping Types:

    This module supports a number of stepping types, as follows:

    Type 0:  Step and Direction
               _____         _____               _____
    STEP  ____/     \_______/     \_____________/     \______
              |     |       |     |             |     |
    Time      |-(1)-|--(2)--|-(1)-|--(3)--|-(4)-|-(1)-|
                                          |__________________
    DIR   ________________________________/

    There are two output pins, STEP and DIR.  Step pulses appear on
    STEP.  A positive frequency command results in DIR low, negative
    frequency command means DIR high.  The minimum period of the
    step pulses is 'steplen' + 'stepspace', and the frequency
    command is clamped to avoid exceeding these limits.  'steplen'
    and 'stepspace' must both be non-zero.  'dirsetup' or 'dirhold'
    may be zero, but their sum must be non-zero, to ensure non-zero
    low time between the last up step and the first down step.

    Type 1:  Up/Down
             _____       _____
    UP    __/     \_____/     \________________________________
            |     |     |     |         |
    Time    |-(1)-|-(2)-|-(1)-|---(5)---|-(1)-|-(2)-|-(1)-|
                                        |_____|     |_____|
    DOWN  ______________________________/     \_____/     \____


    There are two output pins, UP and DOWN.  A positive frequency
    command results in pulses on UP, negative frequency command
    results in pulses on DOWN.  The minimum period of the step
    pulses is 'steplen' + 'stepspace', and the frequency command
    is clamped to avoid exceeding these limits.  'steplen',
    'stepspace', and 'dirdelay' must all be non-zero.

    Types 2 and higher:  State Patterns

    STATE   |---1---|---2---|---3---|----4----|---3---|---2---|
            |       |       |       |         |       |       |
    Time    |--(1)--|--(1)--|--(1)--|--(1+5)--|--(1)--|--(1)--|

    All the remaining stepping types are simply different repeating
    patterns on two to five output pins.  When a step occurs, the
    output pins change to the next (or previous) pattern in the
    state listings that follow.  The output pins are called 'PhaseA'
    thru 'PhaseE'.  Timing constraints are obeyed as indicated
    in the drawing above.  'steplen' must be non-zero.  'dirdelay'
    may be zero.  Because stepspace is not used, state based
    stepping types can run faster than types 0 and 1.

    Type 2:  Quadrature (aka Gray/Grey code)

    State   Phase A   Phase B
      0        1        0
      1        1        1
      2        0        1
      3        0        0
      0        1        0

    Type 3:  Three Wire

    State   Phase A   Phase B   Phase C
      0        1        0         0
      1        0        1         0
      2        0        0         1
      0        1        0         0

    Type 4:  Three Wire HalfStep

    State   Phase A   Phase B   Phase C
      0        1        0         0
      1        1        1         0
      2        0        1         0
      3        0        1         1
      4        0        0         1
      5        1        0         1
      0        1        0         0

    Type 5:  Unipolar Full Step (one winding on)

    State   Phase A   Phase B   Phase C   Phase D
      0        1        0         0         0
      1        0        1         0         0
      2        0        0         1         0
      3        0        0         0         1
      0        1        0         0         0

    Type 6:  Unipolar Full Step (two windings on)

    State   Phase A   Phase B   Phase C   Phase D
      0        1        1         0         0
      1        0        1         1         0
      2        0        0         1         1
      3        1        0         0         1
      0        1        1         0         0

    Type 7:  Bipolar Full Step (one winding on)

    State   Phase A   Phase B   Phase C   Phase D
      0        1        0         0         0
      1        1        1         1         0
      2        0        1         1         1
      3        0        0         0         1
      0        1        0         0         0

    Type 8:  Bipolar Full Step (two windings on)

    State   Phase A   Phase B   Phase C   Phase D
      0        1        0         1         0
      1        0        1         1         0
      2        0        1         0         1
      3        1        0         0         1
      0        1        0         1         0

    Type 9:  Unipolar Half Step

    State   Phase A   Phase B   Phase C   Phase D
      0        1        0         0         0
      1        1        1         0         0
      2        0        1         0         0
      3        0        1         1         0
      4        0        0         1         0
      5        0        0         1         1
      6        0        0         0         1
      7        1        0         0         1
      0        1        0         0         0

    Type 10:  Bipolar Half Step

    State   Phase A   Phase B   Phase C   Phase D
      0        1        0         0         0
      1        1        0         1         0
      2        1        1         1         0
      3        0        1         1         0
      4        0        1         1         1
      5        0        1         0         1
      6        0        0         0         1
      7        1        0         0         1
      0        1        0         0         0

    Type 11:  Five Wire Unipolar

    State   Phase A   Phase B   Phase C   Phase D  Phase E
      0        1        0         0         0        0
      1        0        1         0         0        0
      2        0        0         1         0        0
      3        0        0         0         1        0
      4        0        0         0         0        1
      0        1        0         0         0        0

    Type 12:  Five Wire Wave

    State   Phase A   Phase B   Phase C   Phase D  Phase E
      0        1        1         0         0        0
      1        0        1         1         0        0
      2        0        0         1         1        0
      3        0        0         0         1        1
      4        1        0         0         0        1
      0        1        1         0         0        0

    Type 13:  Five Wire Unipolar HalfStep

    State   Phase A   Phase B   Phase C   Phase D  Phase E
      0        1        0         0         0        0
      1        1        1         0         0        0
      2        0        1         0         0        0
      3        0        1         1         0        0
      4        0        0         1         0        0
      5        0        0         1         1        0
      6        0        0         0         1        0
      7        0        0         0         1        1
      8        0        0         0         0        1
      9        1        0         0         0        1
      0        1        0         0         0        0

    Type 14:  Five Wire Wave HalfStep

    State   Phase A   Phase B   Phase C   Phase D  Phase E
      0        1        1         0         0        0
      1        1        1         1         0        0
      2        0        1         1         0        0
      3        0        1         1         1        0
      4        0        0         1         1        0
      5        0        0         1         1        1
      6        0        0         0         1        1
      7        1        0         0         1        1
      8        1        0         0         0        1
      9        1        1         0         0        1
      0        1        1         0         0        0

*/

/** This program is free software; you can redistribute it and/or
    modify it under the terms of version 2 of the GNU General
    Public License as published by the Free Software Foundation.
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

    THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
    ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
    TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
    harming persons must have provisions for completely removing power
    from all motors, etc, before persons enter any danger area.  All
    machinery must be designed to comply with local and national safety
    codes, and the authors of this software can not, and do not, take
    any responsibility for such compliance.

    This code was written as part of the EMC HAL project.  For more
    information, go to www.linuxcnc.org.
*/
#include "StepGen3.h"
#include <stdio.h>
#include <stdlib.h>
#include <cmath>
#include <cstring>
using namespace std;

/***********************************************************************
 *                STRUCTURES AND GLOBAL VARIABLES                       *
 ************************************************************************/

/** This structure contains the runtime data for a single generator. */

/* structure members are ordered to optimize caching for makepulses,
   which runs in the fastest thread */

/* ptr to array of stepgen_t structs in shared memory, 1 per channel */

/***********************************************************************
 *                  LOCAL FUNCTION DECLARATIONS                         *
 ************************************************************************/

/***********************************************************************
 *              REALTIME STEP PULSE GENERATION FUNCTIONS                *
 ************************************************************************/

/** The frequency generator works by adding a signed value proportional
    to frequency to an accumulator.  When bit PICKOFF of the accumulator
    toggles, a step is generated.
*/

void StepGen3::make_pulses(void *arg, long period)
{
    stepgen_t *stepgen;
    long old_addval, target_addval, new_addval, step_now;
    int n, p;
    unsigned char outbits;

    /* store period so scaling constants can be (re)calculated */
    periodns = period;
    /* point to stepgen data structures */
    stepgen = (stepgen_t *)arg;
    cnt++;

    for (n = 0; n < num_chan; n++)
    {
        /* decrement "timing constraint" timers */
        if (stepgen->timer1 > 0)
        {
            if (stepgen->timer1 > periodns)
            {
                stepgen->timer1 -= periodns;
            }
            else
            {
                stepgen->timer1 = 0;
            }
        }
        if (stepgen->timer2 > 0)
        {
            if (stepgen->timer2 > periodns)
            {
                stepgen->timer2 -= periodns;
            }
            else
            {
                stepgen->timer2 = 0;
            }
        }
        if (stepgen->timer3 > 0)
        {
            if (stepgen->timer3 > periodns)
            {
                stepgen->timer3 -= periodns;
            }
            else
            {
                stepgen->timer3 = 0;
                /* last timer timed out, cancel hold */
                stepgen->hold_dds = 0;
            }
        }
        if (!stepgen->hold_dds && stepgen->enable)
        {
            /* update addval (ramping) */
            old_addval = stepgen->addval;
            target_addval = stepgen->target_addval;
            if (stepgen->deltalim != 0)
            {
                /* implement accel/decel limit */
                if (target_addval > (old_addval + stepgen->deltalim))
                {
                    /* new value is too high, increase addval as far as possible */
                    new_addval = old_addval + stepgen->deltalim;
                }
                else if (target_addval < (old_addval - stepgen->deltalim))
                {
                    /* new value is too low, decrease addval as far as possible */
                    new_addval = old_addval - stepgen->deltalim;
                }
                else
                {
                    /* new value can be reached in one step - do it */
                    new_addval = target_addval;
                }
            }
            else
            {
                /* go to new freq without any ramping */
                new_addval = target_addval;
            }
            /* save result */
            stepgen->addval = new_addval;
            /* check for direction reversal */
            if (((new_addval >= 0) && (old_addval < 0)) ||
                ((new_addval < 0) && (old_addval >= 0)))
            {
                /* reversal required, can we do so now? */
                if (stepgen->timer3 != 0)
                {
                    /* no - hold everything until delays time out */
                    stepgen->hold_dds = 1;
                }
            }
        }
        /* update DDS */
        if (!stepgen->hold_dds && stepgen->enable)
        {
            /* save current value of low half of accum */
            step_now = stepgen->accum;
            /* update the accumulator */
            stepgen->accum += stepgen->addval;
            /* test for changes in low half of accum */
            step_now ^= stepgen->accum;
            /* we only care about the pickoff bit */
            step_now &= (1L << PICKOFF);
            /* update rawcounts parameter */
            stepgen->rawcount = stepgen->accum >> PICKOFF;
        }
        else
        {
            /* DDS is in hold, no steps */
            step_now = 0;
        }
        if (stepgen->timer2 == 0)
        {
            /* update direction - do not change if addval = 0 */
            if (stepgen->addval > 0)
            {
                stepgen->curr_dir = 1;
            }
            else if (stepgen->addval < 0)
            {
                stepgen->curr_dir = -1;
            }
        }
        if (step_now)
        {
            /* (re)start various timers */
            /* timer 1 = time till end of step pulse */
            stepgen->timer1 = stepgen->step_len;
            /* timer 2 = time till allowed to change dir pin */
            stepgen->timer2 = stepgen->timer1 + stepgen->dir_hold_dly;
            /* timer 3 = time till allowed to step the other way */
            stepgen->timer3 = stepgen->timer2 + stepgen->dir_setup;
            if (stepgen->step_type >= 2)
            {
                /* update state */
                stepgen->state += stepgen->curr_dir;
                if (stepgen->state < 0)
                {
                    stepgen->state = stepgen->cycle_max;
                }
                else if (stepgen->state > stepgen->cycle_max)
                {
                    stepgen->state = 0;
                }
            }
        }
        /* generate output, based on stepping type */
        if (stepgen->step_type == 0)
        {
            /* step/dir output */
            if (stepgen->timer1 != 0)
            {
                stepgen->phase[STEP_PIN] = 1;
            }
            else
            {
                stepgen->phase[STEP_PIN] = 0;
            }
            if (stepgen->curr_dir < 0)
            {
                stepgen->phase[DIR_PIN] = 1;
            }
            else
            {
                stepgen->phase[DIR_PIN] = 0;
            }
        }
        else if (stepgen->step_type == 1)
        {
            /* up/down */
            if (stepgen->timer1 != 0)
            {
                if (stepgen->curr_dir < 0)
                {
                    stepgen->phase[UP_PIN] = 0;
                    stepgen->phase[DOWN_PIN] = 1;
                }
                else
                {
                    stepgen->phase[UP_PIN] = 1;
                    stepgen->phase[DOWN_PIN] = 0;
                }
            }
            else
            {
                stepgen->phase[UP_PIN] = 0;
                stepgen->phase[DOWN_PIN] = 0;
            }
        }
        else
        {
            /* step type 2 or greater */
            /* look up correct output pattern */
            outbits = (stepgen->lut)[stepgen->state];
            /* now output the phase bits */
            for (p = 0; p < stepgen->num_phases; p++)
            {
                /* output one phase */
                stepgen->phase[p] = outbits & 1;
                /* move to the next phase */
                outbits >>= 1;
            }
        }
        /* move on to next step generator */
        stepgen++;
    }
    /* done */
}

void StepGen3::update_pos(void *arg, long period)
{
    long long int accum_a, accum_b;
    stepgen_t *stepgen;
    int n;

    stepgen = (stepgen_t *)arg;

    for (n = 0; n < num_chan; n++)
    {
        /* 'accum' is a long long, and its remotely possible that
           make_pulses could change it half-way through a read.
           So we have a crude atomic read routine */
        do
        {
            accum_a = stepgen->accum;
            accum_b = stepgen->accum;
        } while (accum_a != accum_b);
        /* compute integer counts */
        stepgen->count = accum_a >> PICKOFF;
        /* check for change in scale value */
        if (stepgen->pos_scale != stepgen->old_scale)
        {
            /* get ready to detect future scale changes */
            stepgen->old_scale = stepgen->pos_scale;
            /* validate the new scale value */
            if ((stepgen->pos_scale < 1e-20) && (stepgen->pos_scale > -1e-20))
            {
                /* value too small, divide by zero is a bad thing */
                stepgen->pos_scale = 1.0;
            }
            /* we will need the reciprocal, and the accum is fixed point with
               fractional bits, so we precalc some stuff */
            stepgen->scale_recip = (1.0 / (1L << PICKOFF)) / stepgen->pos_scale;
        }
        /* scale accumulator to make floating point position, after
           removing the one-half count offset */
        stepgen->pos_fb = (double)(accum_a - (1 << (PICKOFF - 1))) * stepgen->scale_recip;
        /* move on to next channel */
        stepgen++;
    }
    /* done */
}

/* helper function - computes integral multiple of increment that is greater
   or equal to value */
unsigned long StepGen3::ulceil(unsigned long value, unsigned long increment)
{
    if (value == 0)
    {
        return 0;
    }
    return increment * (((value - 1) / increment) + 1);
}

void StepGen3::update_freq(void *arg, long period)
{
    stepgen_t *stepgen;
    int n, newperiod;
    long min_step_period;
    long long int accum_a, accum_b;
    double pos_cmd, vel_cmd, curr_pos, curr_vel, avg_v, max_freq, max_ac;
    double match_ac, match_time, est_out, est_cmd, est_err, dp, dv, new_vel;
    double desired_freq;
    /*! \todo FIXME - while this code works just fine, there are a bunch of
       internal variables, many of which hold intermediate results that
       don't really need their own variables.  They are used either for
       clarity, or because that's how the code evolved.  This algorithm
       could use some cleanup and optimization. */
    /* this periodns stuff is a little convoluted because we need to
       calculate some constants here in this relatively slow thread but the
       constants are based on the period of the much faster 'make_pulses()'
       thread. */
    /* only recalc constants if period changes */
    newperiod = 0;
    if (periodns != old_periodns)
    {
        /* get ready to detect future period changes */
        old_periodns = periodns;
        /* recompute various constants that depend on periodns */
        periodfp = periodns * 0.000000001;
        freqscale = (1L << PICKOFF) * periodfp;
        accelscale = freqscale * periodfp;
        /* force re-evaluation of the timing parameters */
        newperiod = 1;
    }
    /* now recalc constants related to the period of this funct */
    /* only recalc constants if period changes */
    if (period != old_dtns)
    {
        /* get ready to detect future period changes */
        old_dtns = period;
        /* dT is the period of this thread, used for the position loop */
        dt = period * 0.000000001;
        /* calc the reciprocal once here, to avoid multiple divides later */
        recip_dt = 1.0 / dt;
    }

    /* point at stepgen data */
    stepgen = (stepgen_t *)arg;

    /* loop thru generators */
    for (n = 0; n < num_chan; n++)
    {
        /* check for scale change */
        if (stepgen->pos_scale != stepgen->old_scale)
        {
            /* get ready to detect future scale changes */
            stepgen->old_scale = stepgen->pos_scale;
            /* validate the new scale value */
            if ((stepgen->pos_scale < 1e-20) && (stepgen->pos_scale > -1e-20))
            {
                /* value too small, divide by zero is a bad thing */
                stepgen->pos_scale = 1.0;
            }
            /* we will need the reciprocal, and the accum is fixed point with
               fractional bits, so we precalc some stuff */
            stepgen->scale_recip = (1.0 / (1L << PICKOFF)) / stepgen->pos_scale;
        }
        if (newperiod)
        {
            /* period changed, force recalc of timing parameters */
            stepgen->old_step_len = ~0;
            stepgen->old_step_space = ~0;
            stepgen->old_dir_hold_dly = ~0;
            stepgen->old_dir_setup = ~0;
        }
        /* process timing parameters */
        if (stepgen->step_len != stepgen->old_step_len)
        {
            /* must be non-zero */
            if (stepgen->step_len == 0)
            {
                stepgen->step_len = 1;
            }
            /* make integer multiple of periodns */
            stepgen->old_step_len = ulceil(stepgen->step_len, periodns);
            stepgen->step_len = stepgen->old_step_len;
        }
        if (stepgen->step_space != stepgen->old_step_space)
        {
            /* make integer multiple of periodns */
            stepgen->old_step_space = ulceil(stepgen->step_space, periodns);
            stepgen->step_space = stepgen->old_step_space;
        }
        if (stepgen->dir_setup != stepgen->old_dir_setup)
        {
            /* make integer multiple of periodns */
            stepgen->old_dir_setup = ulceil(stepgen->dir_setup, periodns);
            stepgen->dir_setup = stepgen->old_dir_setup;
        }
        if (stepgen->dir_hold_dly != stepgen->old_dir_hold_dly)
        {
            if ((stepgen->dir_hold_dly + stepgen->dir_setup) == 0)
            {
                /* dirdelay must be non-zero step types 0 and 1 */
                if (stepgen->step_type < 2)
                {
                    stepgen->dir_hold_dly = 1;
                }
            }
            stepgen->old_dir_hold_dly = ulceil(stepgen->dir_hold_dly, periodns);
            stepgen->dir_hold_dly = stepgen->old_dir_hold_dly;
        }
        /* test for disabled stepgen */
        if (stepgen->enable == 0)
        {
            /* disabled: keep updating old_pos_cmd (if in pos ctrl mode) */
            if (stepgen->pos_mode)
            {
                stepgen->old_pos_cmd = stepgen->pos_cmd * stepgen->pos_scale;
            }
            /* set velocity to zero */
            stepgen->freq = 0;
            stepgen->addval = 0;
            stepgen->target_addval = 0;
            /* and skip to next one */
            stepgen++;
            continue;
        }
        /* calculate frequency limit */
        min_step_period = stepgen->step_len + stepgen->step_space;
        max_freq = 1.0 / (min_step_period * 0.000000001);
        /* check for user specified frequency limit parameter */
        if (stepgen->maxvel <= 0.0)
        {
            /* set to zero if negative */
            stepgen->maxvel = 0.0;
        }
        else
        {
            /* parameter is non-zero, compare to max_freq */
            desired_freq = stepgen->maxvel * fabs(stepgen->pos_scale);
            if (desired_freq > max_freq)
            {
                /* parameter is too high, complain about it */
                if (!stepgen->printed_error)
                {
                    printf("STEPGEN: Channel %d: The requested maximum velocity of %d steps/sec is too high.\n",
                           n, (int)desired_freq);
                    printf("STEPGEN: The maximum possible frequency is %d steps/second\n",
                           (int)max_freq);
                    stepgen->printed_error = 1;
                }
                /* parameter is too high, limit it */
                stepgen->maxvel = max_freq / fabs(stepgen->pos_scale);
            }
            else
            {
                /* lower max_freq to match parameter */
                max_freq = stepgen->maxvel * fabs(stepgen->pos_scale);
            }
        }
        /* set internal accel limit to its absolute max, which is
           zero to full speed in one thread period */
        max_ac = max_freq * recip_dt;
        /* check for user specified accel limit parameter */
        if (stepgen->maxaccel <= 0.0)
        {
            /* set to zero if negative */
            stepgen->maxaccel = 0.0;
        }
        else
        {
            /* parameter is non-zero, compare to max_ac */
            if ((stepgen->maxaccel * fabs(stepgen->pos_scale)) > max_ac)
            {
                /* parameter is too high, lower it */
                stepgen->maxaccel = max_ac / fabs(stepgen->pos_scale);
            }
            else
            {
                /* lower limit to match parameter */
                max_ac = stepgen->maxaccel * fabs(stepgen->pos_scale);
            }
        }
        /* at this point, all scaling, limits, and other parameter
           changes have been handled - time for the main control */
        if (stepgen->pos_mode)
        {
            /* calculate position command in counts */
            pos_cmd = stepgen->pos_cmd * stepgen->pos_scale;
            /* calculate velocity command in counts/sec */
            vel_cmd = (pos_cmd - stepgen->old_pos_cmd) * recip_dt;
            stepgen->old_pos_cmd = pos_cmd;
            /* 'accum' is a long long, and its remotely possible that
               make_pulses could change it half-way through a read.
               So we have a crude atomic read routine */
            do
            {
                accum_a = stepgen->accum;
                accum_b = stepgen->accum;
            } while (accum_a != accum_b);
            /* convert from fixed point to double, after subtracting
               the one-half step offset */
            curr_pos = (accum_a - (1 << (PICKOFF - 1))) * (1.0 / (1L << PICKOFF));
            /* get velocity in counts/sec */
            curr_vel = stepgen->freq;
            /* At this point we have good values for pos_cmd, curr_pos,
               vel_cmd, curr_vel, max_freq and max_ac, all in counts,
               counts/sec, or counts/sec^2.  Now we just have to do
               something useful with them. */
            /* determine which way we need to ramp to match velocity */
            if (vel_cmd > curr_vel)
            {
                match_ac = max_ac;
            }
            else
            {
                match_ac = -max_ac;
            }
            /* determine how long the match would take */
            match_time = (vel_cmd - curr_vel) / match_ac;
            /* calc output position at the end of the match */
            avg_v = (vel_cmd + curr_vel) * 0.5;
            est_out = curr_pos + avg_v * match_time;
            /* calculate the expected command position at that time */
            est_cmd = pos_cmd + vel_cmd * (match_time - 1.5 * dt);
            /* calculate error at that time */
            est_err = est_out - est_cmd;
            if (match_time < dt)
            {
                /* we can match velocity in one period */
                if (fabs(est_err) < 0.0001)
                {
                    /* after match the position error will be acceptable */
                    /* so we just do the velocity match */
                    new_vel = vel_cmd;
                }
                else
                {
                    /* try to correct position error */
                    new_vel = vel_cmd - 0.5 * est_err * recip_dt;
                    /* apply accel limits */
                    if (new_vel > (curr_vel + max_ac * dt))
                    {
                        new_vel = curr_vel + max_ac * dt;
                    }
                    else if (new_vel < (curr_vel - max_ac * dt))
                    {
                        new_vel = curr_vel - max_ac * dt;
                    }
                }
            }
            else
            {
                /* calculate change in final position if we ramp in the
                   opposite direction for one period */
                dv = -2.0 * match_ac * dt;
                dp = dv * match_time;
                /* decide which way to ramp */
                if (fabs(est_err + dp * 2.0) < fabs(est_err))
                {
                    match_ac = -match_ac;
                }
                /* and do it */
                new_vel = curr_vel + match_ac * dt;
            }
            /* apply frequency limit */
            if (new_vel > max_freq)
            {
                new_vel = max_freq;
            }
            else if (new_vel < -max_freq)
            {
                new_vel = -max_freq;
            }
            /* end of position mode */
        }
        else
        {
            /* velocity mode is simpler */
            /* calculate velocity command in counts/sec */
            vel_cmd = stepgen->vel_cmd * stepgen->pos_scale;
            /* apply frequency limit */
            if (vel_cmd > max_freq)
            {
                vel_cmd = max_freq;
            }
            else if (vel_cmd < -max_freq)
            {
                vel_cmd = -max_freq;
            }
            /* calc max change in frequency in one period */
            dv = max_ac * dt;
            /* apply accel limit */
            if (vel_cmd > (stepgen->freq + dv))
            {
                new_vel = stepgen->freq + dv;
            }
            else if (vel_cmd < (stepgen->freq - dv))
            {
                new_vel = stepgen->freq - dv;
            }
            else
            {
                new_vel = vel_cmd;
            }
            /* end of velocity mode */
        }
        stepgen->freq = new_vel;
        /* calculate new addval */
        stepgen->target_addval = stepgen->freq * freqscale;
        /* calculate new deltalim */
        stepgen->deltalim = max_ac * accelscale;
        /* move on to next channel */
        stepgen++;
    }
    /* done */
}

/***********************************************************************
 *                   LOCAL FUNCTION DEFINITIONS                         *
 ************************************************************************/

int StepGen3::export_stepgen(int num, stepgen_t *addr, int step_type, int pos_mode)
{
    int n, retval, msg;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */

    /* export output pins */
    if (step_type == 0)
    {
        addr->phase[STEP_PIN] = 0;
        addr->phase[DIR_PIN] = 0;
    }
    else if (step_type == 1)
    {
        addr->phase[UP_PIN] = 0;
        addr->phase[DOWN_PIN] = 0;
    }
    else
    {
        /* stepping types 2 and higher use a varying number of phase pins */
        addr->num_phases = num_phases_lut[step_type - 2];
        for (n = 0; n < addr->num_phases; n++)
        {
            addr->phase[n] = 0;
        }
    }

    printf("Exporting %d\n", num);

    /* set default parameter values */
    addr->pos_scale = 1.0;
    addr->old_scale = 0.0;
    addr->scale_recip = 0.0;
    addr->freq = 0.0;
    addr->maxvel = 0.0;
    addr->maxaccel = 0.0;
    addr->step_type = step_type;
    addr->pos_mode = pos_mode;

    printf("Exporting 1\n");
    /* timing parameter defaults depend on step type */
    addr->step_len = 1;
    if (step_type < 2)
    {
        addr->step_space = 1;
    }
    else
    {
        addr->step_space = 0;
    }
    if (step_type == 0)
    {
        addr->dir_hold_dly = 1;
        addr->dir_setup = 1;
    }
    else
    {
        addr->dir_hold_dly = 1;
        addr->dir_setup = 0;
    }
    /* set 'old' values to make update_freq validate the timing params */

    printf("Exporting 2\n");
    addr->old_step_len = ~0;
    addr->old_step_space = ~0;
    addr->old_dir_hold_dly = ~0;
    addr->old_dir_setup = ~0;
    if (step_type >= 2)
    {
        /* init output stuff */
        addr->cycle_max = cycle_len_lut[step_type - 2] - 1;
        addr->lut = &(master_lut[step_type - 2][0]);
    }
    /* init the step generator core to zero output */
    addr->timer1 = 0;
    addr->timer2 = 0;
    addr->timer3 = 0;
    addr->hold_dds = 0;
    addr->addval = 0;
    /* accumulator gets a half step offset, so it will step half
       way between integer positions, not at the integer positions */
    addr->accum = 1 << (PICKOFF - 1);
    addr->rawcount = 0;
    addr->curr_dir = 0;
    addr->state = 0;
    addr->enable = 0;
    addr->target_addval = 0;
    addr->deltalim = 0;
    /* other init */
    addr->printed_error = 0;
    addr->old_pos_cmd = 0.0;

    printf("Exporting 3\n");
    /* set initial pin values */
    addr->count = 0;
    addr->pos_fb = 0.0;
    if (pos_mode)
    {
        addr->pos_cmd = 0.0;
    }
    else
    {
        addr->vel_cmd = 0.0;
    }
    /* restore saved message level */
    // rtapi_set_msg_level(msg);
    printf("Exporting done\n");
    return 0;
}

int StepGen3::setup_user_step_type(void)
{
    int used_phases = 0;
    int i = 0;
    for (i = 0; i < MAX_CYCLE && user_step_type[i] != -1; i++)
    {
        master_lut[USER_STEP_TYPE][i] = user_step_type[i];
        used_phases |= user_step_type[i];
    }
    cycle_len_lut[USER_STEP_TYPE] = i;
    if (used_phases & ~0x1f)
    {
        printf("STEPGEN: ERROR: bad user step type uses more than 5 phases");
        return -1; // EINVAL more than 5 phases is not allowed
    }

    if (used_phases & 0x10)
        num_phases_lut[USER_STEP_TYPE] = 5;
    else if (used_phases & 0x8)
        num_phases_lut[USER_STEP_TYPE] = 4;
    else if (used_phases & 0x4)
        num_phases_lut[USER_STEP_TYPE] = 3;
    else if (used_phases & 0x2)
        num_phases_lut[USER_STEP_TYPE] = 2;
    else if (used_phases & 0x1)
        num_phases_lut[USER_STEP_TYPE] = 1;

    if (used_phases)
        printf("User step type has %d phases and %d steps per cycle\n",
               num_phases_lut[USER_STEP_TYPE], i);
    return 0;
}

CONTROL StepGen3::parse_ctrl_type(const char *ctrl)
{
    if (!ctrl || !*ctrl || *ctrl == 'p' || *ctrl == 'P')
        return POSITION;
    if (*ctrl == 'v' || *ctrl == 'V')
        return VELOCITY;
    return INVALID;
}

StepGen3::StepGen3(void)
{

    step_type[0] = 0;
    ctrl_type[0] = (char *)malloc(2);
    strcpy(ctrl_type[0], "p");
    stepPin[0] = PA11;
    dirPin[0] = PA12;

    step_type[1] = 0;
    ctrl_type[1] = (char *)malloc(2);
    strcpy(ctrl_type[1], "p");
    stepPin[1] = PC9;
    dirPin[1] = PC8;

    step_type[2] = 0;
    ctrl_type[2] = (char *)malloc(2);
    strcpy(ctrl_type[2], "p");
    stepPin[2] = PD12;
    dirPin[2] = PD11;

    step_type[3] = 0;
    ctrl_type[3] = (char *)malloc(2);
    strcpy(ctrl_type[3], "p");
    stepPin[3] = PE5;
    dirPin[3] = PE4;

    rtapi_app_main();
    stepgen_array[0].pos_scale = JOINT_1_SCALE; // Can and should be overriden by PDO values
    stepgen_array[0].maxaccel = JOINT_1_STEPGEN_MAXACCEL;
    stepgen_array[1].pos_scale = JOINT_2_SCALE;
    stepgen_array[1].maxaccel = JOINT_2_STEPGEN_MAXACCEL;
    stepgen_array[2].pos_scale = JOINT_3_SCALE;
    stepgen_array[2].maxaccel = JOINT_3_STEPGEN_MAXACCEL;
    stepgen_array[3].pos_scale = JOINT_4_SCALE;
    stepgen_array[3].maxaccel = JOINT_4_STEPGEN_MAXACCEL;
    stepgen_array[0].enable = stepgen_array[1].enable =
        stepgen_array[2].enable = stepgen_array[3].enable = 0; // Note that they are off by default
}

void StepGen3::updateStepGen(double pos_cmd1, double pos_cmd2, double pos_cmd3, double pos_cmd4,
                             float pos_scale1, float pos_scale2, float pos_scale3, float pos_scale4,
                             float max_acc1, float max_acc2, float max_acc3, float max_acc4,
                             uint8_t enable1, uint8_t enable2, uint8_t enable3, uint8_t enable4,
                             uint32_t servoPeriod)
{
    stepgen_array[0].pos_cmd = pos_cmd1;
    stepgen_array[1].pos_cmd = pos_cmd2;
    stepgen_array[2].pos_cmd = pos_cmd3;
    stepgen_array[3].pos_cmd = pos_cmd4;
    stepgen_array[0].pos_scale = pos_scale1;
    stepgen_array[1].pos_scale = pos_scale2;
    stepgen_array[2].pos_scale = pos_scale3;
    stepgen_array[3].pos_scale = pos_scale4;
    stepgen_array[0].maxaccel = max_acc1;
    stepgen_array[1].maxaccel = max_acc2;
    stepgen_array[2].maxaccel = max_acc3;
    stepgen_array[3].maxaccel = max_acc4;
    for (int i = 0; i < num_chan; i++)
    {
        stepgen_t *step;
        step = &(stepgen_array[i]);
        update_pos(step, servoPeriod); // servoPeriod is in nanosecs
        update_freq(step, servoPeriod);
    }
}

int StepGen3::rtapi_app_main()
{
    int n, retval;
    retval = setup_user_step_type();
    if (retval < 0)
    {
        return retval;
    }

    for (n = 0; n < MAX_CHAN && step_type[n] != -1; n++)
    {
        if ((step_type[n] > MAX_STEP_TYPE) || (step_type[n] < 0))
        {
            printf("STEPGEN: ERROR: bad stepping type '%i', axis %i\n",
                   step_type[n], n);
            return -1;
        }
        if (parse_ctrl_type(ctrl_type[n]) == INVALID)
        {
            printf("STEPGEN: ERROR: bad control type '%s' for axis %i (must be 'p' or 'v')\n",
                   ctrl_type[n], n);
            return -1;
        }
        num_chan++;
    }
    if (num_chan == 0)
    {
        Serial1.printf("STEPGEN: ERROR: no channels configured\n");
        return -1;
    }
    Serial1.printf("num_chan = %d\n", num_chan);
    /* periodns will be set to the proper value when 'make_pulses()' runs for
       the first time.  We load a default value here to avoid glitches at
       startup, but all these 'constants' are recomputed inside
       'update_freq()' using the real period. */
    old_periodns = periodns = 50000;
    old_dtns = 1000000;
    /* precompute some constants */
    periodfp = periodns * 0.000000001;
    freqscale = (1L << PICKOFF) * periodfp;
    accelscale = freqscale * periodfp;
    dt = old_dtns * 0.000000001;
    recip_dt = 1.0 / dt;
    /* have good config info */

    /* allocate shared memory for counter data */

    stepgen_array = (stepgen_t *)malloc(num_chan * sizeof(stepgen_t));
    if (stepgen_array == 0)
    {
        printf("STEPGEN: ERROR: hal_malloc() failed\n");
        return -1;
    }
    /* export all the variables for each pulse generator */
    for (n = 0; n < num_chan; n++)
    {
        /* export all vars */
        retval = export_stepgen(n, &(stepgen_array[n]),
                                step_type[n], (parse_ctrl_type(ctrl_type[n]) == POSITION));
        if (retval != 0)
        {
            printf("STEPGEN: ERROR: stepgen %d var export failed\n", n);
            return -1;
        }
    }
    return 0;
}
extern volatile uint64_t makePulsesCnt;
void StepGen3::makeAllPulses(void)
{
    makePulsesCnt++;

    make_pulses(stepgen_array, BASE_PERIOD);
    for (int i = 0; i < num_chan; i++)
    {
        digitalWrite(dirPin[i], stepgen_array[i].phase[DIR_PIN] ? LOW : HIGH);
        digitalWrite(stepPin[i], stepgen_array[i].phase[STEP_PIN] ? HIGH : LOW);
    }
}
// See StepGenLoop.h. The algorithm and its comments are LinuxCNC's stepgen.c
// update_freq(), with positions in 64 bit fixed point and the rest in float.

#pragma GCC optimize("O2")  // runs every cycle: speed over size
#include "StepGenLoop.h"

#include <math.h>

void stepgenUpdateFreq(stepgen_t *sg, const StepLoopTiming &k)
{
    float new_vel;

    /* test for disabled stepgen */
    if (sg->enable == 0)
    {
        /* disabled: keep updating old_pos_cmd (if in pos ctrl mode) */
        if (sg->pos_mode)
            sg->old_pos_cmd = sg->pos_cmd;
        /* set velocity to zero */
        sg->freq = 0;
        sg->addval = 0;
        sg->target_addval = 0;
        return;
    }
    /* calculate frequency limit */
    const long min_step_period = sg->step_len + sg->step_space;
    float max_freq = 1.0e9f / (float)min_step_period;
    if (sg->freq_cap > 0.0f && max_freq > sg->freq_cap)
        max_freq = sg->freq_cap;         /* the step generator's own limit */
    const float timing_freq = max_freq;  /* what the step timing allows at all */
    /* check for user specified frequency limit parameter */
    if (sg->maxvel <= 0.0f)
        sg->maxvel = 0.0f;
    else if (sg->maxvel > max_freq)
        sg->maxvel = max_freq;   /* parameter is too high, limit it */
    else
        max_freq = sg->maxvel;   /* lower max_freq to match parameter */
    /* set internal accel limit to its absolute max, which is
       zero to full speed in one thread period */
    float max_ac = max_freq * k.recipDt;
    /* check for user specified accel limit parameter */
    if (sg->maxaccel <= 0.0f)
        sg->maxaccel = 0.0f;
    else if (sg->maxaccel > max_ac)
        sg->maxaccel = max_ac;   /* parameter is too high, lower it */
    else
        max_ac = sg->maxaccel;   /* lower limit to match parameter */

    if (sg->pos_mode)
    {
        const int64_t pos_cmd = sg->pos_cmd;
        /* velocity command in steps/sec, from the change of the position command */
        const float vel_cmd = stepgenFxToFloat(pos_cmd - sg->old_pos_cmd) * k.recipDt;
        sg->old_pos_cmd = pos_cmd;
        const int64_t curr_pos = stepgenPosition(sg);
        const float curr_vel = sg->freq;
        /* determine which way we need to ramp to match velocity */
        float match_ac = (vel_cmd > curr_vel) ? max_ac : -max_ac;
        /* determine how long the match would take */
        const float match_time = (vel_cmd - curr_vel) / match_ac;
        /* output position at the end of the match minus the expected command
           position at that time: est_out - est_cmd, with the absolute positions
           subtracted first (in fixed point), so only a small difference is float */
        const float avg_v = (vel_cmd + curr_vel) * 0.5f;
        const float est_err = stepgenFxToFloat(curr_pos - pos_cmd) + avg_v * match_time -
                              vel_cmd * (match_time - 1.5f * k.dt);
        if (match_time < k.dt)
        {
            /* we can match velocity in one period */
            if (fabsf(est_err) < 0.0001f)
            {
                /* after match the position error will be acceptable */
                new_vel = vel_cmd;
            }
            else
            {
                /* try to correct position error */
                new_vel = vel_cmd - 0.5f * est_err * k.recipDt;
                /* apply accel limits */
                if (new_vel > (curr_vel + max_ac * k.dt))
                    new_vel = curr_vel + max_ac * k.dt;
                else if (new_vel < (curr_vel - max_ac * k.dt))
                    new_vel = curr_vel - max_ac * k.dt;
            }
        }
        else
        {
            /* calculate change in final position if we ramp in the
               opposite direction for one period */
            const float dv = -2.0f * match_ac * k.dt;
            const float dp = dv * match_time;
            /* decide which way to ramp */
            if (fabsf(est_err + dp * 2.0f) < fabsf(est_err))
                match_ac = -match_ac;
            new_vel = curr_vel + match_ac * k.dt;
        }
        /* apply frequency limit */
        if (new_vel > max_freq)
            new_vel = max_freq;
        else if (new_vel < -max_freq)
            new_vel = -max_freq;
        /* the command itself is faster than the step timing allows */
        if (fabsf(vel_cmd) > timing_freq)
            sg->freq_limited = 1;
    }
    else
    {
        /* velocity mode is simpler */
        float vel_cmd = sg->vel_cmd;
        if (fabsf(vel_cmd) > timing_freq)
            sg->freq_limited = 1;
        /* apply frequency limit */
        if (vel_cmd > max_freq)
            vel_cmd = max_freq;
        else if (vel_cmd < -max_freq)
            vel_cmd = -max_freq;
        /* calc max change in frequency in one period, apply accel limit */
        const float dv = max_ac * k.dt;
        if (vel_cmd > (sg->freq + dv))
            new_vel = sg->freq + dv;
        else if (vel_cmd < (sg->freq - dv))
            new_vel = sg->freq - dv;
        else
            new_vel = vel_cmd;
    }
    sg->freq = new_vel;
    sg->accel = max_ac;
    /* calculate new addval and deltalim */
    sg->target_addval = (long)(new_vel * k.freqscale);
    sg->deltalim = (long)(max_ac * k.accelscale);
}

void stepAxisCycle(stepgen_t *sg, StepAxisState &ax, StepPlanState &ps, StepQueue &q,
                   const StepTimingTicks &tm, const StepLoopTiming &k, int64_t nowT,
                   int64_t cycleTicks, float freqCap)
{
    /* the loop continues from where the queue ends ... */
    sg->accum = ps.accum;
    sg->freq = ps.vel;
    sg->freq_cap = freqCap;
    /* ... which is STEP_LEAD_TICKS after now: extrapolate the command by that much */
    if (sg->pos_mode)
    {
        const int64_t raw = sg->pos_cmd;
        if (ax.prevValid)
            sg->pos_cmd = raw + (int64_t)((float)(raw - ax.prevCmd) *
                                          ((float)STEP_LEAD_TICKS / (float)cycleTicks));
        ax.prevCmd = raw;
        ax.prevValid = true;
    }
    else
    {
        ax.prevValid = false;
    }
    stepgenUpdateFreq(sg, k);
    const float vTarget = sg->enable ? sg->freq : 0.0f;
    stepPlanWindow(ps, q, tm, vTarget, sg->accel > 0.0f ? sg->accel : 1e12f,
                   nowT + cycleTicks + STEP_LEAD_TICKS);
    sg->freq = ps.vel;
}

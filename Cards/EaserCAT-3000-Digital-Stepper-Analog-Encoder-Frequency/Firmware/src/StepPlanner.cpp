// See StepPlanner.h
#pragma GCC optimize("O2", "no-math-errno")  // runs for every step: speed over size; vsqrt inline
#include "StepPlanner.h"

#include <math.h>

static const int64_t ONE = 1LL << 28;  // one step in the accumulator
static const int32_t FAR_PAST = -1000000000;

void stepQueueInit(StepQueue &q, uint32_t *buf, uint32_t idleTicks)
{
    q.buf = buf;
    q.head = 0;
    q.tail = 0;
    q.preload = idleTicks - 1u;
    q.dirActive = 1;
    q.dirLoaded = 1;
    q.emitted = 0;
    q.loadedCount = 0;
    q.loadedEnd = 2 * (int64_t)idleTicks;
    q.underrunTicks = 0;
    q.underruns = 0;
}

void stepPlanInit(StepPlanState &s, const StepQueue &q, int64_t accum)
{
    s.end = q.loadedEnd;
    s.qEnd = q.loadedEnd;
    s.open = false;
    s.openStart = q.loadedEnd;
    s.openFlags = 0;
    s.openPulse = false;
    s.accum = accum;
    s.vel = 0.0f;
    s.dir = q.dirLoaded;
    s.lastStep = q.loadedEnd - (int64_t)stepEntryTicks(q.preload);
    s.lastEvent = s.lastStep;
    s.steps = 0;
}

void stepQueueFlush(StepQueue &q, StepPlanState &s)
{
    q.head = q.tail;
    q.underrunTicks = 0;
    s.end = q.loadedEnd;
    s.qEnd = q.loadedEnd;
    s.open = false;
    s.openStart = q.loadedEnd;
    s.accum = ((int64_t)q.loadedCount << 28) + (ONE >> 1);
    s.vel = 0.0f;
    s.dir = q.dirLoaded;
    // The last step and event are at most at the start of the preload period
    s.lastStep = q.loadedEnd - (int64_t)stepEntryTicks(q.preload);
    s.lastEvent = s.lastStep;
}

namespace {

inline int32_t relTicks(int64_t t, int64_t base)
{
    int64_t r = t - base;
    return r < FAR_PAST ? FAR_PAST : (int32_t)r;
}

inline int32_t max32(int32_t a, int32_t b) { return a > b ? a : b; }

// Writes periods into the queue after head, published at the end
struct Planner {
    uint32_t *buf;
    const StepTimingTicks &tm;
    uint32_t pos, n;
    int32_t curStart;    // the open period: from its start ...
    uint32_t curFlags;   // ... with its pulse / direction action
    bool curPulse;
    int32_t lastStep, lastEvent;
    int32_t dir;
    uint32_t pulse;      // pulse ticks
    uint32_t steps;
    int32_t tailPulse;   // shortest period with a pulse (stepMinTail)
    int32_t spacing;     // shortest step to step (length + space)
    uint32_t splitPulse; // periods longer than this are split (see period())

    void put(uint32_t e)
    {
        buf[pos & STEP_QUEUE_MASK] = e;
        pos++;
        n++;
    }

    // Pieces of at most 65536 ticks, the 16 bit timer's limit
    void pieces(uint32_t len, uint32_t flags)
    {
        uint32_t k = (len + STEP_MAX_PERIOD_TICKS - 1u) / STEP_MAX_PERIOD_TICKS;
        uint32_t each = len / k, rem = len % k;
        for (uint32_t i = 0; i < k; i++) {
            uint32_t c = each + (i < rem ? 1u : 0u);
            put((c - 1u) | (i == 0 ? flags : 0u));
        }
    }

    // A period of 'len' ticks: its last piece at most STEP_TAIL_TICKS (see StepPlanner.h).
    // At speed a period is short: one entry, no division.
    inline void period(uint32_t len, uint32_t flags)
    {
        if (len == 0) return;
        if (len <= (stepEntryPulse(flags) ? splitPulse : STEP_TAIL_TICKS + STEP_MIN_PERIOD_TICKS)) {
            put((len - 1u) | flags);
            return;
        }
        periodLong(len, flags);
    }

    void periodLong(uint32_t len, uint32_t flags)
    {
        uint32_t first = STEP_MIN_PERIOD_TICKS;
        if (stepEntryPulse(flags) + 1u > first) first = stepEntryPulse(flags) + 1u;
        if (len > STEP_TAIL_TICKS + first) {
            pieces(len - STEP_TAIL_TICKS, flags);
            put(STEP_TAIL_TICKS - 1u);
        } else {
            pieces(len, flags);
        }
    }

    // An event at te or later: closes the open period, returns where it went
    int32_t event(int32_t te, uint32_t flags, bool isPulse)
    {
        if (curFlags == 0) {  // after an idle stretch: right at its start or a period later
            if (te <= curStart)
                te = curStart;
            else if (te < curStart + (int32_t)STEP_MIN_PERIOD_TICKS)
                te = curStart + (int32_t)STEP_MIN_PERIOD_TICKS;
        } else {
            te = max32(te, curStart + (curPulse ? tailPulse : (int32_t)STEP_MIN_PERIOD_TICKS));
        }
        period((uint32_t)(te - curStart), curFlags);
        curStart = te;
        curFlags = flags;
        curPulse = isPulse;
        lastEvent = te;
        return te;
    }

    // A step in direction sd, ideally at ti
    void step(int32_t ti, int32_t sd)
    {
        if (sd != dir) {
            int32_t td = max32(lastStep + (int32_t)(tm.len + tm.hold), 0);
            td = event(td, sd > 0 ? STEP_E_DIR_POS : STEP_E_DIR_NEG, false);
            dir = sd;
            uint32_t setup = tm.setup + STEP_DIR_MARGIN_TICKS;
            if (setup < STEP_MIN_PERIOD_TICKS) setup = STEP_MIN_PERIOD_TICKS;
            ti = max32(ti, td + (int32_t)setup);
        }
        ti = max32(ti, lastStep + spacing);
        ti = event(ti, pulse << STEP_E_PULSE_SHIFT, true);
        lastStep = ti;
        steps++;
    }
};

// Steps n at distances D, D+1, ... (in steps) from the segment's start: time(d) gives
// the ticks after the segment's start t0, at most tMax. The first step (and any after a
// direction change or a long gap) the general way, the rest in a short loop.
template <typename TimeFn>
inline void runSteps(Planner &p, uint32_t n, int32_t sd, float D, float t0, float tMax, TimeFn time)
{
    uint32_t k = 0;
    for (; k < n && (k == 0 || sd != p.dir || !p.curPulse); k++, D += 1.0f) {
        float t = time(D);
        if (t > tMax) t = tMax;
        p.step((int32_t)(t0 + t + 0.5f), sd);
    }
    if (k == n) return;
    uint32_t *const buf = p.buf;
    uint32_t pos = p.pos, wrote = 0, fsteps = 0;
    int32_t cur = p.curStart;
    const uint32_t flags = p.curFlags;
    const int32_t gap = max32(p.spacing, p.tailPulse);
    const uint32_t split = p.splitPulse;
    const float base0 = t0 + 0.5f;
    for (; k < n; k++, D += 1.0f) {
        float t = time(D);
        if (t > tMax) t = tMax;
        int32_t te = (int32_t)(base0 + t);
        if (te < cur + gap) te = cur + gap;
        const uint32_t len = (uint32_t)(te - cur);
        if (len > split) {  // a long one after all: the general way
            p.pos = pos;
            p.n += wrote;
            p.steps += fsteps;
            wrote = fsteps = 0;
            p.curStart = p.lastStep = p.lastEvent = cur;
            p.step(te, sd);
            pos = p.pos;
            cur = p.curStart;
            continue;
        }
        buf[pos & STEP_QUEUE_MASK] = (len - 1u) | flags;
        pos++;
        wrote++;
        fsteps++;
        cur = te;
    }
    p.pos = pos;
    p.n += wrote;
    p.steps += fsteps;
    p.curStart = p.lastStep = p.lastEvent = cur;
}

}  // namespace

uint32_t stepPlanWindow(StepPlanState &s, StepQueue &q, const StepTimingTicks &tm, float vTarget,
                        float accel, int64_t until)
{
    s.steps = 0;
    int64_t T64 = until - s.end;
    if (T64 < (int64_t)STEP_MIN_PERIOD_TICKS) return 0;  // planned far enough already
    if (s.qEnd - s.end > T64) return 0;
    if (T64 > 8000000) T64 = 8000000;                     // 100 ms
    int32_t T = (int32_t)T64;
    const float a = accel > 0.0f ? accel : 1e12f;
    const float v0 = s.vel, v1 = vTarget;

    // Room in the queue: a step takes at most two entries (and a direction change two),
    // a long gap one per 65536 ticks and its tail. Shorten the window if needed.
    const uint32_t used = q.head - q.tail;
    const uint32_t room = STEP_QUEUE_LEN - 1u - used;
    const float vmax = fabsf(v0) > fabsf(v1) ? fabsf(v0) : fabsf(v1);
    const float perTick = 2.0f * vmax / (float)STEP_TICK_HZ + 1.0f / (float)STEP_TAIL_TICKS;
    if (room < 16u) return 0;
    const float fit = (float)(room - 12u) / perTick;
    if ((float)T > fit) {
        T = (int32_t)fit;
        if (T < (int32_t)STEP_MIN_PERIOD_TICKS) return 0;
    }

    // Velocity profile: ramp from v0 towards v1, split where it crosses zero, then constant
    const float Ts = (float)T / (float)STEP_TICK_HZ;
    const float dv = v1 - v0;
    float tr = fabsf(dv) / a;
    if (tr > Ts) tr = Ts;
    const float acc = dv >= 0.0f ? a : -a;
    const float vr = v0 + acc * tr;
    struct Seg {
        float t0, d, v, a;
    } seg[3];
    int ns = 0;
    if (tr > 0.0f) {
        if ((v0 > 0.0f && vr < 0.0f) || (v0 < 0.0f && vr > 0.0f)) {
            float tz = -v0 / acc;
            seg[ns++] = {0.0f, tz, v0, acc};
            seg[ns++] = {tz, tr - tz, 0.0f, acc};
        } else {
            seg[ns++] = {0.0f, tr, v0, acc};
        }
    }
    if (Ts > tr) seg[ns++] = {tr, Ts - tr, vr, 0.0f};

    // The open period: the last event's, or the idle stretch from the queue's end
    const int64_t base = s.end;
    const uint32_t pulseTicks =
        tm.len < 1u ? 1u : (tm.len > STEP_MAX_PULSE_TICKS ? STEP_MAX_PULSE_TICKS : tm.len);
    const uint32_t firstPulse = pulseTicks + 1u > STEP_MIN_PERIOD_TICKS ? pulseTicks + 1u : STEP_MIN_PERIOD_TICKS;
    Planner p = {q.buf, tm, q.head, 0,
                 s.open ? relTicks(s.openStart, base) : relTicks(s.qEnd, base),
                 s.open ? s.openFlags : 0u, s.open && s.openPulse,
                 relTicks(s.lastStep, base), relTicks(s.lastEvent, base), s.dir, pulseTicks, 0,
                 (int32_t)stepMinTail(tm, true), (int32_t)(tm.len + tm.space),
                 STEP_TAIL_TICKS + firstPulse};
    int64_t accum = s.accum;
    const float HZ = (float)STEP_TICK_HZ;
    for (int i = 0; i < ns; i++) {
        const Seg &g = seg[i];
        const float dx = g.v * g.d + 0.5f * g.a * g.d * g.d;  // steps
        const int64_t accEnd = accum + (int64_t)(dx * (float)ONE);
        const int64_t cross = (accEnd >> 28) - (accum >> 28);
        if (cross != 0) {
            const int32_t sd = cross > 0 ? 1 : -1;
            const uint32_t n = (uint32_t)(cross > 0 ? cross : -cross);
            const float frac = (float)(accum & (ONE - 1)) * (1.0f / (float)ONE);
            float D = sd > 0 ? 1.0f - frac : frac;  // to the first multiple of 2^28
            const float u0 = fabsf(g.v), aa = g.a * (float)sd;
            const float t0 = g.t0 * HZ;
            if (aa == 0.0f) {
                const float inv = u0 > 0.0f ? HZ / u0 : 0.0f;
                runSteps(p, n, sd, D, t0, g.d * HZ, [inv](float d) { return d * inv; });
            } else {
                const float u2 = u0 * u0, a2 = 2.0f * aa, twoHZ = 2.0f * HZ;
                runSteps(p, n, sd, D, t0, g.d * HZ, [u0, u2, a2, twoHZ](float d) {
                    float disc = u2 + a2 * d;
                    if (disc < 0.0f) disc = 0.0f;
                    const float den = u0 + sqrtf(disc);
                    return den > 0.0f ? twoHZ * d / den : 0.0f;
                });
            }
        }
        accum = accEnd;
    }

    // Close the window. The last period stays open: it ends at the next event, so the
    // step timing does not depend on where windows end. If it is already long, the part
    // up to STEP_CARRY_TICKS / 2 before the window's end goes into the queue (as the
    // event's period, or idle) and an idle period stays open from there. Either way the
    // queue runs at least STEP_LEAD_TICKS - STEP_CARRY_TICKS ahead.
    int32_t openAt = p.curStart;
    uint32_t openFlags = p.curFlags;
    bool openPulse = p.curPulse;
    if (T - p.curStart >= (int32_t)STEP_CARRY_TICKS) {
        openAt = T - (int32_t)(STEP_CARRY_TICKS / 2u);
        if (p.curFlags != 0)
            openAt = max32(openAt, p.curStart + (int32_t)stepMinTail(tm, p.curPulse));
        p.period((uint32_t)(openAt - p.curStart), p.curFlags);
        openFlags = 0;
        openPulse = false;
    }

    __asm__ volatile("" ::: "memory");  // the entries before the new head
    q.head = p.pos;

    s.end = base + T;
    s.qEnd = base + openAt;
    s.open = true;
    s.openStart = base + openAt;
    s.openFlags = openFlags;
    s.openPulse = openPulse;
    s.accum = accum;
    s.vel = (tr < Ts) ? v1 : vr;
    s.dir = p.dir;
    s.lastStep = p.lastStep <= FAR_PAST ? s.lastStep : base + p.lastStep;
    s.lastEvent = p.lastEvent <= FAR_PAST ? s.lastEvent : base + p.lastEvent;
    s.steps = p.steps;
    return p.n;
}

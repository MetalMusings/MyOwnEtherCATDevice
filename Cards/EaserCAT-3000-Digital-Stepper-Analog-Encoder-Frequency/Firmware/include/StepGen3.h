
#ifndef STEPGEN3
#define STEPGEN3
#include <HardwareTimer.h>

#define MAX_CHAN 16
#define MAX_CYCLE 18
#define USER_STEP_TYPE 13

#include "StepGenChannel.h"

#define MAX_STEP_TYPE 15

#define UP_PIN 0   /* output phase used for UP signal */
#define DOWN_PIN 1 /* output phase used for DOWN signal */


typedef enum CONTROL
{
    POSITION,
    VELOCITY,
    INVALID
} CONTROL;

class StepGen3
{
public:
    int step_type[MAX_CHAN] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};               // stepping types for up to 16 channels
    char *ctrl_type[MAX_CHAN] = {0};                                                                          // control type ("p"pos or "v"vel) for up to 16 channels
    int user_step_type[MAX_CYCLE] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}; // lookup table for user-defined step type
    uint32_t stepPin[MAX_CHAN] = {0};
    uint32_t dirPin[MAX_CHAN] = {0};

    stepgen_t *stepgen_array = 0;

    int num_chan = 0;          // number of step generators configured */
    long periodns;             // makepulses function period in nanosec */
    long old_periodns;         // used to detect changes in periodns */
    double periodfp;           // makepulses function period in seconds */
    double freqscale;          // conv. factor from Hz to addval counts */
    double accelscale;         // conv. Hz/sec to addval cnts/period */
    long old_dtns;             // update_freq funct period in nsec */
    double dt;                 // update_freq period in seconds */
    double recip_dt;           // recprocal of period, avoids divides */
    volatile uint64_t cnt = 0; // Debug counter
    long basePeriodNs = 10000; // Actual period of the makeAllPulses() calls in ns
    uint32_t stepPinName[MAX_CHAN] = {0}; // PinName of the step outputs, for fast GPIO
    uint32_t dirPinName[MAX_CHAN] = {0};  // PinName of the direction outputs
    GPIO_TypeDef *stepPort[MAX_CHAN] = {0}; // GPIO port and bit of the outputs, for BSRR writes
    GPIO_TypeDef *dirPort[MAX_CHAN] = {0};
    uint32_t stepMask[MAX_CHAN] = {0};
    uint32_t dirMask[MAX_CHAN] = {0};
    bool allStepDir = true;                 // every channel is step type 0: use the fast pulse path
    bool outputsWritten[MAX_CHAN] = {0};    // the outputs show the channel's state since the last write
    StepGen3(void);

    // Set the command of one channel for the next servo period.
    //   posMode: true = position command, false = velocity command
    //   resync : restart the position loop without velocity feed forward (avoids a
    //            velocity step after enable or a mode change)
    void setChannel(int ch, bool enable, bool posMode, double posCmd, double velCmd,
                    double maxVel, double maxAccel, bool resync);
    // Step timing in ns. 0 means one base period (the minimum).
    void setTiming(int ch, uint32_t stepLen, uint32_t stepSpace, uint32_t dirSetup,
                   uint32_t dirHold);
    // Update pos_fb of all channels from the DDS accumulators
    void updatePositions(void);
    // Run the position/velocity loop of all channels, servoPeriodNs = cycle time
    void updateFrequencies(long servoPeriodNs);
    void makeAllPulses(void) __attribute__((optimize("O2")));
    // True when makeAllPulses() would change nothing on any channel: no step pulse or
    // direction timing in progress, and each channel disabled or at zero frequency.
    // The base period timer can then be stopped until the next cycle.
    bool allIdle(void) const;
    int rtapi_app_main();
    int export_stepgen(int num, stepgen_t *addr, int step_type, int pos_mode);
    void make_pulses(void *arg, long period);
    void update_freq(void *arg, long period);
    void update_pos(void *arg, long period);
    int setup_user_step_type(void);
    CONTROL parse_ctrl_type(const char *ctrl);
    unsigned long ulceil(unsigned long value, unsigned long increment);

private:
    /* lookup tables for stepping types 2 and higher - phase A is the LSB */

    unsigned char master_lut[MAX_STEP_TYPE][MAX_CYCLE] = {
        {1, 3, 2, 0, 0, 0, 0, 0, 0, 0},        /* type 2: Quadrature */
        {1, 2, 4, 0, 0, 0, 0, 0, 0, 0},        /* type 3: Three Wire */
        {1, 3, 2, 6, 4, 5, 0, 0, 0, 0},        /* type 4: Three Wire Half Step */
        {1, 2, 4, 8, 0, 0, 0, 0, 0, 0},        /* 5: Unipolar Full Step 1 */
        {3, 6, 12, 9, 0, 0, 0, 0, 0, 0},       /* 6: Unipoler Full Step 2 */
        {1, 7, 14, 8, 0, 0, 0, 0, 0, 0},       /* 7: Bipolar Full Step 1 */
        {5, 6, 10, 9, 0, 0, 0, 0, 0, 0},       /* 8: Bipoler Full Step 2 */
        {1, 3, 2, 6, 4, 12, 8, 9, 0, 0},       /* 9: Unipolar Half Step */
        {1, 5, 7, 6, 14, 10, 8, 9, 0, 0},      /* 10: Bipolar Half Step */
        {1, 2, 4, 8, 16, 0, 0, 0, 0, 0},       /* 11: Five Wire Unipolar */
        {3, 6, 12, 24, 17, 0, 0, 0, 0, 0},     /* 12: Five Wire Wave */
        {1, 3, 2, 6, 4, 12, 8, 24, 16, 17},    /* 13: Five Wire Uni Half */
        {3, 7, 6, 14, 12, 28, 24, 25, 17, 19}, /* 14: Five Wire Wave Half */
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}         /* 15: User-defined */
    };

    unsigned char cycle_len_lut[14] =
        {4, 3, 6, 4, 4, 4, 4, 8, 8, 5, 5, 10, 10, 0};

    unsigned char num_phases_lut[14] =
        {2, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 0};
};

// Default base period (ns) of the step pulse generation, until overridden by SDO 0x2000.
// 10000 ns => 100 kHz. Higher rates may work, depending on the overall load.
#define BASE_PERIOD 10000

#endif


#ifndef STEPGEN3
#define STEPGEN3
#include <HardwareTimer.h>

#define MAX_CHAN 16
#define MAX_CYCLE 18
#define USER_STEP_TYPE 13

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
    double old_pos_cmd;                 /* previous position command (counts) */
    int count;                          /* pin: captured feedback in counts */
#define double float
    double pos_scale;          /* param: steps per position unit */
    double old_scale;          /* stored scale value */
    double scale_recip;        /* reciprocal value used for scaling */
    double vel_cmd;            /* pin: velocity command (pos units/sec) */
    double pos_cmd;            /* pin: position command (position units) */
    double pos_fb;             /* pin: position feedback (position units) */
    double freq;               /* param: frequency command */
    double maxvel;             /* param: max velocity, (pos units/sec) */
    double maxaccel;           /* param: max accel (pos units/sec^2) */
    unsigned int old_step_len; /* used to detect parameter changes */
    unsigned int old_step_space;
    unsigned int old_dir_hold_dly;
    unsigned int old_dir_setup;
    int printed_error; /* flag to avoid repeated printing */
} stepgen_t;

#define MAX_STEP_TYPE 15

#define STEP_PIN 0 /* output phase used for STEP signal */
#define DIR_PIN 1  /* output phase used for DIR signal */
#define UP_PIN 0   /* output phase used for UP signal */
#define DOWN_PIN 1 /* output phase used for DOWN signal */

#define PICKOFF 28 /* bit location in DDS accum */

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
#undef double
    StepGen3(void);
    void updateStepGen(double pos_cmd1, double pos_cmd2);
    void makeAllPulses(void);
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
        {
            2,
            3,
            3,
            4,
            4,
            4,
            4,
            4,
            4,
            5,
            5,
            5,
            5,
            0,
    };
};

// For the example
#define BASE_PERIOD 40000 
#define SERVO_PERIOD 1000000
#define JOINT_X_STEPGEN_MAXACCEL 0.0
#define JOINT_Z_STEPGEN_MAXACCEL 0.0
#define JOINT_X_SCALE -200
#define JOINT_Z_SCALE -80

#endif

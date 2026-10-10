#ifndef MOTION_CONTROL_H
#define MOTION_CONTROL_H
#include <stdint.h>
#include "pid.h"

/* Operator commands are signed speed levels (100..1500, step 100).
   Feedback and the profiled target are encoder rising-edge pulses/s.
   Shaft RPM and ground speed require a separately verified reduction ratio. */
#define MOTION_MAX_LEVEL 1500
#define MOTION_CPS_PER_LEVEL 12
#define MOTION_MAX_CPS (MOTION_MAX_LEVEL * MOTION_CPS_PER_LEVEL)
#define MOTION_PWM_LIMIT 500
enum { MOTION_FAULT_TIMING=1, MOTION_FAULT_ENCODER=2,
       MOTION_FAULT_DIRECTION=4, MOTION_FAULT_STALL=8, MOTION_FAULT_OVERSPEED=16 };
typedef struct {
    PID_t pid[2];
    float command, acceleration, measured[2];
    float duty[2], fraction[2];
    uint32_t wrong_ms[2], stalled_ms[2], overspeed_ms[2], reverse_ms;
    int8_t direction;
    uint8_t fault;
} MotionControl;
void Motion_Init(MotionControl *m);
/* Progressively admit steering while the profiled and measured wheel speed
   falls into the turn envelope. Full steering is available below 1800 CPS. */
int16_t Motion_LimitSteering(const MotionControl *m, int16_t requested);
/* Zero requests always disable both outputs immediately and clear a latched
   fault. Nonzero requests cannot clear a feedback fault or restart a wheel. */
void Motion_Update(MotionControl *m, int16_t request_level, int16_t steering,
                   int16_t delta1, int16_t delta2, uint32_t elapsed_us,
                   uint8_t encoder_valid, int16_t output[2]);
#endif

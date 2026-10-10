#include "motion_control.h"
#include <math.h>
#include <string.h>

static float bound(float v,float lo,float hi) { return v<lo?lo:v>hi?hi:v; }
static void reset_drive(MotionControl *m) {
    m->command=0; m->acceleration=0;
    for(int i=0;i<2;i++) {
        PID_Reset(&m->pid[i]);m->duty[i]=0;m->fraction[i]=0;
        m->wrong_ms[i]=0;m->stalled_ms[i]=0;m->overspeed_ms[i]=0;
    }
}
void Motion_Init(MotionControl *m) {
    memset(m,0,sizeof(*m));
    for(int i=0;i<2;i++) PID_Init(&m->pid[i],0.012f,0.06f,0.0002f,0,MOTION_PWM_LIMIT);
}
int16_t Motion_LimitSteering(const MotionControl *m,int16_t requested) {
    float speed=fmaxf(fabsf(m->command),fmaxf(fabsf(m->measured[0]),fabsf(m->measured[1])));
    float limit=bound((MOTION_MAX_CPS-speed)*1000.0f/(MOTION_MAX_CPS-1800.0f),0,1000);
    return (int16_t)bound(requested,-limit,limit);
}
void Motion_Update(MotionControl *m,int16_t request,int16_t steering,
                   int16_t d1,int16_t d2,uint32_t us,uint8_t valid,int16_t out[2]) {
    out[0]=out[1]=0;
    if(!request) m->fault=0;
    if(us<1000U || us>100000U) {
        if(request) m->fault|=MOTION_FAULT_TIMING;
        reset_drive(m);return;
    }
    const float dt=us/1000000.0f;
    const uint32_t ms=(us+999U)/1000U;
    const float alpha=dt/(0.080f+dt);
    const int16_t deltas[2]={d1,d2};
    if(valid) for(int i=0;i<2;i++)
        m->measured[i]+=alpha*(deltas[i]/dt-m->measured[i]);
    if(!request) {
        m->fault=0;m->reverse_ms=0;reset_drive(m);return;
    }
    if(!valid) m->fault|=MOTION_FAULT_ENCODER;
    if(m->fault) { reset_drive(m);return; }
    request=(int16_t)bound(request,-MOTION_MAX_LEVEL,MOTION_MAX_LEVEL);
    const int8_t direction=request>0?1:-1;
    /* Coast before reversing; never apply opposite torque to a moving wheel. */
    if(m->direction && direction!=m->direction) {
        reset_drive(m);m->reverse_ms+=ms;
        if(m->reverse_ms<150U || fabsf(m->measured[0])>30 || fabsf(m->measured[1])>30) return;
    }
    m->direction=direction;m->reverse_ms=0;
    /* Continuous turn reduction for every motion source, without a speed boost. */
    float turn=fabsf((float)steering);if(turn>1000)turn=1000;
    const float cap=MOTION_MAX_CPS-turn*(MOTION_MAX_CPS-1800.0f)/1000.0f;
    float target=bound(fabsf((float)request)*MOTION_CPS_PER_LEVEL,0,cap);
    float current=fabsf(m->command);
    const float desired=target>current?2500.0f:-4000.0f;
    m->acceleration+=bound(desired-m->acceleration,-8000.0f*dt,8000.0f*dt);
    float next=current+m->acceleration*dt;
    if((target-current)*(target-next)<=0) { next=target;m->acceleration=0; }
    /* A lower level/turn cap decelerates progressively. Do not clamp the
       profiled speed straight to the new target or allow a reduction to
       briefly accelerate while the jerk limiter changes sign. */
    if(target<current) next=fminf(next,current);
    next=bound(next,0,MOTION_MAX_CPS);m->command=direction*next;
    for(int i=0;i<2;i++) {
        float measured=direction*m->measured[i];
        /* Original no-load calibration: 1100 pulses/17ms at PWM=1000.
           Use only as feed-forward; actual CPS feedback corrects load changes. */
        /* Current bench: no pulses below PWM~40; motion starts at 42..47.
           A gradual 38-unit friction bias avoids a many-second integrator wait.
           Unlike the old 200 floor this is cancellable by negative PI feedback. */
        float feedforward=38.0f*fminf(next/100.0f,1.0f)+next*(1000.0f*0.017f/1100.0f);
        /* Reserve headroom before integration so total saturation cannot wind up. */
        m->pid[i].out_min=-feedforward;
        m->pid[i].out_max=MOTION_PWM_LIMIT-feedforward;
        float duty=bound(feedforward+PID_Update(&m->pid[i],next,measured,dt),0,MOTION_PWM_LIMIT);
        /* Signed speed feedback is never converted into reverse braking PWM. */
        if(measured < -50.0f && m->duty[i]>10) m->wrong_ms[i]+=ms;
        else m->wrong_ms[i]=0;
        if(fabsf(m->measured[i])<1.0f && m->duty[i]>=80) m->stalled_ms[i]+=ms;
        else m->stalled_ms[i]=0;
        if(measured>fmaxf(next*1.6f+150.0f,350.0f)) m->overspeed_ms[i]+=ms;
        else m->overspeed_ms[i]=0;
        if(m->wrong_ms[i]>=150U) m->fault|=MOTION_FAULT_DIRECTION;
        if(m->stalled_ms[i]>=1000U) m->fault|=MOTION_FAULT_STALL;
        if(m->overspeed_ms[i]>=400U) m->fault|=MOTION_FAULT_OVERSPEED;
        m->duty[i]+=bound(duty-m->duty[i],-400.0f*dt,150.0f*dt);
        float quantized=m->duty[i]+m->fraction[i];
        int16_t pwm=(int16_t)quantized;m->fraction[i]=quantized-pwm;
        out[i]=direction*pwm;
    }
    if(m->fault) { out[0]=out[1]=0;reset_drive(m); }
}

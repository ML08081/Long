"""Run the real controller with quantized, unequal and loaded wheel plants."""
from pathlib import Path
import os, subprocess, sys, tempfile
f4=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='motion-cps-') as td:
    p=Path(td)
    (p/'test.c').write_text(r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "motion_control.h"
static void simulate(int command,int jitter,int loaded,int steering) {
  MotionControl m;Motion_Init(&m);float velocity[2]={0},position[2]={0};int previous[2]={0};
  float elapsed=0,total=0,window=0,peak=0;int16_t output[2]={0};
  for(int n=0;elapsed<13;n++) {
    unsigned us=jitter?(n%7==0?35000:n%3==0?21000:17000):17000;
    float dt=us/1e6f;int16_t delta[2];
    for(int i=0;i<2;i++) {
      /* Add friction/load after running: a small fixed PWM ceiling would
         pass the unloaded model but fail this change in required torque. */
      float gain=loaded && elapsed>3 ? (i?42.0f:45.0f) : (i?58.0f:65.0f);
      float deadzone=loaded && elapsed>3 ? (i?75.0f:70.0f) : (i?42.0f:40.0f);
      float plant=gain*fmaxf(0,(float)abs(output[i])-deadzone)*(output[i]<0?-1:1);
      velocity[i]+=(plant-velocity[i])*(dt/(0.15f+dt));
      position[i]+=velocity[i]*dt;int count=(int)position[i];delta[i]=count-previous[i];previous[i]=count;
    }
    Motion_Update(&m,command,steering,delta[0],delta[1],us,1,output);
    assert(!m.fault && output[0]*command>=0 && output[1]*command>=0 && abs(output[0])<=500 && abs(output[1])<=500);
    assert(fabsf(m.command)<=abs(command)*12+.001f);
    if(elapsed>2 && !loaded) assert(fabsf(velocity[0])>100 && fabsf(velocity[1])>100);
    if(elapsed>11) {total+=(velocity[0]+velocity[1])*0.5f*dt;window+=dt;}
    if(fabsf(velocity[0])>peak) peak=fabsf(velocity[0]);
    elapsed+=dt;
  }
  float expected=fminf(abs(command)*12.0f,24000.0f-22.2f*abs(steering))*(command>0?1:-1);
  assert(fabsf(m.command-expected)<.01f);
  /* The unchanged PWM=500 ceiling can saturate under load. Validate
     attainable steady speed for the actual asymmetric plants, rather than
     demanding more torque than the retained limit permits. */
  float reachable=loaded ? (fminf(fabsf(expected),45.0f*(500-70))+fminf(fabsf(expected),42.0f*(500-75)))*.5f : fabsf(expected);
  if(command<0) reachable=-reachable;
  float average=total/window;assert(fabsf(average-reachable)<fabsf(reachable)*.12f+20);
  if(loaded && fabsf(expected)>=6000) assert(abs(output[0])>120 || abs(output[1])>120);
  printf("PASS: level=%d target=%.0f cps jitter=%d loaded=%d steady=%.1f cps peak=%.1f PWM=%d/%d\n",command,expected,jitter,loaded,average,peak,output[0],output[1]);
}
int main(void) {
  for(int v=100;v<=2000;v+=100){simulate(v,0,0,0);simulate(v,1,0,0);simulate(-v,1,0,0);simulate(v,1,1,0);}
  simulate(500,1,0,1000);simulate(1000,1,1,1000);simulate(100,1,1,1000);
  MotionControl m;int16_t out[2];Motion_Init(&m);
  m.direction=1;m.command=1800;m.measured[0]=m.measured[1]=1800;
  Motion_Update(&m,500,1000,30,30,17000,1,out);
  assert(fabsf(m.command-1800)<.01f);
  Motion_Update(&m,0,0,2,2,17000,1,out);assert(out[0]==0 && out[1]==0 && !m.fault);
  Motion_Update(&m,500,0,0,0,17000,0,out);assert(m.fault&MOTION_FAULT_ENCODER);
  Motion_Update(&m,500,0,0,0,17000,1,out);assert(out[0]==0 && out[1]==0);
  Motion_Update(&m,0,0,0,0,17000,1,out);assert(!m.fault);
  Motion_Update(&m,500,0,0,0,120000,1,out);assert(m.fault&MOTION_FAULT_TIMING);
  Motion_Init(&m);for(int n=0;n<800 && !m.fault;n++)Motion_Update(&m,1000,0,0,0,17000,1,out);
  assert(m.fault&MOTION_FAULT_STALL);assert(!out[0] && !out[1]);
  Motion_Init(&m);for(int n=0;n<300 && !m.fault;n++)Motion_Update(&m,500,0,-20,-20,17000,1,out);
  assert(m.fault&MOTION_FAULT_DIRECTION);assert(!out[0] && !out[1]);
  Motion_Init(&m);for(int n=0;n<100 && !m.fault;n++)Motion_Update(&m,100,0,170,170,17000,1,out);
  assert(m.fault&MOTION_FAULT_OVERSPEED);assert(!out[0] && !out[1]);
  Motion_Update(&m,0,0,0,0,120000,0,out);assert(!m.fault && !out[0] && !out[1]);
  Motion_Init(&m);m.direction=1;m.command=1200;m.measured[0]=m.measured[1]=1200;
  Motion_Update(&m,-100,0,20,20,17000,1,out);assert(!out[0] && !out[1]);
  for(int n=0;n<5;n++){Motion_Update(&m,-100,0,0,0,17000,1,out);assert(!out[0] && !out[1]);}
  /* A reduction in either level or turn cap must decelerate smoothly,
     while zero remains an immediate stop. Test the populated PI state. */
  Motion_Init(&m);m.direction=1;m.command=6000;m.measured[0]=m.measured[1]=6000;
  for(int n=0;n<120;n++) {
    float previous=m.command;
    Motion_Update(&m,100,0,(int16_t)(previous*.017f),(int16_t)(previous*.017f),17000,1,out);
    assert(!m.fault && m.command<=previous+.001f && previous-m.command<=4000*.017f+.01f);
  }
  assert(fabsf(m.command-1200)<.01f);
  Motion_Init(&m);m.direction=1;m.command=12000;m.measured[0]=m.measured[1]=12000;
  for(int n=0;n<200;n++) {
    float previous=m.command;
    Motion_Update(&m,1000,1000,(int16_t)(previous*.017f),(int16_t)(previous*.017f),17000,1,out);
    assert(!m.fault && previous-m.command<=4000*.017f+.01f);
  }
  assert(fabsf(m.command-1800)<.01f);
  Motion_Update(&m,0,0,30,30,17000,1,out);assert(!out[0] && !out[1]);
  /* A sudden full-turn request at maximum speed cannot bypass the envelope,
     even when the target has fallen but a loaded wheel is still moving. */
  Motion_Init(&m);m.command=24000;m.measured[0]=m.measured[1]=24000;
  assert(Motion_LimitSteering(&m,1000)==0);
  m.command=6000;m.measured[0]=6000;m.measured[1]=9000;
  int limited=Motion_LimitSteering(&m,1000);
  assert(limited>0 && limited<1000 && 24000-22.2f*limited>=9000-.01f);
  assert(Motion_LimitSteering(&m,-1000)==-limited);
  assert(Motion_LimitSteering(&m,100)==100);
  m.command=1800;m.measured[0]=m.measured[1]=-1800;
  assert(Motion_LimitSteering(&m,1000)==1000 && Motion_LimitSteering(&m,-1000)==-1000);
  assert(Motion_TelemetryCps(24000)==24000 && Motion_TelemetryCps(-24000)==-24000);
  assert(Motion_TelemetryCps(40000)==32767 && Motion_TelemetryCps(-40000)==-32768);
  puts("PASS: loaded drive, smooth level/turn reductions, stop, fault latch and reverse coast");
}
''')
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-I'+str(f4/'Core/Motor'),'-I'+str(f4/'Core/PID'),str(p/'test.c'),str(f4/'Core/Motor/motion_control.c'),str(f4/'Core/PID/pid.c'),'-lm','-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)

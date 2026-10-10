"""Exercise the actual F4 obstacle unlock block against held remote commands."""
from pathlib import Path
import subprocess,tempfile,sys
f4=Path(sys.argv[1]).resolve();s=(f4/'Core/Src/freertos.c').read_text()
a=s.index('      /* (2) 前进意图边沿检测');b=s.index('    }\n    else\n    {\n      /* 切出遥控模式',a)
block=s[a:b]
with tempfile.TemporaryDirectory(prefix='obstacle-intent-') as td:
    p=Path(td)
    (p/'test.c').write_text('''#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#define LOG(...) ((void)0)
#define OBS_UNLOCK_WINDOW_MS 5000U
#define OBS_LOCK_OFF_MM 120U
#define OBS_UNLOCK_NEED 2U
static uint8_t s_obs_lock=1,s_obs_unlock_cnt=0;
static int16_t s_obs_prev_cmd_spd=0,current_speed=0;
static uint32_t s_obs_last_intent_ms=0;
static void step(uint8_t operator_update,int16_t operator_speed,uint16_t near_mm,uint32_t now_ms){
 if(operator_update)current_speed=operator_speed;
'''+block+'''
}
int main(void){
 step(1,500,50,10);assert(s_obs_lock && !s_obs_unlock_cnt && !current_speed);
 for(int i=0;i<100;i++){
   step(i%2==0,500,200,100+i*17);
   assert(s_obs_lock && !s_obs_unlock_cnt && !current_speed);
 }
 step(1,0,200,2000);step(1,500,200,2017);assert(s_obs_lock && s_obs_unlock_cnt==1);
 for(int i=0;i<30;i++)step(i%2==0,500,200,2100+i*17);
 assert(s_obs_lock && s_obs_unlock_cnt==1);
 step(1,0,200,3000);step(1,500,200,3017);assert(!s_obs_lock && current_speed==500);
 s_obs_lock=1;s_obs_prev_cmd_spd=0;s_obs_unlock_cnt=0;s_obs_last_intent_ms=0;
 step(1,500,200,4000);assert(s_obs_unlock_cnt==1);
 step(1,0,200,10000);step(1,500,200,10017);assert(s_obs_lock && s_obs_unlock_cnt==1);
 puts("PASS: held forward cannot auto-unlock after safety zero; two real release/forward edges required; timeout resets confirmation");
}
''')
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)

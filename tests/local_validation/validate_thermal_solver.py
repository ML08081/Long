from pathlib import Path
import subprocess,tempfile
import sys
r=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='patrol-solver-test-') as td:
 t=Path(td)
 (t/'test.cpp').write_text(r'''
#include "modules/thermal/ThermalSolver.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
static int extracts=0;
static bool bad=false;
static int corrections=0;
extern "C" {
void MLX90640_BadPixelsCorrection(uint16_t*,float*,int,paramsMLX90640*) { ++corrections; }
int MLX90640_ExtractParameters(uint16_t*,paramsMLX90640*) { ++extracts; return 0; }
float MLX90640_GetTa(uint16_t*,const paramsMLX90640*) { return 25; }
void MLX90640_CalculateTo(uint16_t* f,const paramsMLX90640*,float,float,float* out) {
  for(int i=f[833];i<768;i+=2) out[i]=bad?std::numeric_limits<float>::quiet_NaN():float(25+f[833]);
}
}
int main() {
 patrol::ThermalSolver s;
 uint16_t ee[832]={}, raw[834]={}; int16_t out[768]={};
 assert(!s.solve(raw,out)); assert(s.setEeprom(ee)); assert(extracts==1);
 raw[833]=0; assert(!s.solve(raw,out)); assert(!s.solve(raw,out));
 assert(s.setEeprom(ee)); assert(extracts==1); // calibration refresh cannot discard partial image
 raw[833]=1; assert(s.solve(raw,out));
 assert(corrections==2);
 for(int i=0;i<768;i++) assert(out[i]==(i%2?2600:2500));
 raw[833]=1; assert(!s.solve(raw,out)); s.resetFrame(); raw[833]=0;
 assert(!s.solve(raw,out)); raw[833]=1; assert(s.solve(raw,out));
 raw[833]=2; assert(!s.solve(raw,out)); bad=true;
 raw[833]=0; assert(!s.solve(raw,out));raw[833]=1; assert(!s.solve(raw,out));
 std::puts("PASS: actual ThermalSolver requires both new subpages, resets wake state, caches calibration and rejects invalid/non-finite temperatures");
}
''')
 subprocess.run(['c++','-std=c++11','-pthread','-I'+str(r/'include'),'-I'+str(r/'third_party/mlx90640'),str(t/'test.cpp'),str(r/'src/modules/thermal/ThermalSolver.cpp'),str(r/'src/modules/logger/Logger.cpp'),'-o',str(t/'test')],check=True)
 subprocess.run([str(t/'test')],check=True)

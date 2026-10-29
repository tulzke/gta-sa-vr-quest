"""Compile production lean maths and real Driving.cpp routing with fake poses."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / 'native/src/Driving.cpp').read_text(encoding='utf8')


def function(signature):
    start = SOURCE.index(signature)
    brace = SOURCE.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (SOURCE[end] == '{') - (SOURCE[end] == '}')
        end += 1
    return SOURCE[start:end]


def compile_run(source, name, output):
    exe = output / (name + ('.exe' if os.name == 'nt' else ''))
    if os.name == 'nt':
        vc = os.environ.get('VS_VCVARS64')
        if not vc:
            where = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
            base = subprocess.check_output([str(where), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
            vc = str(Path(base) / 'VC/Auxiliary/Build/vcvars64.bat')
        command = output / (name + '.cmd')
        command.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc /W4 /I"{ROOT / "native/src"}" "{source}" /Fe:"{exe}" /Fo:"{output / (name + ".obj")}"\nif errorlevel 1 exit /b 1\n"{exe}"\n', encoding='utf8')
        subprocess.run(['cmd', '/c', str(command)], cwd=output, check=True)
    else:
        compiler = os.environ.get('CXX') or shutil.which('c++')
        subprocess.run([compiler, '-std=c++20', '-Wall', '-Wextra', '-I'+str(ROOT/'native/src'), str(source), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True, cwd=output)


PREFIX = r'''
#define _CRT_SECURE_NO_WARNINGS
#include "Driving.h"
#include "BikeSteering.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
using namespace savr;
using namespace savr::driving;
namespace savr::driving {
int checks=0,saveCount=0,resetCount=0;
#define CHECK(c) do { ++checks; if(!(c)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c);std::exit(1);} } while(false)
bool Near(float a,float b) {return std::abs(a-b)<0.0001f;}
struct V3 {float x{},y{},z{};};
V3 operator*(V3 v,float k){return {v.x*k,v.y*k,v.z*k};}
float Dot(V3 a,V3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
V3 Cross(V3 a,V3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float Length(V3 v){return std::sqrt(Dot(v,v));}
V3 Normalized(V3 v){const float n=Length(v);return n>0.000001f?v*(1/n):V3{};}
std::atomic<bool> g_bikeLeanSteering{true};
std::atomic<int> g_bikeLeanSensitivity{100};
bikesteering::State g_bikeSteering;
double clockTime=0;
double MonotonicSeconds(){return clockTime;}
alignas(16) char bikeBytes[4096]{};
bool speedAvailable=true;
bool GetActivePlayerBike(void** out,void*){*out=bikeBytes;return speedAvailable;}
bool ReadEntityMatrix(void*,V3* right,V3* forward,V3* up,V3* position){
    *right={1,0,0};*forward={0,1,0};*up={0,0,1};*position={};return true;
}
void EnsureInit(){}
void ResetInteraction(){++resetCount;g_bikeSteering.Reset();}
void Save(){++saveCount;}
'''

TESTS = r'''
int main(){
    std::uintptr_t handling=reinterpret_cast<std::uintptr_t>(bikeBytes+2048);
    std::memcpy(bikeBytes+0x4a8,&handling,sizeof(handling));
    *reinterpret_cast<float*>(bikeBytes+2048+0xa0)=30.0f;
    *reinterpret_cast<V3*>(bikeBytes+0x68)={0,0.2f,0};
    WheelVisualState visual{};visual.bike=true;visual.active=true;
    V3 positions[2]={{0,0,0},{0.62f,0,0}};
    Route(visual,positions,true,true,true,false,0,522);
    positions[1]={0.56f,0.26f,0};
    for(int i=0;i<90;++i){clockTime+=1.0/90;Route(visual,positions,true,true,true,false,0,522);}
    CHECK(visual.steering>0.98f);
    CHECK(Near(visual.physicalAngle,0)); // a lean is not a synthetic bar yaw
    for(int i=0;i<90;++i){clockTime+=1.0/90;Route(visual,positions,true,true,true,false,0,481);}
    CHECK(visual.steering>0.98f);
    CHECK(Near(*reinterpret_cast<float*>(bikeBytes+0x730),visual.steering));
    CHECK(Near(*reinterpret_cast<float*>(bikeBytes+0x600),visual.steering*30*3.14159265f/180));
    // Without speed, the callback must fall back to yaw, rather than carrying
    // a full lean into a stopped/unknown vehicle.
    speedAvailable=false;
    for(int i=0;i<90;++i){clockTime+=1.0/90;Route(visual,positions,true,true,true,false,-0.4f,522);}
    CHECK(Near(visual.steering,-0.4f));speedAvailable=true;
    for(int i=0;i<90;++i){clockTime+=1.0/90;Route(visual,positions,false,true,true,false,0.3f,522);}
    CHECK(Near(visual.steering,0.3f));CHECK(!g_bikeSteering.referenceValid);
    Route(visual,positions,true,true,false,false,-0.7f,481);
    CHECK(Near(visual.steering,-0.7f));CHECK(!g_bikeSteering.clockValid);
    CHECK(Near(*reinterpret_cast<float*>(bikeBytes+0x730),-0.7f));
    Route(visual,positions,true,true,true,true,0,522);
    CHECK(Near(visual.steering,0));CHECK(!g_bikeSteering.clockValid);
    // Level the input axes even when the model frame rolls by 40 degrees.
    V3 right,forward;
    visual.right[0]=std::cos(40*bikesteering::kRadians);
    visual.right[1]=std::sin(40*bikesteering::kRadians);
    visual.up[2]=-1;
    LevelBasis(visual,right,forward);
    CHECK(Near(right.x,1)&&Near(right.y,0)&&Near(right.z,0));
    CHECK(Near(forward.z,-1));
    g_bikeLeanSteering=false;
    LevelBasis(visual,right,forward);
    CHECK(Near(right.y,visual.right[1])); // BARS ONLY retains the old axes
    CHECK(!IsBikeLeanSteeringEnabled());ToggleBikeLeanSteering();
    CHECK(IsBikeLeanSteeringEnabled()&&resetCount==1&&saveCount==1);
    g_bikeLeanSensitivity=200;AdjustBikeLeanSensitivityPercent(1);
    CHECK(GetBikeLeanSensitivityPercent()==200);
    g_bikeLeanSensitivity=25;AdjustBikeLeanSensitivityPercent(-1);
    CHECK(GetBikeLeanSensitivityPercent()==25);
    const int saves=saveCount;AdjustBikeLeanSensitivityPercent(0);CHECK(saveCount==saves);
    ReadSettings("");CHECK(g_bikeLeanSteering&&g_bikeLeanSensitivity==100);
    ReadSettings("BikeLeanSteering=0\nBikeLeanSensitivityPercent=145\n");
    CHECK(!g_bikeLeanSteering&&g_bikeLeanSensitivity==145);
    FILE* file=std::tmpfile();CHECK(file);WriteSettings(file);std::rewind(file);
    char saved[200]{};const auto count=std::fread(saved,1,sizeof(saved)-1,file);std::fclose(file);
    CHECK(count>0);g_bikeLeanSteering=true;g_bikeLeanSensitivity=100;
    ReadSettings(saved);CHECK(!g_bikeLeanSteering&&g_bikeLeanSensitivity==145);
    ReadSettings("BikeLeanSteering=99\nBikeLeanSensitivityPercent=-200\n");
    CHECK(!g_bikeLeanSteering&&g_bikeLeanSensitivity==25);
    ReadSettings("BikeLeanSteering=1\nBikeLeanSensitivityPercent=999\n");
    CHECK(g_bikeLeanSteering&&g_bikeLeanSensitivity==200);
    std::printf("PASS: %d production bike/bicycle routing, level axes and settings checks\n",checks);
}
'''


def integration():
    begin = SOURCE.index('    visual.steering=steering;\n    if (leanSteering')
    end = SOURCE.index('    if (appearance==5)', begin)
    route = SOURCE[begin:end]
    begin = SOURCE.index('    if (visual.bike && IsBicycleModel(activeModel))')
    brace = SOURCE.index('{', begin)
    depth, end = 1, brace + 1
    while depth:
        depth += (SOURCE[end] == '{') - (SOURCE[end] == '}')
        end += 1
    route += SOURCE[begin:end]
    begin = SOURCE.index('    V3 right{visual.right[0]', SOURCE.index('void UpdateInput('))
    end = SOURCE.index('    V3 positions[2]', begin)
    axes = SOURCE[begin:end]
    begin = SOURCE.index('            else if (std::sscanf(line, "BikeLeanSteering=')
    end = SOURCE.index('            else if (std::sscanf(line, "KeepRiderOnFlips=', begin)
    read = SOURCE[begin:end]
    begin = SOURCE.index('    g_bikeLeanSteering.store(bikeLeanSteering==1);')
    end = SOURCE.index('\n', SOURCE.index('g_bikeLeanSensitivity.store(', begin))
    read_finish = SOURCE[begin:end]
    begin = SOURCE.index('    std::fprintf(file, "BikeLeanSteering=')
    end = SOURCE.index('    std::fprintf(file, "KeepRiderOnFlips=', begin)
    write = SOURCE[begin:end]
    accessors = '\n'.join(function(sig) for sig in (
        'bool IsBikeLeanSteeringEnabled(', 'void ToggleBikeLeanSteering(',
        'int GetBikeLeanSensitivityPercent(', 'void AdjustBikeLeanSensitivityPercent('))
    return PREFIX + function('bool IsBicycleModel(') + accessors + '''
void Route(WheelVisualState& visual,V3 positions[2],bool left,bool rightGrabbed,
           bool leanSteering,bool preview,float steering,int activeModel) {
''' + route + '''
}
void LevelBasis(WheelVisualState visual,V3& rightOut,V3& forwardOut) {
''' + axes + '\nrightOut=right;forwardOut=up;\n}\n' + '''
void ReadSettings(const char* text){
    int bikeLeanSteering=1,bikeLeanSensitivity=100;
    while(*text){char line[128]{};int i=0;while(*text&&*text!='\\n'&&i<126)line[i++]=*text++;
    if(*text=='\\n')++text;int value=0;if(false){}
''' + read + '\n}\n' + read_finish + '\n}\nvoid WriteSettings(FILE* file){\n' + write + '\n}\n}\n' + TESTS


def main():
    with tempfile.TemporaryDirectory(prefix='savr-bike-steering-') as temp:
        output = Path(temp)
        compile_run(ROOT/'tools/tests/bike-steering.cpp', 'bike-maths', output)
        source = output/'routing.cpp'
        source.write_text(integration(), encoding='utf8')
        compile_run(source, 'bike-routing', output)
        source = output/'lifecycle.cpp'
        source.write_text(lifecycle(), encoding='utf8')
        compile_run(source, 'bike-lifecycle', output)


def lifecycle():
    begin=SOURCE.index('bool g_grabbed[2]')
    end=SOURCE.index('struct ModelCalibration',begin)
    globals_text=SOURCE[begin:end]
    begin=SOURCE.index('    if (g_grabVehicle!=activeVehicle)')
    end=SOURCE.index('    if (!gameplay||!CabinVisualContext())',begin)
    guard=SOURCE[begin:end]
    begin=SOURCE.index('    const bool bicycleMotion=appearance==2')
    end=SOURCE.index('    g_bicycleGestureActive=false;',begin)
    motion=SOURCE[begin:end]
    return r'''
#include "Driving.h"
#include "BikeSteering.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
namespace savr::driving {
struct V3 {float x{},y{},z{};};
namespace xr {struct HandPose{};void GetHandPoses(HandPose*){}}
std::atomic<bool> g_hornPressed{false};
std::atomic<int> g_bicycleMode{BICYCLE_MOTION};
WheelVisualState g_visual;
void* g_grabVehicle{};
''' + globals_text + function('void ClearGrabStateLocked(') + r'''
void ClearInteractionLocked(){ClearGrabStateLocked();}
void UpdateBicycleGestureLocked(const xr::HandPose*){
    g_bicycleGestureActive=true;g_bicyclePedalPulse+=0.1f;
}
''' + function('bool IsBicycleModel(') + r'''
void Vehicle(void* activeVehicle){
''' + guard + r'''
}
void Motion(void* activeVehicle,bool preview=false){
''' + guard + r'''
    const int appearance=2,activeModel=481;
''' + motion + r'''
}
}
int main(){
    using namespace savr::driving;
    int bikeA=0,bikeB=0;
    for(int i=0;i<20;++i)Motion(&bikeA);
    if(g_bicyclePedalPulse<1.9f||!g_bicycleGestureActive)return 1;
    g_bikeSteering.referenceValid=true;g_grabbed[0]=true;
    Vehicle(&bikeA);
    if(!g_bikeSteering.referenceValid||!g_grabbed[0])return 2;
    Vehicle(&bikeB);
    if(g_bikeSteering.referenceValid||g_grabbed[0]||g_bicyclePedalPulse!=0)return 3;
    g_bikeSteering.referenceValid=true;ClearGrabStateLocked();
    if(g_bikeSteering.referenceValid)return 4;
    Motion(&bikeB);Motion(&bikeB);
    if(g_bicyclePedalPulse<0.19f)return 5;
    Motion(&bikeB,true);
    if(g_bicycleGestureActive||g_bicyclePedalPulse!=0)return 6;
    std::puts("PASS: production bicycle MOTION continuity, vehicle swaps, grab reset and menu preview");
}
'''


if __name__ == '__main__':
    main()

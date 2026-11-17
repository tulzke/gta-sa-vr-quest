"""Exercise production menu visibility, camera edits, fine adjustment and reset."""
from pathlib import Path
import os
import subprocess
import re

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/water-camera-settings-tests'
OUT.mkdir(parents=True,exist_ok=True)
source=(ROOT/'native/src/Driving.cpp').read_text(encoding='utf8')
def function(name):
    start=re.search(r'^(?:bool|float|void|int) '+name+r'\(',source,re.M).start()
    end=source.index('{',start)+1
    depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}')
        end+=1
    return source[start:end]+'\n'

code=r'''
#include "Driving.h"
#include "WaterMonitorCamera.h"
#include "MenuPrecision.h"
#include <mutex>
#include <map>
#include <atomic>
#include <cassert>
#include <cstdio>
#include <iterator>
namespace savr::driving {
int model=407,mode=MODE_IMMERSIVE,saves{};
int GetActiveVehicleModelId(){return model;}
int GetModeForVehicleType(int){return mode;}
void EnsureInit(){}
void Save(){++saves;}
namespace liftlever {bool Supported(int){return false;}}
bool NitroCompatible(int){return false;}
bool HydraulicsCompatible(int){return false;}
struct ModelCalibration {watermonitor::CameraSettings monitorCamera=watermonitor::DefaultCamera;};
std::mutex g_modelMutex;
std::map<int,ModelCalibration> g_modelCalibration;
std::atomic<bool> g_waterCameraPreview{};
'''
start=source.index('constexpr int kDefaultBikeItems[]')
end=source.index('} // namespace',start)
code+=source[start:end]
for name in ('IsWaterMonitorCameraMenuItem','IsHydraulicsMenuItem','GetMenuItemCount','GetMenuItemForRow',
             'GetWaterMonitorCameraSetting','AdjustWaterMonitorCameraSetting','ResetWaterMonitorCamera','SetWaterMonitorCameraPreview'):
    code+=function(name)
code+=r'''
}
int main(){using namespace savr;using namespace savr::driving;
 for(int m:{400,407,601})for(int modeValue:{MODE_DEFAULT,MODE_IMMERSIVE})for(int type:{VEHICLE_CAR,VEHICLE_BIKE,VEHICLE_PLANE,VEHICLE_BOAT}){
  model=m;mode=modeValue;int count=0,rows=GetMenuItemCount(type);assert(rows>0&&rows<=48);
  for(int i=0;i<rows;++i)if(IsWaterMonitorCameraMenuItem(GetMenuItemForRow(type,i)))++count;
  assert(count==((m==407&&type==VEHICLE_CAR)?7:0));assert(GetMenuItemForRow(type,rows)==MENU_BACK);
 }
 model=407;assert(GetWaterMonitorCameraSetting(0)==35&&GetWaterMonitorCameraSetting(2)==30);
 AdjustWaterMonitorCameraSetting(0,1);assert(GetWaterMonitorCameraSetting(0)==35.5f&&saves==1);
 menu::fineAdjustment=true;AdjustWaterMonitorCameraSetting(0,1);assert(std::abs(GetWaterMonitorCameraSetting(0)-35.6f)<.0001f);
 AdjustWaterMonitorCameraSetting(3,-1);assert(GetWaterMonitorCameraSetting(3)==-.1f);
 g_modelCalibration[407].monitorCamera[0]=200;AdjustWaterMonitorCameraSetting(0,1);assert(GetWaterMonitorCameraSetting(0)==200);
 model=601;const int before=saves;AdjustWaterMonitorCameraSetting(0,1);ResetWaterMonitorCamera();assert(saves==before);
 model=407;ResetWaterMonitorCamera();assert(g_modelCalibration[407].monitorCamera==watermonitor::DefaultCamera);
 SetWaterMonitorCameraPreview(true);assert(g_waterCameraPreview.load());SetWaterMonitorCameraPreview(false);assert(!g_waterCameraPreview.load());
 std::puts("water camera: Firetruck-only rows, 48-row capacity, 0.5/0.1 edits, limits, other-model isolation, reset and preview routing PASS");
}
'''
cpp=OUT/'settings.cpp';cpp.write_text(code,encoding='utf8')
exe=OUT/('settings.exe' if os.name=='nt' else 'settings')
if os.name=='nt':
    where=Path(os.environ.get('ProgramFiles(x86)',r'C:\Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
    base=subprocess.check_output([str(where),'-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
    vc=Path(base)/'VC/Auxiliary/Build/vcvars64.bat'
    command=OUT/'run.cmd'
    command.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc /W4 /I"{ROOT/"native/src"}" "{cpp}" /Fe:"{exe}" /Fo:"{OUT/"settings.obj"}"\nif errorlevel 1 exit /b 1\n"{exe}"\n')
    subprocess.run(['cmd','/c',str(command)],cwd=OUT,check=True)
else:
    subprocess.run([os.environ.get('CXX','c++'),'-std=c++20','-I'+str(ROOT/'native/src'),str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)

"""Run actual weapon/HUD settings load-edit-save paths with 0.1 precision."""
from pathlib import Path
import os
import subprocess

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/menu-precision-tests'
OUT.mkdir(parents=True,exist_ok=True)

def production(name):
    text=(ROOT/'native/src'/name).read_text(encoding='utf8')
    text=text.replace('#include "Log.h"','')
    if os.name=='nt':
        text=text.replace('#include <unistd.h>','#include <io.h>\n#define fsync _commit\n#define fileno _fileno')
        # Emulate Android/POSIX rename-over-existing for the local Windows file.
        text=text.replace('std::rename(kTempPath, kPath)',
                          '(std::remove(kPath),std::rename(kTempPath,kPath))')
    text=text.replace('/sdcard/Android/data/com.rockstargames.gtasa/files/vr_calib.ini','calib.ini')
    text=text.replace('/sdcard/Android/data/com.rockstargames.gtasa/files/vr_hud.ini','hud.ini')
    return text

source=r'''
#define LOGI(...) ((void)0)
#define LOGW(...) ((void)0)
#include <cassert>
#include <fstream>
#include "MenuPrecision.h"
'''+production('Calib.cpp')+production('HudSettings.cpp')+r'''
bool Near(float a,float b){return std::abs(a-b)<.0001f;}
int main(){
 using namespace savr;
 menu::fineAdjustment=false;
 assert(Near(menu::SpatialValue(0,1),.5f));
 menu::fineAdjustment=true;
 assert(Near(menu::SpatialValue(0,1),.1f));
 assert(Near(menu::SpatialValue(0,-10),-.1f)); // fine hold never accelerates
 for(float unit:{.1f,.5f,1.f}){
  float value=17/unit;
  for(int i=0;i<1000;++i)value=menu::SpatialValue(value,1,unit);
  for(int i=0;i<1000;++i)value=menu::SpatialValue(value,-1,unit);
  assert(Near(value,17/unit));
 }
 // Legacy integer profiles retain their original half-cm/half-degree meaning.
 {std::ofstream f("calib.ini");f<<"w 22 1 17 6 -12 7 20 0 -3 -3 -8 0 0 0 0 0 0 0 0 0 0\nholster 22 -13 -5 0 180 0 0\n";}
 calib::Init();assert(Near(calib::GetField(1,22,0),17));
 assert(Near(calib::GetHolsterField(22,0),-13));
 for(int set:{0,1}){
  calib::SetModelSet(set);
  for(int field=0;field<18;++field){
   calib::SetField(1,22,field,0);
   menu::fineAdjustment=true;calib::AdjustField(1,22,field,1);
   assert(Near(calib::GetField(1,22,field),.2f));
   menu::fineAdjustment=false;calib::AdjustField(1,22,field,1);
   assert(Near(calib::GetField(1,22,field),1.2f));
  }
 }
 menu::fineAdjustment=true;
 calib::AdjustHolsterField(22,0,1);assert(Near(calib::GetHolsterField(22,0),-12.8f));
 calib::Init(); // reload the actual saved decimal file
 for(int set:{0,1}){calib::SetModelSet(set);for(int field=0;field<18;++field)assert(Near(calib::GetField(1,22,field),1.2f));}
 assert(Near(calib::GetHolsterField(22,0),-12.8f));
 // HUD coordinates stay in mm; angles stay in degrees. Both preserve decimals.
 {std::ofstream f("hud.ini");f<<"Hud.Radar.WristPitchDeg=12\n";}
 hud::Init();
 for(int slot=0;slot<hud::WRIST_SLOT_COUNT;++slot){
  const int model=slot==hud::WRIST_SLOT_VEHICLE?411:-1;
  const auto before=hud::GetWristSettings(hud::RADAR,slot,model);
  hud::AdjustWristField(hud::RADAR,hud::WRIST_PITCH_DEG,1,slot,model);
  hud::AdjustWristField(hud::RADAR,hud::WRIST_ALONG_MM,1,slot,model);
  auto after=hud::GetWristSettings(hud::RADAR,slot,model);
  assert(Near(after.pitchDeg-before.pitchDeg,.1f));
  assert(Near(after.alongMm-before.alongMm,1.f));
  hud::Load();after=hud::GetWristSettings(hud::RADAR,slot,model);
  assert(Near(after.pitchDeg-before.pitchDeg,.1f));
  assert(Near(after.alongMm-before.alongMm,1.f));
 }
 menu::fineAdjustment=false;
 const auto before=hud::GetWristSettings(hud::RADAR,hud::WRIST_SLOT_HAND,-1);
 hud::AdjustWristField(hud::RADAR,hud::WRIST_PITCH_DEG,1,hud::WRIST_SLOT_HAND,-1);
 hud::AdjustWristField(hud::RADAR,hud::WRIST_ALONG_MM,1,hud::WRIST_SLOT_HAND,-1);
 const auto after=hud::GetWristSettings(hud::RADAR,hud::WRIST_SLOT_HAND,-1);
 assert(Near(after.pitchDeg-before.pitchDeg,.5f));assert(Near(after.alongMm-before.alongMm,5.f));
 // Non-finite config values must not poison wrist/dash rendering matrices.
 {std::ofstream f("hud.ini");f<<"Hud.Radar.WristPitchDeg=nan\nHud.Radar.DashM411.PitchDeg=nan\n";}
 hud::Load();
 assert(std::isfinite(hud::GetWristSettings(hud::RADAR,hud::WRIST_SLOT_HAND,-1).pitchDeg));
 assert(std::isfinite(hud::GetWristSettings(hud::RADAR,hud::WRIST_SLOT_VEHICLE,411).pitchDeg));
 std::puts("PASS precision: 0.1/0.5 steps, no drift, legacy units, both weapon sets, all pose fields, holsters and HUD/dash decimal reload, non-finite HUD rejection");
}
'''
(OUT/'test.cpp').write_text(source,encoding='utf8')
exe=OUT/('test.exe' if os.name=='nt' else 'test')
if os.name=='nt':
    where=Path(os.environ.get('ProgramFiles(x86)',r'C:\Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
    base=subprocess.check_output([str(where),'-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
    vc=Path(base)/'VC/Auxiliary/Build/vcvars64.bat'
    script=OUT/'test.cmd'
    script.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc /W4 /D_CRT_SECURE_NO_WARNINGS /I"{ROOT / "native/src"}" "{OUT / "test.cpp"}" /Fe:"{exe}" /Fo:"{OUT / "test.obj"}"\nif errorlevel 1 exit /b 1\n"{exe}"\n',encoding='utf8')
    subprocess.run(['cmd','/c',str(script)],cwd=OUT,check=True)
else:
    subprocess.run(['c++','-std=c++20','-I'+str(ROOT/'native/src'),str(OUT/'test.cpp'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],cwd=OUT,check=True)

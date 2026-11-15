"""Exercise the production static GLB loader and radar boundary calculations."""
from pathlib import Path
import runpy
import struct
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
helpers=runpy.run_path(str(ROOT/'tools/test-bike-steering.py'))
with tempfile.TemporaryDirectory(prefix='savr-watch-') as tmp:
    out=Path(tmp)
    source=out/'watch.cpp'
    header='#define WATCH_FILE "'+(ROOT/'assets/watches/watches.glb').as_posix()+'"\n'
    header+='#define HAND_FILE "'+(ROOT/'assets/vrhands/skeletal/hands.glb').as_posix()+'"\n'
    source.write_text(header+(ROOT/'tools/tests/watch_runtime.cpp').read_text(),encoding='utf-8')
    helpers['compile_run'](source,'watch',out)
    settings=(ROOT/'native/src/HudSettings.cpp').read_text(encoding='utf-8')
    settings=settings.replace('#include "Log.h"','#define LOGI(...)\n#define LOGW(...)')
    settings=settings.replace('/sdcard/Android/data/com.rockstargames.gtasa/files/vr_hud.ini','watch-settings.ini')
    settings='#define _CRT_SECURE_NO_WARNINGS\n#include <fstream>\n#include <iterator>\n#include <cassert>\n'+settings+r'''
int main(){
    using namespace savr::hud;
    {std::ofstream file("watch-settings.ini");file<<"WatchHand=1\nWatchInside=1\nHud.Watch.WristRollDeg=35\n";}
    Init();assert(DeviceHand(WATCH)==1&&WatchInside());
    assert(GetWristSettings(WATCH,WRIST_SLOT_HAND,-1).rollDeg==35);
    ToggleDeviceHand(WATCH);ToggleWatchInside();
    assert(DeviceHand(WATCH)==0&&!WatchInside());
    std::ifstream file("watch-settings.ini");std::string text((std::istreambuf_iterator<char>(file)),{});
    assert(text.find("WatchHand=0")!=text.npos&&text.find("WatchInside=0")!=text.npos);
    assert(text.find("Hud.Watch.WristRollDeg=35")!=text.npos);
    std::puts("PASS watch hand, side and rotation settings load/save");
}
'''
    source=out/'settings.cpp';source.write_text(settings,encoding='utf-8')
    helpers['compile_run'](source,'watch-settings',out)
if len(sys.argv)>1:
    retail=Path(sys.argv[1]).read_bytes()
    # RenderHealthBar, RenderArmorBar, AddHealth use these actual player maxima.
    for address,offset in [(0x350c8c,0x18b),(0x350e50,0x18c),(0x4b0aa8,0x18b)]:
        instruction=struct.unpack_from('<I',retail,address)[0]
        assert instruction&0xffc00000==0x3d400000,hex(address)
        assert (instruction>>10)&0xfff==offset,(hex(address),hex(instruction))
    print('PASS retail current health/armour maximum load fingerprints')

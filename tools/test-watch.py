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
    {std::ofstream file("watch-settings.ini");file<<"WatchHand=1\nWatchInside=1\nWatchMapZoomPercent=75\nHud.Watch.WristRollDeg=35\n";}
    Init();assert(DeviceHand(WATCH)==1&&WatchInside());
    assert(WatchMapZoom()==.75f);
    SetWatchMapZoom(.25f,true);FlushWatchSettings();assert(WatchMapZoom()==.25f);
    SetWatchMapZoom(1.4f,true);FlushWatchSettings();
    assert(std::abs(WatchMapZoom()-1.4f)<.001f);
    assert(GetWristSettings(WATCH,WRIST_SLOT_HAND,-1).rollDeg==35);
    ToggleDeviceHand(WATCH);ToggleWatchInside();
    assert(DeviceHand(WATCH)==0&&!WatchInside());
    const auto other=GetWristSettings(RADAR,WRIST_SLOT_VEHICLE,412);
    const auto original=GetWristSettings(RADAR,WRIST_SLOT_VEHICLE,411);
    auto moved=original;moved.alongMm=80;moved.acrossMm=-100;moved.liftMm=120;moved.yawDeg=90;
    SetVehiclePdaPosition(411,moved);
    const auto actual=GetWristSettings(RADAR,WRIST_SLOT_VEHICLE,411);
    assert(actual.alongMm==80&&actual.acrossMm==-100&&actual.liftMm==120);
    assert(actual.yawDeg==original.yawDeg&&actual.pitchDeg==original.pitchDeg&&actual.scaleTenths==original.scaleTenths);
    assert(GetWristSettings(RADAR,WRIST_SLOT_VEHICLE,412).acrossMm==other.acrossMm);
    SavePdaCalibration();
    std::ifstream file("watch-settings.ini");std::string text((std::istreambuf_iterator<char>(file)),{});
    assert(text.find("WatchMapZoomPercent=140")!=text.npos);
    assert(text.find("WatchHand=0")!=text.npos&&text.find("WatchInside=0")!=text.npos);
    assert(text.find("Hud.Watch.WristRollDeg=35")!=text.npos);
    assert(text.find("Hud.Radar.DashM411.AlongMm=80")!=text.npos);
    assert(text.find("Hud.Radar.DashM411.AcrossMm=-100")!=text.npos);
    assert(text.find("Hud.Radar.DashM411.LiftMm=120")!=text.npos);
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
    # Section request is an idempotent null-check, then synchronous DB lookup.
    assert struct.unpack_from('<I',retail,0x51d854)[0]==0xb50001c8
    # DrawRadarSection's out-of-world colour, immediately before CRGBA ctor.
    for address,value in [(0x51e290,111),(0x51e294,137),(0x51e298,170)]:
        assert (struct.unpack_from('<I',retail,address)[0]>>5)&0xffff==value
    print('PASS retail radar section request and ocean colour fingerprints')
    # RwTextureDestroy only calls CanUnloadTextureEntry at DB-only refCount 1.
    for address,instruction in [(0x76c980,0xb9406408),(0x76c998,0x7100053f),
                                (0x51d880,0xb9406408),(0x51d884,0x11000508)]:
        assert struct.unpack_from('<I',retail,address)[0]==instruction,hex(address)
    print('PASS retail RwTexture reference lifetime / cancellation fingerprints')
    for address,instruction in [(0x76c9a8,0xf9401d00), # raster -> DB entry at +0x38
                                (0x796d48,0x79401809), # entry flags +0xc
                                (0x796d50,0x7140055f), # queued/unrendered state 1
                                (0x796d58,0x5284000a), # rendered queue state 2
                                (0x795c14,0x5282000a), # initial request state 1
                                (0x7959b0,0x5286000b)]: # full texture state 3
        assert struct.unpack_from('<I',retail,address)[0]==instruction,hex(address)
    print('PASS retail thumbnail/requested -> visible -> full streaming states')
    for address,instruction in [(0x496a40,0xb9400108),(0x496a48,0x1a9f17e0),
                                (0x5185e8,0x39412c08), # stock player area byte +0x4b
                                (0x796358,0x9102e2b8), # visible queue +0xb8
                                (0x796394,0x71000a7f)]: # pump tier selection
        assert struct.unpack_from('<I',retail,address)[0]==instruction,hex(address)
    print('PASS retail outside-area ABI, radar interior gate and priority queue layout')

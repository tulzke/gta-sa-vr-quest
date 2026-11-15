"""Compile stock wheel and handlebar calibration from Driving.cpp on the host."""

import os
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "native/src/Driving.cpp").read_text(encoding="utf-8")


def function(signature):
    start = SOURCE.index(signature)
    brace = SOURCE.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (SOURCE[end] == "{") - (SOURCE[end] == "}")
        end += 1
    return SOURCE[start:end] + "\n"


PRELUDE = r'''
#include "Driving.h"
#include "MenuPrecision.h"
#include "IgnitionState.h"
#include "RadioKnobState.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <string>

namespace savr::xr { void SetHandPinchAmount(int,float) {} }
namespace savr::driving {
int activeModel=411, activeMode=MODE_IMMERSIVE;
int saveCount=0, resetCount=0;
void EnsureInit() {}
void Save() { ++saveCount; }
void ResetInteraction() { ++resetCount; }
int GetActiveVehicleModelId() { return activeModel; }
int GetActiveVehicleType() { return VEHICLE_BIKE; }
std::atomic<bool> g_bikeBlockWheelie{false};
std::atomic<bool> g_planeInvertPitch{false};
std::atomic<bool> g_bikeHandsFollowTilt{true};
std::atomic<int> g_activeBikeAccelerator{BIKE_ACCEL_PHYSICAL_OR_TAP};
struct GameSymbols { struct Vec3 {float x,y,z;}; };
bool ComponentPresent(void*,int) {return true;}
void ComponentPosition(void*,int,GameSymbols::Vec3* p) {*p={1,2,3};}
struct { bool (*CBike_IsComponentPresent)(void*,int)=ComponentPresent;
 void (*CBike_GetComponentWorldPosition)(void*,int,GameSymbols::Vec3*)=ComponentPosition; } g;
#define LOGI(...) ((void)0)

int GetModeForVehicleType(int) { return activeMode; }
int GlobalSeatSideField(int,int) { return F_SIDE; }
int GlobalSeatField(int,int,bool height) { return height?F_HEIGHT:F_DISTANCE; }
bool HasCurrentModelForType(int) { return activeModel>=0; }
std::atomic<float> g_value[F_COUNT]{};
std::atomic<int> g_bikeVisualLeanPercent{50};
std::atomic<bool> g_bikeLeanSteering{true};
std::atomic<int> g_bikeLeanSensitivity{100};
std::atomic<int> g_rearMirrorQuality{1},g_rearMirrorFov{80};
int kDefaults[F_COUNT]{};
'''

HARNESS = r'''
int checks=0;
#define CHECK(c) do { ++checks; if (!(c)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#c); std::exit(1); } } while (0)
bool Near(float a,float b) { return std::abs(a-b)<.00001f; }
void CheckVector(V3 a,V3 b) { CHECK(Near(a.x,b.x)); CHECK(Near(a.y,b.y)); CHECK(Near(a.z,b.z)); }
void CheckBasis(const ModelWheelPose& p) {
    CHECK(Near(Length(p.right),1)); CHECK(Near(Length(p.up),1)); CHECK(Near(Length(p.normal),1));
    CHECK(Near(Dot(p.right,p.up),0)); CHECK(Near(Dot(p.right,p.normal),0)); CHECK(Near(Dot(p.up,p.normal),0));
    CheckVector(Cross(p.right,p.normal),p.up);
}


void TestPose() {
    float preset[F_COUNT]{},delta[WHEEL_CAL_FIELD_COUNT]{};
    preset[F_WHEEL_SIDE]=-47;preset[F_WHEEL_DISTANCE]=37;
    preset[F_WHEEL_HEIGHT]=15;preset[F_WHEEL_RADIUS]=17;
    auto wheel=CalibratedWheelPose({1,2,3},{1,0,0},{0,1,0},{0,0,1},preset,delta);
    CheckVector(wheel.center,{.53f,2.37f,3.15f});CHECK(Near(wheel.radius,.17f));CheckBasis(wheel);
    delta[WHEEL_CAL_SIDE]=2;delta[WHEEL_CAL_FORWARD]=-4;
    delta[WHEEL_CAL_HEIGHT]=7;delta[WHEEL_CAL_RADIUS]=1;
    wheel=CalibratedWheelPose({1,2,3},{1,0,0},{0,1,0},{0,0,1},preset,delta);
    CheckVector(wheel.center,{.55f,2.33f,3.22f});CHECK(Near(wheel.radius,.18f));
    auto turned=CalibratedWheelPose({10,20,30},{0,1,0},{-1,0,0},{0,0,1},preset,delta);
    CheckVector(turned.center,{9.67f,19.55f,30.22f});CheckBasis(turned);
    delta[WHEEL_CAL_RADIUS]=-100;
    CHECK(Near(CalibratedWheelPose({},{1,0,0},{0,1,0},{0,0,1},preset,delta).radius,.06f));
    delta[WHEEL_CAL_RADIUS]=200;
    CHECK(Near(CalibratedWheelPose({},{1,0,0},{0,1,0},{0,0,1},preset,delta).radius,.45f));
    std::fill_n(delta,WHEEL_CAL_FIELD_COUNT,0);
    for(int field=WHEEL_CAL_PITCH;field<=WHEEL_CAL_ROLL;++field) {
        delta[field]=180;
        auto rotated=CalibratedWheelPose({},{1,0,0},{0,1,0},{0,0,1},preset,delta);
        CheckBasis(rotated);
        if(field==WHEEL_CAL_PITCH)CheckVector(rotated.normal,{0,0,1});
        if(field==WHEEL_CAL_YAW)CheckVector(rotated.normal,{-1,0,0});
        if(field==WHEEL_CAL_ROLL)CheckVector(rotated.right,{0,0,-1});
        delta[field]=0;
    }
}
void TestProfiles() {
    g_modelCalibration.clear();
    auto& c=g_modelCalibration[411];c.wheel[WHEEL_CAL_SIDE]=-50;
    CHECK(GetWheelCalibrationValue(WHEEL_CAL_SIDE)==-50);
    AdjustWheelCalibrationValue(WHEEL_CAL_SIDE,-1);
    CHECK(Near(c.wheel[WHEEL_CAL_SIDE],-50.5f));CHECK(saveCount==1);
    g_modelCalibration[400].wheel[WHEEL_CAL_SIDE]=19;
    ResetVehiclePreset(VEHICLE_CAR);
    CHECK(c.wheel[WHEEL_CAL_SIDE]==0);CHECK(g_modelCalibration[400].wheel[WHEEL_CAL_SIDE]==19);
    c.wheel[WHEEL_CAL_SIDE]=13;activeMode=MODE_DEFAULT;
    ResetVehiclePreset(VEHICLE_CAR);CHECK(c.wheel[WHEEL_CAL_SIDE]==13);
    activeMode=MODE_IMMERSIVE;
    ResetVehiclePreset(VEHICLE_BIKE);CHECK(c.wheel[WHEEL_CAL_SIDE]==13);
    c.wheel[WHEEL_CAL_RADIUS]=200;
    AdjustWheelCalibrationValue(WHEEL_CAL_RADIUS,1);CHECK(c.wheel[WHEEL_CAL_RADIUS]==200);
    const int saves=saveCount;
    AdjustWheelCalibrationValue(-1,1);AdjustWheelCalibrationValue(WHEEL_CAL_SIDE,0);
    activeModel=-1;AdjustWheelCalibrationValue(WHEEL_CAL_SIDE,1);
    CHECK(GetWheelCalibrationValue(WHEEL_CAL_SIDE)==0);CHECK(saveCount==saves);
    activeModel=411;
}
void TestBars() {
    g_modelCalibration.clear();auto& c=g_modelCalibration[411];
    c.control[0][CONTROL_OFFSET_X]=99;
    CHECK(GetControlCalibrationValue(0,CONTROL_OFFSET_X)==99);
    AdjustControlCalibrationValue(0,CONTROL_OFFSET_X,1);
    CHECK(c.control[0][CONTROL_OFFSET_X]==100);
    CHECK(GetControlCalibrationValue(2,CONTROL_OFFSET_X)==0);
    ResetVehiclePreset(VEHICLE_BIKE);CHECK(c.control[0][CONTROL_OFFSET_X]==0);
    WheelVisualState visual{};visual.modelId=411;visual.bike=true;
    visual.center[0]=.02f;visual.center[1]=.2f;visual.center[2]=.7f;
    visual.right[0]=1;visual.up[1]=1;visual.normal[2]=1;visual.radius=.32f;
    visual.physicalAngle=3.14159265358979323846f*.5f;
    FillHandleVisuals(&visual);
    CheckVector({visual.center[0],visual.center[1],visual.center[2]},{.02f,.2f,.7f});
    CheckVector({visual.handlePosition[0][0],visual.handlePosition[0][1],visual.handlePosition[0][2]},{.02f,-.12f,.7f});
    CheckVector({visual.handlePosition[1][0],visual.handlePosition[1][1],visual.handlePosition[1][2]},{.02f,.52f,.7f});
    FillHandleVisuals(&visual);
    CheckVector({visual.handlePosition[0][0],visual.handlePosition[0][1],visual.handlePosition[0][2]},{.02f,-.12f,.7f});
    visual.physicalAngle=0;FillHandleVisuals(&visual);
    CheckVector({visual.handlePosition[0][0],visual.handlePosition[0][1],visual.handlePosition[0][2]},{-.30f,.2f,.7f});
    g_value[F_BIKE_DISTANCE]=30;g_value[F_BIKE_HEIGHT]=82;g_value[F_BIKE_HALF_WIDTH]=32;
    WheelVisualState built{};CHECK(BuildBikeTracking(&built));CHECK(Near(built.radius,.32f));
    CheckVector({built.center[0],built.center[1],built.center[2]},{1,2,3});
    g_modelCalibration[411].wheel[WHEEL_CAL_SIDE]=10;
    g_modelCalibration[411].wheel[WHEEL_CAL_FORWARD]=-20;
    g_modelCalibration[411].wheel[WHEEL_CAL_HEIGHT]=30;
    g_modelCalibration[411].wheel[WHEEL_CAL_RADIUS]=5;
    CHECK(BuildBikeTracking(&built));CHECK(Near(built.radius,.37f));
    CheckVector({built.center[0],built.center[1],built.center[2]},{1.1f,1.8f,3.3f});
    g_modelCalibration.clear();
    g.CBike_IsComponentPresent=nullptr;CHECK(BuildBikeTracking(&built));
    CheckVector({built.center[0],built.center[1],built.center[2]},{0,.30f,.82f});
    g.CBike_IsComponentPresent=ComponentPresent;
    CHECK(!BuildBikeTracking(nullptr));
}
void TestPersistence() {
    g_modelCalibration.clear();
    ReadWheelLine("WheelCalib.411.0=-47");CHECK(g_modelCalibration[411].wheel[0]==-47);
    ReadWheelLine("WheelCalib.411.4=-15");CHECK(g_modelCalibration[411].wheel[4]==-15);
    ReadWheelLine("WheelCalib.411.999=3");ReadWheelLine("WheelCalib.-1.0=3");
    CHECK(g_modelCalibration.size()==1);
    ReadWheelLine("WheelCalib.411.3=999999");CHECK(g_modelCalibration[411].wheel[3]==200);
    FILE* file=std::tmpfile();CHECK(file!=nullptr);
    WriteWheelLines(file,411,g_modelCalibration[411]);
    std::rewind(file);char line[128];std::string contents;g_modelCalibration.clear();
    while(std::fgets(line,sizeof(line),file)){contents+=line;ReadWheelLine(line);}
    std::fclose(file);
    CHECK(contents.find("WheelCalib.411.0=-47\n")!=std::string::npos);
    CHECK(g_modelCalibration[411].wheel[0]==-47);CHECK(g_modelCalibration[411].wheel[4]==-15);
    CHECK(g_modelCalibration[411].wheel[3]==200);
    ReadWheelLine("WheelCalib.411.0=-47.1");
    ReadWheelLine("WheelCalib.411.4=0.2");
    ReadWheelLine("ModelSeat.411.1.SideCm=0.1");
    ReadWheelLine("Control.411.0.RotationX=0.2");
    ReadWheelLine("Ignition.411.3=0.1");
    CHECK(Near(g_modelCalibration[411].seatSide[1],.1f));
    CHECK(Near(g_modelCalibration[411].control[0][3],.2f));
    CHECK(Near(g_modelCalibration[411].ignition[3],.1f));
    file=std::tmpfile();CHECK(file);WriteWheelLines(file,411,g_modelCalibration[411]);
    std::rewind(file);g_modelCalibration.clear();
    while(std::fgets(line,sizeof(line),file))ReadWheelLine(line);
    std::fclose(file);
    CHECK(Near(g_modelCalibration[411].wheel[0],-47.1f));
    CHECK(Near(g_modelCalibration[411].wheel[4],.2f));
    CHECK(Near(g_modelCalibration[411].ignition[3],.1f));
    menu::fineAdjustment=true;activeModel=411;
    AdjustWheelCalibrationValue(WHEEL_CAL_PITCH,1);
    CHECK(Near(GetWheelCalibrationValue(WHEEL_CAL_PITCH),.4f));
    AdjustControlCalibrationValue(0,CONTROL_OFFSET_X,1);
    CHECK(Near(GetControlCalibrationValue(0,CONTROL_OFFSET_X),.2f));
    menu::fineAdjustment=false;
}
} // namespace savr::driving
int main() {
    {
        using namespace savr::driving;
        const float v[]={.12f,-.34f,.56f};SetDoorCalibration(411,2,false,v);
        float out[3]{};GetDoorCalibration(411,2,false,out);CHECK(Near(out[1],-.34f));
        GetDoorCalibration(412,2,false,out);CHECK(Near(out[1],0));
        GetDoorCalibration(411,2,true,out);CHECK(Near(out[1],0));
        const int wheel[]={17,-22,31};SetDebugPosition(411,MODE_IMMERSIVE,false,wheel);
        CHECK(g_modelCalibration[411].wheel[1]==-22);
        SetDebugPosition(411,MODE_DEFAULT,true,wheel);
        CHECK(g_modelCalibration[411].seatForward[MODE_DEFAULT]==-22);
        CHECK(g_modelCalibration[411].seatForward[MODE_IMMERSIVE]==0);
        FILE* f=std::tmpfile();CHECK(f);WriteDoorLines(f,411,g_modelCalibration[411]);
        std::rewind(f);g_modelCalibration.clear();char line[128];
        while(std::fgets(line,sizeof(line),f)) ReadDoorLine(line);
        std::fclose(f);GetDoorCalibration(411,2,false,out);CHECK(Near(out[2],.56f));
        ReadDoorLine("DoorCalib.411.0.0.0=99999");
        GetDoorCalibration(411,2,false,out);CHECK(Near(out[0],2.f));
        ReadDoorLine("DoorCalib.411.99.0.0=1");
        const float bad[]={NAN,0,0};SetDoorCalibration(411,2,false,bad);
        GetDoorCalibration(411,2,false,out);CHECK(Near(out[0],2.f));
    }
    {
        using namespace savr::driving;g_modelCalibration.clear();float out[3]{};
        const float rear[]{-.55f,-.7f,1.2f},front[]{.4f,.3f,1.4f};
        CHECK(!GetPassengerView(411,1,MODE_IMMERSIVE,out));
        SetPassengerView(411,1,MODE_IMMERSIVE,rear);
        CHECK(GetPassengerView(411,2,MODE_IMMERSIVE,out)&&Near(out[0],.55f)&&Near(out[1],-.7f)&&Near(out[2],1.2f));
        CHECK(!GetPassengerView(412,2,MODE_IMMERSIVE,out)&&!GetPassengerView(411,1,MODE_DEFAULT,out)&&!GetPassengerView(411,-1,MODE_IMMERSIVE,out));
        SetPassengerView(411,0,MODE_IMMERSIVE,front);CHECK(GetPassengerView(411,0,MODE_IMMERSIVE,out)&&Near(out[0],.4f));
        const float right[]{.6f,-.8f,1.3f};SetPassengerView(411,2,MODE_IMMERSIVE,right);
        CHECK(GetPassengerView(411,1,MODE_IMMERSIVE,out)&&Near(out[0],-.6f)&&Near(out[1],-.8f));
        float bad[]{0,0,std::numeric_limits<float>::quiet_NaN()};SetPassengerView(411,1,MODE_IMMERSIVE,bad);
        CHECK(GetPassengerView(411,1,MODE_IMMERSIVE,out)&&Near(out[2],1.3f));
        const float delta[]{.3f,-.2f,.9f};SetExitCalibration(411,0,true,delta);
        GetExitCalibration(411,0,true,out);CHECK(Near(out[0],.3f)&&Near(out[1],-.2f)&&out[2]==0);
        GetExitCalibration(411,1,true,out);CHECK(out[0]==0&&out[1]==0);
        const float large[]{2,-2,2};SetExitCalibration(411,0,true,large);GetExitCalibration(411,0,true,out);CHECK(Near(out[0],.5f)&&Near(out[1],-.5f));
        SetExitCalibration(411,0,false,delta);GetExitCalibration(411,0,false,out);CHECK(Near(out[2],.9f));
        int mirror[12]{};GetRearMirrorCalibration(411,mirror);CHECK(mirror[0]==-15);
        auto& c=g_modelCalibration[411];c.mirrorDefaultValid=true;c.mirrorDefault[0]=-17;c.mirrorDefault[1]=50;c.mirrorDefault[2]=125;
        GetRearMirrorCalibration(411,mirror);CHECK(mirror[0]==-17&&mirror[1]==50&&mirror[2]==125);
        mirror[6]=77;SetRearMirrorCameraCalibration(411,mirror);CHECK(!c.mirrorManual);
        c.mirrorDefault[0]=-18;GetRearMirrorCalibration(411,mirror);CHECK(mirror[0]==-18&&mirror[6]==77);
        mirror[0]=22;SetRearMirrorCalibration(411,mirror);c.mirrorDefault[0]=99;GetRearMirrorCalibration(411,mirror);CHECK(mirror[0]==22);
        FILE* file=std::tmpfile();CHECK(file);WriteVehicleLines(file,411,c);std::rewind(file);g_modelCalibration.clear();char line[128];
        while(std::fgets(line,sizeof(line),file))ReadVehicleLine(line);std::fclose(file);
        CHECK(GetPassengerView(411,2,MODE_IMMERSIVE,out)&&Near(out[0],.6f)&&Near(out[1],-.8f));
        GetExitCalibration(411,0,true,out);CHECK(Near(out[0],.5f)&&out[2]==0);
        GetRearMirrorCalibration(411,mirror);CHECK(mirror[0]==22&&g_modelCalibration[411].mirrorManual);
        ReadVehicleLine("RearMirrorCalib.412.0=0");CHECK(g_modelCalibration[412].mirrorManual);
        ReadVehicleLine("PassengerView.413.1.1.2=120");CHECK(!GetPassengerView(413,1,1,out));
        g_modelCalibration[411].mirrorManual=false;file=std::tmpfile();CHECK(file);WriteVehicleLines(file,411,g_modelCalibration[411]);std::rewind(file);g_modelCalibration.clear();
        while(std::fgets(line,sizeof(line),file))ReadVehicleLine(line);std::fclose(file);CHECK(!g_modelCalibration[411].mirrorManual);
        g_modelCalibration.clear();
    }
    savr::driving::TestPose();
    savr::driving::TestProfiles();
    savr::driving::TestPersistence();
    savr::driving::TestBars();
    std::printf("PASS: %d stock wheel, handlebar, calibration isolation and persistence checks\n",savr::driving::checks);
}
'''


def main():
    # Compile production bodies, including the exact save/load snippets. Stubs
    # replace only the game/Quest API and unrelated seat settings.
    math = SOURCE[SOURCE.index("struct V3 {"):SOURCE.index("double MonotonicSeconds()")]
    math += function("V3 RotateAroundAxis(")
    pose = SOURCE[SOURCE.index("struct ModelCalibration {"):SOURCE.index("float ClampField(")]
    accessors = function("float ClampModelSeat(") + function("void GetDoorCalibration(") + function("void SetDoorCalibration(") + function("void SetDebugPosition(")
    accessors += function("float GetWheelCalibrationValue(")
    accessors += function("void AdjustWheelCalibrationValue(")
    accessors += function("void ResetVehiclePreset(")
    for signature in ("bool GetPassengerView(","void SetPassengerView(","void GetExitCalibration(","void SetExitCalibration(","void GetRearMirrorCalibration(","void SetRearMirrorCalibration(","void SetRearMirrorCameraCalibration("):
        accessors += function(signature)
    save_start = SOURCE.index("            for (int field = 0; field < WHEEL_CAL_FIELD_COUNT; ++field) {")
    save_end = SOURCE.index("\n        }", save_start)
    persistence = "void WriteWheelLines(FILE* file,int model,const ModelCalibration& calibration) {\n"
    persistence += SOURCE[save_start:save_end] + "\n}\n"
    load_start = SOURCE.index('            float spatial{};')
    load_end = SOURCE.index('            int value', load_start)
    persistence += "void ReadWheelLine(const char* line) { float values[F_COUNT]{}; bool immersiveCarSeatSeen=false,immersiveBikeSeatSeen=false; do {\n"
    persistence += SOURCE[load_start:load_end] + "}while(false); }\n"
    door_save_start = SOURCE.index("            for(int door=0;door<4;")
    door_save_end = SOURCE.index("            for (int mode", door_save_start)
    persistence += "void WriteDoorLines(FILE* file,int model,const ModelCalibration& calibration) {\n" + SOURCE[door_save_start:door_save_end] + "}\n"
    door_load_start = SOURCE.index('                if(std::sscanf(line,"DoorCalib.')
    door_load_end = SOURCE.index('                if (std::sscanf(line, "ModelSeat.', door_load_start)
    persistence += "void ReadDoorLine(const char* line) { int model=-1,door=-1,inside=-1,axis=-1,value=0; do {\n" + SOURCE[door_load_start:door_load_end] + "} while(false); }\n"
    calibration_save_start=SOURCE.index('            for (int field=0;field<12;++field)')
    calibration_save_end=SOURCE.index('            for(int side=0;side<2;',calibration_save_start)
    persistence += "void WriteVehicleLines(FILE* file,int model,const ModelCalibration& calibration){\n" + SOURCE[calibration_save_start:calibration_save_end] + "}\n"
    mirror_load_start=SOURCE.index('                if (std::sscanf(line,"RearMirrorCalib.')
    mirror_load_end=SOURCE.index('                if (std::sscanf(line,"IgnitionPoseLean.',mirror_load_start)
    passenger_load_start=SOURCE.index('                float passengerValue{};')
    passenger_load_end=SOURCE.index('                if(std::sscanf(line,"EntryCalib.',passenger_load_start)
    persistence += "void ReadVehicleLine(const char* line){int model=-1,door=-1,axis=-1,value=0;do{\n" + SOURCE[mirror_load_start:mirror_load_end] + SOURCE[passenger_load_start:passenger_load_end] + "}while(false);}\n"
    source = PRELUDE + math + pose + r"""
bool IsBicycleModel(int) {return false;}
bool GetActivePlayerBike(void** vehicle,void*) {static char fake[2048]{};std::uint16_t model=411;std::memcpy(fake+0x32,&model,2);*vehicle=fake;return true;}
bool ReadScaledBikeLeanFrame(void*,V3* right,V3* forward,V3* up,V3* origin) {*right={1,0,0};*forward={0,1,0};*up={0,0,1};*origin={0,0,0};return true;}
bool WorldPointToTracking(V3 p,V3* out) {*out=p;return true;}
""" + function("bool BuildBikeTracking(") + function("float ClampControlValue(") + function("void FillHandleVisuals(") + function("bool IsControlCalibrationAvailable(") + function("float GetControlCalibrationValue(") + function("void AdjustControlCalibrationValue(") + accessors + persistence + HARNESS

    msvc = sorted((Path(subprocess.check_output([
        str(Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"),
        "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
        "-property", "installationPath"], text=True).strip()) / "VC/Tools/MSVC").iterdir())[-1]
    import winreg
    with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Microsoft\Windows Kits\Installed Roots") as key:
        sdk = Path(winreg.QueryValueEx(key, "KitsRoot10")[0])
    version = sorted((sdk / "Include").iterdir())[-1].name
    env = os.environ.copy()
    env["INCLUDE"] = ";".join(str(p) for p in [msvc / "include", *[sdk / "Include" / version / n for n in ("ucrt", "shared", "um")]])
    env["LIB"] = ";".join(str(p) for p in [msvc / "lib/x64", *[sdk / "Lib" / version / n / "x64" for n in ("ucrt", "um")]])
    compiler = msvc / "bin/Hostx64/x64"
    env["PATH"] = str(compiler) + os.pathsep + env.get("PATH", "")
    with tempfile.TemporaryDirectory(prefix="savr-wheel-pose-", dir=ROOT / '.cache') as temp:
        out = Path(temp)
        (out / "test.cpp").write_text(source, encoding="utf-8")
        subprocess.run([str(compiler / "cl.exe"), "/nologo", "/std:c++20", "/EHsc", "/MD",
                        "/I" + str(ROOT / "native/src"), str(out / "test.cpp"),
                        "/Fe:" + str(out / "test.exe"), "/Fo:" + str(out / "test.obj")], env=env, check=True)
        subprocess.run([str(out / "test.exe")], check=True)


if __name__ == "__main__":
    main()

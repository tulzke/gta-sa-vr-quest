#include "VitalsFeedback.h"
#include "PeripheralFeedback.h"
#include "Symbols.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#define CHECK(c) do { if(!(c)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c);std::exit(1); } } while(false)
double testNow=10;
bool focused=true,tracking=true,vrMenu=false,mobileMenu=false;
float audioSeverity{};
std::uint64_t epoch=7;
namespace savr {
GameSymbols g{};
namespace xr {
bool IsSessionFocused() { return focused; }
bool IsMenuVisible() { return vrMenu; }
bool GetHeadPose(float*,float*,std::uint64_t* e) { *e=epoch;return tracking; }
}
namespace vrcam { bool IsMobileMenuOpen() { return mobileMenu; } }
namespace heartbeataudio { void Update(float severity) { audioSeverity=severity; } }
}
// PRODUCTION_VITALS
using namespace savr;
using namespace savr::vitalsfeedback;
alignas(16) unsigned char ped[0xb00]{},other[0xb00]{};
unsigned char info[2*0x1d8]{},camera[0x50]{};
std::uint8_t focus{};
std::uint32_t frame=100,gameMs=10000;
bool userPause{},codePause{},scene{};
void* player=ped;
template<class T> void Write(void* p,int offset,T value) {
    std::memcpy(static_cast<char*>(p)+offset,&value,sizeof(value));
}
void Setup(float health=100,float armour=50) {
    Update(false);g={};testNow+=10;focused=tracking=true;vrMenu=mobileMenu=false;
    userPause=codePause=scene=false;epoch=7;frame=100;gameMs=10000;focus=0;
    std::memset(ped,0,sizeof(ped));std::memset(other,0,sizeof(other));
    std::memset(info,0,sizeof(info));std::memset(camera,0,sizeof(camera));
    player=ped;Write(info,0,player);Write(ped,0x6ac,health);Write(ped,0x6b4,armour);
    g.FindPlayerPed=[](int)->void* { return player; };
    g.CWorld_Players=info;g.CWorld_PlayerInFocus=&focus;g.TheCamera=camera;
    g.CTimer_m_FrameCounter=&frame;g.CTimer_m_snTimeInMilliseconds=&gameMs;
    g.CTimer_m_UserPause=&userPause;g.CTimer_m_CodePause=&codePause;
    g.CCutsceneMgr_ms_running=&scene;Update(true);
}
void Tick(float health,float armour,double dt=.02) {
    testNow+=dt;++frame;gameMs+=static_cast<std::uint32_t>(dt*1000);
    Write(player,0x6ac,health);Write(player,0x6b4,armour);Update(true);
}
Feedback Read() { return GetFeedback(true,epoch); }
int main() {
    Setup();CHECK(Read().healthHit==0&&Read().armourHit==0&&audioSeverity==0);
    Tick(100,40);CHECK(Read().armourHit==1&&Read().healthHit==0);
    auto halo=peripheral::Select({1},{},Read());CHECK(halo.red==halo.blue&&halo.red==.78f);
    Tick(90,0);CHECK(Read().healthHit==1&&Read().armourHit==1);
    halo=peripheral::Select({1},{},Read());CHECK(halo.red==1&&halo.green<.03f);
    for(int i=0;i<40;++i) Tick(90,0);
    CHECK(Read().healthHit==0&&Read().armourHit==0);
    // Healing/equipping never flashes; steady low armour has no persistent cue.
    Tick(100,10);CHECK(!peripheral::Select({}, {},Read()).Active());
    Setup(20,0);CHECK(Read().healthHit==0&&Read().lowHealth>0&&audioSeverity>0);
    halo=peripheral::Select({}, {},Read());CHECK(halo.red==1&&halo.opacity>0&&halo.opacity<.19f);
    Tick(26,0);CHECK(audioSeverity>0);Tick(29,0);CHECK(audioSeverity>0);
    Tick(30,0);CHECK(Read().lowHealth==0&&audioSeverity==0);
    Tick(26,0);CHECK(Read().lowHealth==0);Tick(25,0);CHECK(audioSeverity>0);
    Tick(1,0);CHECK(Read().lowHealth==1);
    halo=peripheral::Select({1},{.08f,.32f,1,.22f},Read());
    CHECK(halo.red>halo.blue&&halo.opacity<=.64f);
    // No damage on re-entry, loading, long stalls or recenter; low HP resumes.
    for(int boundary=0;boundary<6;++boundary) {
        Setup();
        if(boundary==0) ++epoch;
        if(boundary==1) frame=1;
        if(boundary==2) gameMs=1;
        if(boundary==3) { player=other;Write(info,0,player); }
        if(boundary==4) Update(false);
        Tick(20,0,boundary==5?1:.02);
        CHECK(Read().healthHit==0&&Read().armourHit==0&&Read().lowHealth>0);
    }
    for(int gate=0;gate<13;++gate) {
        Setup(20);
        switch(gate) {
        case 0: focused=false;break;case 1: tracking=false;break;
        case 2: vrMenu=true;break;case 3: mobileMenu=true;break;
        case 4: userPause=true;break;case 5: codePause=true;break;
        case 6: scene=true;break;case 7: camera[0x43]=1;break;
        case 8: info[0x118]=2;break;
        case 9: Write(ped,0x6ac,0.f);break;
        case 10: Write<std::uint32_t>(ped,0x54c,54);break;
        case 11: Write(ped,0x6ac,std::numeric_limits<float>::quiet_NaN());break;
        case 12: Write(ped,0x6b4,-1.f);break;
        }
        Update(true);CHECK(audioSeverity==0&&Read().lowHealth==0);
    }
    Setup(20);CHECK(GetFeedback(false,epoch).lowHealth==0);
    CHECK(GetFeedback(true,epoch+1).lowHealth==0);
    testNow+=.6;CHECK(Read().lowHealth==0);
    // Mixed low-health underlay preserves both grey armour cue and red tint.
    halo=peripheral::Select({}, {},{0,1,1});CHECK(halo.red>halo.blue&&halo.blue>.1f);
    CHECK(halo.opacity<.6f&&halo.inner>=.45f);
    std::puts("PASS: vitals observer, health/armour priorities, low-health hysteresis, lifecycle gates and audio stop");
}

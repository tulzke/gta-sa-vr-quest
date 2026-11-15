#include "WantedFeedback.h"
#include "PeripheralFeedback.h"
#include "Symbols.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>

#define CHECK(c) do { if(!(c)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c);std::exit(1); } } while(false)
bool Near(float a,float b) { return std::abs(a-b)<.00001f; }
void Same(savr::wantedfeedback::Feedback a,savr::wantedfeedback::Feedback b) {
    CHECK(Near(a.opacity,b.opacity));
    CHECK(Near(a.red*a.opacity,b.red*b.opacity));
    CHECK(Near(a.green*a.opacity,b.green*b.opacity));
    CHECK(Near(a.blue*a.opacity,b.blue*b.opacity));
}
double testNow=10;
bool focused=true,tracking=true,vrMenu=false,mobileMenu=false;
std::uint64_t epoch=7;
namespace savr {
GameSymbols g{};
namespace xr {
bool IsSessionFocused() { return focused; }
bool IsMenuVisible() { return vrMenu; }
bool GetHeadPose(float*,float*,std::uint64_t* out) { *out=epoch;return tracking; }
}
namespace vrcam { bool IsMobileMenuOpen() { return mobileMenu; } }
}
// PRODUCTION_WANTED

using namespace savr;
using namespace savr::wantedfeedback;
alignas(16) unsigned char ped[0xb00]{},otherPed[0xb00]{},wanted[0x40]{},otherWanted[0x40]{};
unsigned char info[2*0x1d8]{},camera[0x50]{};
std::uint8_t focus=0;
std::uint32_t frame=100,gameMs=10000;
bool userPause=false,codePause=false,scene=false;
void* playerPtr=ped;
void* wantedPtr=wanted;
template<class T> void Write(void* p,int offset,T value) {
    std::memcpy(static_cast<char*>(p)+offset,&value,sizeof(value));
}
void* Player(int id) { CHECK(id==-1);return playerPtr; }
void* Wanted(int id) { CHECK(id==-1);return wantedPtr; }
void Setup(int level=0) {
    Update(false);
    g={};testNow+=10;focused=tracking=true;vrMenu=mobileMenu=false;
    userPause=codePause=scene=false;focus=0;epoch=7;frame=100;gameMs=10000;
    std::memset(info,0,sizeof(info));std::memset(camera,0,sizeof(camera));
    std::memset(ped,0,sizeof(ped));std::memset(otherPed,0,sizeof(otherPed));
    Write(ped,0x6ac,100.f);Write(otherPed,0x6ac,100.f);Write(info,0,static_cast<void*>(ped));
    playerPtr=ped;wantedPtr=wanted;Write(wanted,0x2c,level);
    g.CWorld_Players=info;g.CWorld_PlayerInFocus=&focus;
    g.FindPlayerPed=Player;g.FindPlayerWanted=Wanted;g.TheCamera=camera;
    g.CTimer_m_FrameCounter=&frame;g.CTimer_m_snTimeInMilliseconds=&gameMs;
    g.CTimer_m_UserPause=&userPause;g.CTimer_m_CodePause=&codePause;
    g.CCutsceneMgr_ms_running=&scene;
    Update(true);
}
void Tick(int level,double dt=.02) {
    testNow+=dt;++frame;gameMs+=static_cast<std::uint32_t>(dt*1000);
    if(wantedPtr)Write(wantedPtr,0x2c,level);
    Update(true);
}
Feedback Read() { return GetFeedback(true,epoch); }

int main() {
    // Full envelopes, phase boundaries and alpha budgets without frame-rate dependence.
    for(int direction:{-1,1}) {
        detail::Effect effect{direction,10,{}};
        const double end=direction>0?.55:1.50;
        CHECK(effect.Sample(9).opacity==0);CHECK(effect.Sample(10).opacity==0);
        CHECK(effect.Sample(10+end+.00001).opacity==0);
        for(int i=0;i<=2000;++i) {
            const auto f=effect.Sample(10+end*i/2000);
            CHECK(f.opacity>=0&&f.opacity<=(direction>0?.22001f:.18001f));
            CHECK(f.red>=0&&f.red<=1&&f.green>=0&&f.green<=1&&f.blue>=0&&f.blue<=1);
        }
        for(double boundary:{.06,.10,.15,.25,.55,.85,1.50}) {
            Same(effect.Sample(10+boundary-.0000001),effect.Sample(10+boundary+.0000001));
        }
        CHECK(effect.Sample(std::numeric_limits<double>::quiet_NaN()).opacity==0);
    }
    detail::Effect drop{-1,10,{}};
    Same(drop.Sample(10.85),Feedback{.55f,.55f,.55f,.18f});
    CHECK(drop.Sample(11.2).opacity>0&&drop.Sample(11.2).opacity<.18f);
    CHECK(drop.Sample(10.2).blue>drop.Sample(10.2).red);

    // Nonzero initial level is silent; unchanged levels never retrigger.
    Setup(3);CHECK(Read().opacity==0);Tick(3);CHECK(Read().opacity==0);
    Tick(4);CHECK(Read().opacity==0);Tick(4,.08);CHECK(Near(Read().opacity,.22f));
    Tick(4,.4);Tick(4,.1);CHECK(Read().opacity==0);
    // 0->1, jump over levels, decrease and clear all use one bounded effect.
    Setup();Tick(1);Tick(1,.08);CHECK(Near(Read().opacity,.22f));
    auto before=Read();Tick(3,0);Same(before,Read());
    Tick(3,.06);CHECK(Near(Read().opacity,.22f));
    before=Read();Tick(2,0);Same(before,Read());
    Tick(2,.1);CHECK(Near(Read().opacity,.18f));
    Tick(2,.4);Tick(2,.4);CHECK(Near(Read().red,Read().blue));
    before=Read();Tick(0,0);Same(before,Read()); // repeat down carries grey/alpha
    Tick(0,.1);CHECK(Near(Read().opacity,.18f));
    Tick(0,.4);Tick(0,.4);Tick(0,.4);Tick(0,.3);CHECK(Read().opacity==0);
    Setup(2);Tick(1);Tick(1,.4);Tick(1,.4);
    before=Read();Tick(2,0);Same(before,Read()); // reverse grey->blue, continuous
    Tick(2,.06);CHECK(Read().blue>Read().red);CHECK(Near(Read().opacity,.22f));

    // Priority has one colour/alpha; wanted expires on schedule beneath combat.
    auto halo=peripheral::Select({.8f,1.f},Read());
    CHECK(halo.red==1&&Near(halo.opacity,.44f)&&halo.inner==.48f);
    halo=peripheral::Select({0,1},Read());CHECK(halo.red==halo.blue&&Near(halo.opacity,.42f));
    halo=peripheral::Select({},Read());CHECK(halo.blue>halo.red&&Near(halo.opacity,.22f));
    Tick(2,.4);Tick(2,.2);CHECK(!peripheral::Select({},Read()).Active());

    // Invalid data and every gameplay interruption rebaseline without false clear/replay.
    for(const auto& block:std::initializer_list<std::function<void()>>{
        []{focused=false;},[]{tracking=false;},[]{vrMenu=true;},[]{mobileMenu=true;},
        []{userPause=true;},[]{codePause=true;},[]{scene=true;},[]{camera[0x43]=1;},
        []{Write(ped,0x6ac,0.f);},[]{Write<std::uint32_t>(ped,0x54c,54);},
        []{Write<std::uint32_t>(ped,0x54c,55);},[]{info[0x118]=1;},
        []{info[0x118]=2;},[]{info[0x118]=3;},[]{playerPtr=nullptr;},
        []{wantedPtr=nullptr;},[]{focus=2;},[]{g.CWorld_Players=nullptr;},
        []{g.CTimer_m_FrameCounter=nullptr;},[]{Write(wanted,0x2c,7);},
        []{Write(wanted,0x2c,-1);}}) {
        Setup(2);Tick(3);Tick(3,.08);CHECK(Read().opacity>0);
        block();Update(true);CHECK(Read().opacity==0);
        focused=tracking=true;vrMenu=mobileMenu=userPause=codePause=scene=false;
        camera[0x43]=0;info[0x118]=0;Write(ped,0x6ac,100.f);Write<std::uint32_t>(ped,0x54c,0);
        playerPtr=ped;wantedPtr=wanted;focus=0;g.CWorld_Players=info;g.CTimer_m_FrameCounter=&frame;
        Tick(0);Tick(0,.1);CHECK(Read().opacity==0);
        Tick(1);Tick(1,.08);CHECK(Read().opacity>0);
    }
    Setup(2);Tick(3);Tick(3,.08);Update(false);Tick(0);CHECK(Read().opacity==0);
    Setup(2);Tick(3);Tick(3,.08);GetFeedback(false,epoch);Tick(0);CHECK(Read().opacity==0);

    // Reused addresses after load: either backwards clock invalidates the baseline.
    for(bool resetFrame:{false,true}) {
        Setup(2);Tick(3);Tick(3,.08);
        if(resetFrame)frame=1;else gameMs=1;
        Tick(0);Tick(0,.1);CHECK(Read().opacity==0);
    }
    CHECK(!detail::MovedBackward(0xfffffff0U,5));CHECK(detail::MovedBackward(100,5));
    Setup(2);Tick(3);Tick(3,.08);Tick(0,1);CHECK(Read().opacity==0);
    Setup(2);Tick(3);Tick(3,.08);testNow+=.6;CHECK(Read().opacity==0);
    Setup(2);Tick(3);Tick(3,.08);CHECK(GetFeedback(true,epoch+1).opacity==0);
    ++epoch;Tick(0);CHECK(Read().opacity==0);
    Setup(2);playerPtr=otherPed;Write(info,0,playerPtr);Tick(0);CHECK(Read().opacity==0);
    Setup(2);wantedPtr=otherWanted;Tick(0);CHECK(Read().opacity==0);
    // Current player selection, rather than hard-coded player 0.
    Setup(2);focus=1;Write(info,0,static_cast<void*>(nullptr));
    Write(info,0x1d8,playerPtr);Tick(3);Tick(3,.08);CHECK(Read().opacity>0);
    // Missing hands/weapon functions cannot suppress a valid world cue.
    CHECK(g.FindPlayerVehicle==nullptr);CHECK(Read().opacity>0);
    std::puts("PASS: wanted envelopes, rapid transitions, priorities, raw observer, load/death/arrest/menu/tracking resets");
}

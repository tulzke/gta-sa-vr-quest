#include "Symbols.h"
#include "PhysicalWeapon.h"
#include "PairedWeapons.h"
#include "PlayerLife.h"
#include "VrCamera.h"
#include "ScopeAim.h"
#include "VrFire.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
namespace savr {
GameSymbols g{};
namespace fixture {
using V = GameSymbols::Vec3;
alignas(16) unsigned char player[0xa00]{}, ped[2][0xa00]{}, intelligence[2][0x200]{}, pad[0x160]{}, camera[0x100]{},
    info[48][0x70]{}, group[0x500]{}, task[0x30]{}, nested[0x20]{};
void* taskVtable[8]{};
template<class T> void Put(void* p, int offset, T value) { std::memcpy(static_cast<char*>(p)+offset,&value,sizeof(value)); }
template<class T> T Get(const void* p, int offset) { T v{}; std::memcpy(&v,static_cast<const char*>(p)+offset,sizeof(v)); return v; }
bool focused, stereo, ground, menu, mobileMenu, scripted, scene, tracking, currentEpoch, alive,
    detached[2], rayValid[2], opticValid[2], anchorBlock[2], wall[2], grouped[2], friends, seeing, allocation;
bool pause, codePause, skip, cutscene, deadOption, tyreOption, bikerOption;
void* ignore{}; void* seated{}; void* hit[2]{};
std::uint32_t frame{}; std::uint64_t epoch{};
int held[2]{}, handQuery{}, taskType{}, rayCalls{}, traceCalls{}, pedAdds{}, groupAdds{}, constructs{}, destroys{}, groupConstructs{}, groupDestroys{}, allocations{}, speeches{};
void* targets[8]{};
V endpoints[2]{};
bool validFlag{};
}
namespace xr {
struct HandPose { bool valid{}, aimValid{}; };
bool IsSessionFocused() { return fixture::focused; }
bool IsMenuVisible() { return fixture::menu; }
}
namespace vrcam {
bool IsStereoActive() { return fixture::stereo; }
bool IsMobileMenuOpen() { return fixture::mobileMenu; }
bool GroundWalkControlsActive() { return fixture::ground; }
bool CutsceneCameraControlsActive() { return fixture::scene; }
bool IsWeaponTrackingCurrent(std::uint64_t e) { return fixture::currentEpoch && e==fixture::epoch; }
bool GetWeaponFireSegment(int hand,int,float* o,float* d,float* a) {
    using namespace fixture; ++rayCalls; handQuery=hand;
    o[0]=float(hand); o[1]=1; a[0]=float(hand); d[1]=2; // direction normalisation is exercised
    return rayValid[hand];
}
}
namespace scopeaim {
bool ApplyReticleAim(int hand,int,const float*,float* d) { d[0]=1; return fixture::opticValid[hand]; }
}
namespace physicalweapon {
int HeldSlot(int hand) { return fixture::held[hand]; }
bool IsPrimaryDetached(int hand) { return fixture::detached[hand]; }
bool GetInteractionTrackingSnapshot(float*,float*,xr::HandPose* hands,std::uint64_t* epoch) {
    hands[0]={true,true}; hands[1]={true,true}; *epoch=fixture::epoch; return fixture::tracking;
}
}
namespace vrfire { bool IsScriptedWeaponCamera() { return fixture::scripted; } }
}
// PRODUCTION_NPC_AIM

using namespace savr;
using namespace savr::fixture;
using namespace savr::npcaim;
void Setup() {
    std::memset(player,0,sizeof(player)); std::memset(ped,0,sizeof(ped));
    std::memset(intelligence,0,sizeof(intelligence)); std::memset(pad,0,sizeof(pad));
    std::memset(info,0,sizeof(info)); std::memset(camera,0,sizeof(camera));
    focused=stereo=ground=tracking=currentEpoch=alive=seeing=allocation=true;
    menu=mobileMenu=scripted=scene=pause=codePause=skip=cutscene=friends=false;
    seated=nullptr; ignore=group; deadOption=false; tyreOption=true; bikerOption=false;
    handQuery=taskType=rayCalls=traceCalls=pedAdds=groupAdds=constructs=destroys=groupConstructs=groupDestroys=allocations=speeches=0;
    validFlag=true; frame=1; epoch=10;
    for (int h=0;h<2;++h) {
        held[h]=2; detached[h]=anchorBlock[h]=wall[h]=grouped[h]=false; rayValid[h]=opticValid[h]=true;
        hit[h]=ped[h]; Put(ped[h],0x5a,std::uint8_t(3)); Put(ped[h],0x6ac,100.f);
        Put(ped[h],0x538,static_cast<void*>(intelligence[h])); endpoints[h]={};
    }
    Put(player,0x6ac,100.f); Put(player,0x730+2*0x20,22); Put(player,0x730+4*0x20,31);
    // Empty magazines and non-ready weapon states must not inhibit a threat.
    Put(player,0x730+2*0x20+4,2u); Put(player,0x730+4*0x20+4,3u);
    for (int t=0;t<48;++t) { Put(info[t],0,1); Put(info[t],4,30.f); Put(info[t],8,90.f); Put(info[t],0x18,1u); }
    g={}; g.TheCamera=camera;
    g.FindPlayerPed=[](int)->void* { return player; };
    g.FindPlayerVehicle=[](int,bool) { return seated; };
    g.CPad_GetPad=[](int)->void* { return pad; };
    g.CPed_IsAlive=[](void*) { return alive; };
    g.CPed_GetWeaponSkill=[](void*,int)->std::uint8_t { return 1; };
    g.CWeaponInfo_GetWeaponInfo=[](int type,signed char)->void* { return info[type]; };
    g.CTimer_m_FrameCounter=&frame; g.CTimer_m_UserPause=&pause; g.CTimer_m_CodePause=&codePause;
    g.CTimer_bSkipProcessThisFrame=&skip; g.CCutsceneMgr_ms_running=&cutscene;
    g.CWorld_pIgnoreEntity=&ignore; g.CWorld_bIncludeDeadPeds=&deadOption;
    g.CWorld_bIncludeCarTyres=&tyreOption; g.CWorld_bIncludeBikers=&bikerOption;
    g.CWorld_ProcessLineOfSight=[](const V* from,const V* to,void*,void** entity,bool b,bool v,bool p,bool o,bool d,bool see,bool cam,bool shoot) {
        CHECK(ignore==player && deadOption && tyreOption && bikerOption);
        CHECK(b && v && p && o && d && !see && !cam && !shoot); ++traceCalls;
        if (from->y==0) { *entity=group; return anchorBlock[handQuery]; }
        endpoints[handQuery]=*to; *entity=wall[handQuery]?static_cast<void*>(group):hit[handQuery];
        return *entity!=nullptr;
    };
    ready=true; frameSeen=false; lastPlayer=0; lastFrame=0;
    gunVtable=reinterpret_cast<void*>(0x1000); groupVtable=reinterpret_cast<void*>(0x2000);
    for (int i=0;i<4;++i) emitters[i]={reinterpret_cast<void*>(std::uintptr_t(0x10000+i*0x1000)),0x200};
    gunCtor=[](void* e,void* source) { ++constructs; Put(e,0,gunVtable); Put(e,0x18,source); };
    gunDtor=[](void* e) { CHECK(Get<void*>(e,0x18)==player); ++destroys; };
    allocateEvent=[](std::size_t size)->void* { CHECK(size==0x20); ++allocations; return allocation?nested:nullptr; };
    groupCtor=[](void* e,void* target,void* event) { ++groupConstructs; Put(e,0,groupVtable); Put(e,0x10,target); Put(e,0x18,event); };
    groupDtor=[](void* e) { ++groupDestroys; gunDtor(Get<void*>(e,0x18)); };
    originalPedAdd=[](void* dest,void* e,bool valid)->void* {
        CHECK(dest==intelligence[0]+0xc8 || dest==intelligence[1]+0xc8);
        CHECK(Get<void*>(e,0x18)==player || Get<void*>(e,0x18)==ped[0]);
        targets[pedAdds]=dest; ++pedAdds; validFlag=valid; return e;
    };
    originalGroupAdd=[](void* dest,void* e) {
        CHECK(dest==group+0x60); CHECK(Get<void*>(e,0x18)!=nullptr); ++groupAdds; return true;
    };
    getGroup=[](const void* p)->void* { return grouped[p==ped[0]?0:1]?group:nullptr; };
    sameGroup=[](const void*,const void*) { return friends; };
    seeingRange=[](const void*,const V*) { return seeing; };
    taskVtable[5]=reinterpret_cast<void*>(+[](const void*) { return taskType; });
    Put(task,0,static_cast<void*>(taskVtable));
    activeTask=[](const void*)->void* { return taskType?task:nullptr; };
    say=[](void* p,unsigned short c,unsigned,float,unsigned char,unsigned char,unsigned char)->std::uint16_t { CHECK(p==player && c==176); ++speeches; return 0; };
}
void Tick() { ++frame; Update(false); }
void Restored() { CHECK(ignore==group && !deadOption && tyreOption && !bikerOption); }
int main() {
    Setup(); CHECK(CoreReady()); Update(false);
    CHECK(pedAdds==2 && constructs==2 && destroys==2 && !validFlag);
    CHECK(targets[0]==intelligence[0]+0xc8 && targets[1]==intelligence[1]+0xc8);
    CHECK(std::abs(endpoints[0].x-30/std::sqrt(5.f))<.001f); Restored();
    // The exact same type in both hands still uses two hand-specific rays.
    CHECK(rayCalls==2); Update(false); CHECK(pedAdds==2 && rayCalls==2);
    Tick(); CHECK(pedAdds==4); hit[0]=hit[1]=nullptr; Tick(); CHECK(pedAdds==4); Restored();
    hit[0]=ped[0]; Tick(); CHECK(pedAdds==5); // re-acquisition, no remembered sticky target
    Setup(); held[1]=4; Update(false); CHECK(pedAdds==2);
    Setup(); hit[1]=ped[0]; Update(false); CHECK(pedAdds==1 && constructs==1);
    Setup(); held[1]=15; Update(false); CHECK(pedAdds==2); // mirrored native slot
    Setup(); held[1]=-1; detached[0]=true; Update(false); CHECK(!pedAdds && !rayCalls);
    Setup(); held[1]=-1; anchorBlock[0]=true; Update(false); CHECK(!pedAdds && traceCalls==1); Restored();
    Setup(); wall[0]=wall[1]=true; Update(false); CHECK(!pedAdds); Restored();
    Setup(); hit[0]=player; hit[1]=nullptr; Update(false); CHECK(!pedAdds);
    Setup(); Put(ped[0],0x6ac,0.f); Put(ped[1],0x585,std::uint8_t(1)); Update(false); CHECK(!pedAdds);
    Setup(); rayValid[0]=false; opticValid[1]=false; Update(false); CHECK(!traceCalls && !pedAdds);
    Setup(); Put(info[22],4,std::numeric_limits<float>::quiet_NaN()); Update(false); CHECK(!rayCalls);
    Setup(); Put(info[22],0,2); Update(false); CHECK(!rayCalls);
    Setup(); Put(info[22],0x18,0u); Update(false); CHECK(!rayCalls);
    Setup(); Put(player,0x730+2*0x20,43); Update(false); CHECK(!rayCalls);
    Setup(); taskType=601; Update(false); CHECK(!constructs && !pedAdds && !groupAdds);
    taskType=0; Tick(); CHECK(pedAdds==2); // native scenario finished: react again
    Setup(); friends=true; Update(false); CHECK(!constructs);
    Setup(); seeing=false; Update(false); CHECK(!constructs);
    Setup(); grouped[0]=true; held[1]=-1; Update(false);
    CHECK(groupAdds==1 && !pedAdds && allocations==1 && constructs==1 && destroys==1 && groupConstructs==1 && groupDestroys==1);
    Setup(); grouped[0]=true; held[1]=-1; allocation=false; Update(false); CHECK(!constructs && !groupAdds);
    Setup(); Put(player,0x730+2*0x20,23); Update(false); CHECK(pedAdds==2 && !speeches);
    for (int gate=0;gate<18;++gate) {
        Setup(); bool blocked=false;
        switch(gate) {
        case 0: blocked=true; break; case 1: focused=false; break; case 2: stereo=false; break;
        case 3: ground=false; break; case 4: menu=true; break; case 5: mobileMenu=true; break;
        case 6: pause=true; break; case 7: codePause=true; break; case 8: skip=true; break;
        case 9: cutscene=true; break; case 10: scene=true; break; case 11: scripted=true; break;
        case 12: tracking=false; break; case 13: currentEpoch=false; ++epoch; break;
        case 14: seated=group; break; case 15: Put(player,0x6ac,0.f); break;
        case 16: Put(pad,0x110,std::uint16_t(1)); break; case 17: ready=false; break;
        }
        Update(blocked); CHECK(!rayCalls && !pedAdds && !groupAdds);
    }
    // Caller scoping: suppress only native CJ emitters. Forward group propagation,
    // NPC shooters and every unrelated event, including when the bridge is inactive.
    Setup(); alignas(16) unsigned char e[0x20]{}, wrapper[0x20]{};
    gunCtor(e,player); groupCtor(wrapper,ped[0],e);
    const auto caller=reinterpret_cast<std::uintptr_t>(emitters[0].address)+0x40;
    CHECK(!AddPedEvent(intelligence[0]+0xc8,e,false,caller)); CHECK(!pedAdds);
    CHECK(!AddGroupEvent(group+0x60,wrapper,caller)); CHECK(!groupAdds);
    CHECK(AddPedEvent(intelligence[0]+0xc8,e,false,0x70000)); CHECK(pedAdds==1);
    CHECK(AddGroupEvent(group+0x60,wrapper,0x70000)); CHECK(groupAdds==1);
    Put(e,0x18,static_cast<void*>(ped[0])); CHECK(AddPedEvent(intelligence[0]+0xc8,e,true,caller)); CHECK(pedAdds==2 && validFlag);
    Put(e,0x18,static_cast<void*>(player)); Put(e,0,static_cast<void*>(nullptr));
    CHECK(AddPedEvent(intelligence[0]+0xc8,e,false,caller)); CHECK(pedAdds==3);
    Put(e,0,gunVtable); stereo=false; CHECK(AddPedEvent(intelligence[0]+0xc8,e,false,caller)); CHECK(pedAdds==4);
    std::puts("NPC aim: dual hands, geometry/optics, occlusion, lifecycle, groups, 18 gates and native-emitter filtering passed");
}

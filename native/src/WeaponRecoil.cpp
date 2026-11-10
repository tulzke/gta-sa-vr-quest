#include "WeaponRecoil.h"
#include "Calib.h"
#include "PhysicalWeapon.h"
#include "Symbols.h"
#include "Xr.h"
#include <mutex>
#include <ctime>

namespace savr::recoil {
namespace {
std::mutex mutex;
struct Record { State state{}; int type{},slot{-1}; bool supported{}; } hands[2];
double previousTime{};
std::uint64_t previousEpoch{};
double Now(){timespec t{};clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
}
void Invalidate(int hand){
    if(hand<0 || hand>1)return;
    std::lock_guard<std::mutex> lock(mutex);
    hands[hand]={};
}
void Update(bool blocked){
    xr::HandPose tracked[2]{};float head[3]{},orientation[4]{};std::uint64_t epoch{};
    blocked=blocked || calib::PhysicalRecoilLevel()==0 ||
        !physicalweapon::GetInteractionTrackingSnapshot(head,orientation,tracked,&epoch);
    void* ped=g.FindPlayerPed?g.FindPlayerPed(-1):nullptr;
    int slots[2]{-1,-1},types[2]{};bool support[2]{};
    for(int h=0;h<2;++h){
        if(blocked || !ped || !tracked[h].valid || !tracked[h].aimValid)continue;
        slots[h]=physicalweapon::HeldSlot(h);
        if(slots[h]<=0 || slots[h]>=13)continue;
        // Same audited inventory layout as VrFire, not a render-time cache.
        types[h]=*reinterpret_cast<const int*>(static_cast<const unsigned char*>(ped)+
                                              0x730+slots[h]*0x20);
        support[h]=physicalweapon::SupportHand(h)>=0;
    }
    const double now=Now();
    std::lock_guard<std::mutex> lock(mutex);
    const float dt=previousTime?static_cast<float>(now-previousTime):0.f;
    previousTime=now;
    const bool reset=blocked || !ped || epoch!=previousEpoch || dt>.25f || dt<0;
    previousEpoch=epoch;
    for(int h=0;h<2;++h){
        auto& r=hands[h];
        if(reset || r.type!=types[h] || r.slot!=slots[h] || !Supported(types[h]))r={};
        if(blocked || !ped || !Supported(types[h]))continue;
        r.type=types[h];r.slot=slots[h];r.supported=support[h];
        r.state.Advance(reset?0.f:dt);
    }
}
void OnShot(int hand,int type,int skill){
    if(hand<0 || hand>1 || !Supported(type))return;
    int profileSkill=std::clamp(skill,0,2);
    if((type==33 || type==34) && g.CStats_StatTypesFloat)
        profileSkill=g.CStats_StatTypesFloat[79]<333.f?0:g.CStats_StatTypesFloat[79]<666.f?1:2;
    if(type==38)profileSkill=1; // no native progression for the minigun
    const auto tuning=calib::SnapshotRecoil(type,profileSkill);
    std::lock_guard<std::mutex> lock(mutex);
    auto& r=hands[hand];
    if(r.type!=type || r.slot<=0)return;
    r.state.Shot(ForWeapon(type,r.supported,calib::PhysicalRecoilLevel(),tuning));
}
Sample Snapshot(int hand,int type){
    if(hand<0 || hand>1)return {};
    std::lock_guard<std::mutex> lock(mutex);
    return hands[hand].type==type?hands[hand].state.sample:Sample{};
}
}

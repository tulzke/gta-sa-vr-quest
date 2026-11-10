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
struct Record { State state{}; int type{},slot{-1}; bool supported{},pendingDisarm{}; Sample disarmKick{}; } hands[2];
struct ReleasedHand {State state{};Vec backward{};} released[2];
double previousTime{};
std::uint64_t previousEpoch{};
double Now(){timespec t{};clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
}
void Invalidate(int hand){
    if(hand<0 || hand>1)return;
    std::lock_guard<std::mutex> lock(mutex);
    hands[hand]={};
    released[hand]={};
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
        if(reset || slots[h]>0 || !tracked[h].valid)released[h]={};
        else released[h].state.Advance(dt);
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
    const auto profile=ForWeapon(type,r.supported,calib::PhysicalRecoilLevel(),tuning);
    r.state.Shot(profile);
    if(tuning.disarmOneHanded && !r.supported){
        r.pendingDisarm=true;
        // Evaluate this very spring/jitter impulse at its nominal peak. This
        // supplies signed pitch/yaw from the actual recoil without advancing
        // the published frame or inventing another random horizontal angle.
        State peak=r.state;
        peak.Advance(1.f/profile.omega);
        r.disarmKick=peak.sample;
    }
}
void FinishShots(){
    struct Request{int slot{-1},type{};Sample kick{};State wrist{};} pending[2];
    {
        std::lock_guard<std::mutex> lock(mutex);
        for(int h=0;h<2;++h)if(hands[h].pendingDisarm){
            pending[h]={hands[h].slot,hands[h].type,hands[h].disarmKick,hands[h].state};
            hands[h].pendingDisarm=false;
        }
    }
    // Disarm clears physical ownership, which calls Invalidate back into us.
    // The recoil mutex must be released before entering PhysicalWeapon.
    for(int h=0;h<2;++h)if(pending[h].slot>0){
        physicalweapon::TrackingPose pose{};
        const bool havePose=physicalweapon::GetHeldVisualPoseTracking(h,pending[h].slot,&pose);
        if(physicalweapon::DisarmAfterShot(h,pending[h].slot,pending[h].type,pending[h].kick)){
            if(!havePose)physicalweapon::GetDroppedPoseTracking(h,&pose);
            std::lock_guard<std::mutex> lock(mutex);
            released[h]={pending[h].wrist,Unit(Read(pose.right))*(-1.f)};
        }
    }
}
Sample Snapshot(int hand,int type){
    if(hand<0 || hand>1)return {};
    std::lock_guard<std::mutex> lock(mutex);
    return hands[hand].type==type?hands[hand].state.sample:Sample{};
}
Vec ReleasedHandOffset(int hand){
    if(hand<0 || hand>1)return {};
    std::lock_guard<std::mutex> lock(mutex);
    return released[hand].backward*released[hand].state.sample.rear;
}
}

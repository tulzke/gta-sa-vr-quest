#pragma once
#include "TrackingRecovery.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace savr::serviceradio {
enum class Service { None, Police, Medic, Fire };
// First iteration covers ground service vehicles; air/boat profiles are separate.
inline Service ForModel(int m) {
    if(m==416)return Service::Medic;
    if(m==407)return Service::Fire;
    switch(m){case 427:case 432:case 490:case 523:case 528:
        case 596:case 597:case 598:case 599:case 601:return Service::Police;
        default:return Service::None;}
}
struct Mission { bool known{},onMission{};Service active{};void* token{}; };
struct Hand {bool valid{},free{},near{};float grip{},trigger{};};
struct State {
    int owner{-1};
    tracking::SeatKey seat{};
    bool gripDown[2]{true,true},triggerDown[2]{true,true},releaseBlock[2]{};
    bool pressed{},grabbed{},pending{},cancel{},delivering{};
    Service requested{};void* missionToken{};
    double deadline{},pulseUntil{},cooldown{};
    float press{};
    void ClearRequest(){pending=delivering=false;requested=Service::None;missionToken=nullptr;}
    void Reset(){owner=-1;gripDown[0]=gripDown[1]=true;triggerDown[0]=triggerDown[1]=true;
        pressed=grabbed=false;press=0;ClearRequest();seat={};}
    unsigned TriggerMask()const{
        unsigned mask=owner>=0?1u<<owner:0u;
        for(int h=0;h<2;++h)if(releaseBlock[h])mask|=1u<<h;
        return mask;
    }
    void Observe(const float trigger[2]){
        for(int h=0;h<2;++h)if(std::isfinite(trigger[h])&&trigger[h]<=.30f)releaseBlock[h]=false;
    }
    bool RequestMatches(const Mission& m,Service current)const{
        if(!m.known||current!=requested)return false;
        return cancel?m.active==requested&&m.token==missionToken:
            m.active==Service::None&&!m.onMission;
    }
    void Step(bool enabled,const tracking::SeatKey& key,const Hand hands[2],
              const Mission& mission,double now){
        pressed=grabbed=false;
        enabled=enabled&&std::isfinite(now)&&ForModel(key.model)!=Service::None;
        if(!enabled||!(seat==key)){
            if(owner>=0)releaseBlock[owner]=true;
            Reset();seat=key;
            if(!enabled)return;
        }
        const Service service=ForModel(key.model);
        if(pending&&(now>deadline||!RequestMatches(mission,service)))ClearRequest();
        if(delivering&&now>=pulseUntil)ClearRequest();
        for(int h=0;h<2;++h){
            const auto& in=hands[h];
            const bool valid=in.valid&&std::isfinite(in.grip)&&std::isfinite(in.trigger);
            if(owner==h&&(!valid||!in.free||in.grip<=.30f)){
                if(!valid||!in.free)ClearRequest();
                releaseBlock[h]=!valid||in.trigger>.30f;owner=-1;press=0;
            }
            if(owner<0&&valid&&in.free&&in.near&&!gripDown[h]&&in.grip>=.65f){
                owner=h;grabbed=true;triggerDown[h]=true;releaseBlock[h]=true;
            }
            if(owner==h){
                press=std::clamp((in.trigger-.30f)/.40f,0.f,1.f);
                if(in.trigger<=.30f)triggerDown[h]=false;
                if(in.trigger>=.70f&&!triggerDown[h]){
                    triggerDown[h]=true;pressed=true;
                    if(!pending&&(now>=cooldown||mission.active==service)&&mission.known&&
                       (mission.active==service||(mission.active==Service::None&&!mission.onMission))){
                        pending=true;cancel=mission.active==service;requested=service;
                        missionToken=mission.token;deadline=now+.25;cooldown=now+.60;
                    }
                }
            }
            if(valid&&in.grip<=.30f)gripDown[h]=false;
            else if(!valid||in.grip>=.65f)gripDown[h]=true;
        }
    }
    // Retail COPCAR needs widget 72 in consecutive polls (its own two-phase
    // cancel latch). Keep one bounded pulse, never consume it per individual query.
    bool Widget(int widget,const tracking::SeatKey& key,const Mission& mission,double now){
        if(!std::isfinite(now)||!pending||!(key==seat)||!RequestMatches(mission,ForModel(key.model))||now>deadline){ClearRequest();return false;}
        if(widget!=(cancel?72:70))return false;
        if(!delivering){delivering=true;pulseUntil=now+.12;deadline=pulseUntil;}
        if(now>=pulseUntil){ClearRequest();return false;}
        return true;
    }
};
}

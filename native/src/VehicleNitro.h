#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace savr::nitro {
// Runtime enlargement; the approved BLEND/GLB stay in their source dimensions.
constexpr float kModelScale=1.5f;
constexpr float kRad=.017453292519943295f;
constexpr float kTravel=22.f*kRad,kReach=.088f,kReturnTime=.18f,kSticky=.8f;
constexpr float kStartReserve=.1f;
inline float Angle(float position){return kTravel*(1.f-2.f*position);}
inline bool Sticky(float position){return position>=kSticky-.000001f;}
inline int Segments(float reserve){return std::clamp(static_cast<int>(std::ceil(std::clamp(reserve,0.f,1.f)*10.f)),0,10);}
struct Point {float x{},forward{},up{};};
inline bool Finite(Point p){return std::isfinite(p.x)&&std::isfinite(p.forward)&&std::isfinite(p.up);}
inline float Distance(Point p,float position){
    if(!Finite(p))return 1e6f;
    const float a=Angle(position),x=p.x-std::clamp(p.x,-.041f,.041f);
    const float f=p.forward+kReach*std::sin(a),u=p.up-kReach*std::cos(a);
    return std::max(0.f,std::sqrt(x*x+f*f+u*u)-.014f);
}
struct Hand {Point point{};float grip{};bool valid{},available{};};
struct State {
    int owner{-1};bool down[2]{true,true},on{};
    float position{},offset{},gripX{},returnFrom{},elapsed{kReturnTime};
    void Reset(){*this=State{};}
    void Return(bool enabled){owner=-1;on=enabled;returnFrom=position;elapsed=0;}
    void Step(const Hand hands[2],float dt,bool enabled,bool usable){
        if(!enabled){Reset();return;}
        dt=std::isfinite(dt)?std::clamp(dt,0.f,.1f):0.f;
        if(!usable&&(on||owner>=0)){Return(false);down[0]=down[1]=true;}
        if(owner>=0){
            const auto& h=hands[owner];
            if(!h.valid||!h.available||!Finite(h.point)||!std::isfinite(h.grip)||Distance(h.point,position)>.20f){Return(false);down[0]=down[1]=true;}
            else if(h.grip<=.30f)Return(usable&&Sticky(position));
        }
        if(owner<0){
            elapsed=std::min(kReturnTime,elapsed+dt);
            const float t=elapsed/kReturnTime,s=t*t*(3.f-2.f*t);
            position=returnFrom+((on?1.f:0.f)-returnFrom)*s;
            int best=-1;float distance=.06f;
            for(int h=0;h<2;++h){const auto& hand=hands[h];const float d=Distance(hand.point,position);
                if(hand.valid&&hand.available&&Finite(hand.point)&&std::isfinite(hand.grip)&&!down[h]&&hand.grip>=.65f&&d<distance){best=h;distance=d;}}
            if(best>=0){owner=best;offset=std::atan2(-hands[best].point.forward,hands[best].point.up)-Angle(position);gripX=std::clamp(hands[best].point.x,-.035f,.035f);}
        }
        if(owner>=0){const auto p=hands[owner].point;
            if(std::hypot(p.forward,p.up)<.025f)Return(false);
            else {const float a=std::clamp(std::remainder(std::atan2(-p.forward,p.up)-offset,6.28318530718f),-kTravel,kTravel);
                position=std::clamp((1.f-a/kTravel)*.5f,0.f,1.f);
                // Tiny held-hand hysteresis prevents chatter; release still
                // obeys the exact last-20-percent sticky zone above.
                on=usable&&(on?position>=kSticky-.02f:Sticky(position));}
        }
        for(int h=0;h<2;++h){if(!hands[h].valid||!std::isfinite(hands[h].grip))down[h]=true;
            else if(hands[h].grip<=.30f)down[h]=false;else if(hands[h].grip>=.65f)down[h]=true;}
    }
};
struct FeelEvent {int hand{-1};float amplitude{},duration{};bool click{};};
struct Feel {
    float block{},position{};
    void Reset(){*this=Feel{};}
    FeelEvent Step(const State& before,const State& after,float dt,bool valid){
        dt=std::isfinite(dt)?std::clamp(dt,0.f,.1f):0.f;
        block=std::max(0.f,block-dt);
        if(!valid){Reset();return {};}
        const int hand=after.owner>=0?after.owner:before.owner;
        if(hand>=0&&before.on!=after.on){block=.075f;position=after.position;return {hand,.7f,45.f,true};}
        if(after.owner<0){position=after.position;return {};}
        if(after.owner!=before.owner){block=.025f;position=after.position;return {after.owner,.22f,20.f,false};}
        // Position threshold rejects stationary tracking noise. Do not refresh
        // a weak pulse over a detent impulse that is still playing.
        if(block<=0&&std::abs(after.position-position)>.003f){
            position=after.position;block=.03f;
            return {after.owner,.08f+.06f*after.position,38.f,false};}
        return {};
    }
};
struct SpeedEffect {
    float amount{};
    void Reset(){amount=0;}
    float Step(bool flowing,float dt,bool context){
        if(!context){Reset();return amount;}
        dt=std::isfinite(dt)?std::clamp(dt,0.f,.1f):0.f;
        const float target=flowing?1.f:0.f;
        amount=target+(amount-target)*std::exp(-dt/(flowing?.10f:.14f));
        if(amount<.001f)amount=0;
        return amount;
    }
};
// Native +0xAC0: negative = current burst consumption, positive = cooldown.
// A partial burst must be positive while stopped: the transmission/audio also
// read the sign. Follow native recovery while OFF, keeping the paid burst so
// resuming it does not consume another kit charge, even after reaching 100%.
struct Burst {
    bool partial{},spent{};float progress{};
    void Clear(){*this=Burst{};}
    void Pause(float& value){if(value<0.f&&value>=-1.f){partial=true;progress=value;value=std::max(.000001f,1.f+progress);}}
    void Restore(float& value){if(partial){value=std::min(-.000001f,progress);partial=false;}}
    void AfterRecovery(float value){if(partial)progress=std::clamp(value,0.f,1.f)-1.f;}
    float Reserve(float value)const{return std::clamp(partial?1.f+progress:(value<0.f?1.f+value:value),0.f,1.f);}
    bool Usable(float value,int charges)const{return !spent&&(partial||value<0.f||(value>kStartReserve&&charges>0));}
    void After(float before,float after){if(before<0.f&&after>=0.f)spent=true;}
};
}

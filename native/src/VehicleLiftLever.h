#pragma once
#include <algorithm>
#include <cmath>

namespace savr::liftlever {
constexpr float kRad=.017453292519943295f;
constexpr float kTravel=25.f*kRad;
constexpr float kDeadzone=3.f*kRad;
constexpr float kReach=.18f;
inline bool Supported(int model) { return model==530||model==486||model==525||model==531; }
// Retail 2.11.311: moving collision adds the axis; TowTruckControl adds it
// too, but GetTowBarPos height is 1 - miscAngle / limit.
inline int RetailAxis(int model,float command,bool physical=false) {
    if(!Supported(model)||!std::isfinite(command))return 0;
    command=std::clamp(command,-1.f,1.f);
    // Both native readers discard |axis| <= 10. Preserve the physical 3-degree
    // neutral zone instead of adding a second, invisible angular deadzone.
    const float value=physical&&command!=0?std::copysign(11.f+117.f*std::abs(command),command):command*128.f;
    return static_cast<int>(value)*(model==525||model==531?-1:1);
}
inline float Command(float angle) {
    return std::copysign(std::clamp((std::abs(angle)-kDeadzone)/(kTravel-kDeadzone),0.f,1.f),angle);
}
struct Point { float x{},forward{},up{}; };
inline bool Finite(Point p) {return std::isfinite(p.x)&&std::isfinite(p.forward)&&std::isfinite(p.up);}
// Distance from the tracked palm to the horizontal capsule's surface.
inline float Distance(Point p,float angle) {
    if(!Finite(p))return 1e6f;
    const float x=p.x-std::clamp(p.x,-.039f,.039f);
    const float f=p.forward+kReach*std::sin(angle),u=p.up-kReach*std::cos(angle);
    return std::max(0.f,std::sqrt(x*x+f*f+u*u)-.016f);
}
struct Hand { Point point{}; float grip{}; bool valid{},available{}; };
struct State {
    int owner{-1};
    bool down[2]{true,true};
    float angle{},offset{},gripX{},returnAngle{},returnElapsed{.12f};
    bool neutral{true};
    void Reset() { *this=State{}; }
    float Output()const {return owner<0?0.f:Command(angle);}
    void Release() {owner=-1;returnAngle=angle;returnElapsed=0;}
    // Points are relative to the pivot in the rigid vehicle frame. Heading,
    // roll and driving translation therefore never become lever input.
    void Step(const Hand hands[2],float dt,bool enabled) {
        if(!enabled){Reset();return;}
        dt=std::isfinite(dt)?std::clamp(dt,0.f,.1f):0.f;
        if(owner>=0) {
            const auto& h=hands[owner];
            if(!h.valid||!h.available||!Finite(h.point)||!std::isfinite(h.grip)||
               h.grip<=.30f||Distance(h.point,angle)>.20f)Release();
        }
        if(owner<0) {
            returnElapsed=std::min(.12f,returnElapsed+dt);
            angle=returnAngle*(1.f-returnElapsed/.12f);
            int best=-1;float distance=.06f;
            for(int h=0;h<2;++h) {
                const auto& hand=hands[h];
                const float d=Distance(hand.point,angle);
                if(returnElapsed>=.12f&&hand.valid&&hand.available&&Finite(hand.point)&&std::isfinite(hand.grip)&&
                   !down[h]&&hand.grip>=.65f&&hand.point.up>.03f&&d<distance){best=h;distance=d;}
            }
            if(best>=0) {
                owner=best;const auto p=hands[best].point;
                offset=std::atan2(-p.forward,p.up)-angle;
                gripX=std::clamp(p.x,-.035f,.035f);
            }
        }
        if(owner>=0) {
            const auto p=hands[owner].point;
            if(std::hypot(p.forward,p.up)<.03f)Release();
            else angle=std::clamp(std::remainder(std::atan2(-p.forward,p.up)-offset,6.28318530718f),-kTravel,kTravel);
        }
        for(int h=0;h<2;++h) {
            if(!hands[h].valid||!std::isfinite(hands[h].grip))down[h]=true;
            else if(hands[h].grip<=.30f)down[h]=false;
            else if(hands[h].grip>=.65f)down[h]=true;
        }
    }
};
}

#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace savr::hydrocontrol {
constexpr float kRad=.017453292519943295f,kTravel=22.f*kRad,kReach=.055f;
// Deterministic mission simplification, applied to BOTH native beat data and
// VR hints. Cardinal keys keep their values; input is never chosen for player.
inline std::uint32_t MissionKey(std::uint32_t key){
    switch(key){case 5:return 1;case 6:return 2;case 7:return 3;case 8:return 4;
        case 11:return 9;case 12:return 10;case 15:return 13;case 16:return 14;default:return key;}
}
inline void SimplifyBeatWindow(void* data){
    if(!data)return;auto* p=static_cast<unsigned char*>(data);
    for(int i=0;i<21;++i){const int offset=i<20?i*8+4:0xa4;std::uint32_t key{};
        std::memcpy(&key,p+offset,4);key=MissionKey(key);std::memcpy(p+offset,&key,4);}
}
struct Point {float x{},y{},z{};};
inline Point operator+(Point a,Point b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Point operator-(Point a,Point b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Point operator*(Point a,float s){return {a.x*s,a.y*s,a.z*s};}
inline bool Finite(Point p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
inline float Length(Point p){return std::sqrt(p.x*p.x+p.y*p.y+p.z*p.z);}
// Swing about a horizontal axis; x/y are the tilt vector in radians.
inline Point Swing(Point p,float x,float y){
    const float a=std::hypot(x,y);if(a<1e-7f)return p;
    const Point n{-y/a,x/a,0};const float c=std::cos(a),s=std::sin(a),dot=n.x*p.x+n.y*p.y;
    return p*c+Point{n.y*p.z,-n.x*p.z,n.x*p.y-n.y*p.x}*s+n*(dot*(1-c));
}
inline Point Grip(float x,float y){return Swing({0,0,kReach},x,y);}
inline float Distance(Point p,float x,float y){return Finite(p)?std::max(0.f,Length(p-Grip(x,y))-.0225f):1e6f;}
inline Point Direction(int slot){if(slot<=0||slot>8)return {};const float a=(slot-1)*.7853981633974483f;return {std::sin(a),std::cos(a),0};}
inline int Nearest(float x,float y,bool four){
    const float a=std::atan2(x,y),step=four?1.5707963267948966f:.7853981633974483f;
    int i=static_cast<int>(std::floor(a/step+.5f));i=(i%(four?4:8)+(four?4:8))%(four?4:8);
    return 1+i*(four?2:1);
}
struct Hand {Point point{},finger{};float grip{};bool valid{},available{};};
struct Button {
    bool armed{},pressed{},clicked{};float hover{},depth{};
    void Reset(){*this=Button{};}
    void Step(Point p,bool valid,bool available,float dt){
        clicked=false;dt=std::isfinite(dt)?std::clamp(dt,0.f,.1f):0.f;
        const float radius=std::hypot(p.x,p.y);
        const bool near=valid&&available&&Finite(p)&&radius<.045f&&p.z>-.012f&&p.z<.08f;
        const float target=near?1.f:0.f;hover+=std::clamp(target-hover,-dt/.10f,dt/.10f);
        if(!valid||!available||!Finite(p)){armed=pressed=false;depth=0;return;}
        // Arm only from the front after a deliberate withdrawal; side/rear
        // entry and pose/tracking reacquisition below the cap cannot press.
        if(radius<.023f&&p.z>.018f&&p.z<.08f)armed=true;
        if(radius>.028f||p.z<-.015f){armed=pressed=false;depth=0;return;}
        if(pressed&&p.z>.014f){pressed=false;armed=true;}
        if(!pressed&&armed&&radius<.016f&&p.z<=-.004f){pressed=true;armed=false;clicked=true;}
        depth=near?std::clamp(-p.z/.006f,0.f,1.f):0.f;
    }
};
struct State {
    int owner{-1},slot{};bool down[2]{true,true},four{},changed{},recenterVisual{};
    Point offset{};float visualX{},visualY{},aimX{},aimY{};
    Button button[2];
    void Reset(){*this=State{};}
    void Step(const Hand h[2],float dt,bool enabled,bool fourWay){
        changed=false;dt=std::isfinite(dt)?std::clamp(dt,0.f,.1f):0.f;
        if(!enabled){Reset();return;}
        if(four!=fourWay){Reset();four=fourWay;}
        const int previous=slot;
        if(owner>=0){const auto& hand=h[owner];
            if(!hand.valid||!hand.available||!Finite(hand.point)||!std::isfinite(hand.grip)||
               hand.grip<=.30f||Distance(hand.point,visualX,visualY)>.18f){
                owner=-1;slot=0;aimX=aimY=0;down[0]=down[1]=true;}}
        if(owner<0){
            int best=-1;float distance=.055f;
            for(int i=0;i<2;++i){const auto& hand=h[i];const float d=Distance(hand.point,visualX,visualY);
                if(hand.valid&&hand.available&&Finite(hand.point)&&std::isfinite(hand.grip)&&!down[i]&&hand.grip>=.65f&&d<distance){best=i;distance=d;}}
            if(best>=0){owner=best;offset=h[best].point-Grip(visualX,visualY);}
        }
        if(owner>=0){
            const Point p=h[owner].point-offset;
            const float radius=std::hypot(p.x,p.y),a=std::atan2(radius,p.z);
            if(p.z<.012f||!std::isfinite(a)){owner=-1;slot=0;aimX=aimY=0;down[0]=down[1]=true;}
            else {
                const float x=radius>1e-6f?a*p.x/radius:0.f,y=radius>1e-6f?a*p.y/radius:0.f;
                if(slot&&a<8.f*kRad)slot=0;
                else if(!slot&&a>13.f*kRad)slot=Nearest(x,y,four);
                const Point d=Direction(slot);
                // A detent cannot rotate into its neighbour: return to neutral
                // first. Visual slack is small and cannot alter the command.
                float sx=x-d.x*kTravel,sy=y-d.y*kTravel;
                const float slack=std::hypot(sx,sy),limit=(slot?2.f:3.f)*kRad;
                if(slack>limit){sx*=limit/slack;sy*=limit/slack;}
                aimX=d.x*kTravel+sx;aimY=d.y*kTravel+sy;
            }
        }
        if(previous>0&&slot!=previous)recenterVisual=true;
        const float targetX=recenterVisual?0.f:aimX,targetY=recenterVisual?0.f:aimY;
        const float dx=targetX-visualX,dy=targetY-visualY,distance=std::hypot(dx,dy),step=dt*kTravel/.035f;
        if(distance<=step){visualX=targetX;visualY=targetY;if(recenterVisual)recenterVisual=false;}
        else if(distance>0){visualX+=dx*step/distance;visualY+=dy*step/distance;}
        if(owner<0&&std::hypot(visualX,visualY)<1e-5f)visualX=visualY=0;
        changed=previous!=slot;
        for(int i=0;i<2;++i){if(!h[i].valid||!std::isfinite(h[i].grip))down[i]=true;
            else if(h[i].grip<=.30f)down[i]=false;else if(h[i].grip>=.65f)down[i]=true;}
    }
    Point Command()const{return owner>=0?Direction(slot):Point{};}
};
} // namespace savr::hydrocontrol

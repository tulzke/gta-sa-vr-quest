#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include "CockpitBoundaryHaptic.h"

namespace savr::hydrocontrol {
constexpr float kRad=.017453292519943295f,kTravel=22.f*kRad,kReach=.055f;
constexpr float kModelScale=2.f,kNeutral=.011f,kEngage=.020f,kInputLimit=.026f;
constexpr float kCenterPlay=9.f*kRad;
// The fixed wrist sits behind/above the sphere. Its arc is longer than the
// sphere centre's arc; require a full hand stroke before the detent engages.
constexpr float kHandStroke=2.f;
constexpr bool kCardinalOnly=true;
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
inline float Dot(Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Point Cross(Point a,Point b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline Point Unit(Point p){const float n=Length(p);return n>1e-6f?p*(1.f/n):Point{};}
struct InputFrame {
    Point pivot{},right{1,0,0},forward{0,0,-1},up{0,1,0};float scale{1.f};
    Point Local(Point p)const{p=(p-pivot)*(1.f/scale);return {Dot(p,right),Dot(p,forward),Dot(p,up)};}
    Point Vector(Point p)const{return right*p.x+forward*p.y+up*p.z;}
    Point World(Point p)const{return pivot+Vector(p)*scale;}
};
inline InputFrame MountedFrame(Point root,Point right,Point forward,Point up,float scale){
    InputFrame frame{root,right,forward,up,scale};frame.pivot=frame.World({-.032f,.008f,.018f});return frame;
}
// OpenXR LOCAL uses Y-up. Freeze heading, while removing chassis pitch/roll
// from the control plane: a height change must not become an X/Y command.
inline InputFrame LevelControlFrame(InputFrame frame){
    const Point up{0,1,0};Point forward=frame.forward-up*Dot(frame.forward,up);
    if(Length(forward)>.1f){forward=Unit(forward);frame.right=Unit(Cross(forward,up));frame.forward=forward;}
    else {frame.right=Unit(frame.right-up*Dot(frame.right,up));frame.forward=Unit(Cross(up,frame.right));}
    frame.up=up;return frame;
}
inline float SweptRadius(Point a,Point b){
    const float dx=b.x-a.x,dy=b.y-a.y,n=dx*dx+dy*dy;
    const float t=n>1e-12f?std::clamp(-(a.x*dx+a.y*dy)/n,0.f,1.f):0.f;
    return std::hypot(a.x+dx*t,a.y+dy*t);
}
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
inline float BoundaryDistance(Point handDelta,float scale){
    const float radial=std::max(0.f,(std::hypot(handDelta.x,handDelta.y)-kInputLimit*kHandStroke)*scale);
    const float height=std::max(0.f,std::abs((handDelta.z-kReach)*scale)-.04f);
    return std::hypot(radial,height);
}
struct Hand {Point point{},finger{};float grip{};bool valid{},available{};};
struct Button {
    bool armed{},pressed{},clicked{},pointing{};float hover{},depth{};
    void Reset(){*this=Button{};}
    bool WithinHover(Point p)const{
        return Finite(p)&&std::hypot(p.x,p.y)<(pointing?.060f:.045f)&&
            p.z>(pointing?-.024f:-.012f)&&p.z<(pointing?.105f:.08f);
    }
    void Step(Point p,bool valid,bool available,float dt){
        clicked=false;dt=std::isfinite(dt)?std::clamp(dt,0.f,.1f):0.f;
        const float radius=std::hypot(p.x,p.y);
        pointing=valid&&available&&WithinHover(p);
        const float target=pointing?1.f:0.f;hover+=std::clamp(target-hover,-dt/.10f,dt/.10f);
        if(!valid||!available||!Finite(p)){armed=pressed=false;depth=0;return;}
        // A short withdrawal above the cap rearms it. The same threshold is
        // used on first approach and on repeats; depressed side/rear entry
        // and tracking reacquisition still cannot generate an edge.
        if(radius<.023f&&p.z>.003f&&p.z<.08f){armed=true;pressed=false;}
        if(radius>.028f||p.z<-.015f){armed=pressed=false;depth=0;return;}
        if(!pressed&&armed&&radius<.016f&&p.z<=-.004f){pressed=true;armed=false;clicked=true;}
        // An unarmed contact must not look like a successful button press.
        depth=pointing&&(armed||pressed)?(pressed?1.f:.45f*std::clamp(-p.z/.004f,0.f,1.f)):0.f;
    }
};
struct State {
    int owner{-1},slot{};bool down[2]{true,true},four{},changed{},recenterVisual{};
    Point grabOffset{};float visualX{},visualY{},aimX{},aimY{};
    InputFrame grabFrame{};std::uint64_t inputEpoch{};
    Point previousInput{};bool previousInputValid{};
    float boundaryError{};cockpitboundary::Feedback boundaryHaptic{};
    Button button[2];
    void Reset(){*this=State{};}
    void Step(const Hand h[2],float dt,bool enabled,bool fourWay){
        changed=false;boundaryError=0;fourWay=fourWay||kCardinalOnly;dt=std::isfinite(dt)?std::clamp(dt,0.f,.1f):0.f;
        if(!enabled){Reset();return;}
        if(four!=fourWay){Reset();four=fourWay;}
        const int previous=slot;
        if(owner>=0){const auto& hand=h[owner];
            if(!hand.valid||!hand.available||!Finite(hand.point)||!std::isfinite(hand.grip)||
               hand.grip<=.30f||Distance(hand.point-grabOffset,visualX,visualY)>.18f){
                owner=-1;slot=0;aimX=aimY=0;previousInputValid=false;down[0]=down[1]=true;}}
        if(owner<0){
            int best=-1;float distance=.055f;
            for(int i=0;i<2;++i){const auto& hand=h[i];const float d=Distance(hand.point,visualX,visualY);
                if(hand.valid&&hand.available&&Finite(hand.point)&&std::isfinite(hand.grip)&&!down[i]&&hand.grip>=.65f&&d<distance){best=i;distance=d;}}
            if(best>=0){owner=best;grabOffset=h[best].point-Grip(0,0);previousInputValid=false;}
        }
        if(owner>=0){
            // One immutable origin for this entire grip. Returning to neutral
            // or exceeding the gate must never move the physical centre.
            Point p=h[owner].point-grabOffset;
            boundaryError=BoundaryDistance(p,grabFrame.scale);
            p.x/=kHandStroke;p.y/=kHandStroke;
            float radius=std::hypot(p.x,p.y);
            const bool crossedCentre=previousInputValid&&SweptRadius(previousInput,p)<=kNeutral;
            previousInput=p;previousInputValid=true;
            // Clamp only the evaluated position, never the grab origin.
            // Zones share this coordinate frame; vertical wrist motion does
            // not change the planar command.
            if(radius>kInputLimit){const float factor=kInputLimit/radius;
                p.x*=factor;p.y*=factor;radius=kInputLimit;}
            // Keep a neutral frame even when a fast return skipped the whole
            // centre zone between two tracking samples.
            if(slot&&(radius<=kNeutral||crossedCentre))slot=0;
            else if(slot||radius>=kEngage){
                const int next=Nearest(p.x,p.y,four);
                // About 8 degrees of angular hysteresis at cardinal sector
                // borders. All neighbouring sectors remain reachable by arc.
                if(!slot||Dot(p,Direction(next))-Dot(p,Direction(slot))>radius*.20f)slot=next;
            }
            const float visualPerMetre=kTravel/(kReach*std::sin(kTravel));
            const Point d=Direction(slot);
            // Each destination is still a fixed detent, with small slack.
            float sx=p.x*visualPerMetre-d.x*kTravel,sy=p.y*visualPerMetre-d.y*kTravel;
            const float slack=std::hypot(sx,sy),limit=slot?2.f*kRad:kCenterPlay;
            if(slack>limit){sx*=limit/slack;sy*=limit/slack;}
            aimX=d.x*kTravel+sx;aimY=d.y*kTravel+sy;
            // Neutral has real free play too. Visual shaft/hand/bellows follow
            // the controller within this small circle, while Command() stays zero.
        }
        if(previous>0&&slot==0)recenterVisual=true;
        const float targetX=recenterVisual?0.f:aimX,targetY=recenterVisual?0.f:aimY;
        const float dx=targetX-visualX,dy=targetY-visualY,distance=std::hypot(dx,dy),step=dt*kTravel/.035f;
        if(distance<=step){visualX=targetX;visualY=targetY;if(recenterVisual)recenterVisual=false;}
        else if(distance>0){visualX+=dx*step/distance;visualY+=dy*step/distance;}
        if(owner<0&&std::hypot(visualX,visualY)<1e-5f)visualX=visualY=0;
        changed=previous!=slot;
        for(int i=0;i<2;++i){if(!h[i].valid||!std::isfinite(h[i].grip))down[i]=true;
            else if(h[i].grip<=.30f)down[i]=false;else if(h[i].grip>=.65f)down[i]=true;}
    }
    // Controller samples stay in OpenXR LOCAL space. Only an unowned lever
    // follows the moving cockpit for acquisition; a grab freezes its input
    // frame until release. Chassis translation/roll cannot steer or tear it off.
    void StepTracked(const Hand tracked[2],const InputFrame& cockpit,float dt,bool enabled,bool fourWay,std::uint64_t epoch){
        if(inputEpoch!=epoch)Reset();
        const int previous=owner;Hand local[2]{tracked[0],tracked[1]};
        for(int i=0;i<2;++i)local[i].point=(i==owner?grabFrame:cockpit).Local(tracked[i].point);
        if(owner>=0&&std::abs((local[owner].point.z-grabOffset.z-kReach)*grabFrame.scale)>.12f)
            local[owner].available=false; // deliberate vertical withdrawal: release, then require a fresh squeeze
        Step(local,dt,enabled,fourWay);
        if(owner>=0&&owner!=previous){
            grabFrame=LevelControlFrame(cockpit);
            grabOffset=grabFrame.Local(tracked[owner].point)-Grip(0,0);
            previousInputValid=false;
        }
        inputEpoch=epoch;
    }
    Point Command()const{return owner>=0?Direction(slot):Point{};}
};
} // namespace savr::hydrocontrol

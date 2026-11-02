#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace savr::vehicledoors {
struct Vec {
    float x{}, y{}, z{};
    Vec operator+(Vec b) const { return {x+b.x,y+b.y,z+b.z}; }
    Vec operator-(Vec b) const { return {x-b.x,y-b.y,z-b.z}; }
    Vec operator*(float s) const { return {x*s,y*s,z*s}; }
};
inline bool Finite(Vec v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
inline float Dot(Vec a,Vec b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline float Length(Vec v) { return std::sqrt(Dot(v,v)); }
// Distance to a finite panel, in door-local coordinates. The surface has a
// small interaction thickness; its top edge stays below the window.
inline float PanelDistance(Vec p,float x,float ymin,float ymax,float zmin,float zmax) {
    if(p.z>zmax+.001f)return 1e6f; // Do not extend the hit area into the window.
    const float dy=p.y-std::clamp(p.y,ymin,ymax);
    const float dz=p.z-std::clamp(p.z,zmin,zmax);
    return std::sqrt((p.x-x)*(p.x-x)+dy*dy+dz*dz);
}
inline Vec Cross(Vec a,Vec b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline Vec Rotate(Vec v,float a) {
    const float c=std::cos(a),s=std::sin(a);
    return {c*v.x-s*v.y,s*v.x+c*v.y,v.z};
}
struct Basis {
    Vec right{1,0,0},forward{0,1,0},up{0,0,1},position{};
    Vec Vector(Vec v) const { return right*v.x+forward*v.y+up*v.z; }
    Vec Point(Vec v) const { return position+Vector(v); }
    Vec Local(Vec p) const { const auto v=p-position;return {Dot(v,right),Dot(v,forward),Dot(v,up)}; }
    bool Valid() const {
        return Finite(position)&&Finite(right)&&Finite(forward)&&Finite(up)&&
            std::abs(Dot(right,right)-1)<.05f&&std::abs(Dot(forward,forward)-1)<.05f&&
            std::abs(Dot(up,up)-1)<.05f&&std::abs(Dot(right,forward))<.05f&&
            std::abs(Dot(right,up))<.05f&&std::abs(Dot(forward,up))<.05f;
    }
};
// A fresh squeeze is required after blocked/invalid tracking and after losing a
// grab. Walking into the handle with a closed hand never starts an interaction.
struct GripEdge {
    bool latched{true};
    bool Step(float grip,bool usable) {
        if (!usable||!std::isfinite(grip)) { latched=true;return false; }
        if (grip<=.35f) { latched=false;return false; }
        if (grip<.65f||latched) return false;
        latched=true;return true;
    }
};
struct Motion {
    bool active{};
    float angle{},closed{},open{},previousPhase{};
    float velocity{}; // radians/second, measured in the vehicle frame
    Vec hinge{};
    bool Begin(Vec pivot,Vec hand,float current,float min,float max) {
        const Vec r=hand-pivot;
        if (!Finite(pivot)||!Finite(hand)||r.x*r.x+r.y*r.y<.04f||
            !std::isfinite(current)||!std::isfinite(min)||!std::isfinite(max)||
            std::abs(max-min)<.2f||std::abs(max-min)>2.8f) return false;
        hinge=pivot;closed=min;open=max;
        angle=std::clamp(current,std::min(min,max),std::max(min,max));
        previousPhase=std::atan2(r.y,r.x);velocity=0;active=true;return true;
    }
    bool Step(Vec hand,float dt) {
        if (!active||!Finite(hand)||!std::isfinite(dt)||dt<=0||dt>.15f) return false;
        const Vec r=hand-hinge;
        if (r.x*r.x+r.y*r.y<.04f) return false;
        const float phase=std::atan2(r.y,r.x);
        const float delta=std::remainder(phase-previousPhase,6.28318530718f);
        if (std::abs(delta)>.8f) return false; // discontinuity, never a throw
        previousPhase=phase;
        const float previousAngle=angle;
        // Consume excess at the stops instead of accumulating an invisible
        // overshoot that the player must unwind before closing the door.
        angle=std::clamp(angle+std::clamp(delta,-8.f*dt,8.f*dt),
                         std::min(closed,open),std::max(closed,open));
        if (std::abs(angle-closed)<.012f) angle=closed;
        const float measured=(angle-previousAngle)/dt;
        velocity+=(measured-velocity)*(1.f-std::exp(-dt/.045f));
        return true;
    }
    float ReleaseVelocity(float dt) const {
        if (!active||!std::isfinite(dt)||dt<=0||dt>.15f||!std::isfinite(velocity)||
            std::abs(angle-closed)<.012f||std::abs(velocity)<.12f) return 0;
        // Retail CDoor::Process adds angular velocity once per update, without
        // multiplying by seconds. Convert using this input/physics tick duration.
        float native=std::clamp(velocity*std::clamp(dt,.005f,.05f),-.18f,.18f);
        // Retail only latches a swinging door on a closing impact exceeding
        // 0.1 native units. Preserve slow movements, but let a deliberate fast
        // closing throw cross that threshold even at high headset frame rates.
        if (velocity*(open-closed)<0 && std::abs(velocity)>=.6f &&
            std::abs(native)<.125f)
            native=std::copysign(.125f,velocity);
        return native;
    }
    float Ratio() const { return std::clamp((angle-closed)/(open-closed),0.f,1.f); }
};

struct Marker {
    Vec position{},direction{},up{};bool held{},fist{};
    float opacity{1.f},highlight{},scale{1.f};
    int seatIcon{};float progress{};
    Vec strikeDirection{};
    bool arrowOut{};
    bool doorGrab{};
    bool bikeGrip{};
    bool hideArrow{};
    bool panelVolume{};
    Vec halfExtent{}; // local right, forward, up
};
struct MarkerFrame {
    static constexpr int kCapacity=32;
    std::array<Marker,kCapacity> markers{};
    int count{};
    std::uint64_t epoch{};
    bool Usable(bool valid,std::uint64_t current) const {
        return valid&&epoch==current&&count>0&&count<=kCapacity;
    }
};
inline std::array<Vec,36> PanelVolume(const Marker& m) {
    const Vec right=Cross(m.direction,m.up)*m.halfExtent.x;
    const Vec forward=m.direction*m.halfExtent.y,up=m.up*m.halfExtent.z;
    const Vec p[8]={m.position-right-forward-up,m.position+right-forward-up,
        m.position+right+forward-up,m.position-right+forward-up,
        m.position-right-forward+up,m.position+right-forward+up,
        m.position+right+forward+up,m.position-right+forward+up};
    constexpr int indices[36]={0,2,1,0,3,2,4,5,6,4,6,7,
        0,1,5,0,5,4,1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7};
    std::array<Vec,36> mesh{};for(int i=0;i<36;++i)mesh[i]=p[indices[i]];return mesh;
}
// A horizontal bar socket, in the marker's local basis.
inline std::array<Vec,144> GripCylinder(const Marker& m) {
    std::array<Vec,144> mesh{};int n=0;
    const Vec axis=m.direction,up=m.up,across=Cross(axis,up);
    const Vec left=m.position-axis*.055f,right=m.position+axis*.055f;
    for(int i=0;i<12;++i){
        const float a=6.2831853f*i/12,b=6.2831853f*(i+1)/12;
        const Vec r=(up*std::cos(a)+across*std::sin(a))*.022f;
        const Vec s=(up*std::cos(b)+across*std::sin(b))*.022f;
        mesh[n++]=left+r;mesh[n++]=right+r;mesh[n++]=left+s;
        mesh[n++]=left+s;mesh[n++]=right+r;mesh[n++]=right+s;
        mesh[n++]=left;mesh[n++]=left+s;mesh[n++]=left+r;
        mesh[n++]=right;mesh[n++]=right+r;mesh[n++]=right+s;
    }
    return mesh;
}
// Six-sided 3D arrow, 18 cm long: still visible when looking along its
// opening direction. Its root is the actual grab point.
inline std::array<Vec,54> Arrow(const Marker& m) {
    std::array<Vec,54> mesh{};
    const Vec p=m.position,d=m.direction,u=m.up,v=Cross(d,u);
    int n=0;
    for (int i=0;i<6;++i) {
        const float a=6.28318530718f*i/6,b=6.28318530718f*(i+1)/6;
        const Vec r=u*std::cos(a)+v*std::sin(a),s=u*std::cos(b)+v*std::sin(b);
        const Vec x=p+r*.009f,y=p+s*.009f,z=x+d*.115f,w=y+d*.115f;
        mesh[n++]=x;mesh[n++]=y;mesh[n++]=z;
        mesh[n++]=z;mesh[n++]=y;mesh[n++]=w;
        mesh[n++]=p+d*.105f+r*.035f;
        mesh[n++]=p+d*.105f+s*.035f;
        mesh[n++]=p+d*.18f;
    }
    return mesh;
}
// Reverse the geometry around its existing endpoints without moving the
// calibrated grab point or the visible segment.
inline Marker ReverseArrow(Marker m) {
    m.position=m.position+m.direction*.18f;
    m.direction=m.direction*-1.f;
    return m;
}
struct ReleasedMotion {
    float angle{},closed{},open{},velocity{};
    bool slam{},holdOpen{};
    void Step(float dt) {
        if (!std::isfinite(dt)||dt<=0||dt>.15f) return;
        angle=std::clamp(angle+velocity*dt,std::min(closed,open),std::max(closed,open));
        if (slam && std::abs(angle-closed)<.012f) { angle=closed;velocity=0;return; }
        if (angle==open || angle==closed) velocity=0;
        if (!slam) {
            velocity*=std::exp(-3.f*dt);
            if (std::abs(velocity)<.05f) velocity=0;
        }
    }
    bool Done() const { return angle==closed || (!holdOpen && velocity==0); }
};
} // namespace savr::vehicledoors

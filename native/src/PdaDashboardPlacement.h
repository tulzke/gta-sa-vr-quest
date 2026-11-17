#pragma once
#include "VehicleDoorMotion.h"
#include "HudSettings.h"
namespace savr::pda {
using vehicledoors::Vec;using vehicledoors::Dot;using vehicledoors::Length;
constexpr float kAcross=-.32f,kUp=.12f;
// Offset axes stay in the mount frame even when the screen tilts.
struct Frame {Vec centre,right,up,normal,anchorUp,anchorNormal;};
inline Frame PlaceAtBase(Vec base,Vec right,Vec up,Vec normal,const hud::WristSettings& s){
    const float angle=s.pitchDeg*.01745329252f,c=std::cos(angle),sn=std::sin(angle);
    return {base+right*(s.acrossMm*.001f)+up*(s.liftMm*.001f)+normal*(s.alongMm*.001f),
        right,up*c+normal*sn,normal*c-up*sn,up,normal};
}
inline Frame Place(Vec wheel,Vec right,Vec up,Vec normal,const hud::WristSettings& s){
    return PlaceAtBase(wheel+right*kAcross+up*kUp,right,up,normal,s);
}
inline float Distance(const Frame& f,Vec point,bool portrait,float scaleTenths){
    const auto d=point-f.centre;
    const float width=(portrait?.075f:.12f)*scaleTenths*.1f*1.08f;
    const float height=portrait?width*1.6f:width/1.6f;
    const float x=std::max(0.f,std::abs(Dot(d,f.right))-width*.5f);
    const float y=std::max(0.f,std::abs(Dot(d,f.up))-height*.5f);
    const float z=std::max(0.f,std::abs(Dot(d,f.normal))-.015f);
    return std::sqrt(x*x+y*y+z*z);
}
inline hud::WristSettings Translate(hud::WristSettings start,const Frame& f,Vec delta){
    start.alongMm+=Dot(delta,f.anchorNormal)*1000.f;
    start.acrossMm+=Dot(delta,f.right)*1000.f;
    start.liftMm+=Dot(delta,f.anchorUp)*1000.f;
    return start;
}
// Extract only the twist about the mount's horizontal axis from the relative
// hand rotation. Pure yaw/roll have zero twist; arbitrary grip orientation at
// acquisition cancels out. At a 180-degree perpendicular swing the twist is
// undefined: retain the last valid pitch instead of jumping.
inline bool PitchDelta(const vehicledoors::Basis& start,const vehicledoors::Basis& current,
                       Vec up,Vec normal,float& degrees){
    const auto rotate=[&](Vec v){return current.Vector({Dot(v,start.right),Dot(v,start.forward),Dot(v,start.up)});};
    const Vec u=rotate(up),n=rotate(normal);
    const float sine=Dot(u,normal)-Dot(n,up),cosine=Dot(u,up)+Dot(n,normal);
    if(!std::isfinite(sine)||!std::isfinite(cosine)||std::hypot(sine,cosine)<.0001f)return false;
    degrees=std::atan2(sine,cosine)*57.2957795f;
    return true;
}
}

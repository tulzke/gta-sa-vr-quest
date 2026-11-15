#pragma once
#include "VehicleDoorMotion.h"
#include "HudSettings.h"
namespace savr::pda {
using vehicledoors::Vec;using vehicledoors::Dot;using vehicledoors::Length;
constexpr float kAcross=-.32f,kUp=.12f;
struct Frame {Vec centre,right,up,normal;};
inline Frame Place(Vec wheel,Vec right,Vec up,Vec normal,const hud::WristSettings& s){
    return {wheel+right*(kAcross+s.acrossMm*.001f)+up*(kUp+s.liftMm*.001f)+normal*(s.alongMm*.001f),right,up,normal};
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
    start.alongMm+=Dot(delta,f.normal)*1000.f;
    start.acrossMm+=Dot(delta,f.right)*1000.f;
    start.liftMm+=Dot(delta,f.up)*1000.f;
    return start;
}
}

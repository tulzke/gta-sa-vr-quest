#pragma once
#include "VehicleDoorMotion.h"

namespace savr::pickups {
// Render-only. Keep selection/contact on the weapon; move just the icon and
// its progress ring toward the actual HMD centre used for this stereo pair.
inline vehicledoors::Marker FacingHint(const vehicledoors::Marker& marker,
    vehicledoors::Vec head,vehicledoors::Vec headUp,vehicledoors::Vec headRight) {
    using namespace vehicledoors;
    const auto unit=[](Vec v,Vec fallback){const float n=Length(v);return std::isfinite(n)&&n>.0001f?v*(1.f/n):fallback;};
    Marker hint=marker;
    const Vec toHead=head-marker.position;
    const float distance=Length(toHead);
    const Vec normal=unit(toHead,unit(Cross(headRight,headUp),{0,0,1}));
    Vec right=Cross(headUp,normal);
    if(Length(right)<.0001f)right=headRight-normal*Dot(headRight,normal);
    if(Length(right)<.0001f)right=Cross(std::abs(normal.x)<.8f?Vec{1,0,0}:Vec{0,0,1},normal);
    right=unit(right,{1,0,0});
    hint.direction=right*marker.scale;
    hint.up=unit(Cross(normal,right),{0,1,0})*marker.scale;
    if(std::isfinite(distance))hint.position=marker.position+normal*std::min(.06f,distance*.25f);
    return hint;
}
} // namespace savr::pickups

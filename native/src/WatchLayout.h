#pragma once
#include <algorithm>
#include <cmath>

namespace savr::watchhud {
struct Readout { int time=-1,money=-1,wanted=0;float health=-1,armour=-1; };
// Shared by the producer's blip placement and the consumer's face shader.
constexpr float kMapRadius=.385f, kMapTop=-.28f, kMapBottom=.185f;
constexpr float kTextureScale=.8f, kRasterSize=800.f;
constexpr float kMapPixels=kTextureScale*kRasterSize;
constexpr float kIconScale=2.f, kMapIconRadius=24.f*kIconScale;
inline float Fraction(float value,float maximum) {
    return std::isfinite(value)&&std::isfinite(maximum)&&value>=0&&maximum>0
        ? std::clamp(value/maximum,0.f,1.f) : -1.f;
}
inline void Pin(float& x,float& y,float iconRadius=kMapIconRadius) {
    // Include the rotated icon's corners in the inset, not only its centre.
    const float inset=iconRadius*1.414214f;
    const float radius=kMapRadius*kMapPixels-inset;
    const float top=kMapTop*kMapPixels+inset;
    const float bottom=kMapBottom*kMapPixels-inset;
    const float distance=std::hypot(x,y);
    float scale=distance>radius ? radius/distance : 1.f;
    if(y<top) scale=std::min(scale,top/y);
    if(y>bottom) scale=std::min(scale,bottom/y);
    x*=scale;y*=scale;
}
inline float LocalOpacity(float x,float y) {
    const float outside=std::max({0.f,std::hypot(x,y)-kMapRadius*kMapPixels,
        kMapTop*kMapPixels-y,y-kMapBottom*kMapPixels});
    const float t=std::clamp(outside/kMapIconRadius,0.f,1.f);
    return 1.f-t*t*(3.f-2.f*t);
}
inline float ProjectedBlip(float& x,float& y,bool pin) {
    constexpr float radius=.5f*kMapPixels,icon=24.f;
    const float distance=std::hypot(x,y);
    if(pin){const float factor=std::min(1.f,(radius-icon*1.414214f)/std::max(distance,.001f));x*=factor;y*=factor;return 1.f;}
    const float t=std::clamp((distance-radius)/icon,0.f,1.f);
    return 1.f-t*t*(3.f-2.f*t);
}
}

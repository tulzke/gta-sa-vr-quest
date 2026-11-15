#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace savr::watchhud {
constexpr float kProjectionHeight=.20f, kProjectionDiameter=.20f;
// 640 raster pixels / (400/300 pixels per metre * .25) = 1920 m
// across the projection at its widest view (roughly half Los Santos).
constexpr float kZoomMin=.25f,kZoomMax=2.f;
constexpr float kSliderAcross=.125f,kSliderHalfHeight=.045f;
struct ZoomSlider {
    bool dragging=false,requireRelease=true,touching=false;
    float zoom=1.f;
    // x/y/z are relative to the scale rail in the projection's reading basis.
    bool Update(bool enabled,float trigger,float x,float y,float z,float current) {
        bool ended=false;
        touching=enabled&&std::abs(x)<.018f&&std::abs(y)<=kSliderHalfHeight+.012f&&std::abs(z)<.020f;
        if(!enabled||trigger<.30f){ended=dragging;dragging=false;requireRelease=!enabled;}
        if(touching&&!requireRelease&&trigger>=.65f&&!dragging)dragging=true;
        if(dragging){
            if(std::abs(x)>.065f||std::abs(z)>.065f||std::abs(y)>kSliderHalfHeight+.065f){
                // A physical leave/re-entry is a new touch even while Point
                // stays held. Context loss above still requires a release.
                dragging=false;requireRelease=false;ended=true;
            }
            else zoom=kZoomMin+(kZoomMax-kZoomMin)*std::clamp((y+kSliderHalfHeight)/(2*kSliderHalfHeight),0.f,1.f);
        }else zoom=current;
        return ended;
    }
};
// Updated once before the stereo eye loop. Contact coordinates are measured
// from the calibrated dial, with z pointing out of its visible surface.
struct Projection {
    bool open=false, latched=false, requireRelease=true;
    float progress=0;
    std::uint64_t epoch=0;
    int wrist=-1;
    void Reset(std::uint64_t nextEpoch,int hand) {
        open=latched=false;requireRelease=true;progress=0;epoch=nextEpoch;wrist=hand;
    }
    bool Update(bool valid,std::uint64_t nextEpoch,int hand,bool available,
                float trigger,float radial,float depth,float radius,float dt) {
        if(!valid||epoch!=nextEpoch||wrist!=hand){Reset(nextEpoch,hand);return false;}
        if(!available) { requireRelease=true;latched=false; }
        else if(trigger<.30f){requireRelease=false;latched=false;}
        else if(radial>radius+.025f||depth>.04f)latched=false;
        bool pressed=false;
        if(available&&!requireRelease&&!latched&&trigger>=.65f&&
           radial<=radius+.004f&&depth>=-.008f&&depth<=.014f) {
            open=!open;latched=true;pressed=true;
        }
        const float step=std::clamp(dt,0.f,.05f)/.28f;
        progress=std::clamp(progress+(open?step:-step),0.f,1.f);
        return pressed;
    }
    float Amount() const {return progress*progress*(3.f-2.f*progress);}
    bool Needed() const {return open||progress>0;}
};
}

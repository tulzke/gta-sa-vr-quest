#pragma once
#include <algorithm>
#include <cmath>

namespace savr::punch {
enum class Kind { Jab, Uppercut, Hook };
struct Vec { float x{},y{},z{}; };
inline float Length(Vec v) { return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z); }
struct Trajectory {
    Vec net{},last{},previousHorizontal{};
    float path{},horizontalPath{},turn{},absoluteTurn{},rise{},risingPath{};
    int samples{},curves{};
    Vec steps[96]{};
    int first{},count{};
    void Bend(Vec step) {
        const float h=std::hypot(step.x,step.z);
        if (h<.01f) return;
        const Vec direction{step.x/h,0,step.z/h};
        if (Length(previousHorizontal)>.5f) {
            const float dot=std::clamp(direction.x*previousHorizontal.x+direction.z*previousHorizontal.z,-1.f,1.f);
            const float angle=std::atan2(previousHorizontal.x*direction.z-previousHorizontal.z*direction.x,dot);
            turn+=angle;absoluteTurn+=std::abs(angle);
            if (std::abs(angle)>.025f) ++curves;
        }
        previousHorizontal=direction;
    }
    void Rebuild() {
        net={};last={};previousHorizontal={};horizontalPath=turn=absoluteTurn=rise=risingPath=0;
        samples=count;curves=0;
        Vec bin{};float binPath=0,low=0;
        for (int i=0;i<count;++i) {
            const Vec step=steps[(first+i)%96];
            const float length=Length(step);
            net.x+=step.x;net.y+=step.y;net.z+=step.z;
            horizontalPath+=std::hypot(step.x,step.z);
            risingPath+=length;
            if (net.y<low) { low=net.y;risingPath=0; }
            // Spatial 6 cm bins remove per-frame controller jitter without
            // making shape depend on the game/XR sampling frequency.
            float remaining=length;
            while (remaining>1.e-5f) {
                const float take=std::min(.06f-binPath,remaining),t=take/length;
                bin.x+=step.x*t;bin.y+=step.y*t;bin.z+=step.z*t;
                remaining-=take;binPath+=take;
                if (binPath>=.05999f) { Bend(bin);bin={};binPath=0; }
            }
        }
        if (binPath>=.025f) Bend(bin);
        rise=net.y-low;
        // The last 12 cm represent impact direction, not one noisy sample.
        float tail=.12f;
        for (int i=count-1;i>=0 && tail>0;--i) {
            const Vec step=steps[(first+i)%96];const float length=Length(step);
            const float take=std::min(tail,length),t=take/length;
            last.x+=step.x*t;last.y+=step.y*t;last.z+=step.z*t;tail-=take;
        }
    }
    void Add(Vec step) {
        const float length=Length(step);
        if (!std::isfinite(length) || length<1.e-5f) return;
        if (count==96) { path-=Length(steps[first]);first=(first+1)%96;--count; }
        steps[(first+count)%96]=step;++count;path+=length;
        // Classify the final strike, never metres of earlier preparation.
        while (path>.60f && count) {
            Vec& old=steps[first];const float n=Length(old),excess=path-.60f;
            if (n<=excess+1.e-6f) { path-=n;first=(first+1)%96;--count; }
            else { const float scale=(n-excess)/n;old={old.x*scale,old.y*scale,old.z*scale};path=.60f; }
        }
        Rebuild();
    }
    Kind Classify(Vec headRight) const {
        if (samples<2 || path<.399f) return Kind::Jab;
        const float lastLength=Length(last);
        const float chord=std::hypot(net.x,net.z),lastHorizontal=std::hypot(last.x,last.z);
        const float lateral=std::abs(net.x*headRight.x+net.z*headRight.z);
        const float sideAtContact=lastHorizontal>.001f
            ?std::abs(last.x*headRight.x+last.z*headRight.z)/lastHorizontal:0;
        // A sideward straight punch is still a jab: three positions must
        // show a coherent signed bend and an arc longer than its chord.
        if (curves>=1 && horizontalPath>=.28f && std::abs(net.y)<.28f &&
            std::abs(turn)>=.30f && std::abs(turn)>=.50f*absoluteTurn &&
            horizontalPath>=1.01f*chord && lateral>=.10f && sideAtContact>=.35f)
            return Kind::Hook;
        // Measure ascent from the low point of the recent stroke, so drawing
        // the fist down before driving up cannot cancel an actual uppercut.
        if (rise>=.12f && rise>=.30f*risingPath && lastLength>0 && last.y>=.25f*lastLength)
            return Kind::Uppercut;
        return Kind::Jab;
    }
};
}

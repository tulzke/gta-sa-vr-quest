#pragma once
#include <algorithm>
#include <cmath>

namespace savr::punch {
enum class Kind { Jab, Uppercut, Hook };
struct Vec { float x{},y{},z{}; };
inline float Length(Vec v) { return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z); }
struct Trajectory {
    Vec net{},last{},previousHorizontal{};
    float path{},horizontalPath{},turn{},absoluteTurn{};
    int samples{},curves{};
    void Add(Vec step) {
        net.x+=step.x;net.y+=step.y;net.z+=step.z;
        path+=Length(step);last=step;++samples;
        const float h=std::hypot(step.x,step.z);
        if (h<.003f) return;
        const Vec direction{step.x/h,0,step.z/h};
        if (Length(previousHorizontal)>.5f) {
            const float dot=std::clamp(direction.x*previousHorizontal.x+direction.z*previousHorizontal.z,-1.f,1.f);
            const float angle=std::atan2(previousHorizontal.x*direction.z-previousHorizontal.z*direction.x,dot);
            turn+=angle;absoluteTurn+=std::abs(angle);
            if (std::abs(angle)>.025f) ++curves;
        }
        previousHorizontal=direction;horizontalPath+=h;
    }
    Kind Classify(Vec headRight) const {
        if (samples<3 || path<.399f) return Kind::Jab;
        const float lastLength=Length(last);
        // Uppercut must both climb over the stroke and still travel upward
        // at contact. Raising the hand and then jabbing is not an uppercut.
        if (net.y>=.24f && net.y>=.55f*path && lastLength>0 && last.y>=.5f*lastLength)
            return Kind::Uppercut;
        const float chord=std::hypot(net.x,net.z),lastHorizontal=std::hypot(last.x,last.z);
        const float lateral=std::abs(net.x*headRight.x+net.z*headRight.z);
        const float sideAtContact=lastHorizontal>.001f
            ?std::abs(last.x*headRight.x+last.z*headRight.z)/lastHorizontal:0;
        // A sideward straight punch is still a jab: require several curved
        // samples, a coherent signed bend and arc longer than its chord.
        if (curves>=2 && horizontalPath>=.32f && std::abs(net.y)<.20f &&
            std::abs(turn)>=.60f && std::abs(turn)>=.8f*absoluteTurn &&
            horizontalPath>=1.02f*chord && lateral>=.16f && sideAtContact>=.65f)
            return Kind::Hook;
        return Kind::Jab;
    }
};
}

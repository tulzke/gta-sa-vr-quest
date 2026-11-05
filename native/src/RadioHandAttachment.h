#pragma once

#include <cstdint>

namespace savr::radio {

struct HandVector { float x{},y{},z{}; };
inline HandVector operator+(HandVector a,HandVector b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline HandVector operator-(HandVector a,HandVector b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline HandVector operator*(HandVector a,float b) { return {a.x*b,a.y*b,a.z*b}; }
inline float Dot(HandVector a,HandVector b) { return a.x*b.x+a.y*b.y+a.z*b.z; }

struct HandFrame { HandVector center,right,up,normal; };
struct HandPose { HandVector position,right,up,forward; };

// Cache the tracked wrist pose in the knob's turning frame on the first
// rendered frame of each grab. Both hands and each new grab are independent.
struct HandAttachment {
    std::uint64_t grabId{};
    HandPose local{};

    void Reset() { grabId=0; }
    HandPose Update(std::uint64_t id,const HandFrame& frame,const HandPose& tracked) {
        const auto toLocal=[&](HandVector v) {
            return HandVector{Dot(v,frame.right),Dot(v,frame.up),Dot(v,frame.normal)};
        };
        if (id!=grabId || id==0) {
            grabId=id;
            local={toLocal(tracked.position-frame.center),toLocal(tracked.right),
                   toLocal(tracked.up),toLocal(tracked.forward)};
            return tracked;
        }
        const auto toWorld=[&](HandVector v) {
            return frame.right*v.x+frame.up*v.y+frame.normal*v.z;
        };
        return {frame.center+toWorld(local.position),toWorld(local.right),
                toWorld(local.up),toWorld(local.forward)};
    }
};
} // namespace savr::radio

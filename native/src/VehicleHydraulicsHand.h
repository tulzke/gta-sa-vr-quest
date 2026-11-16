#pragma once
#include "HandRenderFrame.h"
#include <algorithm>

namespace savr::hydrocontrol {
struct HandAttachment {
    radio::HandAttachment attachment{};
    radio::HandPose last{},delta{};bool held{};float release{};
    void Reset(){*this=HandAttachment{};}
    radio::HandPose Grab(std::uint64_t id,const radio::HandFrame& frame,const radio::HandPose& tracked){
        held=true;release=0;last=attachment.Update(id,frame,tracked);return last;
    }
    radio::HandPose Free(const radio::HandPose& tracked,float dt){
        if(held){held=false;release=1.f;attachment.Reset();
            delta={last.position-tracked.position,last.right-tracked.right,last.up-tracked.up,last.forward-tracked.forward};}
        const auto pose=radio::HandPose{tracked.position+delta.position*release,
            handrender::Unit(tracked.right+delta.right*release),handrender::Unit(tracked.up+delta.up*release),
            handrender::Unit(tracked.forward+delta.forward*release)};
        release=std::max(0.f,release-std::clamp(dt,0.f,.1f)/.12f);
        return pose;
    }
};
}

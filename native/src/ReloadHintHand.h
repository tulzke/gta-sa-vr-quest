#pragma once
#include "ReloadHint.h"
#include "HandSkeleton.h"

namespace savr::reloadhint {
inline std::shared_ptr<const Model> CopyPumpHandModel(const handskeleton::Library& library) {
    if (!library.loaded || library.vertices.empty() || library.vertices.size()>65535) return {};
    handskeleton::Palette palette{};
    library.Evaluate({handanimation::Gesture::GripShotgunOffhand,handanimation::Gesture::GripShotgunOffhand,1,0},0,0,palette);
    auto mesh=std::make_shared<Model>();mesh->indices=library.indices;
    Vec low{100,100,100},high{-100,-100,-100};
    for(const auto& v:library.vertices) {
        float p[3]{};
        for(int j=0;j<8;++j)for(int r=0;r<3;++r) {
            const auto& m=palette[v.joints[j]];
            p[r]+=v.weights[j]*(m[12+r]+m[r]*v.pos[0]+m[4+r]*v.pos[1]+m[8+r]*v.pos[2]);
        }
        const Vec point{p[0],-p[2],p[1]}; // Same glTF -> hand mapping as the GPU.
        mesh->vertices.insert(mesh->vertices.end(),{point.x,point.y,point.z});
        low={std::min(low.x,point.x),std::min(low.y,point.y),std::min(low.z,point.z)};
        high={std::max(high.x,point.x),std::max(high.y,point.y),std::max(high.z,point.z)};
    }
    mesh->center=arcadereload::Scale(arcadereload::Add(low,high),.5f);
    mesh->extent=std::max({high.x-low.x,high.y-low.y,high.z-low.z});
    return mesh;
}
inline void AddPumpHands(VisualFrame& frame,const std::shared_ptr<const Model>& mesh) {
    if (!mesh) return;
    for(int h=0;h<2;++h) {
        const auto& weapon=frame.items[h];
        if (!weapon.model || weapon.pumpHand<0 || weapon.opacity<=0) continue;
        const auto& wrist=weapon.pumpWrist;auto& hand=frame.items[2+h];
        hand.model=mesh;hand.opacity=weapon.opacity;
        hand.pose={arcadereload::Add(wrist.position,arcadereload::Scale(wrist.top,-.055f)),
            wrist.top,arcadereload::Scale(wrist.side,-1),wrist.barrel};
    }
}
} // namespace savr::reloadhint

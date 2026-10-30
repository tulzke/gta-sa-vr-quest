#pragma once
#include <cmath>
#include <cstdint>

namespace savr::theater {
// A level screen in LOCAL space. The right and forward axes contain yaw only.
struct Screen {
    float x{}, y{}, z{}, rightX{1}, rightZ{}, forwardX{}, forwardZ{-1};
    float width{3.2f}, height{2.24f};
    std::uint64_t opening{}, epoch{};
    bool valid{};
    bool Update(bool usable, bool flat, std::uint64_t nextOpening,
                std::uint64_t nextEpoch, float px, float py, float pz,
                float fx, float fz) {
        if (!usable || !flat) { valid=false; return false; }
        if (valid && opening==nextOpening && epoch==nextEpoch) return true;
        valid=false;
        const float length=std::hypot(fx,fz);
        // Looking straight up/down has no well-defined horizontal heading.
        if (!std::isfinite(length) || length<0.01f) return false;
        forwardX=fx/length; forwardZ=fz/length;
        rightX=-forwardZ; rightZ=forwardX;
        x=px+forwardX*2.2f; y=py; z=pz+forwardZ*2.2f;
        opening=nextOpening; epoch=nextEpoch; valid=true;
        return true;
    }
    bool Hit(float ox,float oy,float oz,float dx,float dy,float dz,float& u,float& v) const {
        if (!valid) return false;
        const float denom=dx*forwardX+dz*forwardZ;
        if (denom<=1e-4f) return false;
        const float t=((x-ox)*forwardX+(z-oz)*forwardZ)/denom;
        if (t<=0) return false;
        const float hx=(ox+t*dx-x)*rightX+(oz+t*dz-z)*rightZ;
        const float hy=oy+t*dy-y;
        if (!std::isfinite(hx) || !std::isfinite(hy) ||
            std::abs(hx)>width*0.5f || std::abs(hy)>height*0.5f) return false;
        u=hx/width+0.5f; v=0.5f-hy/height;
        return true;
    }
};
}

#pragma once
#include <algorithm>
#include <cmath>
namespace savr::punch {
struct JabCombo {
    double last{};
    unsigned count{};
    float energy{};
    // Returns shove speed in m/s. Even the hardest single jab returns zero.
    float Hit(double now,float speed) {
        if (!std::isfinite(speed) || speed<1.25f) { *this={};return 0; }
        if (last<=0 || now<last || now-last>.75) { count=0;energy=0; }
        last=now;++count;energy+=std::clamp(speed/4.f,.2f,1.5f);
        if (count<3 || energy<2.7f) return 0;
        const float shove=std::clamp(.7f+.35f*energy,1.f,1.8f);
        count=0;energy=0;
        return shove;
    }
};
}

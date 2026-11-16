#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace savr::nitroaudio {
constexpr int kClickRate=22050;
inline auto MechanicalClickPcm(){
    std::array<std::int16_t,1102> pcm{};
    std::uint32_t seed=0x4e324fu;float previous{};
    for(std::size_t i=0;i<pcm.size();++i){
        const float t=static_cast<float>(i)/kClickRate;
        seed=seed*1664525u+1013904223u;
        const float noise=static_cast<float>(seed>>8)/8388607.5f-1.f;
        const float high=noise-previous*.7f;previous=noise;
        const float second=std::max(0.f,t-.006f);
        const float impact=std::exp(-t*430.f)+(t>=.006f?.55f*std::exp(-second*520.f):0.f);
        const float body=(.21f*std::sin(6.2831853f*2200.f*t)+.16f*std::sin(6.2831853f*340.f*t))*std::exp(-t*135.f);
        const float edge=std::clamp(t/.00015f,0.f,1.f)*std::clamp((.0499f-t)/.006f,0.f,1.f);
        pcm[i]=static_cast<std::int16_t>(std::clamp((.55f*high*impact+body)*edge,-1.f,1.f)*27000.f);
    }
    return pcm;
}
}

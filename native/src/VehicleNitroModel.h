#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
namespace savr::nitro {
struct Vertex {float position[3],normal[3],color[3],uv[2],role;};
static_assert(sizeof(Vertex)==48);
struct Model {
    std::vector<Vertex> vertices;std::vector<std::uint16_t> indices;
    bool Load(const void* bytes,std::size_t size){
        vertices.clear();indices.clear();std::uint32_t h[4]{};
        if(!bytes||size<16)return false;std::memcpy(h,bytes,16);
        if(h[0]!=0x544e5653u||h[1]!=1||!h[2]||h[2]>65535||!h[3]||h[3]>15000||h[3]%3||size!=16+h[2]*sizeof(Vertex)+h[3]*2)return false;
        const auto* p=static_cast<const unsigned char*>(bytes)+16;
        std::vector<Vertex> v(h[2]);std::memcpy(v.data(),p,v.size()*sizeof(Vertex));
        std::vector<std::uint16_t> i(h[3]);std::memcpy(i.data(),p+v.size()*sizeof(Vertex),i.size()*2);
        for(const auto& x:v){for(float f:x.position)if(!std::isfinite(f)||std::abs(f)>.5f)return false;
            float n=0;for(float f:x.normal){if(!std::isfinite(f))return false;n+=f*f;}if(n<.98f||n>1.02f)return false;
            for(float f:x.color)if(!std::isfinite(f)||f<0||f>1)return false;
            for(float f:x.uv)if(!std::isfinite(f)||f< -1||f>1)return false;
            if(!std::isfinite(x.role)||x.role!=std::floor(x.role)||!((x.role>=0&&x.role<=1)||(x.role>=10&&x.role<=22)||x.role==30))return false;}
        for(auto index:i)if(index>=v.size())return false;
        vertices=std::move(v);indices=std::move(i);return true;
    }
};
}

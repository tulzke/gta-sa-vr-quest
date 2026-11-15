#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace savr::liftlever {
struct Vertex {float position[3],normal[3],color[3],moving;};
static_assert(sizeof(Vertex)==40);
struct Model {
    std::vector<Vertex> vertices;
    std::vector<std::uint16_t> indices;
    bool Load(const void* bytes,std::size_t size) {
        vertices.clear();indices.clear();
        if(!bytes||size<16)return false;
        std::uint32_t header[4];std::memcpy(header,bytes,16);
        const auto nv=header[2],ni=header[3];
        if(header[0]!=0x4c564153u||header[1]!=1||!nv||nv>65535||!ni||ni>4500||ni%3||
           size!=16+std::size_t(nv)*sizeof(Vertex)+std::size_t(ni)*2)return false;
        const auto* raw=static_cast<const unsigned char*>(bytes)+16;
        std::vector<Vertex> vv(nv);std::memcpy(vv.data(),raw,nv*sizeof(Vertex));
        std::vector<std::uint16_t> ii(ni);std::memcpy(ii.data(),raw+nv*sizeof(Vertex),ni*2);
        for(const auto& v:vv) {
            float length=0;
            for(int a=0;a<3;++a) {
                if(!std::isfinite(v.position[a])||std::abs(v.position[a])>.5f||
                   !std::isfinite(v.normal[a])||!std::isfinite(v.color[a])||
                   v.color[a]<0||v.color[a]>1)return false;
                length+=v.normal[a]*v.normal[a];
            }
            if(length<.98f||length>1.02f||(v.moving!=0.f&&v.moving!=1.f))return false;
        }
        for(auto i:ii)if(i>=nv)return false;
        vertices=std::move(vv);indices=std::move(ii);return true;
    }
};
}

#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
namespace savr::serviceradio {
struct Vertex {float position[3],normal[3],color[3],role;};
static_assert(sizeof(Vertex)==40);
struct Model {
    std::vector<Vertex> vertices;std::vector<std::uint16_t> indices;
    bool Load(const void* bytes,std::size_t size){
        vertices.clear();indices.clear();if(!bytes||size<16)return false;
        std::uint32_t h[4];std::memcpy(h,bytes,16);const auto nv=h[2],ni=h[3];
        if(h[0]!=0x37395253u||h[1]!=1||!nv||nv>65535||!ni||ni>60000||ni%3||size!=16+std::size_t(nv)*40+std::size_t(ni)*2)return false;
        const auto* raw=static_cast<const unsigned char*>(bytes)+16;
        std::vector<Vertex> vv(nv);std::memcpy(vv.data(),raw,nv*40);
        std::vector<std::uint16_t> ii(ni);std::memcpy(ii.data(),raw+nv*40,ni*2);
        for(const auto& v:vv){float n=0;for(int i=0;i<3;++i){
            if(!std::isfinite(v.position[i])||std::abs(v.position[i])>.25f||!std::isfinite(v.normal[i])||
               !std::isfinite(v.color[i])||v.color[i]<0||v.color[i]>1)return false;n+=v.normal[i]*v.normal[i];}
            if(n<.98f||n>1.02f||!std::isfinite(v.role)||v.role<0||v.role>3||std::floor(v.role)!=v.role)return false;}
        for(auto i:ii)if(i>=nv)return false;
        vertices=std::move(vv);indices=std::move(ii);return true;
    }
};
}

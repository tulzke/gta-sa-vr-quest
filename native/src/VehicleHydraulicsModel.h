#pragma once
#include "VehicleHydraulics.h"
#include <cstdint>
#include <cstring>
#include <vector>
namespace savr::hydrocontrol {
struct Vertex {float position[3],normal[3],color[3],uv[2],role,weight,z;};
static_assert(sizeof(Vertex)==56);
struct Model {
    std::vector<Vertex> vertices;std::vector<std::uint16_t> indices;
    bool Load(const void* bytes,std::size_t size){
        vertices.clear();indices.clear();std::uint32_t h[4]{};
        if(!bytes||size<16)return false;std::memcpy(h,bytes,16);
        if(h[0]!=0x59485653u||h[1]!=1||!h[2]||h[2]>65535||!h[3]||h[3]>16000||h[3]%3||size!=16+h[2]*sizeof(Vertex)+h[3]*2)return false;
        const auto* p=static_cast<const unsigned char*>(bytes)+16;
        std::vector<Vertex> v(h[2]);std::memcpy(v.data(),p,v.size()*sizeof(Vertex));
        std::vector<std::uint16_t> i(h[3]);std::memcpy(i.data(),p+v.size()*sizeof(Vertex),i.size()*2);
        for(const auto& a:v){for(float f:a.position)if(!std::isfinite(f)||std::abs(f)>.5f)return false;
            float n=0;for(float f:a.normal){if(!std::isfinite(f))return false;n+=f*f;}if(n<.98f||n>1.02f)return false;
            for(float f:a.color)if(!std::isfinite(f)||f<0||f>1)return false;
            for(float f:a.uv)if(!std::isfinite(f)||f< -1||f>1)return false;
            if(!std::isfinite(a.role)||a.role!=std::floor(a.role)||!((a.role>=0&&a.role<=5)||(a.role>=10&&a.role<=17)))return false;
            if(!std::isfinite(a.weight)||a.weight<0||a.weight>1||!std::isfinite(a.z))return false;}
        for(auto a:i)if(a>=v.size())return false;
        vertices=std::move(v);indices=std::move(i);return true;
    }
    static Vertex Deform(Vertex a,float x,float y,float press){
        Point p{a.position[0],a.position[1],a.position[2]},n{a.normal[0],a.normal[1],a.normal[2]};
        if(a.role==1){p=Swing(p,x,y)+Point{-.032f,.008f,.018f};n=Swing(n,x,y);}
        if(a.role==2){const Point center{-.032f,.008f,a.z};
            const Point delta=Swing({0,0,.027f},x,y)-Point{0,0,.027f};
            p=center+delta*a.weight+Swing(p-center,x*a.weight,y*a.weight);n=Swing(n,x*a.weight,y*a.weight);}
        if(a.role==3)p.z-=std::clamp(press,0.f,1.f)*.006f;
        a.position[0]=p.x;a.position[1]=p.y;a.position[2]=p.z;
        a.normal[0]=n.x;a.normal[1]=n.y;a.normal[2]=n.z;return a;
    }
};
}

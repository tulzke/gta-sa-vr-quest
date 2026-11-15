#pragma once
#include "ReloadHint.h"
#include <cstring>
#include <limits>
#include <utility>

namespace savr::reloadhint {
// Same audited 2.11.311 geometry/morph layout as Weapon.cpp. Frame parent at
// +8 and local matrix at +0x20 are confirmed by retail RwFrameAddChild.
template<class T> inline T Read(const void* object,int offset) {
    T value{};std::memcpy(&value,static_cast<const unsigned char*>(object)+offset,sizeof(value));return value;
}
using AtomicCallback=void*(*)(void*,void*);
using IterateAtomics=void*(*)(void*,AtomicCallback,void*);
struct ModelReader {
    Model mesh;
    const void* root{};
    bool failed{};
    static bool Finite(Vec v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
    static Vec Transform(const float* m,Vec p) {
        return {m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12],
                m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13],
                m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14]};
    }
    static void* Atomic(void* atomic,void* data) {
        auto& self=*static_cast<ModelReader*>(data);
        if (!atomic || self.failed) return atomic;
        const auto* bytes=static_cast<unsigned char*>(atomic);
        if (!(bytes[2]&4)) return atomic; // rpATOMICRENDER
        const void* geometry=Read<void*>(atomic,0x30);
        if (!geometry) return atomic;
        const int nv=Read<int>(geometry,0x1c),nt=Read<int>(geometry,0x18);
        const void* morph=Read<void*>(geometry,0xa0);
        const auto* triangles=Read<unsigned char*>(geometry,0x38);
        const float* vertices=morph?Read<float*>(morph,0x18):nullptr;
        if (nv<=0 || nt<=0) return atomic;
        if (nv>60000 || nt>200000 ||
            self.mesh.indices.size()/3+nt>300000 || !vertices || !triangles ||
            !morph || Read<void*>(morph,0)!=geometry) {self.failed=true;return atomic;}
        const void* frame=Read<void*>(atomic,8);
        std::array<std::array<float,16>,32> chain{};int depth=0;
        while (frame && frame!=self.root && depth<32) {
            std::memcpy(chain[depth++].data(),static_cast<const unsigned char*>(frame)+0x20,64);
            frame=Read<void*>(frame,8);
        }
        if (frame!=self.root) {self.failed=true;return atomic;}
        // HD country/sniper rifles contain unused vertices with garbage
        // coordinates. Match the indexed draw: copy/validate only referenced
        // vertices, so unused data cannot reject or distort the hint bounds.
        std::vector<int> remap(nv,-1);
        for(int i=0;i<nt;++i) {
            const auto* t=triangles+i*8;
            const auto a=Read<unsigned short>(t,0),b=Read<unsigned short>(t,2),c=Read<unsigned short>(t,4);
            if (a>=nv || b>=nv || c>=nv) continue;
            for(const auto index:{a,b,c}) {
                auto& mapped=remap[index];
                if (mapped<0) {
                    Vec p{vertices[index*3],vertices[index*3+1],vertices[index*3+2]};
                    for(int f=0;f<depth;++f)p=Transform(chain[f].data(),p);
                    if (!Finite(p) || self.mesh.vertices.size()/3>=65535) {self.failed=true;return atomic;}
                    mapped=static_cast<int>(self.mesh.vertices.size()/3);
                    self.mesh.vertices.insert(self.mesh.vertices.end(),{p.x,p.y,p.z});
                }
                self.mesh.indices.push_back(static_cast<unsigned short>(mapped));
            }
        }
        return atomic;
    }
};
inline std::shared_ptr<const Model> CopyModel(void* clump,IterateAtomics iterate) {
    if (!clump || !iterate) return {};
    ModelReader reader;reader.root=Read<void*>(clump,8);
    if (!reader.root) return {};
    iterate(clump,&ModelReader::Atomic,&reader);
    if (reader.failed || reader.mesh.indices.empty()) return {};
    const float inf=std::numeric_limits<float>::infinity();
    Vec lo{inf,inf,inf},hi{-inf,-inf,-inf};
    const auto& v=reader.mesh.vertices;
    for(size_t i=0;i<v.size();i+=3) {
        lo={std::min(lo.x,v[i]),std::min(lo.y,v[i+1]),std::min(lo.z,v[i+2])};
        hi={std::max(hi.x,v[i]),std::max(hi.y,v[i+1]),std::max(hi.z,v[i+2])};
    }
    reader.mesh.center={(lo.x+hi.x)*.5f,(lo.y+hi.y)*.5f,(lo.z+hi.z)*.5f};
    reader.mesh.extent=std::max({hi.x-lo.x,hi.y-lo.y,hi.z-lo.z});
    if (reader.mesh.extent<.001f || reader.mesh.extent>10.f) return {};
    return std::make_shared<const Model>(std::move(reader.mesh));
}
} // namespace savr::reloadhint

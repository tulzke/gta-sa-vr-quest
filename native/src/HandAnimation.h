#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace savr::handanimation {

enum class Gesture { Grip, Pinch, Point, OpenPalm, Beckon, GripPistol,
                     GripPistolOffhand, Fist, GripPistolShoot, Count };
inline constexpr std::array<const char*, static_cast<int>(Gesture::Count)> kNames{
    "Grip", "Pinch", "Point", "OpenPalm", "Beckon", "GripPistol",
    "GripPistolOffhand", "Fist", "GripPistolShoot"};

struct Vertex { float pos[3]{}, normal[3]{}; };
struct UxrhVertex { float pos[4][3], normal[4][3], uv[2]; };
static_assert(sizeof(UxrhVertex) == 104);
using Uv = std::array<float, 2>;

inline float Unit(float value) {
    return std::isfinite(value) ? std::clamp(value, 0.f, 1.f) : 0.f;
}
inline Vertex Mix(const Vertex& a, const Vertex& b, float t) {
    Vertex v;
    for (int i=0;i<3;++i) {
        v.pos[i]=a.pos[i]+(b.pos[i]-a.pos[i])*t;
        v.normal[i]=a.normal[i]+(b.normal[i]-a.normal[i])*t;
    }
    return v; // The final transformed normal is normalized by the renderer.
}

struct Segment { std::size_t first{}, last{}; float fraction{}; };
struct Clip {
    std::vector<float> frames;
    std::vector<Vertex> vertices; // frame-major, canonical right hand
    std::vector<Uv> uv;
    std::vector<unsigned short> indices;
    float fps{};

    bool Loaded() const { return frames.size() >= 2 && !uv.empty(); }
    Segment At(float progress) const {
        if (!Loaded()) return {};
        const float frame=frames.front()+(frames.back()-frames.front())*Unit(progress);
        const auto next=std::upper_bound(frames.begin(),frames.end(),frame);
        if (next==frames.end()) return {frames.size()-1,frames.size()-1,0.f};
        const std::size_t last=static_cast<std::size_t>(next-frames.begin());
        if (!last) return {};
        return {last-1,last,(frame-frames[last-1])/(frames[last]-frames[last-1])};
    }
    Vertex Sample(Segment segment, std::size_t vertex) const {
        return Mix(vertices[segment.first*uv.size()+vertex],
                   vertices[segment.last*uv.size()+vertex],segment.fraction);
    }
};

inline bool ReadBlock(const std::string& path, std::vector<UxrhVertex>& vertices,
                      std::vector<unsigned short>& indices) {
    FILE* file=std::fopen(path.c_str(),"rb");
    if (!file) return false;
    char magic[4]{};
    std::uint32_t version{},vc{},ic{};
    bool ok=std::fread(magic,1,4,file)==4 &&
        std::fread(&version,4,1,file)==1 && std::fread(&vc,4,1,file)==1 &&
        std::fread(&ic,4,1,file)==1 && std::memcmp(magic,"UXRH",4)==0 &&
        version==1 && vc>0 && vc<=2500 && ic>0 && ic<=27000 && ic%3==0;
    if (ok) {
        vertices.resize(vc); indices.resize(ic);
        ok=std::fread(vertices.data(),sizeof(UxrhVertex),vc,file)==vc &&
           std::fread(indices.data(),sizeof(unsigned short),ic,file)==ic &&
           std::fgetc(file)==EOF;
    }
    std::fclose(file);
    if (!ok) return false;
    for (auto index:indices) if (index>=vc) return false;
    for (const auto& v:vertices) {
        for (float x:v.uv) if (!std::isfinite(x)) return false;
        for (int pose=0;pose<4;++pose) {
            float norm=0;
            for (int a=0;a<3;++a) {
                if (!std::isfinite(v.pos[pose][a]) || std::abs(v.pos[pose][a])>2.f ||
                    !std::isfinite(v.normal[pose][a])) return false;
                norm+=v.normal[pose][a]*v.normal[pose][a];
            }
            if (std::abs(norm-1.f)>.01f) return false;
        }
    }
    return true;
}

inline bool LoadClip(const std::string& root, Gesture gesture, Clip& destination) {
    const std::string folder=root+"/"+kNames[static_cast<int>(gesture)]+"/";
    FILE* timing=std::fopen((folder+"timing.txt").c_str(),"rb");
    if (!timing) return false;
    char magic[5]{};
    unsigned version{},count{};
    Clip clip;
    bool ok=std::fscanf(timing,"%4s %u %u %f",magic,&version,&count,&clip.fps)==4 &&
        std::strcmp(magic,"UXRA")==0 && version==1 && count>=2 && count<=512 &&
        std::isfinite(clip.fps) && clip.fps>0.f && clip.fps<=1000.f;
    if (ok) {
        clip.frames.resize(count);
        for (unsigned i=0;i<count;++i) {
            if (std::fscanf(timing,"%f",&clip.frames[i])!=1 ||
                !std::isfinite(clip.frames[i]) ||
                (i && clip.frames[i]<=clip.frames[i-1])) {ok=false;break;}
        }
        char extra;
        if (ok && std::fscanf(timing," %c",&extra)==1) ok=false;
    }
    std::fclose(timing);
    if (!ok) return false;
    for (unsigned first=0;first<count;first+=4) {
        char name[64];
        std::snprintf(name,sizeof(name),"BigHandRight-%03u.uxrh",first/4);
        std::vector<UxrhVertex> block;
        std::vector<unsigned short> indices;
        if (!ReadBlock(folder+name,block,indices)) return false;
        if (first==0) {
            clip.indices=indices;
            for (const auto& v:block) clip.uv.push_back({v.uv[0],v.uv[1]});
            clip.vertices.reserve(count*block.size());
        } else if (indices!=clip.indices || block.size()!=clip.uv.size()) return false;
        for (std::size_t v=0;v<block.size();++v)
            if (clip.uv[v]!=Uv{block[v].uv[0],block[v].uv[1]}) return false;
        for (unsigned pose=0;pose<std::min(4u,count-first);++pose)
            for (const auto& v:block) {
                Vertex sample;
                for (int axis=0;axis<3;++axis) {
                    sample.pos[axis]=v.pos[pose][axis];
                    sample.normal[axis]=v.normal[pose][axis];
                }
                clip.vertices.push_back(sample);
            }
    }
    destination=std::move(clip);
    return true;
}

struct Library {
    std::array<Clip, kNames.size()> clips;
    const Clip& Get(Gesture gesture) const { return clips[static_cast<int>(gesture)]; }
    bool Load(const std::string& root) {
        Library fresh;
        for (std::size_t i=0;i<clips.size();++i) {
            if (!LoadClip(root,static_cast<Gesture>(i),fresh.clips[i])) return false;
            if (i && (fresh.clips[i].uv!=fresh.clips[0].uv ||
                      fresh.clips[i].indices!=fresh.clips[0].indices)) return false;
        }
        *this=std::move(fresh);
        return true;
    }
};

// Two paths meeting at the diagonal produce a continuous analog grip/trigger
// surface. Along either axis, use the authored animation, not endpoint lerp.
struct Selection {
    Gesture first{Gesture::Grip}, second{Gesture::Grip};
    float progress{}, blend{};
};
inline Selection FreeHand(float grip, float trigger) {
    grip=Unit(grip);trigger=Unit(trigger);
    if (grip>=trigger)
        return {Gesture::Grip,Gesture::Fist,grip,grip>0.f?trigger/grip:0.f};
    return {Gesture::Point,Gesture::Fist,trigger,grip/trigger};
}
inline bool Firearm(int weaponType) {
    return (weaponType>=22 && weaponType<=34) || weaponType==38;
}

struct Context {
    bool climb{}, pinchGrab{}, steering{}, ball{}, support{}, holdsWeapon{};
    int weaponType{};
};
inline Selection Select(float grip, float trigger, const Context& context) {
    const auto fixed=[](Gesture gesture,float progress=1.f) {
        return Selection{gesture,gesture,Unit(progress),0.f};
    };
    if (context.climb) return fixed(Gesture::Grip);
    if (context.pinchGrab) return fixed(Gesture::Pinch);
    if (context.steering) return fixed(Gesture::Grip,grip);
    if (context.ball) return fixed(Gesture::OpenPalm);
    if (context.support) return fixed(Gesture::Grip,grip);
    if (context.holdsWeapon) {
        if (Firearm(context.weaponType)) return fixed(Gesture::GripPistolShoot,trigger);
        return fixed(Gesture::Grip);
    }
    return FreeHand(grip,trigger);
}

} // namespace savr::handanimation

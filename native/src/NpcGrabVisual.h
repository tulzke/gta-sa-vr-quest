#pragma once
#include "VehicleDoorMotion.h"

namespace savr::npcgrab {
// Flat outlined grab and fist symbols without an opaque backdrop.
// Marker direction is billboard right, marker up is billboard up.
struct PalmMesh {
    std::array<vehicledoors::Vec,2048> vertices{};
    int count{};
};
inline PalmMesh Palm(const vehicledoors::Marker& marker) {
    PalmMesh mesh;
    const auto point=[&](float x,float y) { return marker.position+marker.direction*x+marker.up*y; };
    struct Point { float x,y; };
    constexpr float radius=.004f;
    const auto stroke=[&](std::initializer_list<Point> path) {
        auto prev=path.begin();
        for (auto it=path.begin();it!=path.end();++it) {
            for (int i=0;i<8;++i) {
                const float a=6.2831853f*i/8,b=6.2831853f*(i+1)/8;
                for (auto p:{point(it->x,it->y),point(it->x+radius*std::cos(a),it->y+radius*std::sin(a)),
                             point(it->x+radius*std::cos(b),it->y+radius*std::sin(b))}) mesh.vertices[mesh.count++]=p;
            }
            if (it==path.begin()) continue;
            const float dx=it->x-prev->x,dy=it->y-prev->y,len=std::hypot(dx,dy);
            if (len>0) {
                const float x=-dy*radius/len,y=dx*radius/len;
                const auto a=point(prev->x+x,prev->y+y),b=point(prev->x-x,prev->y-y);
                const auto c=point(it->x+x,it->y+y),d=point(it->x-x,it->y-y);
                for (auto p:{a,b,c,c,b,d}) mesh.vertices[mesh.count++]=p;
            }
            prev=it;
        }
    };
    // Curled fingers, viewed from the palm, with the thumb separated on the left.
    stroke({{-.025f,-.001f},{-.022f,.011f},{-.022f,.027f},{-.020f,.033f},
        {-.016f,.036f},{-.011f,.036f},{-.007f,.033f},{-.006f,.027f},
        {-.006f,.041f},{-.004f,.045f},{.001f,.047f},{.006f,.045f},
        {.008f,.041f},{.008f,.027f},{.008f,.035f},{.011f,.039f},
        {.016f,.040f},{.021f,.038f},{.023f,.034f},{.023f,.024f},
        {.024f,.030f},{.027f,.032f},{.031f,.031f},{.035f,.028f},
        {.038f,.022f},{.039f,.014f},{.039f,-.012f},{.037f,-.025f},
        {.034f,-.033f},{.029f,-.039f},{.022f,-.042f},{.014f,-.043f},
        {-.002f,-.043f},{-.013f,-.042f},{-.021f,-.039f},{-.028f,-.034f},
        {-.036f,-.025f},{-.039f,-.019f},{-.039f,.001f},{-.038f,.010f},
        {-.035f,.016f},{-.030f,.018f},{-.022f,.018f}});
    // Taper the thumb into its pointed inner edge, as in the reference.
    for (auto p:{point(-.026f,-.005f),point(-.026f,.005f),point(-.018f,.006f)})
        mesh.vertices[mesh.count++]=p;
    return mesh;
}
inline PalmMesh Fist(const vehicledoors::Marker& marker) {
    PalmMesh mesh;
    const auto point=[&](float x,float y) { return marker.position+marker.direction*x+marker.up*y; };
    // Rounded outline of a fist pointing at the viewer. The four curled
    // fingers are defined by the three open grooves and the thumb wraps below.
    struct Point { float x,y; };
    constexpr float radius=.0032f;
    const auto stroke=[&](std::initializer_list<Point> path) {
        auto prev=path.begin();
        for (auto it=path.begin();it!=path.end();++it) {
            for (int i=0;i<8;++i) {
                const float a=6.2831853f*i/8,b=6.2831853f*(i+1)/8;
                for (auto p:{point(it->x,it->y),point(it->x+radius*std::cos(a),it->y+radius*std::sin(a)),
                             point(it->x+radius*std::cos(b),it->y+radius*std::sin(b))}) mesh.vertices[mesh.count++]=p;
            }
            if (it==path.begin()) continue;
            const float dx=it->x-prev->x,dy=it->y-prev->y,len=std::hypot(dx,dy);
            if (len>0) {
                const float x=-dy*radius/len,y=dx*radius/len;
                const auto a=point(prev->x+x,prev->y+y),b=point(prev->x-x,prev->y-y);
                const auto c=point(it->x+x,it->y+y),d=point(it->x-x,it->y-y);
                for (auto p:{a,b,c,c,b,d}) mesh.vertices[mesh.count++]=p;
            }
            prev=it;
        }
    };
    stroke({{-.027f,-.019f},{-.034f,-.022f},{-.041f,-.020f},{-.046f,-.014f},
        {-.048f,-.004f},{-.048f,.011f},{-.045f,.025f},{-.041f,.032f},{-.036f,.035f},
        {-.031f,.033f},{-.028f,.030f},{-.025f,.037f},{-.019f,.040f},{-.013f,.040f},
        {-.008f,.037f},{-.006f,.033f},{-.004f,.040f},{.002f,.043f},{.009f,.043f},
        {.015f,.040f},{.018f,.033f},{.021f,.039f},{.027f,.040f},{.033f,.038f},
        {.038f,.033f},{.041f,.026f},{.043f,.013f},{.043f,-.008f},{.041f,-.018f},
        {.037f,-.024f},{.031f,-.027f},{.025f,-.027f},{.019f,-.024f},{.016f,-.020f}});
    stroke({{-.027f,.006f},{-.027f,-.020f},{-.025f,-.025f},{-.021f,-.029f},
        {-.015f,-.030f},{-.009f,-.028f},{-.006f,-.024f},{-.006f,.008f}});
    stroke({{-.006f,-.024f},{-.003f,-.029f},{.002f,-.031f},{.009f,-.031f},
        {.014f,-.028f},{.016f,-.023f},{.016f,.008f}});
    stroke({{.041f,.026f},{.046f,.013f},{.049f,-.003f},{.050f,-.017f},{.048f,-.026f},
        {.044f,-.032f},{.038f,-.036f},{.027f,-.040f},{.016f,-.044f},{.009f,-.046f},
        {.004f,-.045f},{.001f,-.043f},{-.001f,-.040f}});
    return mesh;
}
}

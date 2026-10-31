#pragma once
#include "VehicleDoorMotion.h"

namespace savr::npcgrab {
// Small flat open-palm silhouette. No texture, asset pack or opaque backdrop.
// Marker direction is billboard right, marker up is billboard up.
struct PalmMesh {
    std::array<vehicledoors::Vec,2048> vertices{};
    int count{};
};
inline PalmMesh Palm(const vehicledoors::Marker& marker) {
    PalmMesh mesh;
    const auto point=[&](float x,float y) {
        return marker.position+marker.direction*x+marker.up*y;
    };
    const auto triangle=[&](float ax,float ay,float bx,float by,float cx,float cy) {
        mesh.vertices[mesh.count++]=point(ax,ay);
        mesh.vertices[mesh.count++]=point(bx,by);
        mesh.vertices[mesh.count++]=point(cx,cy);
    };
    const auto rectangle=[&](float x0,float y0,float x1,float y1) {
        triangle(x0,y0,x1,y0,x1,y1);triangle(x0,y0,x1,y1,x0,y1);
    };
    rectangle(-.031f,-.035f,.033f,.023f); // palm
    rectangle(-.021f,-.054f,.023f,-.033f); // wrist
    constexpr float roots[]={-.024f,-.006f,.012f,.030f};
    constexpr float tips[]={.068f,.084f,.078f,.060f};
    for (int finger=0;finger<4;++finger) {
        const float x=roots[finger],y=tips[finger],radius=.007f;
        rectangle(x-radius,.018f,x+radius,y);
        for (int i=0;i<4;++i) {
            const float a=3.14159265f*i/4,b=3.14159265f*(i+1)/4;
            triangle(x,y,x+radius*std::cos(a),y+radius*std::sin(a),
                x+radius*std::cos(b),y+radius*std::sin(b));
        }
    }
    // Opposed thumb, separated from the fingers by a visible notch.
    triangle(-.027f,-.027f,-.060f,.011f,-.051f,.023f);
    triangle(-.027f,-.027f,-.051f,.023f,-.023f,.002f);
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

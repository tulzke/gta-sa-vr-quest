#pragma once
#include "VehicleDoorMotion.h"

namespace savr::npcgrab {
// Small flat open-palm silhouette. No texture, asset pack or opaque backdrop.
// Marker direction is billboard right, marker up is billboard up.
struct PalmMesh {
    std::array<vehicledoors::Vec,120> vertices{};
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
}

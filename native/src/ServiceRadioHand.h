#pragma once
#include "HandRenderFrame.h"
#include <array>
#include <algorithm>
namespace savr::serviceradio {
using Vec=handrender::Vec;
struct Frame {Vec center{},right{1,0,0},forward{0,0,1},up{0,1,0};};
inline Frame Blend(const Frame& a,const Frame& b,float t){
    const auto quaternion=[](const Frame& f){
        const float m[3][3]{{f.right.x,f.forward.x,f.up.x},{f.right.y,f.forward.y,f.up.y},{f.right.z,f.forward.z,f.up.z}};
        std::array<float,4> q{};
        const float trace=m[0][0]+m[1][1]+m[2][2];
        if(trace>0){const float s=std::sqrt(trace+1.f)*2;q={ (m[2][1]-m[1][2])/s,(m[0][2]-m[2][0])/s,(m[1][0]-m[0][1])/s,.25f*s};}
        else{int i=0;if(m[1][1]>m[i][i])i=1;if(m[2][2]>m[i][i])i=2;const int j=(i+1)%3,k=(i+2)%3;
            const float s=std::sqrt(std::max(0.f,1+m[i][i]-m[j][j]-m[k][k]))*2;
            if(s<1e-6f)return std::array<float,4>{0,0,0,1};
            q[i]=.25f*s;q[j]=(m[j][i]+m[i][j])/s;q[k]=(m[k][i]+m[i][k])/s;q[3]=(m[k][j]-m[j][k])/s;}
        return q;
    };
    auto q=quaternion(a),r=quaternion(b);float dot=0;
    for(int i=0;i<4;++i)dot+=q[i]*r[i];
    float length=0;for(int i=0;i<4;++i){q[i]=q[i]*(1-t)+r[i]*t*(dot<0?-1.f:1.f);length+=q[i]*q[i];}
    const float inv=1/std::sqrt(length);for(float& v:q)v*=inv;
    const float x=q[0],y=q[1],z=q[2],w=q[3];
    return {a.center*(1-t)+b.center*t,{1-2*(y*y+z*z),2*(x*y+w*z),2*(x*z-w*y)},
        {2*(x*y-w*z),1-2*(x*x+z*z),2*(y*z+w*x)},{2*(x*z+w*y),2*(y*z-w*x),1-2*(x*x+y*y)}};
}
inline Frame HeldFrame(const xr::HandPose& hand,int side){
    const auto wrist=handrender::TrackedPose(hand,side);
    Frame f;
    f.center=handrender::MeshPoint(wrist,{.112f,.025f,-.060f});
    f.right=wrist.forward*-1.f;f.up=wrist.right;
    // The shared hand mesh is reflected by its wrist basis. Turn the radio's
    // front/back instead of reflecting its geometry; PTT remains under index.
    f.forward=handrender::Unit(handrender::Cross(f.up,f.right));
    return f;
}
}

#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace savr::rhythm {
constexpr int kHitWindowMs=126;
constexpr float kLookAheadMs=1900.f,kPastMs=220.f;
constexpr float kTargetX=-.32f,kMetresPerMs=.0004f;
constexpr float kHalfWindow=kHitWindowMs*.5f*kMetresPerMs;
struct Note {int direction{},ms{},id{};};
struct Frame {bool active{};int current{-1},count{};std::array<Note,20> notes{};};
inline int Direction(int key){
    switch(key){case 13:case 3:return 1;case 14:case 1:return 2;
        case 9:case 2:return 3;case 10:case 4:return 4;default:return 0;}
}
inline int Read(const void* data,int offset){int value{};std::memcpy(&value,static_cast<const char*>(data)+offset,4);return value;}
// Android 2.11.311 GetBeatInfo writes signed (track time - play time) deltas:
// entries 0..9 are past, 10 is the next graded beat, 11..19 are upcoming.
// Reading this snapshot must never call GetBeatInfo or consume a native edge.
inline Frame Decode(const void* data,bool active){
    Frame f;if(!active||!data||!Read(data,0xa0))return f;
    f.active=true;f.current=Read(data,0xa8);
    for(int i=0;i<20;++i){const int direction=Direction(Read(data,i*8+4));
        if(direction)f.notes[f.count++]={direction,Read(data,i*8),f.current+i-10};}
    return f;
}
inline float Position(float ms){return kTargetX+(ms-kHitWindowMs*.5f)*kMetresPerMs;}
inline bool InWindow(float ms){return ms>=0.f&&ms<kHitWindowMs;}

struct Colour {float r,g,b,a;};
struct Vertex {float x,y;Colour colour;};
struct Mesh {
    std::array<Vertex,2048> vertices{};int count{};
    void Triangle(float ax,float ay,float bx,float by,float cx,float cy,Colour c){
        if(count+3>static_cast<int>(vertices.size()))return;
        vertices[count++]={ax,ay,c};vertices[count++]={bx,by,c};vertices[count++]={cx,cy,c};
    }
    void Rect(float left,float bottom,float right,float top,Colour c){
        Triangle(left,bottom,right,bottom,right,top,c);Triangle(left,bottom,right,top,left,top,c);
    }
    void Outline(float left,float bottom,float right,float top,float width,Colour c){
        Rect(left,bottom,right,bottom+width,c);Rect(left,top-width,right,top,c);
        Rect(left,bottom+width,left+width,top-width,c);Rect(right-width,bottom+width,right,top-width,c);
    }
    void Arrow(float x,int direction,float scale,Colour c){
        // A long rectangular shaft and a distinct arrowhead, never a bare triangle.
        const auto point=[&](float px,float py){
            if(direction==2){px=-px;py=-py;}else if(direction==3){const float t=px;px=-py;py=t;}
            else if(direction==4){const float t=px;px=py;py=-t;}
            return std::array<float,2>{x+px*scale,py*scale};};
        const auto a=point(0,.034f),b=point(.026f,.004f),d=point(-.026f,.004f);
        Triangle(a[0],a[1],b[0],b[1],d[0],d[1],c);
        const auto s0=point(-.010f,-.033f),s1=point(.010f,-.033f),s2=point(.010f,.005f),s3=point(-.010f,.005f);
        Triangle(s0[0],s0[1],s1[0],s1[1],s2[0],s2[1],c);
        Triangle(s0[0],s0[1],s2[0],s2[1],s3[0],s3[1],c);
    }
};
inline Mesh Build(const Frame& frame,float elapsedMs){
    Mesh mesh;if(!frame.active||!std::isfinite(elapsedMs)||elapsedMs<0||elapsedMs>250)return mesh;
    const Colour border{.28f,.38f,.43f,.85f},ink{.94f,.97f,1.f,1.f},gold{1.f,.77f,.30f,1.f};
    mesh.Rect(-.49f,-.083f,.49f,.083f,{.018f,.029f,.038f,.86f});
    mesh.Outline(-.49f,-.083f,.49f,.083f,.002f,border);
    mesh.Rect(-.477f,-.001f,.477f,.001f,{.32f,.44f,.49f,.5f});
    // The band's edges are the ACTUAL 126 ms acceptance window. Its centre
    // line is the middle of that interval, not a separate animation clock.
    mesh.Rect(kTargetX-kHalfWindow,-.073f,kTargetX+kHalfWindow,.073f,{1.f,.67f,.18f,.19f});
    mesh.Outline(kTargetX-kHalfWindow,-.073f,kTargetX+kHalfWindow,.073f,.003f,gold);
    mesh.Rect(kTargetX-.001f,-.078f,kTargetX+.001f,.078f,{1.f,.83f,.48f,.7f});
    for(int i=0;i<std::clamp(frame.count,0,20);++i){const auto& note=frame.notes[i];
        const float ms=note.ms-elapsedMs;
        if(ms<-kPastMs||ms>kLookAheadMs||note.direction<1||note.direction>4)continue;
        const float x=Position(ms);if(x<-.455f||x>.455f)continue;
        Colour colour=InWindow(ms)&&note.id==frame.current?gold:ink;
        if(ms<0)colour.a=.35f*std::clamp(1.f+ms/kPastMs,0.f,1.f);
        else if(ms>kLookAheadMs-180.f)colour.a=std::clamp((kLookAheadMs-ms)/180.f,0.f,1.f);
        mesh.Arrow(x,note.direction,1.13f,{.005f,.011f,.015f,colour.a});
        mesh.Arrow(x,note.direction,1.f,colour);
        // This centre tick, identical for all four arrow orientations, makes
        // alignment with the fixed gate readable independently of arrow shape.
        mesh.Rect(x-.002f,-.055f,x+.002f,-.045f,colour);
    }
    return mesh;
}
} // namespace savr::rhythm

#pragma once
#include "VehicleRadio.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>

namespace savr::radio {
constexpr int kUnknownStation=-1;
constexpr int kOffStation=13;
// Android 2.11 GetCurrentRadioStationID @38b738 maps its internal -1 to 13.
// Zero is the emergency stream, not a civilian station or the OFF selection.
inline const char* StationName(int id) {
    static constexpr const char* names[]{"", "PLAYBACK FM", "K-ROSE", "K-DST",
        "BOUNCE FM", "SF-UR", "RADIO LOS SANTOS", "RADIO X", "CSR 103.9",
        "K-JAH WEST", "MASTER SOUNDS 98.3", "WCTR", "USER TRACKS", "RADIO OFF"};
    return id>=1 && id<=kOffStation?names[id]:"";
}
using Glyph=std::array<std::uint8_t,7>;
inline Glyph LabelGlyph(char c) {
    // Digits are the exact 5x7 rows from Xr.cpp's ammoDigit shader.
    static constexpr Glyph digits[]{
        {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
        {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
        {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
        {14,17,17,15,1,1,14}};
    static constexpr Glyph letters[]{
        {14,17,17,31,17,17,17}, // A
        {30,17,17,30,17,17,30}, {14,17,16,16,16,17,14},
        {30,17,17,17,17,17,30}, {31,16,16,30,16,16,31},
        {31,16,16,30,16,16,16}, {14,17,16,23,17,17,15},
        {17,17,17,31,17,17,17}, {14,4,4,4,4,4,14},
        {7,2,2,2,2,18,12}, {17,18,20,24,20,18,17},
        {16,16,16,16,16,16,31}, {17,27,21,21,17,17,17},
        {17,25,21,19,17,17,17}, {14,17,17,17,17,17,14},
        {30,17,17,30,16,16,16}, {14,17,17,17,21,18,13},
        {30,17,17,30,20,18,17}, {15,16,16,14,1,1,30},
        {31,4,4,4,4,4,4}, {17,17,17,17,17,17,14},
        {17,17,17,17,17,10,4}, {17,17,17,21,21,21,10},
        {17,17,10,4,10,17,17}, {17,17,10,4,4,4,4},
        {31,1,2,4,8,16,31}};
    if(c>='0' && c<='9')return digits[c-'0'];
    if(c>='A' && c<='Z')return letters[c-'A'];
    if(c=='-')return {0,0,0,31,0,0,0};
    if(c=='.')return {0,0,0,0,0,12,12};
    return {};
}
constexpr float kLabelPixel=.012f/7.f;
constexpr float kLabelPadding=.008f;
constexpr float kLabelLift=.080f;
inline float LabelTextWidth(const char* text) {
    const auto size=std::strlen(text);
    return size?static_cast<float>(size*6-1)*kLabelPixel:0.f;
}
struct LabelVertex { float x,y,ink; };
struct LabelMesh {
    std::vector<LabelVertex> vertices;
    std::vector<std::uint16_t> indices;
    float width{},height{};
};
inline LabelMesh BuildLabelMesh(int station) {
    LabelMesh mesh;
    const char* text=StationName(station);
    if(!*text)return mesh;
    const float textWidth=LabelTextWidth(text);
    mesh.width=textWidth+2*kLabelPadding;mesh.height=.026f;
    // Rounded backdrop first, followed by opaque pixel quads in the same plane.
    // The render pass tests world/hand depth but never writes transparent depth.
    mesh.vertices.push_back({0,0,0});
    constexpr float radius=.003f;
    for(int corner=0;corner<4;++corner)for(int step=0;step<=4;++step) {
        const float angle=(corner+step/4.f)*1.5707963268f;
        const float x=(corner==0||corner==3?1.f:-1.f)*(mesh.width*.5f-radius);
        const float y=(corner<2?1.f:-1.f)*(mesh.height*.5f-radius);
        mesh.vertices.push_back({x+radius*std::cos(angle),y+radius*std::sin(angle),0});
    }
    const auto rim=static_cast<std::uint16_t>(mesh.vertices.size()-1);
    for(std::uint16_t i=1;i<=rim;++i)
        mesh.indices.insert(mesh.indices.end(),{0,i,static_cast<std::uint16_t>(i==rim?1:i+1)});
    for(int i=0;text[i];++i) {
        const auto glyph=LabelGlyph(text[i]);
        for(int row=0;row<7;++row)for(int col=0;col<5;++col) {
            if(!(glyph[row]&(1u<<(4-col))))continue;
            const float x=-textWidth*.5f+(i*6+col)*kLabelPixel;
            const float y=(3.5f-row)*kLabelPixel;
            const auto base=static_cast<std::uint16_t>(mesh.vertices.size());
            mesh.vertices.insert(mesh.vertices.end(),{{x,y,1},{x,y-kLabelPixel,1},
                {x+kLabelPixel,y,1},{x+kLabelPixel,y-kLabelPixel,1}});
            mesh.indices.insert(mesh.indices.end(),{base,static_cast<std::uint16_t>(base+1),
                static_cast<std::uint16_t>(base+2),static_cast<std::uint16_t>(base+2),
                static_cast<std::uint16_t>(base+1),static_cast<std::uint16_t>(base+3)});
        }
    }
    return mesh;
}
}

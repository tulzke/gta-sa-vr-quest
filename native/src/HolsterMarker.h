#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace savr::holstermarker {
enum class Kind { Grab, Return, Support, Preview };
struct Marker { float position[3]{}; Kind kind{Kind::Grab}; bool unavailable{}; };
constexpr int kCapacity = 8;
struct Frame {
    std::array<Marker, kCapacity> markers{};
    float eye[3]{};
    int count{};
    std::uint64_t epoch{};
    bool Usable(bool trackingValid, std::uint64_t currentEpoch) const {
        return trackingValid && epoch == currentEpoch && count > 0 && count <= kCapacity;
    }
};

// Gameplay policy shared with tests. Calibration intentionally overrides point
// visibility/ownership, but never enables physical interaction.
inline bool Visible(bool enabled, bool calibration, bool shown, bool owned,
                    bool held, bool dropped, bool drawable) {
    return enabled && (calibration || (shown && owned && !dropped && (held || drawable)));
}

struct Mesh {
    static constexpr int kSegments = 24;
    static constexpr int kMaxVertices = kSegments * 4 + 8;
    static constexpr int kMaxIndices = kSegments * 6 + 12;
    std::array<float, kMaxVertices * 3> vertices{};
    std::array<unsigned short, kMaxIndices> indices{};
    int vertexCount{}, indexCount{};
};
inline Mesh ReturnMesh(const float p[3], const float eye[3]) {
    Mesh m;
    float nx=eye[0]-p[0], ny=eye[1]-p[1], nz=eye[2]-p[2];
    float n=std::sqrt(nx*nx+ny*ny+nz*nz);
    if (n<0.0001f) { nx=0; ny=0; nz=1; n=1; }
    nx/=n; ny/=n; nz/=n;
    float rx=nz, ry=0, rz=-nx;
    n=std::sqrt(rx*rx+rz*rz);
    if (n<0.001f) { rx=1; rz=0; n=1; }
    rx/=n; rz/=n;
    const float ux=ny*rz-nz*ry, uy=nz*rx-nx*rz, uz=nx*ry-ny*rx;
    auto vertex=[&](float x,float y) {
        const int i=m.vertexCount++;
        m.vertices[i*3]=p[0]+rx*x+ux*y;
        m.vertices[i*3+1]=p[1]+ry*x+uy*y;
        m.vertices[i*3+2]=p[2]+rz*x+uz*y;
    };
    auto quad=[&](int base) {
        for (int i : {0,1,2, 2,1,3}) m.indices[m.indexCount++]=static_cast<unsigned short>(base+i);
    };
    constexpr float pi=3.14159265358979323846f;
    for(int i=0;i<Mesh::kSegments;++i) {
        const float a=2*pi*i/Mesh::kSegments, b=2*pi*(i+1)/Mesh::kSegments;
        const int base=m.vertexCount;
        vertex(std::cos(a)*0.035f,std::sin(a)*0.035f);
        vertex(std::cos(a)*0.026f,std::sin(a)*0.026f);
        vertex(std::cos(b)*0.035f,std::sin(b)*0.035f);
        vertex(std::cos(b)*0.026f,std::sin(b)*0.026f);
        quad(base);
    }
    // Two broad legs point inward, towards the socket at the ring centre.
    int base=m.vertexCount;
    vertex(-0.014f,0.018f); vertex(-0.010f,0.022f);
    vertex(0,0.004f); vertex(0,0.010f); quad(base);
    base=m.vertexCount;
    vertex(0,0.004f); vertex(0,0.010f);
    vertex(0.014f,0.018f); vertex(0.010f,0.022f); quad(base);
    return m;
}
} // namespace savr::holstermarker

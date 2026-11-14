#pragma once
#include "HandAnimation.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace savr::handskeleton {
constexpr int kMaxNodes=64, kMaxJoints=32;
using Mat=std::array<float,16>;
using Vec=std::array<float,3>;
using Quat=std::array<float,4>; // xyzw, glTF
struct Transform { Vec t{}, s{1,1,1}; Quat q{0,0,0,1}; bool operator==(const Transform&) const = default; };
struct Vertex { float pos[3], normal[3], uv[2]; std::uint16_t joints[8]; float weights[8]; };
struct Node { Transform rest; Mat matrix{}; int parent=-1; bool fixedMatrix=false; };
enum class Path { Translation, Rotation, Scale };
struct Track {
    int node{}; Path path{}; bool step{};
    std::vector<float> times;
    std::vector<Quat> values;
};
struct Clip { float duration{}; std::vector<Track> tracks; };
using Pose=std::array<Transform,kMaxNodes>;
using Palette=std::array<Mat,kMaxJoints>;
Mat Identity();
Mat Multiply(const Mat& a,const Mat& b);
Mat Matrix(const Transform& t);
Quat Slerp(Quat a,Quat b,float t);
Transform Mix(const Transform& a,const Transform& b,float t);
struct Library {
    std::vector<Vertex> vertices;
    std::vector<std::uint16_t> indices;
    std::vector<Node> nodes;
    std::vector<int> order, joints;
    std::vector<Mat> inverseBind;
    std::vector<unsigned char> albedoPng; // released after the one-time GPU upload
    std::array<Clip,handanimation::kNames.size()> clips;
    std::string error;
    bool loaded=false;
    bool Load(const std::string& path); // transactional, no per-frame I/O
    void Sample(handanimation::Gesture gesture,float progress,Pose& out) const;
    void Blend(const Pose& a,const Pose& b,float weight,Pose& out) const;
    void Matrices(const Pose& pose,Palette& out) const;
    void Evaluate(const handanimation::Selection& selection,float pinchProgress,float pinchWeight,Palette& out) const;
    std::size_t ResidentBytes() const;
};
} // namespace savr::handskeleton

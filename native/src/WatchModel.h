#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace savr::watchmodel {
struct Vertex { float position[3],normal[3],uv[2]; };
struct Material {
    std::array<float,4> color{1,1,1,1};
    float metallic=1,roughness=1;
    int image=-1,metallicImage=-1;
};
struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint16_t> indices;
    int material=-1;
    bool display=false;
};
struct Model {
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<std::vector<unsigned char>> images;
    // All positions below, including centre, are glTF Y-up metres.
    std::array<float,3> centre{};
    std::array<float,3> right{},up{},normal{};
    std::string error;
    bool Load(const void* data,std::size_t size);
};
}

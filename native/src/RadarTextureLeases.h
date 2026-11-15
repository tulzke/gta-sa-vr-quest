#pragma once
#include <array>
namespace savr::radarmap {
// One extra RW reference per visible section. A fixed table, shared by all map
// passes: the stock 3x3 streamer may drop its ref without cancelling full-size
// streaming when the texture would otherwise return to its DB-only reference.
struct TextureLeases {
    std::array<void*,144> textures{};
    std::array<bool,144> visible{};
    void Begin(){visible.fill(false);}
    template<class Retain,class Release>
    void Keep(int index,void* texture,Retain retain,Release release){
        if(index<0||index>=144||!texture)return;
        visible[index]=true;
        if(textures[index]==texture)return;
        if(textures[index])release(textures[index]);
        retain(texture);textures[index]=texture;
    }
    template<class Release> void Finish(Release release){
        for(int i=0;i<144;++i)if(textures[i]&&!visible[i]){release(textures[i]);textures[i]=nullptr;}
    }
};
}

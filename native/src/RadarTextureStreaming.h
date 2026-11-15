#pragma once
#include <cstdint>
#include <cstring>
namespace savr::radarmap {
inline void* TextureEntry(void* texture){
    if(!texture)return nullptr;
    void* raster=nullptr;std::memcpy(&raster,texture,sizeof(raster));
    if(!raster)return nullptr;
    void* entry=nullptr;std::memcpy(&entry,static_cast<char*>(raster)+0x38,sizeof(entry));
    return entry;
}
inline unsigned StreamState(void* entry){
    if(!entry)return 0;
    std::uint16_t flags=0;std::memcpy(&flags,static_cast<char*>(entry)+0xc,sizeof(flags));
    return flags>>12;
}
template<class Mark> bool MarkVisibleTexture(void* texture,Mark mark){
    void* entry=TextureEntry(texture);
    // Request/GetRWTexture queues a thumbnail as state 1 (not yet rendered).
    // RenderingTextureEntry promotes it to the visible queue (state 2).
    // Retaining a RW reference alone does not perform this transition.
    if(StreamState(entry)!=1)return false;
    mark(entry);return true;
}
}

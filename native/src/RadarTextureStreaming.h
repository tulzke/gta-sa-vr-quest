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
// Reorder the engine-owned visible queue, then let its normal one-texture
// transaction remove/load the entry. Never call LoadFullTexture behind a queue.
template<class GetDatabase,class Pump> unsigned LoadVisibleFirst(void* texture,GetDatabase database,Pump pump,unsigned* storedTexels){
    auto* entry=TextureEntry(texture);if(StreamState(entry)!=2||!storedTexels)return 0;
    unsigned index=0;auto* db=static_cast<char*>(database(entry,&index));if(!db)return 0;
    unsigned count=0;unsigned* queue=nullptr;
    std::memcpy(&count,db+0xbc,4);std::memcpy(&queue,db+0xc0,sizeof(queue));
    if(!queue)return 0;
    for(unsigned i=0;i<count;++i)if(queue[i]==index){
        const auto first=queue[0];queue[0]=index;queue[i]=first;
        const auto loaded=pump(db,0u,1u); // zero texel budget completes one entry
        *storedTexels+=loaded; // same accounting as UpdateStreaming
        return loaded;
    }
    return 0;
}
inline bool OutdoorMap(bool outside,unsigned playerArea){return outside&&playerArea==0;}
}

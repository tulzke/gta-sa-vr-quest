#pragma once
#include <cmath>
namespace savr::radarmap {
// CRadar::DrawRadarSection's out-of-world fill in the retail ARM64 build.
inline constexpr unsigned char kSea[4]={111,137,170,255};
struct Rect {float left,bottom,right,top;};
// Enumerate the complete raster footprint, including corners that become
// visible when the consumer rotates the circular map. Out-of-world stays sea.
template<class Draw> void VisibleTiles(float playerX,float playerY,float zoom,Draw draw){
    if(!std::isfinite(playerX)||!std::isfinite(playerY)||!std::isfinite(zoom)||zoom<=0)return;
    const float scale=400.f/300.f*zoom,size=500.f*scale;
    for(int y=0;y<12;++y)for(int x=0;x<12;++x){
        const float left=400.f+(x*500.f-3000.f-playerX)*scale;
        const float top=400.f+(playerY-(3000.f-y*500.f))*scale;
        if(left>=800||left+size<=0||top>=800||top+size<=0)continue;
        constexpr float overlap=.5f;
        draw(x,y,Rect{left-overlap,top+size+overlap,left+size+overlap,top-overlap});
    }
}
}

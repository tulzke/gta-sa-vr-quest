#include "VehicleHydraulicsHandConfig.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace savr::hydrohand {
bool Config::Save(){
    const auto tmp=path_+".tmp";FILE* file=std::fopen(tmp.c_str(),"w");if(!file)return saved_=false;
    std::fprintf(file,"# Hydraulics hand fit; global across car models\n# X=right, Y=forward, Z=up in moving lever axes\n# Positions in cm; rotations in degrees about the grip opening\n");
    for(int h=0;h<2;++h)for(int f=0;f<6;++f)std::fprintf(file,"%s.%s=%.3f\n",h?"Right":"Left",kKeys[f],fit_[h][f]);
    const bool written=std::ferror(file)==0;const bool closed=std::fclose(file)==0;
    if(!written||!closed){std::remove(tmp.c_str());return saved_=false;}
#ifdef _WIN32
    saved_=MoveFileExA(tmp.c_str(),path_.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    saved_=std::rename(tmp.c_str(),path_.c_str())==0;
#endif
    if(!saved_)std::remove(tmp.c_str());return saved_;
}
void Config::Load(){
    if(loaded_)return;loaded_=true;
    FILE* file=std::fopen(path_.c_str(),"r");
    if(!file){Save();return;}
    char line[160],side[16],key[32];float value{};
    while(std::fgets(line,sizeof(line),file)){
        if(std::sscanf(line,"%15[^.].%31[^=]=%f",side,key,&value)!=3||!std::isfinite(value))continue;
        const int h=std::strcmp(side,"Left")==0?0:std::strcmp(side,"Right")==0?1:-1;
        if(h<0)continue;
        for(int f=0;f<6;++f)if(std::strcmp(key,kKeys[f])==0)fit_[h][f]=std::clamp(value,f<3?-25.f:-360.f,f<3?25.f:360.f);
    }
    saved_=std::ferror(file)==0;std::fclose(file);
}
Fit Config::Get(int hand){std::lock_guard lock(mutex_);Load();return hand>=0&&hand<2?fit_[hand]:Fit{};}
bool Config::Adjust(int hand,int field,int direction){
    std::lock_guard lock(mutex_);Load();if(hand<0||hand>1||field<0||field>5||direction==0)return saved_;
    auto& v=fit_[hand][field];const float step=field<3?.1f:1.f,limit=field<3?25.f:360.f;
    v=std::clamp(std::round((v+direction*step)/step)*step,-limit,limit);return Save();
}
bool Config::Reset(int hand){std::lock_guard lock(mutex_);Load();if(hand<0||hand>1)return saved_;fit_[hand]={};return Save();}
bool Config::Saved(){std::lock_guard lock(mutex_);Load();return saved_;}
namespace {Config config{kPath};std::atomic<int> selected{1},preview{-1};}
Fit Get(int hand){return config.Get(hand);}
void Adjust(int field,int direction){config.Adjust(Selected(),field,direction);}
void Reset(){config.Reset(Selected());}
int Selected(){return selected.load(std::memory_order_relaxed);}
void Select(int direction){if(direction)selected.store(1-Selected(),std::memory_order_relaxed);}
void SetPreview(bool active){preview.store(active?Selected():-1,std::memory_order_release);}
int PreviewHand(){return preview.load(std::memory_order_acquire);}
bool Saved(){return config.Saved();}
}

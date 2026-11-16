#pragma once
#include "RadioLabel.h"
#include <cstddef>
#include <cstring>

namespace savr::radio {
constexpr unsigned kRetuneSelectionMs=350;
constexpr unsigned kRetuneRestartMs=500;
struct RetunePatch {std::size_t offset;std::uint32_t before,after;};
// Android 2.11 CheckForStationRetune @3889a8. Only the input-settle and
// restart timers change; the native audio stop/load/start state machine stays.
constexpr RetunePatch kRetunePatches[]{
    {0x15c,0x11177129u,0x11000129u|(kRetuneSelectionMs<<10)}, // add w9,w9,#1500 -> 350
    {0x16c,0x5281f40au,0x5280000au|(kRetuneRestartMs<<5)},   // mov w10,#4000 -> 500
    {0x188,0x5280fa09u,0x52800009u|(kRetuneRestartMs<<5)}}; // mov w9,#2000 -> 500
template<class T> inline T RetuneRead(const void* manager,std::size_t offset) {
    T out{};std::memcpy(&out,static_cast<const char*>(manager)+offset,sizeof(out));return out;
}
inline bool RetuneLayoutMatches(const void* code) {
    if(!code)return false;
    // Validate the exact field-reading instructions as well as every patch site.
    if(RetuneRead<std::uint32_t>(code,0)!=0x3943a408u ||
       RetuneRead<std::uint32_t>(code,0x4c)!=0xb9406e68u ||
       RetuneRead<std::uint32_t>(code,0x100)!=0xb9407e68u ||
       RetuneRead<std::uint32_t>(code,0x108)!=0x39c2b66au)return false;
    bool stock=true,patched=true;
    for(const auto& patch:kRetunePatches) {
        const auto word=RetuneRead<std::uint32_t>(code,patch.offset);
        stock=stock&&word==patch.before;patched=patched&&word==patch.after;
    }
    return stock||patched;
}
inline bool UserTracksAvailable(const void* userRadioManager) {
    // RetuneRadio @38b1dc and CheckForStationRetune @388a44 read this count.
    // Without the optional symbol, let native radio decide whether to skip 12.
    return !userRadioManager || RetuneRead<std::uint16_t>(userRadioManager,0xa)!=0;
}
inline int WrapStation(int base,int delta,bool userTracks) {
    if(base<1 || base>kOffStation || delta < -1024 || delta>1024)return kUnknownStation;
    int station=((base-1+delta)%kOffStation+kOffStation)%kOffStation+1;
    if(station==12 && !userTracks)station=delta<0?11:kOffStation;
    return station;
}
inline int SelectedStation(const void* manager,int playing,bool userTracks) {
    if(!manager)return playing;
    // Absolute requests (menu/scripts) precede next/previous offsets. The
    // selected base at +ad changes before +e9's actually playing station does.
    for(const auto offset:{0x7cu,0x78u}) {
        const int absolute=RetuneRead<int>(manager,offset);
        if(absolute>=1 && absolute<=kOffStation)
            return absolute==12&&!userTracks?kOffStation:absolute;
    }
    const int base=RetuneRead<std::int8_t>(manager,0xad);
    const int delta=RetuneRead<int>(manager,0x6c);
    if(base<1 || base>kOffStation)return playing;
    // Already playing user tracks remain valid if catalogue access is absent
    // or being rebuilt. Skipping applies when choosing a different station.
    if(delta==0)return base;
    const int selected=WrapStation(base,delta,userTracks);
    return selected==kUnknownStation?playing:selected;
}
}

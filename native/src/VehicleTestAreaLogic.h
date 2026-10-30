#pragma once
#include "VehicleDoorMotion.h"
#include <algorithm>
#include <array>
#include <cstdint>

namespace savr::testarea {
using Vec = vehicledoors::Vec;
constexpr int kPageSize=10, kFirstModel=400, kLastModel=611;
constexpr int kFloorModel=3095, kTilesX=12, kTilesY=8, kTileCount=kTilesX*kTilesY;
constexpr float kTilePitch=8.98f, kFloorZ=80.f, kCentreX=0.f, kCentreY=-2800.f;
constexpr Vec kArrival{kCentreX,kCentreY,kFloorZ+1.2f};
constexpr Vec Parking(int index) {
    return {kCentreX+(index%5-2)*16.f,kCentreY+(index<5?17.f:-17.f),kFloorZ};
}
inline bool InBounds(Vec p) {
    return vehicledoors::Finite(p)&&std::abs(p.x-kCentreX)<52.f&&
           std::abs(p.y-kCentreY)<34.f&&p.z>kFloorZ-3.f&&p.z<kFloorZ+30.f;
}
struct Recovery {
    enum Action { None, Rescue, Leave };
    double outsideSince{-1};
    bool rescued{};
    Action Step(Vec position,double now,bool seated) {
        if(seated||!vehicledoors::Finite(position)||InBounds(position)) {
            outsideSince=-1;return None;
        }
        if(outsideSince<0||now<outsideSince){outsideSince=now;return None;}
        if(now-outsideSince<.75)return None;
        outsideSince=-1;
        // A broken floor/physics transition must never become a teleport loop.
        if(rescued)return Leave;
        rescued=true;return Rescue;
    }
};
struct Catalogue {
    std::array<int,kLastModel-kFirstModel+1> ids{};
    int count{};
    // The verified retail factory's model-info +0x6c enum: automobile=0,
    // monster=1. Bikes, quad, aircraft, boats, trains and trailers are separate.
    void Add(int id,int type) {
        if(id<kFirstModel||id>kLastModel||(type!=0&&type!=1)||
           count==static_cast<int>(ids.size())||(count&&id<=ids[count-1]))return;
        ids[count++]=id;
    }
    int Pages() const {return (count+kPageSize-1)/kPageSize;}
    int Size(int page) const {
        return page>=0&&page<Pages()?std::min(kPageSize,count-page*kPageSize):0;
    }
    int Model(int page,int index) const {
        return index>=0&&index<Size(page)?ids[page*kPageSize+index]:-1;
    }
    bool CanStep(int page,int direction) const {
        return direction!=0&&Size(page)>0&&Pages()>1;
    }
    int Next(int page,int direction) const {
        return CanStep(page,direction)?(page+(direction<0?-1:1)+Pages())%Pages():page;
    }
};

// Ownership includes pool allocation identity as well as the slot's generation.
// Never delete the closest vehicle or a replacement that reused our slot.
struct PoolView {
    void* identity{};
    char* objects{};
    const std::uint8_t* flags{};
    int count{},stride{};
};
struct OwnedEntity {
    PoolView pool{};
    int slot{-1};
    std::uint8_t generation{};
    void* Pointer(const PoolView& current) const {
        return slot>=0&&slot<current.count&&current.identity==pool.identity&&
            current.objects==pool.objects&&current.flags==pool.flags&&
            current.count==pool.count&&current.stride==pool.stride&&current.objects&&current.flags&&
            !(generation&0x80)&&current.flags[slot]==generation?
            current.objects+slot*current.stride:nullptr;
    }
    bool Capture(void* entity,PoolView p) {
        *this={};
        if(!entity||!p.objects||!p.flags||p.stride<=0||p.count<=0)return false;
        const auto address=reinterpret_cast<std::uintptr_t>(entity);
        const auto begin=reinterpret_cast<std::uintptr_t>(p.objects);
        if(address<begin||address-begin>=static_cast<std::uintptr_t>(p.count)*p.stride||
           (address-begin)%p.stride)return false;
        const int index=static_cast<int>((address-begin)/p.stride);
        if(p.flags[index]&0x80)return false;
        pool=p;slot=index;generation=p.flags[index];return true;
    }
};
} // namespace savr::testarea

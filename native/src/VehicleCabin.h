#pragma once
#include "Symbols.h"
#include "TrackingRecovery.h"
#include <cmath>
#include <cstring>
namespace savr::vehiclecabin {
// Include pool storage and generation: an allocator may reuse the same vehicle
// address/model/seat after loading a save or replacing a destroyed vehicle.
inline tracking::SeatKey SeatKey(const GameSymbols& symbols,void* vehicle,
                                void* ped,int model,int seat) {
    if(!vehicle)return {};
    tracking::SeatKey key{};
    key.vehicle=vehicle;key.ped=ped;key.model=model;key.seat=seat;
    void* pool=symbols.CPools_ms_pVehiclePool?*symbols.CPools_ms_pVehiclePool:nullptr;
    if(!pool)return key;
    int size{};void* objects{};const std::uint8_t* flags{};
    std::memcpy(&size,static_cast<const char*>(pool)+0x10,sizeof(size));
    std::memcpy(&objects,pool,sizeof(objects));
    std::memcpy(&flags,static_cast<const char*>(pool)+8,sizeof(flags));
    if(size!=110 || !objects || !flags)return key;
    const auto address=reinterpret_cast<std::uintptr_t>(vehicle);
    const auto start=reinterpret_cast<std::uintptr_t>(objects);
    if(address<start || (address-start)%0xc68!=0)return key;
    const auto slot=(address-start)/0xc68;
    if(slot>=110 || (flags[slot]&0x80))return key;
    key.pool=pool;key.objects=objects;key.flags=flags;key.generation=flags[slot];
    return key;
}

// Neutral eyes are derived from the loaded seating dummy, never a live rider
// animation. Retail SetPedPositionInCar @590648/@5906d4 uses front +0x30,
// rear +0x3c and mirrors X for left seats. Bike passengers use the rear dummy.
inline bool ReadSeatEye(const GameSymbols& symbols,int model,int appearance,
                        int seat,GameSymbols::Vec3* eye) {
    if(!eye || !symbols.CModelInfo_ms_modelInfoPtrs || model<400 || model>611 ||
       (appearance!=1 && appearance!=2) || seat < -1 || seat>2 ||
       (appearance==2 && seat>0))return false;
    const void* info=symbols.CModelInfo_ms_modelInfoPtrs[model];
    if(!info)return false;
    void* structure{};
    std::memcpy(&structure,static_cast<const char*>(info)+0x90,sizeof(structure));
    if(!structure)return false;
    const bool bike=appearance==2;
    const bool rear=bike?seat>=0:seat>=1;
    GameSymbols::Vec3 point{};
    std::memcpy(&point,static_cast<const char*>(structure)+(rear?0x3c:0x30),sizeof(point));
    if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z)||
       std::abs(point.x)>3.f || std::abs(point.y)>12.f || point.z<-.5f || point.z>5.f ||
       point.x*point.x+point.y*point.y+point.z*point.z<.01f)return false;
    // Upright neutral eye extension. The bike extension matches the existing
    // Sanchez reference relative to its front dummy (-.20, .40). Model/mode
    // calibrations are still applied by the camera, independently of this base.
    const bool left=seat==-1 || seat==1;
    *eye={bike?0.f:(left?-point.x:point.x),
          point.y+(bike?.06f:0.f),point.z+(bike?.55f:.60f)};
    return true;
}

// ARM64 2.11: CVehicleModelInfo::SetClump @533508 stores the structure
// at +0x90; CPed::SetPedPositionInCar @590648 reads front dummy +0x30
// and mirrors its X for the driver. Read the loaded model, stock or HD.
inline bool ReadCombineEye(const GameSymbols& symbols, GameSymbols::Vec3* eye) {
    if(!eye || !symbols.CModelInfo_GetModelInfoByName)return false;
    int id=-1;void* model=symbols.CModelInfo_GetModelInfoByName("combine",&id);
    if(!model || id!=532)return false;
    void* structure=nullptr;
    std::memcpy(&structure,static_cast<const char*>(model)+0x90,sizeof(structure));
    if(!structure)return false;
    GameSymbols::Vec3 seat{};
    std::memcpy(&seat,static_cast<const char*>(structure)+0x30,sizeof(seat));
    if(!std::isfinite(seat.x)||!std::isfinite(seat.y)||!std::isfinite(seat.z)||
       std::abs(seat.x)>3.f || std::abs(seat.y)>12.f || seat.z<-.5f || seat.z>5.f ||
       seat.x*seat.x+seat.y*seat.y+seat.z*seat.z<.01f)return false;
    *eye={-seat.x,seat.y,seat.z+.60f};
    return true;
}
// Migrate the recognized legacy seat triple at load; preserve custom values.
template<class Number>
inline bool MigrateCombineSeat(int model,int mode,Number& side,Number& forward,Number& height) {
    if(model!=532 || mode!=1 || side!=-98 || forward!=290 || height!=43)return false;
    side=forward=height=0;return true;
}
}

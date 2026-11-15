#pragma once
#include <cstdint>
namespace savr::tracking {
struct Recovery {
    bool pending{true};
    unsigned stable{};
    std::uint64_t epoch{};
    void Invalidate() { pending=true;stable=0; }
    bool Sample(bool usable) {
        if(!usable) { Invalidate();return false; }
        if(pending) {
            if(++stable<8)return false;
            pending=false;++epoch;
        }
        return true;
    }
};
// Physical tracking resets do not change the identity/geometry of a seat.
struct SeatKey {
    void* vehicle{};
    void* ped{};
    void* pool{};
    void* objects{};
    const void* flags{};
    int model{-1};
    int seat{-2};
    std::uint8_t generation{};
    bool operator==(const SeatKey&) const = default;
};
struct SeatTransition {
    SeatKey key{};
    std::uint64_t serial{};
    bool centered{};
    bool Update(const SeatKey& current) {
        if(current==key)return false;
        key=current;++serial;centered=false;
        return true;
    }
    bool Center(bool ready) {
        if(!ready || !key.vehicle || key.seat < -1 || centered)return false;
        centered=true;
        return true;
    }
};
struct SeatLatch {
    void* vehicle{};
    std::uint64_t resetSerial{~std::uint64_t{0}};
    double captureUntil{};
    bool valid{};
    bool Begin(void* current, std::uint64_t serial, double nowMs) {
        if(current==vehicle && serial==resetSerial)return false;
        vehicle=current;resetSerial=serial;captureUntil=nowMs+2000.0;valid=false;
        return true;
    }
    bool Capture(double nowMs)const{return !valid || nowMs<captureUntil;}
};
}

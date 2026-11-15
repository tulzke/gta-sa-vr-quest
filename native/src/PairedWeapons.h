#pragma once

#include "PhysicalGunCycle.h"
#include "ArcadeReload.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace savr::pairedweapon {

constexpr int kNativeSlots = 13;
constexpr int kSlots = 26;
inline bool Mirror(int slot) { return slot > kNativeSlots && slot < kSlots; }
inline int Base(int slot) { return Mirror(slot) ? slot - kNativeSlots : slot; }
inline bool Valid(int slot) { return slot > 0 && slot < kSlots && slot != kNativeSlots; }
inline bool TwinType(int type) { return type == 22 || type == 26 || type == 28 || type == 32; }
inline bool Allowed(int type, int skill, unsigned flags) {
    return TwinType(type) && skill == 2 && (flags & 0x800u);
}
inline std::uint8_t* Native(void* ped, int slot) {
    slot = Base(slot);
    return ped && slot > 0 && slot < kNativeSlots
        ? static_cast<std::uint8_t*>(ped) + 0x730 + slot * 0x20 : nullptr;
}
inline int Type(void* ped, int slot) {
    const auto* weapon = Native(ped, slot);
    return weapon ? *reinterpret_cast<const int*>(weapon) : 0;
}
inline unsigned& Field(std::uint8_t* p, int offset) {
    return *reinterpret_cast<unsigned*>(p + offset);
}
struct Copy { alignas(16) std::uint8_t bytes[0x20]{}; bool enabled{}; };
inline Copy copies[kNativeSlots]{};
inline void* owner{};
inline thread_local bool insideShot{};

inline bool Enabled(int slot) { return Valid(slot) && copies[Base(slot)].enabled; }
inline void Reset() { for (auto& copy : copies) copy = {}; owner = nullptr; arcadereload::ResetGuns(); }
inline void Refresh(void* ped, unsigned mask, const unsigned capacities[kNativeSlots], unsigned now = 0) {
    if (owner != ped) { Reset(); owner = ped; }
    for (int slot = 1; slot < kNativeSlots; ++slot) {
        auto& copy = copies[slot];
        auto* native = Native(ped, slot);
        const int type = Type(ped, slot);
        const bool enabled = (mask & (1u << slot)) && TwinType(type);
        if (type != static_cast<int>(Field(copy.bytes, 0))) copy = {};
        if (enabled && !copy.enabled) {
            arcadereload::guns[slot+kNativeSlots]={};
            // The original total stays authoritative, including BOTH magazines.
            // Reserve rounds are loaded into the new copy without minting ammo.
            std::memcpy(copy.bytes, native, 0x20);
            std::memset(copy.bytes + 0x18, 0, 8); // never clone an FX owner
            Field(native, 8) = std::min({Field(native, 8), Field(native, 12), capacities[slot]});
            Field(copy.bytes, 4) = 0;
            Field(copy.bytes, 16) = 0;
            Field(copy.bytes, 8) = std::min(capacities[slot], Field(native, 12) - Field(native, 8));
        }
        if (enabled) Field(native,8) = std::min({Field(native,8),Field(native,12),capacities[slot]});
        if (enabled) Field(copy.bytes, 8) = std::min(Field(copy.bytes, 8), Field(native, 12) - std::min(Field(native, 12),Field(native, 8)));
        copy.enabled = enabled;
        if (!enabled) Field(copy.bytes, 8) = 0;
        else arcadereload::Advance(slot+kNativeSlots,type,Field(copy.bytes, 4), Field(copy.bytes, 8),
            Field(native, 12) - Field(native, 8), Field(copy.bytes, 16), now, capacities[slot]);
    }
}
inline unsigned Clip(void* ped, int slot) {
    auto* native = Native(ped, slot);
    if (!native) return 0;
    return Mirror(slot) ? (Enabled(slot) ? Field(copies[Base(slot)].bytes, 8) : 0) : Field(native, 8);
}
inline unsigned Available(void* ped, int slot) {
    auto* native = Native(ped, slot);
    if (!native) return 0;
    const unsigned other = Enabled(slot) ? Clip(ped, Mirror(slot) ? Base(slot) : slot + kNativeSlots) : 0;
    return Field(native, 12) - std::min(Field(native, 12), other);
}

// Retail sees a real, in-range CPed slot during each synchronous native shot.
// Only its CWeapon state is exchanged; the mirror never occupies a game slot.
class Shot {
    std::uint8_t* native_{};
    int slot_{};
    alignas(16) std::uint8_t saved_[0x20]{};
    unsigned excluded_{};
public:
    Shot(void* ped, int slot) : native_(Native(ped, slot)), slot_(slot) {
        if (!native_) return;
        std::memcpy(saved_, native_, sizeof(saved_));
        if (Mirror(slot)) {
            const unsigned total = Available(ped, slot);
            std::memcpy(native_, copies[Base(slot)].bytes, 0x20);
            Field(native_, 12) = total;
            excluded_ = std::min(Field(saved_, 12),Field(saved_, 8));
        } else {
            excluded_ = Enabled(slot) ? std::min(Field(native_, 12),Clip(ped, slot + kNativeSlots)) : 0;
            Field(native_, 12) -= std::min(Field(native_, 12), excluded_);
        }
        insideShot = true;
    }
    ~Shot() {
        if (!native_) return;
        const unsigned total = Field(native_, 12) + excluded_;
        if (Mirror(slot_)) {
            std::memcpy(copies[Base(slot_)].bytes, native_, 0x20);
            std::memcpy(native_, saved_, 0x20);
        }
        Field(native_, 12) = total;
        insideShot = false;
    }
    Shot(const Shot&) = delete;
    Shot& operator=(const Shot&) = delete;
};
} // namespace savr::pairedweapon

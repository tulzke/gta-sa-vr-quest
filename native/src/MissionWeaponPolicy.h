#pragma once
#include <cstdint>
namespace savr::missionweapon {
struct WeaponSnapshot { std::uint32_t type{}, state{}, clip{}, total{}; };
inline int GrantedSlot(const WeaponSnapshot (&before)[13], const WeaponSnapshot (&after)[13]) {
    int result=-1;
    for (int i=1;i<13;++i) {
        if (i==10 || i==11) continue;
        if (after[i].type && (after[i].type!=before[i].type || after[i].total>before[i].total))
            result=i;
    }
    return result;
}
}

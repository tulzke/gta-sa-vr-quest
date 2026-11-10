#pragma once

#include <array>
#include <cmath>

namespace savr::cheats {

// Stat IDs 69..79: pistol through rifle. Micro Uzi and Tec-9 share stat 75.
// CPed::GetWeaponSkill reads weapon-data thresholds; a fixed 500 is not
// necessarily the middle tier for every gun. Probe only on explicit activation,
// on the GameThread, and restore all stats if the requested tier is unavailable.
// https://github.com/gta-reversed/gta-reversed/blob/master/source/game_sa/Entity/Ped/Ped.cpp
template<class GetStat, class SetStat, class GetSkill>
bool ApplyWeaponSkillPreset(int level, GetStat get, SetStat set, GetSkill skill,int weaponType=0) {
    if (level < 0 || level > 2 || (weaponType!=0 && (weaponType<22 || weaponType>34)))return false;
    const int onlyStat=weaponType==0?-1:weaponType==32?75:weaponType>=33?79:69+weaponType-22;
    std::array<float, 11> before{};
    for (int i = 0; i < 11; ++i) {
        if(onlyStat!=-1 && onlyStat!=69+i)continue;
        before[i] = get(69 + i);
        if (!std::isfinite(before[i])) return false;
    }
    const auto restore = [&] {
        for (int i = 0; i < 11; ++i)
            if(onlyStat==-1 || onlyStat==69+i)set(69 + i, before[i]);
        return false;
    };
    for (int i = 0; i < 10; ++i) {
        const int stat = 69 + i, weapon = 22 + i;
        if(onlyStat!=-1 && onlyStat!=stat)continue;
        const auto query = [&](int value) {
            set(stat, static_cast<float>(value));
            return skill(weapon);
        };
        int value = level == 0 ? 0 : 1000;
        if (level == 1) {
            const auto boundary = [&](int tier) {
                int lo = 0, hi = 1001;
                while (lo < hi) {
                    const int mid = lo + (hi - lo) / 2;
                    int current = query(mid);
                    if (weapon == 28) {
                        // A shared progress stat must satisfy both weapons'
                        // weapon-data thresholds, even if those differ.
                        const int tec9 = skill(32);
                        current = tier == 1 ? (current < tec9 ? current : tec9)
                                            : (current > tec9 ? current : tec9);
                    }
                    if (current >= tier) hi = mid;
                    else lo = mid + 1;
                }
                return lo;
            };
            const int first = boundary(1), last = boundary(2) - 1;
            if (first > last || first > 1000) return restore();
            value = first + (last - first) / 2;
        }
        if (query(value) != level) return restore();
        if (weapon == 28 && skill(32) != level) return restore();
    }
    // Native rifles have no POOR/STD/PRO weapon-info variants. Keep their
    // progress stat testable too; do not fake a CPed skill variant for them.
    if(onlyStat==-1 || onlyStat==79)set(79, static_cast<float>(level * 500));
    return true;
}

} // namespace savr::cheats

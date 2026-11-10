#include "WeaponSkillPreset.h"
#include <cassert>
#include <limits>

int main() {
    std::array<float, 100> stats{};
    for (int i = 0; i < 100; ++i) stats[i] = static_cast<float>(i * 7);
    const auto get = [&](int stat) { return stats.at(stat); };
    int writes = 0;
    const auto set = [&](int stat, float value) {
        assert(stat >= 69 && stat <= 79);
        assert(value >= 0 && value <= 1000);
        stats.at(stat) = value;
        ++writes;
    };
    // Different thresholds include weapons for which stat 500 is already PRO,
    // the inclusive POOR boundary, and a very narrow middle-tier interval.
    const int poor[]{200, 50, 400, 200, 900, 50, 999, 200, 200, 200};
    const int pro[]{999, 500, 999, 999, 999, 300, 1001, 999, 999, 999};
    const auto skill = [&](int weapon) {
        if (weapon == 32) weapon = 28; // Tec-9 shares Micro Uzi's stat.
        assert(weapon >= 22 && weapon <= 31);
        const int i = weapon - 22;
        const float value = stats[69 + i];
        return value >= pro[i] ? 2 : (value <= poor[i] ? 0 : 1);
    };
    const auto original = stats;
    for (int level : {0, 1}) {
        assert(savr::cheats::ApplyWeaponSkillPreset(level, get, set, skill));
        for (int type = 22; type <= 32; ++type) assert(skill(type) == level);
        assert(stats[79] == level * 500);
        for (int i = 0; i < 100; ++i)
            if (i < 69 || i > 79) assert(stats[i] == original[i]);
    }
    // Unreachable PRO at 1000 restores every previously changed weapon stat.
    const auto before = stats;
    assert(!savr::cheats::ApplyWeaponSkillPreset(2, get, set, skill));
    assert(stats == before);
    const auto ordinarySkill = [&](int weapon) {
        if (weapon == 32) weapon = 28;
        const float v = stats[69 + weapon - 22];
        return v >= 999 ? 2 : (v <= 200 ? 0 : 1);
    };
    assert(savr::cheats::ApplyWeaponSkillPreset(2, get, set, ordinarySkill));
    for (int stat = 69; stat <= 79; ++stat) assert(stats[stat] == 1000);
    const auto sharedSkill = [&](int weapon) {
        if (weapon == 32) return stats[75] >= 700 ? 2 : (stats[75] <= 600 ? 0 : 1);
        return ordinarySkill(weapon);
    };
    assert(savr::cheats::ApplyWeaponSkillPreset(1, get, set, sharedSkill));
    assert(sharedSkill(28) == 1 && sharedSkill(32) == 1);
    // A missing middle tier fails and rolls back, rather than silently applying
    // HITMAN while the menu says GANGSTER.
    const auto saved = stats;
    assert(!savr::cheats::ApplyWeaponSkillPreset(1, get, set,
        [](int) { return 2; }));
    assert(stats == saved);
    // The recoil-page action changes only its weapon, not all skills. Shared
    // Uzi/Tec-9 and rifle progress are the only intentional coupled cases.
    for(int type:{22,24,28,32,33,34}){
        const auto old=stats;
        const int changed=type==32?75:type>=33?79:69+type-22;
        assert(savr::cheats::ApplyWeaponSkillPreset(0,get,set,ordinarySkill,type));
        for(int stat=0;stat<100;++stat)
            if(stat!=changed)assert(stats[stat]==old[stat]);
        assert(stats[changed]==0);
        const auto atPoor=stats;
        if(type<33){
            assert(!savr::cheats::ApplyWeaponSkillPreset(1,get,set,
                [](int){return 2;},type));
            assert(stats==atPoor);
        }
    }
    writes = 0;
    assert(!savr::cheats::ApplyWeaponSkillPreset(-1, get, set, skill));
    assert(!savr::cheats::ApplyWeaponSkillPreset(3, get, set, skill));
    assert(!savr::cheats::ApplyWeaponSkillPreset(1,get,set,skill,38));
    assert(writes == 0);
    stats[79] = std::numeric_limits<float>::quiet_NaN();
    assert(!savr::cheats::ApplyWeaponSkillPreset(1, get, set, skill));
    assert(writes == 0);
}

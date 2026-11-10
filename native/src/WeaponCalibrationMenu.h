#pragma once
namespace savr::weaponmenu {
enum Group { ROOT, MODEL, AIM, SUPPORT, EFFECTS, OPTIC, AMMO };
constexpr const char* Name(int group) {
    return group==MODEL?"WEAPON MODEL":group==AIM?"AIM / LASER":
           group==SUPPORT?"SUPPORT GRIP":group==EFFECTS?"RECOIL / TRACERS":
           group==OPTIC?"SCOPE":group==AMMO?"AMMO":"WEAPON CALIBRATION";
}
constexpr int Rows(int group) {
    return group==ROOT?7:group==MODEL?7:group==AIM?10:group==SUPPORT?8:
           group==EFFECTS?11:group==OPTIC?12:9;
}
// Stable legacy field/action identifiers shared by input and rendering.
constexpr int Field(int group,int row) {
    return group==MODEL?6+row:group==AIM?(row<6?row:19+row-6):
           group==SUPPORT?12+row:group==EFFECTS?(row==0?41:row==1?42:row<7?34+row:15+row):group==OPTIC?25+row:row;
}
static_assert(Field(AIM,6)==19 && Field(AIM,8)==21);
static_assert(Field(MODEL,5)==11 && Field(SUPPORT,6)==18);
static_assert(Field(OPTIC,10)==35 && Rows(AMMO)==9);
constexpr bool CompleteLegacyMapping() {
    bool seen[43]{};
    for (int group=MODEL;group<=OPTIC;++group)
        for (int row=0;row<Rows(group)-1;++row) {
            const int field=Field(group,row);
            if (field<0 || field>=43 || seen[field]) return false;
            seen[field]=true;
        }
    for (bool value:seen) if (!value) return false;
    return true;
}
static_assert(CompleteLegacyMapping(),"Every original calibration field/action must occur exactly once");
}

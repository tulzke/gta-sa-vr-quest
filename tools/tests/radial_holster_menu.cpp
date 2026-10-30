#include "RadialHolsterMenu.h"
#include "MissionWeaponPolicy.h"
#include <cassert>
#include <limits>

using namespace savr::radialholster;

int main() {
    Menu m;
    const float origin[]{0, 0, 0}, right[]{1, 0, 0}, up[]{0, 1, 0};
    const float top[]{0, .1f, 0}, east[]{.1f, 0, 0}, far[]{.4f, 0, 0};
    const int entries[]{2, 3, 8, -1};
    int slot = 99;
    assert(!m.Begin(0, 0, origin, right, up, entries, 4, 7));
    m.ObserveReleased(0);
    assert(m.Begin(0, 0, origin, right, up, entries, 4, 7));
    assert(m.Update(.8f, false, true, 7, top, &slot) == Result::None);
    assert(m.selected == 0);
    const float nearBoundary[]{.074f, .067f, 0};
    assert(m.Update(.8f, false, true, 7, nearBoundary, &slot) == Result::None);
    assert(m.selected == 0); // suppress chatter just across the boundary
    assert(m.Update(.8f, false, true, 7, east, &slot) == Result::None);
    assert(m.selected == 1);
    assert(m.Update(0, false, true, 7, east, &slot) == Result::Selected);
    assert(slot == 3);
    assert(m.Begin(1, 1, origin, right, up, entries, 4, 8));
    assert(m.Update(0, false, true, 8, origin, &slot) == Result::Cancelled);
    assert(m.Begin(1, 1, origin, right, up, entries, 4, 8));
    assert(m.Update(.8f, false, true, 9, top, &slot) == Result::Cancelled);
    assert(!m.Begin(1, 1, origin, right, up, entries, 4, 9));
    m.ObserveReleased(0);
    assert(m.Begin(1, 1, origin, right, up, entries, 4, 9));
    assert(m.Update(.8f, false, true, 9, far, &slot) == Result::Cancelled);
    m.ObserveReleased(0);
    assert(m.Begin(1, 1, origin, right, up, entries, 4, 9));
    assert(m.Update(.8f, true, true, 9, top, &slot) == Result::Cancelled);
    m.ObserveReleased(0);
    assert(!m.Begin(1, 1, origin, right, up, entries, 0, 9));
    assert(!m.Begin(1, 1, origin, right, up, entries, kMaxEntries + 1, 9));
    assert(m.Begin(1, 1, origin, right, up, entries, 4, 9));
    const float invalid[]{std::numeric_limits<float>::quiet_NaN(), 0, 0};
    assert(m.Update(.8f, false, true, 9, invalid, &slot) == Result::Cancelled);
    m.ObserveReleased(0);
    assert(m.Begin(0,0,origin,right,up,entries,4,10));
    const float turnedRight[]{0,0,-1}, turnedUp[]{0,1,0};
    assert(m.SetAxes(turnedRight,turnedUp));
    const float turnedEast[]{0,0,-.1f};
    assert(m.Update(.8f,false,true,10,turnedEast,&slot)==Result::None&&m.selected==1);
    const float invalidAxis[]{0,0,0};
    assert(!m.SetAxes(invalidAxis,turnedUp));
    assert(m.Update(0,false,true,10,turnedEast,&slot)==Result::Selected&&slot==3);
    const auto first=IconCell(0), last=IconCell(12), fourth=IconCell(3);
    assert(first.left==0&&first.top==0&&first.right==.25f&&first.bottom==.25f);
    assert(last.left==0&&last.top==.75f&&last.bottom==1.f);
    assert(fourth.left==.75f&&fourth.right==1.f&&fourth.top==0);
    savr::missionweapon::WeaponSnapshot before[13]{}, after[13]{};
    assert(savr::missionweapon::GrantedSlot(before,after)==-1);
    after[2].type=22; after[2].total=20;
    assert(savr::missionweapon::GrantedSlot(before,after)==2);
    before[2]=after[2]; after[2].total=25;
    assert(savr::missionweapon::GrantedSlot(before,after)==2);
    before[2]=after[2]; after[11].type=46;
    assert(savr::missionweapon::GrantedSlot(before,after)==-1);
    after[12].type=40;
    assert(savr::missionweapon::GrantedSlot(before,after)==12);
}

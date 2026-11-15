#pragma once
#include "RecoilModel.h"

#include <cstdint>
#include "WeaponOptic.h"

// VR weapon calibration, based on the Vice City VR mod's weapon-calibration
// page.  There is one canonical RIGHT/master profile per weapon; LEFT consumes
// that same profile, so a weapon only needs to be calibrated once.  AIM ray,
// WEAPON model, and SUPPORT grip offsets/rotations are tuned live from the VR
// menu and saved so they survive restarts.
//
// Calibration values are fractional "internal units" (Vice City WEAPON_VALUE_SCALE = 2):
//   display value = raw * 0.5            (centimetres for offsets, degrees for rot)
//   render metres = raw * 0.005          (offsets)
//   render radians= raw * 0.5 * D2R      (rotations)
// One factor per domain — never double-apply.
namespace savr::calib {

// Field indices are stable storage identifiers.  Menu rows may add actions
// around them, but the 19 values and their order must not change.
enum Field {
    F_AIM_OX = 0, F_AIM_OY, F_AIM_OZ,   // CM   laser / shot ray offset
    F_AIM_RX,     F_AIM_RY, F_AIM_RZ,   // DEG  laser / shot ray rotation
    F_WPN_OX,     F_WPN_OY, F_WPN_OZ,   // CM   weapon model offset (LIVE)
    F_WPN_RX,     F_WPN_RY, F_WPN_RZ,   // DEG  weapon model rotation (LIVE)
    F_SUP_OX,     F_SUP_OY, F_SUP_OZ,   // CM   support grip offset
    F_SUP_RX,     F_SUP_RY, F_SUP_RZ,   // DEG  support grip rotation
    F_SUP_STYLE,                        // support grip style
    F_COUNT                             // == 19
};

// Support grip style values (row 19).
inline constexpr float SUPPORT_MAGAZINE = 0.f, SUPPORT_FROM_BELOW = 1.f;

// One weapon's full stored profile. Aim/model defaults remain zero because the SA
// authored transform already places them correctly. Legacy all-zero SUPPORT rows
// are exposed through Snapshot/GetField as the generic Quest socket
// {0,60,-10}/{0,360,0}; the first support edit materialises those values for that
// weapon, avoiding a jump while keeping VC's model-specific numbers out of SA.
// Pistols (22..24) instead use the authored GripPistolOffhand wrist at the grip.
struct WeaponCalib {
    float aimOffX = 0, aimOffY = 0, aimOffZ = 0;
    float aimRotX = 0, aimRotY = 0, aimRotZ = 0;
    float wpnOffX = 0, wpnOffY = 0, wpnOffZ = 0;
    float wpnRotX = 0, wpnRotY = 0, wpnRotZ = 0;
    float supOffX = 0, supOffY = 0, supOffZ = 0;
    float supRotX = 0, supRotY = 0, supRotZ = 0;
    float supStyle = SUPPORT_MAGAZINE;
};

// Holstered model pose, shared by every hand and every body point but stored
// independently for each SA eWeaponType.  This mirrors the mature PC SA VR
// implementation: the six values adjust the final holstered model matrix only
// and never alter the loadout point itself.
enum HolsterField {
    H_OFF_X = 0, H_OFF_Y, H_OFF_Z,
    H_ROT_X,     H_ROT_Y, H_ROT_Z,
    H_COUNT
};

struct HolsterCalib {
    float offX = 0, offY = 0, offZ = 0;
    float rotX = 0, rotY = 0, rotZ = 0;
};

// Load saved profiles (call once at startup). Missing file = all defaults.
// Legacy per-hand files are accepted: a LEFT profile is migrated when the same
// weapon has no RIGHT profile; an explicit RIGHT profile always wins.
void Init();
// Persist edited profiles and laser state. Only canonical RIGHT/master rows are
// written; old readers still understand their unchanged `w ...` row format.
void Save();

weaponoptic::Profile SnapshotOptic(int weaponType);
void AdjustOptic(int weaponType, int field, int steps);
void ResetOptic(int weaponType);
const char* OpticFieldLabel(int field);

// The weapon type currently held / rendered. The GameThread sets it each frame so
// the menu edits — and the renderer applies — the right profile.
void SetActiveWeapon(int weaponType);
int  ActiveWeapon();
// Select which weapon-calibration profile is live: 0 = original models,
// 1 = HD/modern models. Call each frame with the set actually being rendered
// so grip/aim edits land in the matching profile and never cross over.
void SetModelSet(int set);

// Thread-safe immutable profile copy for the renderer. `hand` is accepted for
// source compatibility; both values deliberately select the RIGHT/master data.
WeaponCalib Snapshot(int hand, int weaponType);

// Per-weapon holstered pose used by the renderer and the dedicated calibration
// page.  Unlike WeaponCalib, this data has no hand dimension.
HolsterCalib SnapshotHolster(int weaponType);
float      GetHolsterField(int weaponType, int field);
void         SetHolsterField(int weaponType, int field, float value);     // clamps
void         AdjustHolsterField(int weaponType, int field, int steps);  // clamps + Save
void         HolsterFieldRange(int field, int& lo, int& hi);
float        HolsterDisplayValue(float raw);                           // raw * 0.5
const char*  HolsterFieldLabel(int field);
const char*  HolsterFieldUnit(int field);                                // CM / DEG

// Persistent laser controls. AIM fields are live and are saved on every real
// adjustment, just like the VC calibration path. LockLaser records only the
// per-weapon SAVED/UI confirmation state; it is never a second persistence
// gate. WEAPON pose edits deliberately do not dirty that marker because the
// attached ray follows the model.
bool LaserEnabled();
void SetLaserEnabled(bool enabled);   // persists immediately when changed
void ToggleLaser();                   // persists immediately

// Legacy fallback for unlisted weapons and persisted confirmation sound.
// Fixed per-weapon gestures and sound have no menu selectors.
void CycleReloadMode(int direction);  // wraps and persists immediately
void SetReloadMode(int mode);         // persists immediately
void CycleReloadSound(int direction);
void SetReloadSound(int sound);       // persists immediately
// Temporary downward-nod amplitude tuning, persisted in vr_calib.ini.
void SetReloadNodDistance(int centimetres);
void SetReloadNodAngle(int degrees);
void ResetReloadNodTuning();

// Controller recoil kick per shot. Level 0..3 (OFF/LOW/MEDIUM/HIGH).
int RecoilLevel();
const char* RecoilLevelName();
float RecoilAmplitude();
void SetRecoilLevel(int level);       // persists immediately when changed
void CycleRecoil(int direction);      // wraps OFF..HIGH, persists
// Physical weapon displacement, independent of controller vibration.
int PhysicalRecoilLevel();
const char* PhysicalRecoilLevelName();
void SetPhysicalRecoilLevel(int level);
void CyclePhysicalRecoil(int direction);
recoil::Tuning SnapshotRecoil(int weaponType,int skill=0);
void SetRecoilTuning(int weaponType,recoil::Tuning tuning,int skill=0); // persists
void AdjustRecoilTuning(int weaponType,int field,int direction,int skill=0); // 0..3
int RecoilEditingSkill();
void CycleRecoilEditingSkill(int direction);
const char* RecoilSkillName(int skill);
bool LaserLocked(int weaponType);
bool LaserLocked();
void MarkLaserDirty();                // active weapon's working AIM is unconfirmed
void LockLaser(int weaponType);       // marks this weapon confirmed and persists
void LockLaser();                     // marks active weapon confirmed and persists

// Per-weapon visible-laser override. The global toggle stays the default;
// each weapon can force its beam ON or OFF (0 GLOBAL / 1 ON / 2 OFF),
// persisted with the profiles. The fire ray never depends on visibility.
int  LaserModeForWeapon(int weaponType);
const char* LaserModeName(int weaponType);
void CycleLaserModeForWeapon(int weaponType, int direction);
bool LaserVisibleForWeapon(int weaponType);

// --- VR menu access. `field` is a Field enum value in [0, F_COUNT). ---
int         FieldCount();                                           // == F_COUNT (19)
float     GetField(int hand, int weaponType, int field);
void        SetField(int hand, int weaponType, int field, float value);     // clamps
void        AdjustField(int hand, int weaponType, int field, int steps);  // clamps + Save
void        FieldRange(int field, int& lo, int& hi);
float       DisplayValue(int field, float raw);                  // raw * 0.5
const char* FieldLabel(int field);                                 // "WEAPON OFFSET X" ...
const char* FieldUnit(int field);                                  // "CM" / "DEG" / ""
const char* StyleName(float style);                              // "MAGAZINE"/"FROM BELOW"

// Reset the active weapon's master profile. `hand` is ignored for compatibility.
void        ResetActive(int hand);

// Human name for the weapon type, for the menu heading.
const char* WeaponName(int weaponType);

}  // namespace savr::calib

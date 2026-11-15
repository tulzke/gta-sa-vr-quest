#include "HudSettings.h"
#include "MenuPrecision.h"
#include "WatchInteraction.h"

#include "Log.h"

#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <mutex>

namespace savr::hud {
namespace {

constexpr const char* kPath =
    "/sdcard/Android/data/com.rockstargames.gtasa/files/vr_hud.ini";

// reference Quest build ships CLASSIC as the approachable default.  SA follows that behaviour
// so a new test build immediately proves that the integration is active.
std::atomic<int>  g_preset{CLASSIC};
std::atomic<bool> g_enabled{true};
std::atomic<bool> g_gazeAutoHide{true};
std::atomic<int>  g_objectiveMarkerMode{OBJECTIVE_ORIGINAL};
std::atomic<int> g_pdaHand{1}, g_watchHand{0};
std::atomic<bool> g_pdaPortrait{false};
std::atomic<bool> g_watchInside{false};
std::atomic<int> g_watchZoomPercent{100};
std::atomic<bool> g_watchZoomSave{false};
std::once_flag    g_initOnce;
std::mutex        g_saveMutex;

struct ElementDefaults {
    int enabled;
    int sourceX, sourceY, sourceWidth, sourceHeight;
    int screenX, screenY, screenWidth, screenHeight;
    int scaleTenths;
};

// Radar and Health use image crops. Health includes the full top-right status block.
constexpr ElementDefaults kElementDefaults[ELEMENT_COUNT] = {
    {1,   0, 104, 284, 284,  25, 47, 28, 34,  2}, // radar
    {1, 776,  16, 244,  84,  53, 42,  8,  3, 14}, // complete top-right status
    {0, 348, 308, 676, 264,  40, 40, 23,  5, 10}, // large mission messages
    {1, 200,   0, 624, 240,  36, 34, 24, 15, 10}, // help/tutorial/brief text
    {1,   0,   0,  64,  64,  62, 26, 24,  8, 10}, // mission timers/counters
    {1,   0,   0,  64,  64,  50, 50, 10, 10, 10}, // watch
    {1,   0,   0,  64,  64,  50, 50, 10, 10, 10}, // weapon ammo
};

std::atomic<int> g_element[ELEMENT_COUNT][ELEMENT_FIELD_COUNT]{};
std::atomic<float> g_wrist[WRIST_SLOT_COUNT][ELEMENT_COUNT][WRIST_FIELD_COUNT]{};
std::atomic<int> g_calibrationElement{RADAR};

const char* const kElementKeys[ELEMENT_COUNT] = {
    "Radar", "Health", "Messages", "Help", "Timers", "Watch", "Ammo"
};
const char* const kFieldKeys[ELEMENT_FIELD_COUNT] = {
    "Enabled", "SourceX", "SourceY", "SourceWidth", "SourceHeight",
    "ScreenX", "ScreenY", "ScreenWidth", "ScreenHeight", "ScaleTenths"
};
// Read millimetre-resolution keys (0.1 cm calibration steps); ignore superseded *Cm keys.
const char* const kWristFieldKeys[WRIST_SLOT_COUNT][WRIST_FIELD_COUNT] = {
    {"WristAlongMm", "WristAcrossMm", "WristLiftMm",
     "WristPitchDeg", "WristYawDeg", "WristRollDeg", "WristScaleTenths"},
    {"DashAlongMm", "DashAcrossMm", "DashLiftMm",
     "DashPitchDeg", "DashYawDeg", "DashRollDeg", "DashScaleTenths"},
    {"GripAlongMm", "GripAcrossMm", "GripLiftMm",
     "GripPitchDeg", "GripYawDeg", "GripRollDeg", "GripScaleTenths"},
    {"TwoHandAlongMm", "TwoHandAcrossMm", "TwoHandLiftMm",
     "TwoHandPitchDeg", "TwoHandYawDeg", "TwoHandRollDeg",
     "TwoHandScaleTenths"},
};
// Per-vehicle dashboard overrides use the bare field names under a
// "DashM<model>" scope: Hud.Radar.DashM462.AlongMm=15
const char* const kDashModelFieldKeys[WRIST_FIELD_COUNT] = {
    "AlongMm", "AcrossMm", "LiftMm",
    "PitchDeg", "YawDeg", "RollDeg", "ScaleTenths"
};
constexpr int kWristFieldDefaults[WRIST_FIELD_COUNT] = {0, 0, 0, 0, 0, 0, 10};
// PDA sits slightly left of the wheel. Saved per-model placements still win.
constexpr int kDashRadarDefaults[WRIST_FIELD_COUNT] =
    {0, 140, -60, 0, 0, 0, 10};
constexpr int kDashHealthDefaults[WRIST_FIELD_COUNT] =
    {0, -47, -169, 0, 0, 0, 23};

// Sparse per-vehicle dashboard overrides; created on first adjustment from a
// copy of the shared dash slot. Guarded by g_dashModelMutex (read twice per
// present frame + on menu edits — cold).
std::mutex g_dashModelMutex;
std::map<int, std::array<float, WRIST_FIELD_COUNT>>
    g_dashModel[ELEMENT_COUNT];

int ClampElement(int element) {
    return std::clamp(element, 0, ELEMENT_COUNT - 1);
}

int ClampElementField(int field, int value) {
    switch (field) {
        case ELEMENT_ENABLED: return value != 0;
        case SOURCE_X:        return std::clamp(value, 0, 1016);
        case SOURCE_Y:        return std::clamp(value, 0, 568);
        case SOURCE_WIDTH:    return std::clamp(value, 8, 1024);
        case SOURCE_HEIGHT:   return std::clamp(value, 8, 576);
        case SCREEN_X:
        case SCREEN_Y:        return std::clamp(value, 0, 100);
        case SCREEN_WIDTH:
        case SCREEN_HEIGHT:   return std::clamp(value, 2, 100);
        case ELEMENT_SCALE:   return std::clamp(value, 1, 30);
        default:              return 0;
    }
}

float ClampWristField(int field, float value) {
    switch (field) {
        case WRIST_ALONG_MM:
        case WRIST_ACROSS_MM:     return std::clamp<float>(value, -400, 400);
        case WRIST_LIFT_MM:       return std::clamp<float>(value, -300, 300);
        case WRIST_PITCH_DEG:
        case WRIST_YAW_DEG:
        case WRIST_ROLL_DEG:      return std::clamp<float>(value, -180, 180);
        case WRIST_SCALE_TENTHS:  return std::clamp<float>(value, 3, 30);
        default:                  return 0;
    }
}

int ClampWristSlot(int slot) {
    return std::clamp(slot, 0, WRIST_SLOT_COUNT - 1);
}

void LoadWristDefaults() {
    for (int slot = 0; slot < WRIST_SLOT_COUNT; ++slot)
        for (int element = 0; element < ELEMENT_COUNT; ++element)
            for (int field = 0; field < WRIST_FIELD_COUNT; ++field)
                g_wrist[slot][element][field].store(
                    kWristFieldDefaults[field], std::memory_order_relaxed);
    for (int field = 0; field < WRIST_FIELD_COUNT; ++field) {
        g_wrist[WRIST_SLOT_VEHICLE][RADAR][field].store(
            kDashRadarDefaults[field], std::memory_order_relaxed);
        g_wrist[WRIST_SLOT_VEHICLE][HEALTH][field].store(
            kDashHealthDefaults[field], std::memory_order_relaxed);
    }
    // One-hand grip places the panel outboard; two-hand grip stacks panels beside the primary hand.
    for (int element = RADAR; element <= HEALTH; ++element) {
        g_wrist[WRIST_SLOT_WEAPON][element][WRIST_ACROSS_MM].store(
            -120, std::memory_order_relaxed);
        g_wrist[WRIST_SLOT_TWOHAND][element][WRIST_ACROSS_MM].store(
            -150, std::memory_order_relaxed);
    }
    g_wrist[WRIST_SLOT_TWOHAND][RADAR][WRIST_LIFT_MM].store(
        70, std::memory_order_relaxed);
    g_wrist[WRIST_SLOT_TWOHAND][HEALTH][WRIST_LIFT_MM].store(
        -70, std::memory_order_relaxed);
}

void LoadElementDefaults() {
    for (int element = 0; element < ELEMENT_COUNT; ++element) {
        const int values[ELEMENT_FIELD_COUNT] = {
            kElementDefaults[element].enabled,
            kElementDefaults[element].sourceX,
            kElementDefaults[element].sourceY,
            kElementDefaults[element].sourceWidth,
            kElementDefaults[element].sourceHeight,
            kElementDefaults[element].screenX,
            kElementDefaults[element].screenY,
            kElementDefaults[element].screenWidth,
            kElementDefaults[element].screenHeight,
            kElementDefaults[element].scaleTenths,
        };
        for (int field = 0; field < ELEMENT_FIELD_COUNT; ++field)
            g_element[element][field].store(values[field], std::memory_order_relaxed);
    }
}

int ClampPreset(int value) { return value == IMMERSIVE ? IMMERSIVE : CLASSIC; }
int ClampObjectiveMarkerMode(int value) {
    return std::clamp(value, 0, OBJECTIVE_MODE_COUNT - 1);
}

void Load() {
    LoadElementDefaults();
    LoadWristDefaults();
    {
        std::lock_guard<std::mutex> lock(g_dashModelMutex);
        for (auto& map : g_dashModel) map.clear();
    }
    int preset = CLASSIC;
    int enabled = 1;
    int gazeAutoHide = 1;
    int objectiveMarkerMode = OBJECTIVE_ORIGINAL;
    if (FILE* file = std::fopen(kPath, "r")) {
        char line[96];
        while (std::fgets(line, sizeof(line), file)) {
            int value = 0;
            if (std::sscanf(line, "HudPreset=%d", &value) == 1)
                preset = ClampPreset(value);
            else if (std::sscanf(line, "GameplayHud=%d", &value) == 1)
                enabled = value != 0;
            else if (std::sscanf(line, "HudGazeAutoHide=%d", &value) == 1)
                gazeAutoHide = value != 0;
            else if (std::sscanf(line, "ObjectiveMarkers=%d", &value) == 1)
                objectiveMarkerMode = ClampObjectiveMarkerMode(value);
            else if (std::sscanf(line, "PdaHand=%d", &value) == 1)
                g_pdaHand.store(value == 1);
            else if (std::sscanf(line, "WatchHand=%d", &value) == 1)
                g_watchHand.store(value == 1);
            else if (std::sscanf(line, "WatchInside=%d", &value) == 1)
                g_watchInside.store(value != 0);
            else if (std::sscanf(line, "WatchMapZoomPercent=%d", &value) == 1)
                g_watchZoomPercent.store(std::clamp(value,int(watchhud::kZoomMin*100),int(watchhud::kZoomMax*100)));
            else if (std::sscanf(line, "PdaPortrait=%d", &value) == 1)
                g_pdaPortrait.store(value != 0);
            else {
                bool matched = false;
                for (int element = 0; element < ELEMENT_COUNT && !matched; ++element) {
                    for (int field = 0; field < ELEMENT_FIELD_COUNT; ++field) {
                        char pattern[96];
                        std::snprintf(pattern, sizeof(pattern), "Hud.%s.%s=%%d",
                                      kElementKeys[element], kFieldKeys[field]);
                        if (std::sscanf(line, pattern, &value) == 1) {
                            g_element[element][field].store(
                                ClampElementField(field, value),
                                std::memory_order_relaxed);
                            matched = true;
                            break;
                        }
                    }
                    float wristValue{};
                    for (int slot = 0; slot < WRIST_SLOT_COUNT && !matched;
                         ++slot) {
                        for (int field = 0; field < WRIST_FIELD_COUNT &&
                             !matched; ++field) {
                            char pattern[96];
                            std::snprintf(pattern, sizeof(pattern),
                                          "Hud.%s.%s=%%f",
                                          kElementKeys[element],
                                          kWristFieldKeys[slot][field]);
                            if (std::sscanf(line, pattern, &wristValue) == 1 &&
                                std::isfinite(wristValue)) {
                                g_wrist[slot][element][field].store(
                                    ClampWristField(field, wristValue),
                                    std::memory_order_relaxed);
                                matched = true;
                            }
                        }
                    }
                    for (int field = 0; field < WRIST_FIELD_COUNT && !matched;
                         ++field) {
                        char pattern[96];
                        std::snprintf(pattern, sizeof(pattern),
                                      "Hud.%s.DashM%%d.%s=%%f",
                                      kElementKeys[element],
                                      kDashModelFieldKeys[field]);
                        int model = -1;
                        if (std::sscanf(line, pattern, &model, &wristValue) == 2 &&
                            std::isfinite(wristValue) &&
                            model >= 0 && model < 65536) {
                            std::lock_guard<std::mutex> lock(g_dashModelMutex);
                            auto& entry = g_dashModel[element][model];
                            static_assert(WRIST_FIELD_COUNT == 7, "");
                            if (entry == std::array<float, WRIST_FIELD_COUNT>{})
                                for (int f = 0; f < WRIST_FIELD_COUNT; ++f)
                                    entry[f] = g_wrist[WRIST_SLOT_VEHICLE]
                                        [element][f].load(
                                            std::memory_order_relaxed);
                            entry[field] = ClampWristField(field, wristValue);
                            matched = true;
                        }
                    }
                }
            }
        }
        std::fclose(file);
    }
    // Migrate obsolete crop-scale values while preserving the configured panel position and bounds.
    if (g_element[MESSAGES][ELEMENT_SCALE].load(std::memory_order_relaxed)<5)
        g_element[MESSAGES][ELEMENT_SCALE].store(10,std::memory_order_relaxed);
    g_preset.store(preset, std::memory_order_release);
    g_enabled.store(enabled != 0, std::memory_order_release);
    g_gazeAutoHide.store(gazeAutoHide != 0, std::memory_order_release);
    g_objectiveMarkerMode.store(objectiveMarkerMode, std::memory_order_release);
    LOGI("hud settings: preset=%s enabled=%d objective=%d",
         preset == CLASSIC ? "CLASSIC" : "IMMERSIVE",
         enabled != 0 ? 1 : 0, objectiveMarkerMode);
}

void EnsureLoaded() { std::call_once(g_initOnce, Load); }

void Save() {
    EnsureLoaded();
    std::lock_guard<std::mutex> lock(g_saveMutex);
    FILE* file = std::fopen(kPath, "w");
    if (file == nullptr) {
        // Player-report fix: the installer pushes a default vr_hud.ini over
        // adb, which some firmwares leave shell-owned and unwritable for the
        // app through FUSE - every crop calibration then silently died with
        // this fopen and "reset on restart". Unlinking the foreign file is
        // allowed (the directory is ours) and the rewrite then succeeds.
        std::remove(kPath);
        file = std::fopen(kPath, "w");
        LOGW("hud settings: save retry after unlink -> %s",
             file ? "ok" : "still failing");
    }
    if (file != nullptr) {
        std::fprintf(file, "HudPreset=%d\n", g_preset.load(std::memory_order_acquire));
        std::fprintf(file, "GameplayHud=%d\n",
                     g_enabled.load(std::memory_order_acquire) ? 1 : 0);
        std::fprintf(file, "HudGazeAutoHide=%d\n",
                     g_gazeAutoHide.load(std::memory_order_acquire) ? 1 : 0);
        std::fprintf(file, "ObjectiveMarkers=%d\n",
                     g_objectiveMarkerMode.load(std::memory_order_acquire));
        std::fprintf(file, "PdaHand=%d\nWatchHand=%d\nPdaPortrait=%d\n",
                     g_pdaHand.load(), g_watchHand.load(), int(g_pdaPortrait.load()));
        std::fprintf(file,"WatchInside=%d\n",int(g_watchInside.load()));
        std::fprintf(file,"WatchMapZoomPercent=%d\n",g_watchZoomPercent.load());
        for (int element = 0; element < ELEMENT_COUNT; ++element) {
            for (int field = 0; field < ELEMENT_FIELD_COUNT; ++field)
                std::fprintf(file, "Hud.%s.%s=%d\n",
                             kElementKeys[element], kFieldKeys[field],
                             g_element[element][field].load(
                                 std::memory_order_acquire));
            for (int slot = 0; slot < WRIST_SLOT_COUNT; ++slot)
                for (int field = 0; field < WRIST_FIELD_COUNT; ++field)
                    std::fprintf(file, "Hud.%s.%s=%.7g\n",
                                 kElementKeys[element],
                                 kWristFieldKeys[slot][field],
                                 g_wrist[slot][element][field].load(
                                     std::memory_order_acquire));
            std::lock_guard<std::mutex> lock(g_dashModelMutex);
            for (const auto& [model, values] : g_dashModel[element])
                for (int field = 0; field < WRIST_FIELD_COUNT; ++field)
                    std::fprintf(file, "Hud.%s.DashM%d.%s=%.7g\n",
                                 kElementKeys[element], model,
                                 kDashModelFieldKeys[field], values[field]);
        }
        std::fclose(file);
    } else {
        LOGW("hud settings: cannot save %s", kPath);
    }
}

}  // namespace

void Init() { EnsureLoaded(); }

Preset CurrentPreset() {
    EnsureLoaded();
    return static_cast<Preset>(ClampPreset(g_preset.load(std::memory_order_acquire)));
}

const char* PresetName() { return CurrentPreset() == CLASSIC ? "CLASSIC" : "IMMERSIVE"; }

void SetPreset(Preset preset) {
    EnsureLoaded();
    const int value = ClampPreset(static_cast<int>(preset));
    if (g_preset.exchange(value, std::memory_order_acq_rel) != value) Save();
}

void TogglePreset() {
    SetPreset(CurrentPreset() == CLASSIC ? IMMERSIVE : CLASSIC);
}

bool GameplayHudEnabled() {
    EnsureLoaded();
    return g_enabled.load(std::memory_order_acquire);
}

void SetGameplayHudEnabled(bool enabled) {
    EnsureLoaded();
    if (g_enabled.exchange(enabled, std::memory_order_acq_rel) != enabled) Save();
}

void ToggleGameplayHud() { SetGameplayHudEnabled(!GameplayHudEnabled()); }

bool GazeAutoHideEnabled() {
    EnsureLoaded();
    return g_gazeAutoHide.load(std::memory_order_acquire);
}

void SetGazeAutoHideEnabled(bool enabled) {
    EnsureLoaded();
    if (g_gazeAutoHide.exchange(enabled, std::memory_order_acq_rel) != enabled)
        Save();
}

void ToggleGazeAutoHide() {
    SetGazeAutoHideEnabled(!GazeAutoHideEnabled());
}

int DeviceHand(int element) {
    EnsureLoaded();
    return (element == WATCH ? g_watchHand : g_pdaHand).load();
}
void ToggleDeviceHand(int element) {
    EnsureLoaded();
    auto& hand = element == WATCH ? g_watchHand : g_pdaHand;
    hand.store(1 - hand.load());
    Save();
}
bool PdaPortrait() { EnsureLoaded(); return g_pdaPortrait.load(); }
bool WatchInside() { EnsureLoaded(); return g_watchInside.load(); }
float WatchMapZoom(){EnsureLoaded();return g_watchZoomPercent.load()*.01f;}
void SetWatchMapZoom(float zoom,bool persist){
    EnsureLoaded();
    if(std::isfinite(zoom))g_watchZoomPercent.store(static_cast<int>(std::round(std::clamp(zoom,watchhud::kZoomMin,watchhud::kZoomMax)*100.f)));
    if(persist)g_watchZoomSave.store(true,std::memory_order_release);
}
void FlushWatchSettings(){if(g_watchZoomSave.exchange(false,std::memory_order_acq_rel))Save();}
void ToggleWatchInside() { EnsureLoaded();g_watchInside.store(!g_watchInside.load());Save(); }
void TogglePdaPortrait() { EnsureLoaded(); g_pdaPortrait.store(!g_pdaPortrait.load()); Save(); }

bool ShouldRenderClassicHud() {
    return GameplayHudEnabled() && CurrentPreset() == CLASSIC;
}

bool ShouldRenderWristHud() {
    return GameplayHudEnabled() && CurrentPreset() == IMMERSIVE;
}

ObjectiveMarkerMode GetObjectiveMarkerMode() {
    // Fixed to the classic markers by request — the selector was removed from
    // the VR menu; storage/API kept for compatibility with older ini files.
    return OBJECTIVE_ORIGINAL;
}

const char* ObjectiveMarkerModeName() {
    static const char* const names[OBJECTIVE_MODE_COUNT] = {
        "OFF", "ORIGINAL", "HIGHLIGHT", "BOTH"
    };
    return names[GetObjectiveMarkerMode()];
}

void SetObjectiveMarkerMode(ObjectiveMarkerMode mode) {
    EnsureLoaded();
    const int value = ClampObjectiveMarkerMode(static_cast<int>(mode));
    if (g_objectiveMarkerMode.exchange(value, std::memory_order_acq_rel) != value)
        Save();
}

void CycleObjectiveMarkerMode(int direction) {
    if (!direction) return;
    const int current = static_cast<int>(GetObjectiveMarkerMode());
    const int next = (current + OBJECTIVE_MODE_COUNT +
                      (direction < 0 ? -1 : 1)) % OBJECTIVE_MODE_COUNT;
    SetObjectiveMarkerMode(static_cast<ObjectiveMarkerMode>(next));
}

bool ObjectiveMarkersIncludeOriginal() {
    const auto mode = GetObjectiveMarkerMode();
    return mode == OBJECTIVE_ORIGINAL || mode == OBJECTIVE_BOTH;
}

bool ObjectiveMarkersIncludeHighlight() {
    const auto mode = GetObjectiveMarkerMode();
    return mode == OBJECTIVE_HIGHLIGHT || mode == OBJECTIVE_BOTH;
}

const char* ElementName(int element) {
    static const char* const names[ELEMENT_COUNT] = {
        "RADAR", "HEALTH / TOP-RIGHT HUD", "BIG MESSAGES", "HELP TEXT",
        "MISSION TIMERS", "WATCH", "WEAPON AMMO"
    };
    return names[ClampElement(element)];
}

ElementSettings GetElementSettings(int element) {
    EnsureLoaded();
    element = ClampElement(element);
    auto value = [element](int field) {
        return g_element[element][field].load(std::memory_order_acquire);
    };
    ElementSettings settings{
        value(ELEMENT_ENABLED) != 0,
        value(SOURCE_X), value(SOURCE_Y),
        value(SOURCE_WIDTH), value(SOURCE_HEIGHT),
        value(SCREEN_X), value(SCREEN_Y),
        value(SCREEN_WIDTH), value(SCREEN_HEIGHT), value(ELEMENT_SCALE)
    };
    settings.sourceX = std::clamp(settings.sourceX, 0, 1016);
    settings.sourceY = std::clamp(settings.sourceY, 0, 568);
    settings.sourceWidth = std::clamp(settings.sourceWidth, 8,
                                      1024 - settings.sourceX);
    settings.sourceHeight = std::clamp(settings.sourceHeight, 8,
                                       576 - settings.sourceY);
    return settings;
}

WristSettings GetWristSettings(int element, int slot, int vehicleModel) {
    EnsureLoaded();
    element = ClampElement(element);
    slot = ClampWristSlot(slot);
    if (slot == WRIST_SLOT_VEHICLE && vehicleModel >= 0) {
        std::lock_guard<std::mutex> lock(g_dashModelMutex);
        const auto found = g_dashModel[element].find(vehicleModel);
        if (found != g_dashModel[element].end()) {
            const auto& v = found->second;
            return WristSettings{
                ClampWristField(WRIST_ALONG_MM, v[WRIST_ALONG_MM]),
                ClampWristField(WRIST_ACROSS_MM, v[WRIST_ACROSS_MM]),
                ClampWristField(WRIST_LIFT_MM, v[WRIST_LIFT_MM]),
                ClampWristField(WRIST_PITCH_DEG, v[WRIST_PITCH_DEG]),
                ClampWristField(WRIST_YAW_DEG, v[WRIST_YAW_DEG]),
                ClampWristField(WRIST_ROLL_DEG, v[WRIST_ROLL_DEG]),
                ClampWristField(WRIST_SCALE_TENTHS, v[WRIST_SCALE_TENTHS])
            };
        }
    }
    auto value = [element, slot](int field) {
        return ClampWristField(field, g_wrist[slot][element][field].load(
                                          std::memory_order_acquire));
    };
    return WristSettings{
        value(WRIST_ALONG_MM), value(WRIST_ACROSS_MM), value(WRIST_LIFT_MM),
        value(WRIST_PITCH_DEG), value(WRIST_YAW_DEG), value(WRIST_ROLL_DEG),
        value(WRIST_SCALE_TENTHS)
    };
}

void SetVehiclePdaPosition(int model,const WristSettings& position){
    EnsureLoaded();if(model<400||model>611||!std::isfinite(position.alongMm)||
        !std::isfinite(position.acrossMm)||!std::isfinite(position.liftMm))return;
    std::lock_guard<std::mutex> lock(g_dashModelMutex);
    auto [it,inserted]=g_dashModel[RADAR].try_emplace(model);
    if(inserted)for(int f=0;f<WRIST_FIELD_COUNT;++f)it->second[f]=g_wrist[WRIST_SLOT_VEHICLE][RADAR][f].load();
    it->second[WRIST_ALONG_MM]=ClampWristField(WRIST_ALONG_MM,position.alongMm);
    it->second[WRIST_ACROSS_MM]=ClampWristField(WRIST_ACROSS_MM,position.acrossMm);
    it->second[WRIST_LIFT_MM]=ClampWristField(WRIST_LIFT_MM,position.liftMm);
}
void SavePdaCalibration(){Save();}
void AdjustWristField(int element, int field, int direction, int slot,
                      int vehicleModel) {
    EnsureLoaded();
    element = ClampElement(element);
    slot = ClampWristSlot(slot);
    if (!direction || field < 0 || field >= WRIST_FIELD_COUNT) return;
    const auto adjusted=[field,direction](float current) {
        return field==WRIST_SCALE_TENTHS ? current+direction :
            menu::SpatialValue(current,direction,field<3?.1f:1.f);
    };
    if (slot == WRIST_SLOT_VEHICLE && vehicleModel >= 0) {
        // Per-vehicle override, seeded from the shared dash slot on first
        // touch so the calibrated defaults carry over.
        std::lock_guard<std::mutex> lock(g_dashModelMutex);
        auto& entry = g_dashModel[element][vehicleModel];
        if (entry == std::array<float, WRIST_FIELD_COUNT>{})
            for (int f = 0; f < WRIST_FIELD_COUNT; ++f)
                entry[f] = g_wrist[WRIST_SLOT_VEHICLE][element][f].load(
                    std::memory_order_acquire);
        entry[field] =
            ClampWristField(field, adjusted(entry[field]));
    } else {
        const float current =
            g_wrist[slot][element][field].load(std::memory_order_acquire);
        g_wrist[slot][element][field].store(
            ClampWristField(field, adjusted(current)),
            std::memory_order_release);
    }
    Save();
}

int CalibrationElement() {
    EnsureLoaded();
    return ClampElement(g_calibrationElement.load(std::memory_order_acquire));
}

void CycleCalibrationElement(int direction) {
    EnsureLoaded();
    if (!direction) return;
    const int current = CalibrationElement();
    int next=(current+ELEMENT_COUNT+(direction<0?-1:1))%ELEMENT_COUNT;
    if (next==AMMO) next=(next+ELEMENT_COUNT+(direction<0?-1:1))%ELEMENT_COUNT;
    g_calibrationElement.store(
        next,
        std::memory_order_release);
}

void SetElementEnabled(int element, bool enabled) {
    EnsureLoaded();
    element = ClampElement(element);
    g_element[element][ELEMENT_ENABLED].store(enabled ? 1 : 0,
                                               std::memory_order_release);
    Save();
}

void AdjustElementField(int element, int field, int direction) {
    EnsureLoaded();
    element = ClampElement(element);
    if (!direction || field <= ELEMENT_ENABLED || field >= ELEMENT_FIELD_COUNT)
        return;
    const int increment = field==ELEMENT_SCALE ? 1 :
                          (field >= SCREEN_X ? 1 : 4);
    const int current = g_element[element][field].load(std::memory_order_acquire);
    g_element[element][field].store(
        ClampElementField(field, current + direction * increment),
        std::memory_order_release);
    // Keep the source rectangle valid while either its origin or extent moves.
    ElementSettings settings = GetElementSettings(element);
    g_element[element][SOURCE_WIDTH].store(settings.sourceWidth,
                                            std::memory_order_release);
    g_element[element][SOURCE_HEIGHT].store(settings.sourceHeight,
                                             std::memory_order_release);
    Save();
}

void ResetElement(int element) {
    EnsureLoaded();
    element = ClampElement(element);
    const int values[ELEMENT_FIELD_COUNT] = {
        kElementDefaults[element].enabled,
        kElementDefaults[element].sourceX,
        kElementDefaults[element].sourceY,
        kElementDefaults[element].sourceWidth,
        kElementDefaults[element].sourceHeight,
        kElementDefaults[element].screenX,
        kElementDefaults[element].screenY,
        kElementDefaults[element].screenWidth,
        kElementDefaults[element].screenHeight,
        kElementDefaults[element].scaleTenths,
    };
    for (int field = 0; field < ELEMENT_FIELD_COUNT; ++field)
        g_element[element][field].store(values[field], std::memory_order_release);
    for (int slot = 0; slot < WRIST_SLOT_COUNT; ++slot)
        for (int field = 0; field < WRIST_FIELD_COUNT; ++field)
            g_wrist[slot][element][field].store(kWristFieldDefaults[field],
                                                std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(g_dashModelMutex);
        g_dashModel[element].clear();
    }
    Save();
}

}  // namespace savr::hud

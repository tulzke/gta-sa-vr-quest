#pragma once
#include <array>
#include <mutex>
#include <string>
#include <utility>

namespace savr::hydrohand {
using Fit=std::array<float,6>; // moving lever X/Y/Z in cm, rotations about X/Y/Z in degrees
inline constexpr const char* kPath="/sdcard/Android/data/com.rockstargames.gtasa/files/vr_hydraulics_hands.ini";
inline constexpr const char* kKeys[6]{"Xcm","Ycm","Zcm","RotXDeg","RotYDeg","RotZDeg"};
class Config {
    std::mutex mutex_;std::string path_;Fit fit_[2]{};bool loaded_{},saved_{};
    void Load();bool Save();
public:
    explicit Config(std::string path):path_(std::move(path)){}
    Fit Get(int hand);
    bool Adjust(int hand,int field,int direction);
    bool Reset(int hand);
    bool Saved();
};
Fit Get(int hand);
void Adjust(int field,int direction);
void Reset();
int Selected();
void Select(int direction);
void SetPreview(bool active);
int PreviewHand();
bool Saved();
}

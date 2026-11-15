#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace savr::bicyclesway {

constexpr std::uintptr_t kControlAddress=0x6be2a8;
constexpr std::uintptr_t kBaseCallAddress=0x810530;
constexpr std::size_t kLeanOffset=0x838, kFrameSwayOffset=0xa20;
constexpr std::size_t kResumeOffset=24, kTrampolineBytes=72;

inline float ComfortableLean(float lean) {
    constexpr float limit=10.0f*3.14159265358979323846f/180.0f;
    return std::clamp(lean*.25f,-limit,limit);
}

inline bool Eligible(void* bike,void* playerVehicle,bool driver,bool stereo,int model) {
    return bike&&bike==playerVehicle&&driver&&stereo&&
        (model==481||model==509||model==510);
}

// The whole retail control function still runs. Capture the physical lean at
// the return from CBike, before CBmx adds its animation, then restore exactly
// a comfortable fraction of that sample. No render-time matrix swaps. The
// native smoothed lean at +0x83c and the physical entity basis remain intact.
struct Frame { void* bike{}; float baseLean{}; bool captured{}; };
inline thread_local Frame* current{};

inline void CaptureBaseLean(void* bike) {
    if (!current||!bike||current->bike!=bike) return;
    float lean{};
    std::memcpy(&lean,static_cast<char*>(bike)+kLeanOffset,sizeof(lean));
    current->baseLean=lean;
    current->captured=std::isfinite(lean);
}

class Scope {
    Frame frame_;
    Frame* previous_;
public:
    explicit Scope(void* bike):frame_{bike},previous_(current) { current=&frame_; }
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
    ~Scope() { current=previous_; }
    void Finish() const {
        if (!frame_.bike||!frame_.captured) return;
        auto* bytes=static_cast<char*>(frame_.bike);
        const float zero=0.0f;
        const float lean=ComfortableLean(frame_.baseLean);
        std::memcpy(bytes+kLeanOffset,&lean,sizeof(float));
        std::memcpy(bytes+kFrameSwayOffset,&zero,sizeof(float));
    }
};

struct Instruction { std::size_t offset; std::uint32_t word; };
// Android 2.11 arm64: entry/base call, both animation writes and exit.
// Reject the complete experiment before patching if any evidence differs.
constexpr Instruction kExpected[]={
    {0x00,0xfc1e0fe8u},{0x04,0xa900fbfdu},{0x08,0xf9000ff3u},
    {0x0c,0x910023fdu},{0x10,0xaa0003f3u},{0x14,0x9405489du},
    {0x18,0x3940a268u},{0x1c,0x37300c68u},
    {0xf4,0x9404df05u},{0xf8,0x1e200900u},{0xfc,0x1400002au},
    {0x180,0x9404dee2u},{0x184,0xbd483a61u},{0x188,0x1f000500u},
    {0x18c,0xbd083a60u},{0x190,0x52866668u},{0x194,0xbd4a2260u},
    {0x198,0x72a7ee68u},{0x19c,0x1e270101u},{0x1a0,0x1e210800u},
    {0x1a4,0xbd0a2260u},{0x1a8,0xa940fbfdu},{0x1ac,0xf9400ff3u},
    {0x1b0,0xfc4207e8u},{0x1b4,0xd65f03c0u}
};

inline bool MatchesRetail(const void* entry) {
    if (!entry) return false;
    for (const auto& item:kExpected) {
        std::uint32_t actual{};
        std::memcpy(&actual,static_cast<const char*>(entry)+item.offset,4);
        if (actual!=item.word) return false;
    }
    return true;
}

inline void BuildTrampoline(void* memory,std::uintptr_t baseCall,
                            std::uintptr_t capture,std::uintptr_t resume) {
    auto* code=static_cast<std::uint32_t*>(memory);
    // Five PC-independent instructions establish the original stack/X19.
    for (int i=0;i<5;++i) code[i]=kExpected[i].word;
    code[5]=0x580000f1u;  // LDR X17,baseCall at +48
    code[6]=0xd63f0220u;  // BLR X17 (original CBike::ProcessControl PLT)
    code[7]=0xaa1303e0u;  // MOV X0,X19 (same bicycle)
    code[8]=0x580000d1u;  // LDR X17,capture at +56
    code[9]=0xd63f0220u;  // BLR X17 (void callback; base has no return value)
    code[10]=0x580000d1u; // LDR X17,resume at +64
    code[11]=0xd61f0220u; // BR X17; retail epilogue restores saved LR/D8
    std::memcpy(code+12,&baseCall,8);
    std::memcpy(code+14,&capture,8);
    std::memcpy(code+16,&resume,8);
}

} // namespace savr::bicyclesway

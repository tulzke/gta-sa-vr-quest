#include "AudioDiagnostics.h"
#include "AudioOutputRate.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef SAVR_EXPERIMENTAL_AUDIO_48KHZ
#include <sys/mman.h>
#endif

#include "Log.h"

namespace savr::audiodiag {
namespace {
constexpr char kLog[] = "/sdcard/Android/data/com.rockstargames.gtasa/files/vr_audio_diag.log";
constexpr char kPrevious[] = "/sdcard/Android/data/com.rockstargames.gtasa/files/vr_audio_diag.previous.log";
constexpr char kBackend[] = "/sdcard/Android/data/com.rockstargames.gtasa/files/vr_openal.log";
constexpr char kBackendPrevious[] = "/sdcard/Android/data/com.rockstargames.gtasa/files/vr_openal.previous.log";
// OpenAL 1.1 ABI; resolve from the game's library, without adding a link dependency.
struct Api {
    void* (*context)(){};
    void* (*device)(void*){};
    void (*deviceInt)(void*, int, int, int*){};
    const char* (*string)(int){};
    void (*listener)(int, float*){};
    char (*isSource)(unsigned){};
    void (*sourceInt)(unsigned, int, int*){};
    void (*sourceFloat)(unsigned, int, float*){};
    void (*bufferInt)(unsigned, int, int*){};
    bool ready{};
} api;
double lastFrame{}, lastSample{}, maxGap{};
unsigned frames{}, gaps{};
bool configured{};
std::atomic<bool> outputRatePatched{false};
std::atomic<bool> outputRateRxRestored{true};

double Now() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}
FILE* Open() {
    struct stat st{};
    if (!stat(kLog, &st) && st.st_size >= 2 * 1024 * 1024) {
        // rename replaces the older generation atomically on Android.
        if (rename(kLog, kPrevious)) return nullptr;
    }
    return std::fopen(kLog, "a");
}
bool Resolve() {
    if (api.ready) return true;
    void* lib = dlopen("libopenal.so", RTLD_NOW | RTLD_NOLOAD);
    if (!lib) return false;
#define RESOLVE(field, name) api.field = reinterpret_cast<decltype(api.field)>(dlsym(lib, name))
    RESOLVE(context, "alcGetCurrentContext");
    RESOLVE(device, "alcGetContextsDevice");
    RESOLVE(deviceInt, "alcGetIntegerv");
    RESOLVE(string, "alGetString");
    RESOLVE(listener, "alGetListenerf");
    RESOLVE(isSource, "alIsSource");
    RESOLVE(sourceInt, "alGetSourcei");
    RESOLVE(sourceFloat, "alGetSourcef");
    RESOLVE(bufferInt, "alGetBufferi");
#undef RESOLVE
    api.ready = api.context && api.device && api.deviceInt && api.string &&
        api.listener && api.isSource && api.sourceInt && api.sourceFloat && api.bufferInt;
    dlclose(lib); // Game owns the library for its entire lifetime.
    return api.ready;
}
}

void Configure() {
    if (configured) return;
    configured = true;
    // OpenAL Soft reads these on first initialization. Keep the preceding run,
    // since the reported failure is cleared by restarting the process.
    if (access(kBackend, F_OK) == 0) rename(kBackend, kBackendPrevious);
    const int levelResult = setenv("ALSOFT_LOGLEVEL", "3", 0);
    const int fileResult = setenv("ALSOFT_LOGFILE", kBackend, 0);
    if (FILE* log = Open()) {
        std::fprintf(log, "SESSION v1 epoch=%lld mono=%.3f pid=%d backend_level=%s backend_file=%s env_results=%d/%d\n",
            static_cast<long long>(time(nullptr)), Now(), getpid(),
            getenv("ALSOFT_LOGLEVEL"), getenv("ALSOFT_LOGFILE"), levelResult, fileResult);
        std::fclose(log);
    } else {
        LOGE("[audio.diag] cannot open persistent diagnostic log");
    }
}

void PrepareGameAudio(void* gameHandle) {
    const char* status = "stock";
    int failure = 0;
#ifdef SAVR_EXPERIMENTAL_AUDIO_48KHZ
    status = "symbol_missing";
    auto* init = static_cast<std::uint8_t*>(
        dlsym(gameHandle, "_ZN16CAEAudioHardware18InitOpenALListenerEv"));
    if (init) {
        auto* words = reinterpret_cast<std::uint32_t*>(init + outputrate::kAttributeOffset);
        status = "fingerprint_mismatch";
        if (outputrate::Matches(words)) {
            // Do not claim to change an already running context. Installation
            // normally happens before the game's JNI/frame entry points bind.
            const bool resolved = Resolve();
            status = resolved ? "context_already_running" : "api_unavailable";
            if (resolved && !api.context()) {
                const long page = sysconf(_SC_PAGESIZE);
                status = "invalid_page_size";
                if (page > 0) {
                    auto* instruction = words + 1;
                    const auto address = reinterpret_cast<std::uintptr_t>(instruction);
                    const auto start = address & ~(static_cast<std::uintptr_t>(page) - 1);
                    status = "write_protection_failed";
                    if (!mprotect(reinterpret_cast<void*>(start), page,
                                  PROT_READ | PROT_WRITE | PROT_EXEC)) {
                        __atomic_store_n(instruction, outputrate::k48kInstruction, __ATOMIC_RELEASE);
                        __builtin___clear_cache(reinterpret_cast<char*>(instruction),
                                                reinterpret_cast<char*>(instruction + 1));
                        outputRatePatched.store(true);
                        const bool restored = !mprotect(reinterpret_cast<void*>(start), page,
                                                        PROT_READ | PROT_EXEC);
                        outputRateRxRestored.store(restored);
                        status = restored ? "patched" : "patched_rx_restore_failed";
                        if (!restored) failure = errno;
                    } else failure = errno;
                }
            }
        }
    }
#else
    (void)gameHandle;
#endif
    if (FILE* log = Open()) {
        std::fprintf(log, "OUTPUT_RATE requested=%d patch=%d rx_restored=%d status=%s errno=%d\n",
            outputrate::kRequestedHz, outputRatePatched.load(), outputRateRxRestored.load(), status, failure);
        std::fclose(log);
    }
    if (outputrate::kRequestedHz == 48000 && (!outputRatePatched.load() || !outputRateRxRestored.load()))
        LOGE("[audio.rate] requested=48000 status=%s errno=%d", status, failure);
}

void Tick(bool focused, bool stereo, bool menu, bool paused) {
    const double now = Now();
    if (lastFrame) {
        const double gap = now - lastFrame;
        maxGap = std::max(maxGap, gap);
        if (gap > .1) ++gaps;
    }
    lastFrame = now;
    ++frames;
    if (lastSample && now - lastSample < 5.) return;
    const double span = lastSample ? now - lastSample : 0.;
    lastSample = now;
    FILE* log = Open();
    if (!log) return;
    std::fprintf(log, "t=%.3f epoch=%lld span=%.3f frames=%u max_frame_gap_ms=%.2f gaps_gt100ms=%u focus=%d stereo=%d menu=%d pause=%d ",
        now, static_cast<long long>(time(nullptr)), span, frames, maxGap * 1000., gaps,
        focused, stereo, menu, paused);
    frames = gaps = 0;
    maxGap = 0.;
    void* ctx = Resolve() ? api.context() : nullptr;
    void* dev = ctx ? api.device(ctx) : nullptr;
    if (!dev) {
        std::fprintf(log, "api=%d context=%p device=%p\n", api.ready, ctx, dev);
        std::fclose(log);
        return;
    }
    int frequency{}, refresh{}, sync{};
    api.deviceInt(dev, 0x1007, 1, &frequency); // ALC_FREQUENCY
    api.deviceInt(dev, 0x1008, 1, &refresh);   // ALC_REFRESH
    api.deviceInt(dev, 0x1009, 1, &sync);      // ALC_SYNC
    float listenerGain{};
    api.listener(0x100a, &listenerGain);       // AL_GAIN
    std::fprintf(log, "context=%p device=%p frequency=%d refresh=%d sync=%d listener_gain=%.5g requested_hz=%d rate_patch=%d rx_restored=%d rate_match=%d\n",
        ctx, dev, frequency, refresh, sync, listenerGain, outputrate::kRequestedHz,
        outputRatePatched.load(), outputRateRxRestored.load(), frequency == outputrate::kRequestedHz);
    static void* describedContext{};
    if (describedContext != ctx) {
        describedContext = ctx;
        std::fprintf(log, "renderer=%s version=%s\n", api.string(0xb003), api.string(0xb002));
    }
    unsigned sources{}, playing{}, stopped{}, streaming{}, drained{}, invalid{}, details{};
    float pitchMin = INFINITY, pitchMax = -INFINITY, gainMax{};
    // OpenAL has no source enumeration API. This is explicitly a bounded ID
    // probe, NOT a complete census. alIsSource accepts unused IDs without error.
    // No alGetError/alcGetError: those would consume the game's pending errors.
    for (unsigned id = 1; id <= 1024; ++id) {
        if (!api.isSource(id)) continue;
        ++sources;
        int state{}, type{}, queued{}, processed{}, looping{}, buffer{};
        float pitch{}, gain{};
        api.sourceInt(id, 0x1010, &state); // AL_SOURCE_STATE
        api.sourceInt(id, 0x1027, &type);  // AL_SOURCE_TYPE
        api.sourceInt(id, 0x1015, &queued);
        api.sourceInt(id, 0x1016, &processed);
        api.sourceInt(id, 0x1007, &looping);
        api.sourceInt(id, 0x1009, &buffer);
        api.sourceFloat(id, 0x1003, &pitch);
        api.sourceFloat(id, 0x100a, &gain);
        playing += state == 0x1012;
        stopped += state == 0x1014;
        streaming += type == 0x1029;
        // Can also be a normal end-of-track, not proof of an underrun.
        const bool drain = type == 0x1029 && queued > 0 && processed >= queued && !looping;
        drained += drain;
        const bool bad = !std::isfinite(pitch) || !std::isfinite(gain);
        invalid += bad;
        if (state == 0x1012) {
            pitchMin = std::min(pitchMin, pitch);
            pitchMax = std::max(pitchMax, pitch);
            gainMax = std::max(gainMax, gain);
        }
        if ((type == 0x1029 || bad || (state == 0x1012 && (pitch < .25f || pitch > 4.f))) && details++ < 8) {
            int hz{}, bits{}, channels{};
            if (buffer > 0) {
                api.bufferInt(buffer, 0x2001, &hz);
                api.bufferInt(buffer, 0x2002, &bits);
                api.bufferInt(buffer, 0x2003, &channels);
            }
            std::fprintf(log, " source=%u state=%x type=%x queued=%d processed=%d loop=%d pitch=%.5g gain=%.5g buffer=%d hz=%d bits=%d channels=%d drained=%d\n",
                id, state, type, queued, processed, looping, pitch, gain, buffer, hz, bits, channels, drain);
        }
    }
    std::fprintf(log, "probe_ids=1..1024 sources=%u playing=%u stopped=%u streaming=%u drained=%u nonfinite=%u playing_pitch_min=%.5g max=%.5g gain_max=%.5g sample_ms=%.3f\n",
        sources, playing, stopped, streaming, drained, invalid,
        playing ? pitchMin : 0.f, playing ? pitchMax : 0.f, gainMax, (Now() - now) * 1000.);
    std::fclose(log);
}
}

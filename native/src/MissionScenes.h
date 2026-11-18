#pragma once

#include <cstdint>

namespace savr::perf {
enum class StereoProjectionOutcome : int;
enum class StereoFallbackSource : int;
}

namespace savr::missionscenes {
void Install(void* handle);
void Update();
bool Theater();

struct Smoke2Presentation {
    std::uint64_t cameraPresence;
    std::uint64_t presenceRejections;
    std::int64_t surfaceTimestampNs;
    int newestStereo;
    int submittedStereo;
    perf::StereoProjectionOutcome outcome;
    perf::StereoFallbackSource fallback;
    int endFrameResult;
    unsigned layers;
    bool theater;
    bool projection;
    bool shouldRender;
    float fullscreenFade;
};

bool Smoke2Tracing();
void TraceSmoke2Presentation(const Smoke2Presentation& frame);
}

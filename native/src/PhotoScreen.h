#pragma once

#include <algorithm>

namespace savr::photoscreen {

constexpr int PreviewWidth = 768;
constexpr int PreviewHeight = 576;
constexpr int CaptureWidth = 1600;
constexpr int CaptureHeight = 1200;
constexpr float Aspect = 4.f / 3.f;
constexpr double ShutterMs = 100.0;
constexpr double ReviewMs = 500.0;
constexpr double ShrinkMs = 125.0;
constexpr double SlideMs = 150.0;

struct Review {
    bool photo{};
    float shutter{}, scale{1.f}, offset{};
};

inline float Ease(double fraction) {
    const float t = static_cast<float>(std::clamp(fraction, 0.0, 1.0));
    return t * t * (3.f - 2.f * t);
}

inline Review Sample(double now, double pressed, double ready, bool rendered) {
    if (pressed < 0) return {};
    const double closed = pressed + ShutterMs;
    if (now < closed || ready < 0)
        return {false, Ease((now - pressed) / ShutterMs)};
    const double elapsed = now - std::max(closed, ready);
    if (elapsed < ShutterMs)
        return {rendered, 1.f - Ease(elapsed / ShutterMs)};
    if (!rendered) return {};
    const double dismissal = elapsed - ShutterMs - ReviewMs;
    if (dismissal < 0) return {.photo = true};
    if (dismissal < ShrinkMs)
        return {.photo = true, .scale = 1.f - .55f * Ease(dismissal / ShrinkMs)};
    if (dismissal < ShrinkMs + SlideMs)
        return {.photo = true, .scale = .45f,
                .offset = 1.45f * Ease((dismissal - ShrinkMs) / SlideMs)};
    return {};
}

} // namespace savr::photoscreen

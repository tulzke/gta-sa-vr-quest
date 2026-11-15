#pragma once

namespace savr::vehiclehorn {
struct Vec { float x{},y{},z{}; };
// Immutable render publication in the rigid hub frame; no tracking/GL dependency.
struct Visual {
    bool attached{},pressed{};
    float weight{};
    Vec point{},fingers{0,1,0}; // point.z is the outward gap
    bool mirrored{};
};
} // namespace savr::vehiclehorn

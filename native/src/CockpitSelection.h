#pragma once
#include <cstddef>

namespace savr::driving {
// Distances outside an object's reach must be set to the sentinel by the caller.
template<std::size_t N>
inline int SelectCockpitTarget(const float (&distances)[N], int previous) {
    int best=0;
    for (int i=1;i<static_cast<int>(N);++i) if (distances[i]<distances[best]) best=i;
    if (previous>0&&previous<static_cast<int>(N)&&distances[previous]<1e6f&&
        distances[previous]<=distances[best]+.015f) return previous;
    return best;
}
}

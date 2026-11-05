#pragma once

namespace savr::driving {
// Distances outside an object's reach must be set to the sentinel by the caller.
inline int SelectCockpitTarget(const float distances[5], int previous) {
    int best=0;
    for (int i=1;i<=4;++i) if (distances[i]<distances[best]) best=i;
    if (previous>0&&previous<=4&&distances[previous]<1e6f&&
        distances[previous]<=distances[best]+.015f) return previous;
    return best;
}
}

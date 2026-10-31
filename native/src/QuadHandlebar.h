#pragma once
#include <cmath>

namespace savr::quadbar {
// CQuadBike::PreRender replaces the node rotation with SetRotateZ while
// preserving translation. Replace that rotation, never accumulate it.
inline bool SetRotation(float* matrix,float angle) {
    if (!matrix||!std::isfinite(angle)) return false;
    const float c=std::cos(angle),s=std::sin(angle);
    matrix[0]=c; matrix[1]=s; matrix[2]=0.0f;
    matrix[4]=-s; matrix[5]=c; matrix[6]=0.0f;
    matrix[8]=0.0f; matrix[9]=0.0f; matrix[10]=1.0f;
    return true;
}
}

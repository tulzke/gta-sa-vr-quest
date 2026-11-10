#pragma once

#include <cstddef>
#include <cstdint>

namespace savr::streamingbuffer {
// Init2 allocates one aligned block, splits it in half and stores the half
// size in sectors. Shutdown frees only buffers[0] with FreeAlign. Grow using
// that same ownership/layout, after FlushChannels has consumed pending reads.
inline bool Ensure(int requiredSectors, std::int32_t* halfSectors,
                   std::uint8_t** buffers,
                   void* (*allocate)(std::size_t, std::size_t),
                   void (*release)(void*), void (*flush)()) {
    if (!halfSectors || *halfSectors <= 0 || requiredSectors <= 0) return false;
    if (requiredSectors <= 2LL * *halfSectors) return true;
    // Bound optional payload memory to 8 MiB. Unknown layouts retain stock.
    if (requiredSectors > 4096 || !buffers || !buffers[0] || !allocate ||
        !release || !flush || buffers[1] != buffers[0] + std::size_t(*halfSectors)*2048)
        return false;
    const int nextHalf=(requiredSectors+1)/2;
    auto* next=static_cast<std::uint8_t*>(allocate(std::size_t(nextHalf)*4096,2048));
    if (!next) return false;
    flush();
    auto* old=buffers[0];
    buffers[0]=next;
    buffers[1]=next+std::size_t(nextHalf)*2048;
    *halfSectors=nextHalf;
    release(old);
    return true;
}
} // namespace savr::streamingbuffer

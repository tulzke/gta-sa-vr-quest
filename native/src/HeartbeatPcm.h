#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace savr::heartbeataudio::detail {
struct Pcm { std::vector<std::int16_t> samples; int rate{}; };
inline unsigned U16(const unsigned char* p) { return p[0]|(unsigned(p[1])<<8); }
inline std::uint32_t U32(const unsigned char* p) { return U16(p)|(std::uint32_t(U16(p+2))<<16); }
// Decode the attributed PCM24 stereo original without replacing the source asset.
// Chunk walking handles the recording's BWF metadata and RIFF padding.
inline Pcm Decode(const unsigned char* wav,std::size_t size) {
    if (size<12 || std::memcmp(wav,"RIFF",4) || std::memcmp(wav+8,"WAVE",4)) return {};
    unsigned channels{},bits{},format{},rate{},align{};
    const unsigned char* data{};std::size_t bytes{};
    for (std::size_t p=12;p+8<=size;) {
        const auto n=U32(wav+p+4);
        if (n>size-p-8) return {};
        const auto* chunk=wav+p+8;
        if (!std::memcmp(wav+p,"fmt ",4) && n>=16) {
            format=U16(chunk);channels=U16(chunk+2);rate=U32(chunk+4);
            align=U16(chunk+12);bits=U16(chunk+14);
        } else if (!std::memcmp(wav+p,"data",4)) { data=chunk;bytes=n; }
        p+=8+std::size_t(n)+(n&1);
    }
    if (format!=1 || channels!=2 || bits!=24 || align!=6 || rate<8000 || rate>192000 ||
        !data || bytes%6 || bytes<6 || bytes/6>rate*60u) return {};
    Pcm result;result.rate=static_cast<int>(rate);result.samples.resize(bytes/6);
    for (std::size_t i=0;i<result.samples.size();++i) {
        const auto sample=[](const unsigned char* p) {
            const int value=p[0]|(int(p[1])<<8)|(int(p[2])<<16);
            return value>=0x800000?value-0x1000000:value;
        };
        const auto* p=data+i*6;
        const int mono=(sample(p)+sample(p+3))/512;
        // Short edge ramps avoid a click when looping the 5.59 s recording.
        const std::size_t ramp=std::min<std::size_t>(rate/100,result.samples.size()/2);
        const std::size_t edge=std::min(i,result.samples.size()-1-i);
        result.samples[i]=static_cast<std::int16_t>(ramp && edge<ramp?mono*int(edge)/int(ramp):mono);
    }
    return result;
}
} // namespace savr::heartbeataudio::detail

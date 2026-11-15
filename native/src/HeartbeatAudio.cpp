#include "HeartbeatAudio.h"
#include "HeartbeatPcm.h"
#include <cmath>
#include <dlfcn.h>

extern "C" {
extern const unsigned char savr_heartbeat_wav[],savr_heartbeat_wav_end[];
}

namespace savr::heartbeataudio {
namespace {
// Use the game's OpenAL context and listener volume, never a second audio device.
struct Api {
    void* (*context)(){};
    void (*genSources)(int,unsigned*){};
    void (*genBuffers)(int,unsigned*){};
    void (*bufferData)(unsigned,int,const void*,int,int){};
    void (*sourcei)(unsigned,int,int){};
    void (*sourcef)(unsigned,int,float){};
    void (*source3f)(unsigned,int,float,float,float){};
    void (*getSourcei)(unsigned,int,int*){};
    void (*play)(unsigned){};
    void (*stop)(unsigned){};
    char (*isSource)(unsigned){};
    char (*isBuffer)(unsigned){};
    bool ready{};
} api;
void* ownerContext{};
unsigned source{},buffer{};
bool playing{};
detail::Pcm EmbeddedPcm() {
    return detail::Decode(savr_heartbeat_wav,
        static_cast<std::size_t>(savr_heartbeat_wav_end-savr_heartbeat_wav));
}
bool Resolve() {
    if (api.ready) return true;
    void* lib=dlopen("libopenal.so",RTLD_NOW|RTLD_NOLOAD);
    if (!lib) return false;
#define LOAD(field,name) api.field=reinterpret_cast<decltype(api.field)>(dlsym(lib,name))
    LOAD(context,"alcGetCurrentContext");LOAD(genSources,"alGenSources");
    LOAD(genBuffers,"alGenBuffers");LOAD(bufferData,"alBufferData");
    LOAD(sourcei,"alSourcei");LOAD(sourcef,"alSourcef");LOAD(source3f,"alSource3f");
    LOAD(getSourcei,"alGetSourcei");LOAD(play,"alSourcePlay");LOAD(stop,"alSourceStop");
    LOAD(isSource,"alIsSource");LOAD(isBuffer,"alIsBuffer");
#undef LOAD
    api.ready=api.context && api.genSources && api.genBuffers && api.bufferData &&
        api.sourcei && api.sourcef && api.source3f && api.getSourcei && api.play &&
        api.stop && api.isSource && api.isBuffer;
    dlclose(lib);return api.ready;
}
}
void Update(float severity) {
    severity=std::isfinite(severity)?std::clamp(severity,0.f,1.f):0;
    if (!api.ready && severity<=0) return;
    if (!Resolve()) return;
    void* ctx=api.context();
    if (!ctx) return; // The game owns context activation/destruction.
    if (ctx!=ownerContext) {
        // IDs belong to the old context; never delete/stop them in another one.
        ownerContext=ctx;source=buffer=0;playing=false;
    }
    if (source && !api.isSource(source)) { source=0;playing=false; }
    if (buffer && !api.isBuffer(buffer)) buffer=0;
    if (severity<=0) {
        if (source && playing) { api.stop(source);playing=false; }
        return;
    }
    static const auto pcm=EmbeddedPcm();
    if (pcm.samples.empty()) return;
    if (!buffer) {
        api.genBuffers(1,&buffer);
        if (!buffer || !api.isBuffer(buffer)) { buffer=0;return; }
        api.bufferData(buffer,0x1101,pcm.samples.data(),
            static_cast<int>(pcm.samples.size()*sizeof(std::int16_t)),pcm.rate); // AL_FORMAT_MONO16
    }
    if (!source) {
        api.genSources(1,&source);
        if (!source || !api.isSource(source)) { source=0;return; }
        playing=false;
        api.sourcei(source,0x1009,static_cast<int>(buffer)); // AL_BUFFER
        api.sourcei(source,0x202,1); // AL_SOURCE_RELATIVE
        api.source3f(source,0x1004,0,0,0); // AL_POSITION: centred inside the head
        api.sourcef(source,0x1021,0); // AL_ROLLOFF_FACTOR
        api.sourcei(source,0x1007,1); // AL_LOOPING
    }
    api.sourcef(source,0x100a,.18f+.22f*severity); // AL_GAIN; listener gain still applies
    api.sourcef(source,0x1003,1.f); // Preserve the chosen recording's rhythm/pitch.
    int status{};api.getSourcei(source,0x1010,&status); // AL_SOURCE_STATE
    if (status!=0x1012) api.play(source); // AL_PLAYING; resumes after game audio reset
    playing=true;
}
} // namespace savr::heartbeataudio

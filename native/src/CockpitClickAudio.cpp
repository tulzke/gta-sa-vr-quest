#include "CockpitClickAudio.h"
#include "CockpitClickPcm.h"
#include <dlfcn.h>

namespace savr::cockpitclick {
namespace {
struct Api {
    void* (*context)(){};
    void (*genSources)(int,unsigned*){};
    void (*genBuffers)(int,unsigned*){};
    void (*bufferData)(unsigned,int,const void*,int,int){};
    void (*sourcei)(unsigned,int,int){};
    void (*sourcef)(unsigned,int,float){};
    void (*source3f)(unsigned,int,float,float,float){};
    void (*play)(unsigned){};
    void (*stop)(unsigned){};
    char (*isSource)(unsigned){};
    char (*isBuffer)(unsigned){};
    bool ready{};
} api;
void* owner{};
unsigned source{},buffer{};
bool Resolve(){
    if(api.ready)return true;
    void* lib=dlopen("libopenal.so",RTLD_NOW|RTLD_NOLOAD);
    if(!lib)return false;
#define LOAD(field,name) api.field=reinterpret_cast<decltype(api.field)>(dlsym(lib,name))
    LOAD(context,"alcGetCurrentContext");LOAD(genSources,"alGenSources");LOAD(genBuffers,"alGenBuffers");
    LOAD(bufferData,"alBufferData");LOAD(sourcei,"alSourcei");LOAD(sourcef,"alSourcef");
    LOAD(source3f,"alSource3f");LOAD(play,"alSourcePlay");LOAD(stop,"alSourceStop");
    LOAD(isSource,"alIsSource");LOAD(isBuffer,"alIsBuffer");
#undef LOAD
    api.ready=api.context&&api.genSources&&api.genBuffers&&api.bufferData&&api.sourcei&&api.sourcef&&
        api.source3f&&api.play&&api.stop&&api.isSource&&api.isBuffer;
    dlclose(lib);return api.ready;
}
}
void Click(bool on){
    if(!Resolve())return;
    void* ctx=api.context();if(!ctx)return;
    // Never operate on identifiers from a destroyed/replaced game context.
    if(ctx!=owner){owner=ctx;source=buffer=0;}
    if(source&&!api.isSource(source))source=0;
    if(buffer&&!api.isBuffer(buffer))buffer=0;
    if(!buffer){
        api.genBuffers(1,&buffer);if(!buffer||!api.isBuffer(buffer)){buffer=0;return;}
        static const auto pcm=MechanicalClickPcm();
        api.bufferData(buffer,0x1101,pcm.data(),static_cast<int>(pcm.size()*sizeof(pcm[0])),kClickRate); // MONO16
    }
    if(!source){
        api.genSources(1,&source);if(!source||!api.isSource(source)){source=0;return;}
        api.sourcei(source,0x202,1); // AL_SOURCE_RELATIVE
        api.source3f(source,0x1004,0,0,0);
        api.sourcef(source,0x1021,0); // AL_ROLLOFF_FACTOR
        api.sourcei(source,0x1007,0); // AL_LOOPING
    }
    api.stop(source);
    api.sourcei(source,0x1009,static_cast<int>(buffer));
    api.sourcef(source,0x100a,.75f); // AL_GAIN; game listener/master gain still applies
    api.sourcef(source,0x1003,on?1.f:.86f);
    api.play(source);
}
}

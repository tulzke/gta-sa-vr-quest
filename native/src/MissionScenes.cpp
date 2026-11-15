// Retail mission camera policy and post-cutscene streaming service.
#include "MissionScenes.h"
#include "Symbols.h"
#include "ModelSets.h"
#include "Log.h"
#include <atomic>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <chrono>
#include <cstdarg>
#include <dlfcn.h>
namespace savr::missionscenes {
namespace {
void** scripts{};
void (*loadAll)(bool){};
int (*animIndex)(const char*){};
void (*suspendTimer)(){};
void (*resumeTimer)(){};
bool servicing{};
bool (*canStartMission)(void*){};
std::atomic<bool> theater{};
template<class T>T Read(const void* p,unsigned off=0) {
    T v{};std::memcpy(&v,static_cast<const std::uint8_t*>(p)+off,sizeof(v));return v;
}
void Trace(const char* event,int a=0,int b=0,int c=0) {
    LOGI("[mission.scene] %s %d %d %d",event,a,b,c);
    if(FILE* f=std::fopen("/sdcard/Android/data/com.rockstargames.gtasa/files/vr_mission_scenes.log","a")) {
        std::fprintf(f,"%u %s %d %d %d\n",g.CTimer_m_snTimeInMilliseconds?*g.CTimer_m_snTimeInMilliseconds:0,event,a,b,c);
        std::fclose(f);
    }
}
bool Name(void* s,const char* name,bool missionOnly=true) {
    if(missionOnly && !Read<std::uint8_t>(s,0x12c))return false;
    const auto* n=static_cast<const unsigned char*>(s)+0x10;
    unsigned i=0;for(;name[i];++i)if((n[i]>='A'&&n[i]<='Z'?n[i]+32:n[i])!=name[i])return false;
    return n[i]==0;
}
struct Script {
    void* thread{};std::uint8_t* base{};bool valid{};
    bool Check(void* s,unsigned size,std::uint32_t expected) {
        auto* bytes=s?Read<std::uint8_t*>(s,0x18):nullptr;
        if(s!=thread || bytes!=base) {
            thread=s;base=bytes;valid=false;
            if(base) {
                std::uint32_t hash=2166136261u;
                for(unsigned i=0;i<size;++i)hash=(hash^base[i])*16777619u;
                valid=hash==expected;Trace("BYTECODE",expected,valid);
            }
        }
        return valid;
    }
    int Ip() const {
        if(!valid)return -1;
        const auto current=reinterpret_cast<std::uintptr_t>(Read<void*>(thread,0x20));
        const auto begin=reinterpret_cast<std::uintptr_t>(base);
        return current>=begin && current<begin+42218?static_cast<int>(current-begin):-1;
    }
} mansion,burning;
// SWEET5's phone scene stages CJ in Seville at IP 1527, then returns him
// to Grove Street at 3103. Observe its dialogue gates; never advance them.
Script sweet;
std::uint8_t* missionLocals{};
void* audioEngine{};
unsigned char (*audioLoading)(void*,unsigned char){};
bool (*audioFinished)(void*,unsigned char){};
int (*audioEvent)(void*,unsigned char){};
void* (*getPed)(int){};
bool (*pedTalking)(void*){};
constexpr char kSweetLog[]="/sdcard/Android/data/com.rockstargames.gtasa/files/vr_sweet5.log";
constexpr char kSweetPrevious[]="/sdcard/Android/data/com.rockstargames.gtasa/files/vr_sweet5.previous.log";
double SweetNow() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void SweetTrace(const char* format,...) {
    char line[1024]{};
    va_list args;va_start(args,format);std::vsnprintf(line,sizeof(line),format,args);va_end(args);
    LOGI("[sweet5] %s",line);
    if(FILE* f=std::fopen(kSweetLog,"a")) {
        // Bound persistent evidence even if the headset sits in the scene.
        if(std::fseek(f,0,SEEK_END)==0 && std::ftell(f)>=512*1024) {
            std::fclose(f);std::rename(kSweetLog,kSweetPrevious);f=std::fopen(kSweetLog,"w");
        }
        if(f) {
            std::fprintf(f,"game=%u mono=%.3f %s\n",
                g.CTimer_m_snTimeInMilliseconds?*g.CTimer_m_snTimeInMilliseconds:0,SweetNow(),line);
            std::fclose(f);
        }
    }
}
void TraceSweet(void* s) {
    static bool present=false;
    static int lastStage=-1,lastScene=-1,lastLine=-1,lastAudio=-1;
    static double last=0;
    if(!s) {
        if(present)SweetTrace("END");
        present=false;sweet.Check(nullptr,15739,0x66d80caau);return;
    }
    if(!present || s!=sweet.thread || Read<std::uint8_t*>(s,0x18)!=sweet.base) {
        std::rename(kSweetLog,kSweetPrevious);
        lastStage=lastScene=lastLine=lastAudio=-1;last=0;
        SweetTrace("BEGIN v1 locals=%d audio=%d/%d/%d ped=%d/%d",
            missionLocals!=nullptr,audioEngine&&audioLoading,audioEngine&&audioFinished,
            audioEngine&&audioEvent,getPed!=nullptr,pedTalking!=nullptr);
    }
    present=true;
    if(!sweet.Check(s,15739,0x66d80caau)) {
        if(last==0){SweetTrace("UNSUPPORTED BYTECODE expected=66d80caa");last=SweetNow();}
        return;
    }
    if(!missionLocals)return;
    auto local=[](unsigned i){return Read<int>(missionLocals,i*4);};
    const int stage=local(135),scene=local(156),line=local(360),audio=local(363);
    if(stage>1 && stage==lastStage)return;
    const bool changed=stage!=lastStage||scene!=lastScene||line!=lastLine||audio!=lastAudio;
    const double now=SweetNow();
    // Sample the opening through its return to gameplay. Wall time keeps the
    // heartbeat alive when a pause or a stopped game timer is the actual gate.
    if(!changed && (stage!=1 || now-last<2.0))return;
    last=now;lastStage=stage;lastScene=scene;lastLine=line;lastAudio=audio;
    const int ip=sweet.Ip();
    SweetTrace("STATE ip=%d stage=%d scene=%d line=%d audio_phase=%d timer=%d scene_due=%d audio_due=%d next=%d remaining=%d buffers=%d/%d slot=%d speech_gate=%d speaker=%d",
        ip>=0&&ip<15739?ip:-1,stage,scene,line,audio,local(40),local(43),local(364),
        local(361),local(362),local(357),local(358),local(359),local(412),local(414));
    const auto pos=g.FindPlayerCoors?g.FindPlayerCoors(-1):GameSymbols::Vec3{};
    void* pad=g.CPad_GetPad?g.CPad_GetPad(0):nullptr;
    SweetTrace("WORLD pos=%.3f/%.3f/%.3f controls=%d wide=%d fade=%d cutscene=%d pause=%d/%d",
        pos.x,pos.y,pos.z,pad?Read<std::uint16_t>(pad,0x110):-1,
        g.TheCamera?Read<std::uint8_t>(g.TheCamera,0x43):-1,
        g.TheCamera&&g.CCamera_GetScreenFadeStatus?g.CCamera_GetScreenFadeStatus(g.TheCamera):-1,
        g.CCutsceneMgr_ms_running?*g.CCutsceneMgr_ms_running:-1,
        g.CTimer_m_UserPause?*g.CTimer_m_UserPause:-1,g.CTimer_m_CodePause?*g.CTimer_m_CodePause:-1);
    // SCM slots 1/2 map to native slots 0/1 (03D0/03D2 subtract one).
    for(unsigned char slot=0;slot<2;++slot)
        SweetTrace("AUDIO slot=%d event=%d loading=%d finished=%d",slot+1,
            audioEngine&&audioEvent?audioEvent(audioEngine,slot):-1,
            audioEngine&&audioLoading?audioLoading(audioEngine,slot):-1,
            audioEngine&&audioFinished?audioFinished(audioEngine,slot):-1);
    for(unsigned i:{83u,51u,414u}) {
        const int handle=local(i);void* ped=getPed&&handle>=0?getPed(handle):nullptr;
        SweetTrace("ACTOR local=%u handle=%d exists=%d talking=%d",i,handle,ped!=nullptr,
            ped&&pedTalking?pedTalking(ped):-1);
    }
}
int lastPhase{},attempts{};
void TraceKingMarker(void* cat) {
    auto* space=static_cast<unsigned char*>(g.CTheScripts_ScriptSpace);
    if(!space || !g.FindPlayerCoors || !g.FindPlayerPed || !canStartMission)return;
    const auto now=g.CTimer_m_snTimeInMilliseconds?*g.CTimer_m_snTimeInMilliseconds:0;
    static std::uint32_t last=0;
    if(!now || (now>=last && now-last<2000))return;
    void* ped=g.FindPlayerPed(-1);if(!ped)return;
    last=now;
    static const unsigned char* checked=nullptr;
    static bool verified=false;
    // ScriptSpace is allocated before the SCM/save is loaded. A failed early
    // fingerprint must not be cached forever at that same address.
    if(checked!=space || !verified) {
        checked=space;std::uint32_t hash=2166136261u;
        for(unsigned i=72537;i<73178;++i)hash=(hash^space[i])*16777619u;
        verified=hash==0x94fb56ebu;
        Trace("KING CODE",static_cast<int>(hash),verified,cat!=nullptr);
    }
    if(!verified)return;
    const auto p=g.FindPlayerCoors(-1);
    const float dx=p.x-Read<float>(space,0x7b4),dy=p.y-Read<float>(space,0x7b8),
                dz=p.z-Read<float>(space,0x7bc);
    if(!std::isfinite(dx)||!std::isfinite(dy)||!std::isfinite(dz)||
       std::abs(dx)>20000 || std::abs(dy)>20000 || std::abs(dz)>20000){
        Trace("KING BAD COORD");return;
    }
    const auto ip=cat?reinterpret_cast<std::uintptr_t>(Read<void*>(cat,0x20)):0;
    const auto base=reinterpret_cast<std::uintptr_t>(space);
    const int offset=ip>=base && ip<base+262245?static_cast<int>(ip-base):-1;
    const bool ready=canStartMission(ped);
    // Repair only the legacy, confirmed Body Harvest debug-save combination:
    // CATCUT has really passed, yet First Date is still zero and CAT's caller
    // stage was left pending. Normal progression never needs this migration.
    // The exact retail CAT bytecode was verified above; never run during a
    // mission or before CAT reaches its native idle loop.
    const int stage=Read<int>(space,0xb38);
    if(ready && offset==72559 && Read<int>(space,0x660)==0 &&
       Read<int>(space,0x1818)==32 && Read<int>(space,0x17f0)==84 &&
       Read<int>(space,0x7a8)==1 && Read<int>(space,0x3778)==1 &&
       Read<int>(space,0xfc)==0 && (stage==1 || stage==2)) {
        const int nextDate=1,finishedStage=2;
        std::memcpy(space+0xfc,&nextDate,sizeof(nextDate));
        std::memcpy(space+0xb38,&finishedStage,sizeof(finishedStage));
        Trace("CATALINA DEBUG REPAIR",stage,0,nextDate);
    }
    Trace("KING FLAGS",Read<int>(space,0x7a8),Read<int>(space,0xb38),Read<int>(space,0x3778));
    Trace("KING DIST CM",static_cast<int>(dx*100),static_cast<int>(dy*100),static_cast<int>(dz*100));
    Trace("KING GATES",ready,g.FindPlayerVehicle&&g.FindPlayerVehicle(-1,false)!=nullptr,offset);
    Trace("KING STORY",Read<int>(space,0x660),Read<int>(space,0xfc));
}
std::uint32_t entered{},lastTrace{};
int WaitPhase(int ip) {
    if(ip>=39436 && ip<39468)return 1;
    if(ip>=39468 && ip<39507)return 2;
    if(ip>=39507 && ip<39544)return 3;
    if(ip>=39544 && ip<39586)return 4;
    if(ip>=39586 && ip<39630)return 5;
    if(ip>=39641 && ip<39676)return 6;
    return 0;
}
void ServiceBurning() {
    const int ip=burning.Ip();
    const bool faded=burning.valid && g.TheCamera && g.CCamera_GetScreenFadeStatus &&
        g.CCamera_GetScreenFadeStatus(g.TheCamera)==2;
    const int phase=faded?WaitPhase(ip):0;
    const auto now=g.CTimer_m_snTimeInMilliseconds?*g.CTimer_m_snTimeInMilliseconds:0;
    if(phase!=lastPhase) {lastPhase=phase;attempts=0;entered=now;lastTrace=now;Trace("BURNING PHASE",phase,ip);}
    if(servicing || !faded || !now || (g.CTimer_m_UserPause && *g.CTimer_m_UserPause) ||
       (g.CTimer_m_CodePause && *g.CTimer_m_CodePause))return;
    if(now-lastTrace<2000)return;
    lastTrace=now;
    Trace("BURNING FADE",phase,ip,g.CStreaming_ms_numModelsRequested?*g.CStreaming_ms_numModelsRequested:-1);
    if(!phase || !g.CStreaming_ms_aInfoForModel)return;
    int models[4]{-1,-1,-1,-1};
    if(phase==1){models[0]=412;models[1]=596;}
    if(phase==2){models[0]=108;models[1]=109;models[2]=110;models[3]=195;}
    if(phase==3){models[0]=346;models[1]=344;models[2]=366;}
    if(phase==4){models[0]=3098;models[1]=3097;models[2]=3099;models[3]=3127;}
    if(phase==5){models[0]=3013;models[1]=3064;models[2]=2950;models[3]=3063;}
    if(phase==6 && animIndex) {const int index=animIndex("BD_FIRE");if(index>=0 && index<180)models[0]=25575+index;}
    // Loading after the script's LOAD_ALL_MODELS_NOW can leave dependencies
    // requested by converted HD clumps queued while the fully opaque fade
    // suppresses RenderScene. Service the native queue at this exact wait only.
    // Never skip the script, force loaded flags, remove models, or force a fade.
    const bool hd=modelsets::Applied(modelsets::PROPER_FIXES) ||
        modelsets::Applied(modelsets::HD_WEAPONS);
    const bool retry=hd && attempts<3 && now-entered>=2000 && loadAll && suspendTimer && resumeTimer && g.CStreaming_RequestModel &&
        (!g.CStreaming_ms_disableStreaming || !*g.CStreaming_ms_disableStreaming);
    bool queued=false;
    for(int id:models) {
        if(id<0)continue;
        auto* info=g.CStreaming_ms_aInfoForModel+id*0x14;
        const int state=Read<std::uint32_t>(info,0x10);
        const auto sectors=Read<std::uint32_t>(info,0xc);
        Trace("BURNING MODEL",id,state,sectors);
        Trace("BURNING SOURCE",id,info[7],Read<std::uint32_t>(info,8));
        // A missing directory entry needs an asset fix, not repeated requests.
        if(retry && sectors && state!=1){g.CStreaming_RequestModel(id,0xc);queued=true;}
    }
    if(queued){
        ++attempts;servicing=true;suspendTimer();loadAll(false);resumeTimer();servicing=false;
        Trace("BURNING SERVICE",phase,attempts);
    }
}
}
void Install(void* h) {
    if(!h)return;
    scripts=reinterpret_cast<void**>(dlsym(h,"_ZN11CTheScripts14pActiveScriptsE"));
    loadAll=reinterpret_cast<decltype(loadAll)>(dlsym(h,"_ZN10CStreaming22LoadAllRequestedModelsEb"));
    animIndex=reinterpret_cast<decltype(animIndex)>(dlsym(h,"_ZN12CAnimManager22GetAnimationBlockIndexEPKc"));
    suspendTimer=reinterpret_cast<decltype(suspendTimer)>(dlsym(h,"_ZN6CTimer7SuspendEv"));
    resumeTimer=reinterpret_cast<decltype(resumeTimer)>(dlsym(h,"_ZN6CTimer6ResumeEv"));
    canStartMission=reinterpret_cast<decltype(canStartMission)>(dlsym(h,"_ZN10CPlayerPed21CanPlayerStartMissionEv"));
    missionLocals=static_cast<std::uint8_t*>(dlsym(h,"_ZN11CTheScripts31LocalVariablesForCurrentMissionE"));
    audioEngine=dlsym(h,"AudioEngine");
    audioLoading=reinterpret_cast<decltype(audioLoading)>(dlsym(h,"_ZN12CAudioEngine28GetMissionAudioLoadingStatusEh"));
    audioFinished=reinterpret_cast<decltype(audioFinished)>(dlsym(h,"_ZN12CAudioEngine28IsMissionAudioSampleFinishedEh"));
    audioEvent=reinterpret_cast<decltype(audioEvent)>(dlsym(h,"_ZN12CAudioEngine20GetMissionAudioEventEh"));
    getPed=reinterpret_cast<decltype(getPed)>(dlsym(h,"_ZN6CPools6GetPedEi"));
    pedTalking=reinterpret_cast<decltype(pedTalking)>(dlsym(h,"_ZN4CPed13GetPedTalkingEv"));
    Trace("INSTALLED",scripts!=nullptr,loadAll!=nullptr,animIndex!=nullptr);
    Trace("KING DIAG V2",canStartMission!=nullptr,g.FindPlayerCoors!=nullptr,g.CTheScripts_ScriptSpace!=nullptr);
}
bool Theater(){return theater.load(std::memory_order_relaxed);}
void Update() {
    if(!scripts)return;
    void* m=nullptr;void* b=nullptr;void* cat=nullptr;void* sw=nullptr;unsigned count=0;
    for(void* s=*scripts;s && count++<128;) {
        if(Name(s,"strap2"))m=s;
        if(Name(s,"crash1"))b=s;
        if(Name(s,"cat",false))cat=s;
        if(Name(s,"sweet5"))sw=s;
        void* next=Read<void*>(s);if(next==s)break;s=next;
    }
    const bool verified=mansion.Check(m,10840,0x5861cb0cu);
    const bool next=verified && g.TheCamera && Read<std::uint8_t>(g.TheCamera,0x43)!=0;
    if(theater.exchange(next)!=next)Trace("MANSION THEATER",next);
    burning.Check(b,42218,0xfc3eeacfu);
    ServiceBurning();
    TraceKingMarker(cat);
    TraceSweet(sw);
}
}

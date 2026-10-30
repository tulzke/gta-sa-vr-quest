#include "MissionWeaponGrant.h"
#include "MissionWeaponPolicy.h"
#include "Log.h"
#include "Symbols.h"
#include <atomic>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>

namespace savr::missionweapon {
namespace {
using Process = int (*)(void*, int);
Process original400{}, original1300{};
bool (*onMission)(){};
std::atomic<int> pending{-1};

int ProcessAndObserve(Process fn, void* script, int opcode) {
    void* ped = onMission && onMission() && g.FindPlayerPed ? g.FindPlayerPed(-1) : nullptr;
    WeaponSnapshot before[13]{};
    if (ped) for (int i=1;i<13;++i)
        std::memcpy(&before[i],static_cast<char*>(ped)+0x730+i*0x20,sizeof(WeaponSnapshot));
    const int result=fn(script,opcode);
    if (ped && g.FindPlayerPed && g.FindPlayerPed(-1)==ped) {
        WeaponSnapshot after[13]{};
        for (int i=1;i<13;++i)
            std::memcpy(&after[i],static_cast<char*>(ped)+0x730+i*0x20,sizeof(WeaponSnapshot));
        const int slot=GrantedSlot(before,after);
        if (slot>=0) pending.store(slot,std::memory_order_release);
    }
    return result;
}
int On400(void* s,int op) { return ProcessAndObserve(original400,s,op); }
int On1300(void* s,int op) { return ProcessAndObserve(original1300,s,op); }

bool Hook(void* target,void* replacement,Process& original,const std::uint32_t expected[4]) {
    if (!target || std::memcmp(target,expected,16)) return false;
    const long page=sysconf(_SC_PAGESIZE);
    if (page<=0) return false;
    void* trampoline=mmap(nullptr,page,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if (trampoline==MAP_FAILED) return false;
    auto* code=static_cast<std::uint32_t*>(target); auto* t=static_cast<std::uint32_t*>(trampoline);
    std::memcpy(t,code,16); t[4]=0x58000051; t[5]=0xd61f0220;
    const auto resume=reinterpret_cast<std::uintptr_t>(target)+16; std::memcpy(t+6,&resume,8);
    __builtin___clear_cache(reinterpret_cast<char*>(t),reinterpret_cast<char*>(t)+32);
    const auto address=reinterpret_cast<std::uintptr_t>(target);
    const auto start=address&~(static_cast<std::uintptr_t>(page)-1);
    const auto end=(address+16+page-1)&~(static_cast<std::uintptr_t>(page)-1);
    if (mprotect(reinterpret_cast<void*>(start),end-start,PROT_READ|PROT_WRITE|PROT_EXEC)) {
        munmap(trampoline,page); return false;
    }
    original=reinterpret_cast<Process>(trampoline);
    const auto hook=reinterpret_cast<std::uintptr_t>(replacement);
    std::memcpy(code+2,&hook,8); code[1]=0xd61f0220;
    __atomic_store_n(code,0x58000051,__ATOMIC_RELEASE);
    __builtin___clear_cache(reinterpret_cast<char*>(code),reinterpret_cast<char*>(code)+16);
    return true;
}
}
void Install(void* handle) {
    if (!handle || original400 || original1300) return;
    onMission=reinterpret_cast<bool (*)()>(dlsym(handle,"_ZN11CTheScripts18IsPlayerOnAMissionEv"));
    if (!onMission || !g.FindPlayerPed) return;
    constexpr std::uint32_t p400[]={0xd10303ff,0xfd003bea,0x6d07a3e9,0xa908fbfd};
    constexpr std::uint32_t p1300[]={0xd10303ff,0xfd002bea,0x6d0623e9,0xa9077bfd};
    const bool first=Hook(dlsym(handle,"_ZN14CRunningScript23ProcessCommands400To499Ei"),reinterpret_cast<void*>(&On400),original400,p400);
    const bool second=Hook(dlsym(handle,"_ZN14CRunningScript25ProcessCommands1300To1399Ei"),reinterpret_cast<void*>(&On1300),original1300,p1300);
    LOGI("[missionweapon] script grants 400=%d 1300=%d",first,second);
}
int TakePendingSlot() { return pending.exchange(-1,std::memory_order_acq_rel); }
}

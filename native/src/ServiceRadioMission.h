#pragma once
#include "ServiceRadio.h"
#include <cstring>
namespace savr::serviceradio {
inline bool RetailReady(const void* space,void** scripts){
    const auto* p=static_cast<const std::uint8_t*>(space);
    if(!p||!scripts)return false;
    for(unsigned at:{83951u,84214u,84473u,84719u})
        if(p[at]!=0x52||p[at+1]!=0x0a||p[at+2]!=4||p[at+3]!=70)return false;
    return true;
}
inline Mission ReadMission(const void* space,void** scripts){
    Mission result{};result.known=RetailReady(space,scripts);
    if(!result.known)return result;
    int on{};std::memcpy(&on,static_cast<const char*>(space)+0x660,4);
    result.onMission=on!=0;
    void* script=*scripts;
    for(int n=0;script&&n<128;++n){
        const auto* b=static_cast<const unsigned char*>(script);
        if(b[0x12c]){
            char name[9]{};for(int i=0;i<8;++i){const char c=b[0x10+i];name[i]=c>='A'&&c<='Z'?c+32:c;}
            if(std::strcmp(name,"copcar")==0)result.active=Service::Police;
            else if(std::strcmp(name,"ambulan")==0)result.active=Service::Medic;
            else if(std::strcmp(name,"firetru")==0)result.active=Service::Fire;
            if(result.active!=Service::None){result.token=script;break;}
        }
        std::memcpy(&script,b,sizeof(script));
    }
    return result;
}
}

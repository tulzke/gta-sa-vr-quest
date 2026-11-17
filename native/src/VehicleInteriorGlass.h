#pragma once
#include <array>
#include <cstdint>
#include <cstring>
namespace savr::interiorglass {
inline bool WindowAtomic(const char* name){return name&&(std::strstr(name,"windscreen")||std::strstr(name,"window")||std::strstr(name,"glass"));}
inline bool WindowMaterial(const char* texture,std::uint8_t alpha,std::uint8_t r,std::uint8_t g,std::uint8_t b){
 std::array<char,33> name{};if(texture)for(int i=0;i<32&&texture[i];++i){const char c=texture[i];name[i]=c>='A'&&c<='Z'?c-'A'+'a':c;}
 if(std::strstr(name.data(),"glass")||std::strstr(name.data(),"window")||std::strstr(name.data(),"windscreen")||std::strstr(name.data(),"vehicleshatter"))return true;
 if(alpha==0||alpha>=250)return false;
 if(std::strstr(name.data(),"vehiclegeneric"))return true;
 return name[0]==0&&alpha<=200&&r<16&&g<16&&b<16; // untextured interior tint
}
}

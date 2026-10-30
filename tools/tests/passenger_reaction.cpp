#include "Symbols.h"
#include "VehicleWeaponPolicy.h"
#include "PlayerLife.h"
#include <chrono>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
namespace savr {GameSymbols g{};
namespace vehicleentry {int SeatIndex(void* car,void* ped){if(!car||!ped)return -2;void* p{};std::memcpy(&p,static_cast<char*>(car)+0x5a0,8);if(p==ped)return -1;for(int i=0;i<3;++i){std::memcpy(&p,static_cast<char*>(car)+0x5a8+i*8,8);if(p==ped)return i;}return -2;}}
}
// PRODUCTION_REACTION
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
using namespace savr::passengerreaction;
char driver[0x800]{},player[0x800]{},car[0xc68]{},stats[0x34]{},intelligence[0x80]{},taskMemory[16][0x80]{};
int tasks{},leaves{},drags{},flees{},fights{};bool friends{};
template<class T>void Put(void* p,int off,T value){std::memcpy(static_cast<char*>(p)+off,&value,sizeof(value));}
void Setup(){
 if(cleanRef)Clear();std::memset(car,0,sizeof(car));std::memset(intelligence,0,sizeof(intelligence));
 Put(driver,0x538,static_cast<void*>(intelligence));Put(driver,0x728,static_cast<void*>(stats));
 Put(driver,0x548,static_cast<unsigned char>(1));Put(driver,0x6ac,100.f);Put(player,0x6ac,100.f);
 Put(car,0x5a0,static_cast<void*>(driver));Put(car,0x5a8,static_cast<void*>(player));
 Put(stats,0x24,static_cast<unsigned char>(20));Put(stats,0x25,static_cast<unsigned char>(80));
 tasks=leaves=drags=flees=fights=0;friends=false;
 allocate=[](std::size_t size)->void*{CHECK(size<=0x80&&tasks<16);return taskMemory[tasks++];};
 setTask=[](void* manager,void* task,int slot,bool){Put(manager,slot*8,task);};
 leaveCtor=[](void*,void*,int,int,bool,bool){++leaves;};dragCtor=[](void*,void* target,int){CHECK(target==player);++drags;};
 fleeCtor=[](void*,void* target,bool,float,int,int,float){CHECK(target==player);++flees;};meleeCtor=[](void*,void* target){CHECK(target==player);++fights;};
 registerRef=[](void*,void**){};cleanRef=[](void*,void**){};getGroup=[](const void*)->void*{return friends?stats:nullptr;};pedFlag=[](int)->unsigned{return 1;};
}
int main(){
 Setup();friends=true;OnBoard(car,player);CHECK(!leaves);
 Setup();Put(driver,0x548,static_cast<unsigned char>(2));OnBoard(car,player);CHECK(!leaves);
 Setup();Put(driver,0x63c,1u);OnBoard(car,player);CHECK(!leaves);Put(driver,0x63c,0u);
 Setup();OnBoard(car,player);CHECK(leaves==1);Update(true);CHECK(!drags);
 Put(car,0x5a0,static_cast<void*>(nullptr));Update(true);CHECK(drags==1);
 Put(car,0x5a8,static_cast<void*>(nullptr));Put(car,0x5a0,static_cast<void*>(player));Update(true);CHECK(drags==2);
 Put(car,0x5a0,static_cast<void*>(nullptr));Update(true);CHECK(fights==1&&state.phase==0);
 Setup();Put(stats,0x24,static_cast<unsigned char>(90));OnBoard(car,player);Put(car,0x5a0,static_cast<void*>(nullptr));Update(true);CHECK(flees==1&&!drags&&!fights);
 Setup();OnBoard(car,player);Put(driver,0x6ac,0.f);Update(true);CHECK(!state.phase&&!drags);
 Setup();OnBoard(car,player);state.car=nullptr;Update(true);CHECK(!state.phase);
 Setup();OnBoard(car,player);Put(intelligence,8+3*8,static_cast<void*>(stats));Update(true);CHECK(!state.phase&&!drags);
 for(int type=0;type<47;++type)CHECK(savr::vehicleweapon::Allowed(type,true)==(type>=22&&type<=34));
 std::puts("passenger reaction: friends/mission exemption, leave, flee, pursuit after transfer, fight, death, deletion and script interruption passed; passenger weapon whitelist passed");
}

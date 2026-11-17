#include "VehicleInteriorGlass.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
using namespace savr;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(3);}}while(0)
int g_renderingStereoEyeIndex=0;std::atomic<bool> g_stereoActive{true};
namespace driving {bool hidden=true,third=false;bool IsInteriorGlassHidden(){return hidden;}bool ThirdPersonViewActive(){return third;}}
unsigned char player[0xc68]{};int appearance=1;
struct Material {void* texture{};std::uint8_t rgba[4]{255,255,255,255};};
struct Geometry {Material* list[10]{};int count{};};
struct Symbols {
 void*(*FindPlayerVehicle)(int,bool)=[](int,bool)->void*{return player;};
 int(*CVehicle_GetVehicleAppearance)(void*)=[](void*){return appearance;};
 const char*(*GetFrameNodeName)(void*)=[](void* p){return static_cast<const char*>(p);};
 void*(*RpGeometryForAllMaterials)(void*,void*(*)(void*,void*),void*)=[](void* p,void*(*fn)(void*,void*),void* data)->void*{
  auto& geo=*static_cast<Geometry*>(p);for(int i=0;i<geo.count;++i)fn(geo.list[i],data);return p;};
} g;
#include "VehicleInteriorGlass.inl"
int main(){
 std::array<char,0x80> atomic{},texture{};int clump{};void* clumpPointer=&clump;
 std::memcpy(player+0x20,&clumpPointer,8);std::memcpy(atomic.data()+0x58,&clumpPointer,8);
 char door[]="door_lf_ok";void* frame=door;std::memcpy(atomic.data()+8,&frame,8);
 std::memcpy(texture.data()+0x20,"vehiclegeneric256",18);
 Material window{texture.data(),{255,255,255,128}},body{},decal{nullptr,{255,255,255,242}},tint{nullptr,{0,0,0,128}};
 Geometry geo{{&window,&body,&window,&decal,&tint},5};void* geometry=&geo;std::memcpy(atomic.data()+0x30,&geometry,8);
 CHECK(PlayerInteriorGlassAtomic(atomic.data())&&!NamedInteriorWindow(atomic.data()));
 {ScopedInteriorGlass mask;mask.Apply(atomic.data());CHECK(mask.count==2&&window.rgba[3]==0&&tint.rgba[3]==0&&body.rgba[3]==255&&decal.rgba[3]==242);}
 CHECK(window.rgba[3]==128&&tint.rgba[3]==128);
 driving::hidden=false;CHECK(!PlayerInteriorGlassAtomic(atomic.data()));driving::hidden=true;
 driving::third=true;CHECK(!PlayerInteriorGlassAtomic(atomic.data()));driving::third=false;
 g_renderingStereoEyeIndex=-1;CHECK(!PlayerInteriorGlassAtomic(atomic.data()));g_renderingStereoEyeIndex=1;
 int otherClump{};void* other=&otherClump;std::memcpy(atomic.data()+0x58,&other,8);CHECK(!PlayerInteriorGlassAtomic(atomic.data()));
 std::memcpy(atomic.data()+0x58,&clumpPointer,8);
 char rear[]="rear_window_ok";frame=rear;std::memcpy(atomic.data()+8,&frame,8);CHECK(NamedInteriorWindow(atomic.data()));
 CHECK(interiorglass::WindowMaterial("vehicleshatter128",255,255,255,255));
 CHECK(interiorglass::WindowMaterial("VEHICLEGENERIC256",128,255,255,255));
 CHECK(!interiorglass::WindowMaterial("vehiclelights128",128,255,255,255));
 CHECK(!interiorglass::WindowMaterial("firetruk92decal",242,0,0,0));
 std::puts("interior glass: own clump only, front/side/rear and shattered windows, mixed doors preserved, exact restore, no effect on traffic or secondary/third-person cameras PASS");
}

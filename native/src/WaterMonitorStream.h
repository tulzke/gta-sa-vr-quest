#pragma once
#include "WaterMonitorLogic.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>
namespace savr::watermonitor {
constexpr std::size_t CannonStride=0x3f8,CannonCount=3,StreamSections=32;
struct WaterCannon {
 std::uint32_t id{};std::int16_t current{};
 std::array<Vec,StreamSections> positions{};
 std::array<std::uint8_t,StreamSections> active{};
};
inline WaterCannon ReadCannon(const void* memory,std::size_t size){
 WaterCannon out{};if(!memory||size<CannonStride)return out;
 const auto* bytes=static_cast<const unsigned char*>(memory);
 std::memcpy(&out.id,bytes,4);std::memcpy(&out.current,bytes+4,2);
 if(out.current<0||out.current>=32){out.id=0;return out;}
 static_assert(sizeof(Vec)==12);
 std::memcpy(out.positions.data(),bytes+0xc,sizeof(Vec)*StreamSections);
 std::memcpy(out.active.data(),bytes+0x30c,StreamSections);
 return out;
}
struct WaterVertex {Vec position{};std::uint32_t color{};};
struct WaterMesh {std::vector<WaterVertex> vertices;std::vector<unsigned short> indices;};
// Read-only copy of the retail 32-section ribbon. Simulation, random numbers,
// extinguishing, splash particles and audio are deliberately absent.
template<class Blocked> void AppendWater(const WaterCannon& cannon,Vec viewForward,Vec viewRight,Blocked blocked,WaterMesh& mesh){
 if(!cannon.id||cannon.current<0||cannon.current>=32)return;
 int current=cannon.current;
 for(int age=0;age<31;++age){
  const int previous=(current+31)&31;
  const Vec a=cannon.positions[current],b=cannon.positions[previous],delta=b-a;
  const float length2=dot(delta,delta);
  if(cannon.active[current]==1&&cannon.active[previous]==1&&finite(a)&&finite(b)&&length2>1e-8f&&length2<25.f){
   const Vec tangent=unit(delta);Vec side=unit(cross(tangent,viewForward));
   if(dot(side,side)<.5f)side=unit(viewRight-tangent*dot(viewRight,tangent));
   if(dot(side,side)<.5f)side=unit(cross(tangent,std::abs(tangent.z)<.9f?Vec{0,0,1}:Vec{0,1,0}));
   const Vec other=unit(cross(tangent,side));
   const float radius=.05f*(3.f+age*age/32.f);
   const auto alpha=static_cast<std::uint32_t>(64-2*age);
   const std::uint32_t color=0x00ffc8c8u|(alpha<<24);
   // Three crossed ribbons reproduce the native volumetric envelope without
   // consuming rand() again when this extra camera is drawn.
   for(const Vec axis:{side,side*-.5f+other*.8660254f,side*-.5f-other*.8660254f}){
    const Vec width=axis*radius;const auto base=static_cast<unsigned short>(mesh.vertices.size());
    for(const Vec p:{a-width,a+width,b+width,b-width})mesh.vertices.push_back({p,color});
    for(int i:{0,1,2,0,2,3})mesh.indices.push_back(static_cast<unsigned short>(base+i));
   }
   // Retail renders the intersecting segment, then stops following the tail.
   // The camera depth buffer clips the part behind the surface as usual.
   if(blocked(a,b))break;
  }
  current=previous;
 }
}
}

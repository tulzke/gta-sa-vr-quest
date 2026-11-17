#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
namespace savr::watermonitor {
struct Vertex {float position[3],normal[3],color[3],uv[2],role;};
static_assert(sizeof(Vertex)==48);
struct Model {
 std::vector<Vertex> vertices;std::vector<std::uint16_t> indices;
 bool Load(const void* data,std::size_t size){
  vertices.clear();indices.clear();if(!data||size<16)return false;
  std::uint32_t h[4];std::memcpy(h,data,16);const auto nv=h[2],ni=h[3];
  if(h[0]!=0x33314d57||h[1]!=1||!nv||nv>65535||!ni||ni>12000||ni%3||size!=16+std::size_t(nv)*48+std::size_t(ni)*2)return false;
  const auto* b=static_cast<const unsigned char*>(data)+16;
  std::vector<Vertex> v(nv);std::memcpy(v.data(),b,nv*48);
  std::vector<std::uint16_t> ix(ni);std::memcpy(ix.data(),b+nv*48,ni*2);
  for(const auto& p:v){float n=0;for(int i=0;i<3;++i){
   if(!std::isfinite(p.position[i])||std::abs(p.position[i])>.4f||!std::isfinite(p.normal[i])||!std::isfinite(p.color[i])||p.color[i]<0||p.color[i]>1)return false;n+=p.normal[i]*p.normal[i];}
   if(n<.98f||n>1.02f||!std::isfinite(p.role)||p.role<0||p.role>4||std::floor(p.role)!=p.role)return false;
   for(float uv:p.uv)if(!std::isfinite(uv)||uv<-.001f||uv>1.001f)return false;
  }
  for(auto i:ix)if(i>=nv)return false;
  vertices=std::move(v);indices=std::move(ix);return true;
 }
};
}

#pragma once
#include "WaterMonitorStream.h"
namespace savr::watermonitor {
// Keep the retail 32-section budget, sampled at the original 30-Hz cadence.
// Movement/extinguishing still run at the real game timestep every frame.
struct Sampling {
 static constexpr double Period=50.0/30.0; // native timer units, 50 per second
 std::uint32_t id{},born{},inputFrame{},updatedFrame{};bool updated{};
 Vec position{},velocity{};double phase{};
 void Input(std::uint32_t identity,std::uint32_t birth,std::uint32_t frame,Vec p,Vec v){
  if(id!=identity||born!=birth){*this={};id=identity;born=birth;}
  position=p;velocity=v;inputFrame=frame;
 }
 bool Matches(const void* memory)const{
  std::uint32_t identity{},birth{};std::memcpy(&identity,memory,4);std::memcpy(&birth,static_cast<const char*>(memory)+8,4);
  return id&&id==identity&&born==birth;
 }
 void Prepare(void* memory,float dt,std::uint32_t frame,bool emitting){
  if(!std::isfinite(dt)||dt<0)return;
  auto* bytes=static_cast<unsigned char*>(memory);std::int16_t current{};std::memcpy(&current,bytes+4,2);
  if(current<0||current>=32)return;
  const bool fresh=static_cast<std::uint32_t>(frame-inputFrame)<=1&&finite(position)&&finite(velocity);
  emitting=emitting&&fresh;
  const double oldPhase=phase,total=phase+dt;
  const double due=std::floor((total+1e-5)/Period);
  phase=std::max(0.0,total-due*Period);
  const int steps=static_cast<int>(std::min(due,32.0));
  if(due>32){std::memset(bytes+0x30c,0,32);current=static_cast<std::int16_t>((current+static_cast<int>(std::fmod(due-32,32.0)))&31);}
  const Vec gravity{0,0,-.004f};
  const auto write=[&](int slot,Vec p,Vec v){std::memcpy(bytes+0xc+slot*12,&p,12);std::memcpy(bytes+0x18c+slot*12,&v,12);bytes[0x30c+slot]=1;};
  for(int i=0;i<steps;++i){
   // Freeze the head born at the PREVIOUS sample boundary. The new head is
   // born now; treating both as newborn would lose one section of range.
   const float offset=std::min(static_cast<float>(Period-oldPhase+(due-steps+i)*Period),dt)-static_cast<float>(Period);
   if(bytes[0x30c+current]==1&&fresh){
    // A section born within a low-FPS frame must age only for the remaining
    // fraction. Precompensate the retail semi-implicit integration below.
    const Vec endVelocity=velocity+gravity*(dt-offset);
    write(current,position-endVelocity*offset,velocity-gravity*offset);
   }
   current=static_cast<std::int16_t>((current+1)&31);bytes[0x30c+current]=0;
   if(emitting)write(current,position,velocity);
  }
  std::memcpy(bytes+4,&current,2);
  // The live head follows the nozzle; the older sections retain native motion.
  if(emitting)write(current,position-velocity*dt,velocity-gravity*dt);
 }
};
}

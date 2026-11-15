"""Exercise production pitch feedback publication and mirrored wrist rotation."""
from pathlib import Path
import runpy

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/bike-pitch-feedback-tests'
OUT.mkdir(parents=True, exist_ok=True)
driving = (ROOT / 'native/src/Driving.cpp').read_text(encoding='utf8')
xr = (ROOT / 'native/src/Xr.cpp').read_text(encoding='utf8')


def function(source, signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


code = r'''
#include "Driving.h"
#include "BikePitchFeedback.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>
using namespace savr;
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#e);std::exit(3);}}while(0)
bool Near(float a,float b){return std::abs(a-b)<1e-5f;}
namespace savr::driving {
WheelVisualState g_visual;
float g_bikeLean{};
std::atomic<bool> g_bikeBlockWheelie{false};
bool allowed=true,cabin=true,rider=true;int mode=MODE_IMMERSIVE;
bool VrGameplayInputAllowed(){return allowed;}
bool CabinVisualContext(){return cabin;}
bool GetActivePlayerBike(){return rider;}
int ModeForAppearance(int){return mode;}
double now{};double MonotonicSeconds(){return now;}
struct Event{int hand;float amplitude,frequency,duration;};
std::vector<Event> events;
namespace xr {void TriggerHaptic(int h,float a,float f,float d){events.push_back({h,a,f,d});}}
''' + function(driving, 'void PublishBikePitchFeedback(') + r'''
}
struct XrVector3f {float x,y,z;};
''' + xr[xr.index('inline XrVector3f vadd'):xr.index('bool TwoHandStateUsable(')] + function(xr, 'void ApplyBikePitchWrist(') + r'''
int main(){
 using namespace savr::driving;
 g_visual.active=true;g_visual.bike=true;
 g_visual.grabbed[0]=g_visual.grabbed[1]=true;
 g_visual.right[0]=1;g_visual.normal[1]=1;
 g_bikeLean=1;PublishBikePitchFeedback(true);
 CHECK(events.size()==2&&events[0].hand==0&&events[1].hand==1);
 CHECK(Near(events[0].amplitude,.38f)&&Near(events[0].frequency,160));
 CHECK(g_visual.bikePitchVisual<0);const float forwardAngle=g_visual.bikePitchVisual;
 now=.01;PublishBikePitchFeedback(true);CHECK(events.size()==2);
 now=.05;PublishBikePitchFeedback(true);CHECK(events.size()==4);
 CHECK(events.back().duration==75&&Near(events.back().amplitude,.38f));
 g_bikeLean=-1;now=.06;PublishBikePitchFeedback(true);
 CHECK(events.size()==6&&Near(events.back().frequency,80));
 CHECK(Near(g_visual.bikePitchVisual,-forwardAngle));
 float low=1,high=0;
 for(int i=1;i<80;++i){now=.06+i*.05;PublishBikePitchFeedback(true);
  const auto& e=events.back();CHECK(e.amplitude>0&&e.amplitude<.38f);
  low=std::min(low,e.amplitude);high=std::max(high,e.amplitude);}
 CHECK(high-low>.14f); // envelope stays distinct even if frequency is ignored
 // Neutral sends one stop per hand; it does not continuously cancel other cues.
 g_bikeLean=0;now+=.01;auto size=events.size();PublishBikePitchFeedback(true);
 CHECK(events.size()==size+2&&events.back().amplitude==0&&g_visual.bikePitchVisual==0);
 PublishBikePitchFeedback(true);CHECK(events.size()==size+2);
 for(int excluded=0;excluded<9;++excluded){
  g_bikeLean=1;now+=.1;PublishBikePitchFeedback(true);CHECK(g_visual.bikePitchVisual!=0);
  if(excluded==0)allowed=false;
  if(excluded==1)cabin=false;
  if(excluded==2)rider=false;
  if(excluded==3)mode=MODE_DEFAULT;
  if(excluded==4)g_visual.active=false;
  if(excluded==5)g_visual.bike=false;
  if(excluded==6)g_visual.grabbed[0]=false;
  if(excluded==7)g_visual.grabbed[1]=false;
  now+=.01;size=events.size();PublishBikePitchFeedback(excluded!=8);
  CHECK(g_visual.bikePitchVisual==0&&events.size()==size+2&&events.back().amplitude==0);
  allowed=cabin=rider=true;mode=MODE_IMMERSIVE;
  g_visual.active=g_visual.bike=g_visual.grabbed[0]=g_visual.grabbed[1]=true;
 }
 g_bikeLean=-1;g_bikeBlockWheelie=true;PublishBikePitchFeedback(true);
 CHECK(g_visual.bikePitchVisual==0);g_bikeBlockWheelie=false;
 g_bikeLean=std::numeric_limits<float>::quiet_NaN();PublishBikePitchFeedback(true);
 CHECK(g_visual.bikePitchVisual==0);
 // Every refresh covers the next one at normal headset frame rates.
 for(int fps:{72,90,120})for(float command:{-1.f,1.f}){
  bikepitch::Feedback state;double expires=0;int count=0;
  for(int frame=0;frame<fps*3;++frame){const double t=frame/double(fps);
   auto effect=state.Update(command,t);
   if(effect.send){CHECK(t<=expires||frame==0);expires=t+effect.durationMs*.001;++count;}
   CHECK(t<expires);
  }
  CHECK(count<fps*3&&count>40);
 }
 bikepitch::Feedback state;CHECK(state.Update(1,5).send);
 CHECK(state.Update(-1,5.001).send);CHECK(state.Update(1,1).send);
 CHECK(state.Update(0,1.01).send);CHECK(!state.Update(0,1.02).send);
 // Both mirrored hands pitch in the same bike-relative direction. The cue
 // composes with steering, per-hand calibration and either wrist follow mode.
 for(float yaw:{-.6f,0.f,.6f})for(bool follow:{false,true})for(int hand:{0,1})for(float command:{-1.f,1.f}){
  g_visual.bikeHandsFollowTilt=follow;g_visual.physicalAngle=yaw;
  g_visual.bikePitchVisual=bikepitch::WristAngle(command);
  const XrVector3f normal{0,1,0};
  XrVector3f f=RotateAroundAxis({0,0,-1},normal,yaw);
  XrVector3f r=RotateAroundAxis({hand==0?-1.f:1.f,0,0},normal,yaw),u{0,1,0};
  ApplyBikePitchWrist(g_visual,hand,r,u,f);
  CHECK(command>0?f.y<0:f.y>0);
  CHECK(Near(vdot(f,f),1)&&Near(vdot(r,r),1)&&Near(vdot(u,u),1));
  CHECK(Near(vdot(f,r),0)&&Near(vdot(f,u),0)&&Near(vdot(r,u),0));
 }
 for(int invalid=0;invalid<5;++invalid){
  XrVector3f r{1,0,0},u{0,1,0},f{0,0,-1};g_visual.bikePitchVisual=forwardAngle;
  if(invalid==0)g_visual.active=false;
  if(invalid==1)g_visual.bike=false;
  if(invalid==2)g_visual.grabbed[0]=false;
  if(invalid==3)g_visual.bikePitchVisual=0;
  if(invalid==4)g_visual.bikePitchVisual=std::numeric_limits<float>::quiet_NaN();
  ApplyBikePitchWrist(g_visual,0,r,u,f);CHECK(f.y==0&&f.z==-1);
  g_visual.active=g_visual.bike=g_visual.grabbed[0]=true;
 }
 std::puts("PASS: production pitch haptics, both wrists, direction/stop, exclusions, frame coverage and mirrored steering axes");
}
'''
test = OUT / 'feedback.cpp'
test.write_text(code, encoding='utf8')
runpy.run_path(str(ROOT / 'tools/test-bike-steering.py'))['compile_run'](test, 'feedback', OUT)

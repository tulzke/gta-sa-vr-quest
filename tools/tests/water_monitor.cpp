#include "WaterMonitorLogic.h"
#include "WaterMonitorHand.h"
#include "WaterMonitorModel.h"
#include <fstream>
#include <iterator>
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace savr;
using namespace savr::watermonitor;
int checks{};
#define CHECK(e) do{++checks;if(!(e)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#e);std::exit(3);}}while(0)
bool near(float a,float b,float tolerance=1e-4f){return std::abs(a-b)<tolerance;}
int main(int argc,char** argv){
 CHECK(argc==2);
 tracking::SeatKey key{};key.vehicle=reinterpret_cast<void*>(1);key.model=407;key.seat=-1;key.generation=3;
 Hand hands[2]{{{},1,true,true,true},{ {},1,true,true,true}};
 State s;s.Step(true,key,hands,false,0);CHECK(s.owner==-1);
 hands[0].grip=0;hands[1].near=false;s.Step(true,key,hands,false,.01);
 hands[0].grip=1;s.Step(true,key,hands,true,.02);CHECK(s.owner==0&&!s.fire&&s.ConsumesFire());
 s.Step(true,key,hands,true,.03);CHECK(!s.fire); // no inherited shot at grab
 s.Step(true,key,hands,false,.04);s.Step(true,key,hands,true,.05);CHECK(s.fire);
 hands[0].frame.forward={1,0,0};s.Step(true,key,hands,true,.06);CHECK(s.held.forward.x==1); // no smoothing of target
 hands[0].grip=0;s.Step(true,key,hands,true,1);CHECK(s.owner==-1&&!s.fire&&s.ConsumesFire());
 CHECK(s.CameraActive(3.999)&&!s.CameraActive(4));
 CHECK(s.Display(3.999).x==1&&s.Display(3.999).y==1);
 CHECK(s.Display(4.10).x==1&&s.Display(4.10).y<.6f);
 CHECK(s.Display(4.30).x<1&&s.Display(4.30).y<.02f&&s.Display(4.36).light==0);
 s.Observe(false);CHECK(!s.ConsumesFire());
 hands[1].near=true;hands[1].grip=0;s.Step(true,key,hands,false,4.4);
 hands[1].grip=1;s.Step(true,key,hands,false,4.5);CHECK(s.owner==1&&s.CameraActive(9));
 hands[1].valid=false;s.Step(true,key,hands,true,4.6);CHECK(s.owner==-1&&!s.fire&&!s.CameraActive(4.61)&&s.ConsumesFire());
 hands[1].valid=true;s.Step(true,key,hands,true,4.7);CHECK(s.owner==-1);
 hands[1].grip=0;s.Step(true,key,hands,false,4.8);hands[1].grip=1;s.Step(true,key,hands,false,4.9);CHECK(s.owner==1);
 key.generation++;s.Step(true,key,hands,true,5);CHECK(s.owner==-1&&!s.CameraActive(5)&&s.ConsumesFire());
 hands[1].grip=0;s.Step(true,key,hands,false,5.1);hands[1].grip=1;s.Step(true,key,hands,true,5.2);
 CHECK(s.owner==1&&!s.fire);hands[1].available=false;s.Step(true,key,hands,true,5.3);CHECK(s.owner==-1&&!s.fire);
 s.Step(false,key,hands,false,5.4);CHECK(!s.ConsumesFire());
 for(int hz:{36,72,90,120}){
  Servo servo;const float dt=1.f/hz;
  servo.Step(1.5f,1,dt);CHECK(servo.yaw>0&&servo.yaw<.02f&&servo.pitch<PitchLimit);
  for(int i=0;i<hz*3;++i){const float old=servo.yaw;servo.Step(1.5f,1,dt);CHECK(std::abs(wrap(servo.yaw-old))<=2.5f*dt+.0001f&&servo.pitch<=PitchLimit);}
  CHECK(near(servo.yaw,1.5f,.003f)&&near(servo.pitch,PitchLimit,.003f));
  for(int i=0;i<hz*3;++i)servo.Step(0,0,dt);
  CHECK(near(servo.yaw,0,.003f)&&near(servo.pitch,0,.003f));
 }
 Servo seam;seam.yaw=Pi-.01f;seam.Step(-Pi+.05f,0,.02f);CHECK(seam.yawSpeed>0);
 const auto before=seam.yaw;seam.Step(std::numeric_limits<float>::quiet_NaN(),0,.01f);CHECK(seam.yaw==before);
 Frame camera{{},{1,0,0},{0,1,0},{0,0,1}};
 CHECK(near(Circle({0,1,0},camera).x,0));CHECK(Circle({.5f,1,0},camera).x>0);
 CHECK(Circle({0,1,.2f},camera).y>0);CHECK(Circle({10,1,10},camera).x==.9f);
 CHECK(std::isfinite(Circle({0,-1,0},camera).x));
 xr::HandPose pose{};pose.valid=pose.aimValid=true;pose.gripOri[3]=pose.aimOri[3]=1;
 auto held=HeldFrame(pose,0);CHECK(near(held.forward.z,-1)&&near(held.right.x,1));
 pose.gripOri[2]=std::sin(.6f);pose.gripOri[3]=std::cos(.6f);
 auto rolled=HeldFrame(pose,1);CHECK(near(rolled.up.y,1)&&near(rolled.forward.z,-1)); // roll ignored
 pose.aimOri[1]=std::sin(.35f);pose.aimOri[3]=std::cos(.35f);
 auto aimed=HeldFrame(pose,0);CHECK(aimed.forward.x<-.6f&&near(dot(aimed.forward,aimed.up),0));
 auto blended=Blend(held,aimed,.5f);CHECK(near(dot(blended.forward,blended.forward),1));
 pose.aimOri[1]=0;pose.aimOri[0]=std::sin(Pi/4);pose.aimOri[3]=std::cos(Pi/4);
 const auto vertical=HeldFrame(pose,0);CHECK(near(dot(vertical.right,vertical.right),1)&&near(dot(vertical.up,vertical.up),1));
 std::ifstream file(argv[1],std::ios::binary);std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{});
 Model m;CHECK(m.Load(bytes.data(),bytes.size()));CHECK(m.indices.size()/3==3222);
 unsigned roles[5]{};for(std::size_t i=0;i<m.indices.size();i+=3){const auto role=static_cast<unsigned>(m.vertices[m.indices[i]].role);roles[role]++;
  CHECK(m.vertices[m.indices[i+1]].role==role&&m.vertices[m.indices[i+2]].role==role);}
 CHECK(roles[0]&&roles[2]&&roles[3]);
 CHECK(m.indices.size()/3+roles[3]+256+64+4<=4000); // camera layer, scan lines, circle, crosshair
 CHECK(!m.Load(bytes.data(),bytes.size()-1));bytes[0]=0;CHECK(!m.Load(bytes.data(),bytes.size()));
 std::printf("Water monitor: %d checks passed; 3222 asset triangles, %u screen triangles\n",checks,roles[3]);
}

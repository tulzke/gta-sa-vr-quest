#include "WaterMonitorLogic.h"
#include "WaterMonitorHand.h"
#include "WaterMonitorModel.h"
#include "WaterMonitorCamera.h"
#include "WaterMonitorStream.h"
#include "WaterMonitorSampling.h"
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
 s.Step(true,key,hands,false,.04);s.Step(true,key,hands,true,.05);CHECK(s.fire&&s.press>0&&s.press<1);
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
 const auto pivot=camera.Point(NozzlePivot);
 const auto rest=NozzleFrame(camera,pivot,0,0);
 const auto mounted=CameraFrame(rest,camera,DefaultCamera);
 CHECK(near(mounted.center.x,.35f)&&near(mounted.center.y,1.63911438f)&&near(mounted.center.z,1.94957905f));
 for(float yaw:{-2.f,0.f,1.5f})for(float pitch:{-.349f,0.f,.349f}){
  const auto nozzle=NozzleFrame(camera,pivot,yaw,pitch);
  CHECK(near(dot(cross(nozzle.right,nozzle.forward),nozzle.up),1));
  const auto view=CameraFrame(nozzle,camera,DefaultCamera);const auto delta=view.center-pivot;
  CHECK(near(dot(delta,nozzle.right),.35f)&&near(dot(delta,nozzle.forward),0)&&near(dot(delta,nozzle.up),.30f));
  CHECK(view.forward.y==1); // camera orientation never inherits the servo lag
  CameraSettings adjusted{75,40,-10,15,25,30};const auto changed=CameraFrame(nozzle,camera,adjusted);
  CHECK(near(dot(changed.forward,changed.up),0)&&near(dot(changed.forward,changed.forward),1));
  CHECK(nozzle.center.x==pivot.x&&nozzle.center.y==pivot.y&&nozzle.center.z==pivot.z); // calibration cannot move water pivot
 }
 CHECK(ClampCamera(0,999)==200&&ClampCamera(3,-999)==-180&&ClampCamera(2,std::numeric_limits<float>::quiet_NaN())==30);
 CHECK(near(Circle({0,1,0},camera).x,0));CHECK(Circle({.5f,1,0},camera).x>0);
 CHECK(Circle({0,1,.2f},camera).y>0);CHECK(Circle({10,1,10},camera).x==.9f);
 CHECK(std::isfinite(Circle({0,-1,0},camera).x));
 xr::HandPose pose{};pose.valid=pose.aimValid=true;pose.gripOri[3]=pose.aimOri[3]=1;
 auto held=HeldFrame(pose,0);CHECK(near(held.forward.z,-1)&&near(held.right.x,1));
 pose.gripOri[2]=std::sin(.6f);pose.gripOri[3]=std::cos(.6f);
 auto rolled=HeldFrame(pose,1);CHECK(std::abs(rolled.up.x)>.8f&&near(rolled.forward.z,-1)); // body rolls, aim does not
 for(int side:{0,1}){const auto inHand=HeldFrame(pose,side);const auto wrist=FixedGrip(inHand,side).position;
  CHECK(near(wrist.x,pose.gripPos[0])&&near(wrist.y,pose.gripPos[1])&&near(wrist.z,pose.gripPos[2]));}
 for(const auto& device:{held,rolled})for(int side:{0,1}){
  const auto pinned=FixedGrip(device,side);
  const auto contact=handrender::MeshPoint(pinned,GripMeshAnchor),target=device.Point(GripAnchor);
  CHECK(dot(contact-target,contact-target)<1e-10f);
  CHECK(near(dot(pinned.forward,device.forward),1)&&near(dot(pinned.right,device.up),1));
  CHECK(near(dot(pinned.up,device.right),side==0?-1.f:1.f));
 }
 pose.aimOri[1]=std::sin(.35f);pose.aimOri[3]=std::cos(.35f);
 auto aimed=HeldFrame(pose,0);CHECK(aimed.forward.x<-.6f&&near(dot(aimed.forward,aimed.up),0));
 auto blended=Blend(held,aimed,.5f);CHECK(near(dot(blended.forward,blended.forward),1));
 pose.aimOri[1]=0;pose.aimOri[0]=std::sin(Pi/4);pose.aimOri[3]=std::cos(Pi/4);
 const auto vertical=HeldFrame(pose,0);CHECK(near(dot(vertical.right,vertical.right),1)&&near(dot(vertical.up,vertical.up),1));
 for(const Vec direction:{Vec{0,0,1},Vec{0,0,-1},Vec{0,1,0}}){const auto aim=LevelAim(direction);
  CHECK(near(dot(aim.right,aim.right),1)&&near(dot(aim.up,aim.up),1)&&near(dot(aim.forward,aim.up),0));}
 std::ifstream file(argv[1],std::ios::binary);std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{});
 Model m;CHECK(m.Load(bytes.data(),bytes.size()));CHECK(m.indices.size()/3==3222);
 unsigned roles[5]{};for(std::size_t i=0;i<m.indices.size();i+=3){const auto role=static_cast<unsigned>(m.vertices[m.indices[i]].role);roles[role]++;
  CHECK(m.vertices[m.indices[i+1]].role==role&&m.vertices[m.indices[i+2]].role==role);}
 CHECK(roles[0]&&roles[2]&&roles[3]);
 CHECK(m.indices.size()/3+roles[3]+256+64+4==3726); // collapse, CRT, circle, crosshair
 CHECK(m.indices.size()/3+256+64+4==3546); // active screen replaces the black surface
 CHECK(!m.Load(bytes.data(),bytes.size()-1));bytes[0]=0;CHECK(!m.Load(bytes.data(),bytes.size()));
 std::array<unsigned char,CannonStride> cannonBytes{};const std::uint32_t id=123;const std::int16_t current=2;
 std::memcpy(cannonBytes.data(),&id,4);std::memcpy(cannonBytes.data()+4,&current,2);
 for(int age=0;age<32;++age){const int slot=(current-age+32)&31;const Vec p{0,age*.5f,2-age*.01f};
  std::memcpy(cannonBytes.data()+0xc+slot*12,&p,12);cannonBytes[0x30c+slot]=1;}
 const auto originalCannon=cannonBytes;auto cannon=ReadCannon(cannonBytes.data(),cannonBytes.size());CHECK(cannon.id==123&&cannon.current==2);
 WaterMesh water;int queries{};const auto clear=[&](Vec,Vec){++queries;return false;};
 AppendWater(cannon,{0,1,0},{1,0,0},clear,water);
 CHECK(water.vertices.size()==31*12&&water.indices.size()==31*18&&queries==31);
 CHECK(cannonBytes==originalCannon);CHECK(water.vertices.front().color>>24==64&&water.vertices.back().color>>24==4);
 for(const auto& v:water.vertices)CHECK(finite(v.position));
 for(auto ix:water.indices)CHECK(ix<water.vertices.size());
 Vec centre{};for(int i=0;i<4;++i)centre=centre+water.vertices[i].position*.25f;
 CHECK(near(centre.y,.25f)&&near(centre.z,1.995f)); // real section positions, no predicted arc
 water={};queries=0;AppendWater(cannon,{0,1,0},{1,0,0},[&](Vec,Vec){++queries;return true;},water);
 CHECK(queries==1&&water.indices.size()==18); // tail stops at first obstacle
 water={};cannon.id=0;AppendWater(cannon,{0,1,0},{1,0,0},clear,water);CHECK(water.vertices.empty());
 cannon=ReadCannon(originalCannon.data(),originalCannon.size());cannon.active.fill(0);AppendWater(cannon,{0,1,0},{1,0,0},clear,water);CHECK(water.vertices.empty());
 cannon=ReadCannon(originalCannon.data(),originalCannon.size());cannon.positions[current].x=std::numeric_limits<float>::quiet_NaN();
 AppendWater(cannon,{0,1,0},{1,0,0},clear,water);for(const auto& v:water.vertices)CHECK(finite(v.position));
 CHECK(ReadCannon(cannonBytes.data(),10).id==0);cannonBytes[4]=255;cannonBytes[5]=255;CHECK(ReadCannon(cannonBytes.data(),cannonBytes.size()).id==0);
 for(float press:{0.f,.35f,1.f}){const auto animation=PistolPose(press);
  const auto weapon=handanimation::Select(1.f,press,{false,false,false,false,false,true,22,0});
  CHECK(animation.first==weapon.first&&animation.second==weapon.second&&animation.progress==weapon.progress&&animation.blend==weapon.blend);}
 float referenceRange=0;
 for(int hz:{15,20,30,36,60,72,90,120,144}){
  std::array<unsigned char,CannonStride> data{};const std::uint32_t identity=99,birth=1;
  std::memcpy(data.data(),&identity,4);std::memcpy(data.data()+8,&birth,4);
  Sampling clock;const Vec p{0,0,2},v{0,.5f,.05f};const float dt=50.f/hz;
  const auto emit=[&](unsigned frame){std::int16_t index{};std::memcpy(&index,data.data()+4,2);
   std::memcpy(data.data()+0xc+index*12,&p,12);std::memcpy(data.data()+0x18c+index*12,&v,12);data[0x30c+index]=1;
   clock.Input(identity,birth,frame,p,v);};
  const auto native=[&](){for(int i=0;i<32;++i)if(data[0x30c+i]){Vec a{},speed{};std::memcpy(&a,data.data()+0xc+i*12,12);std::memcpy(&speed,data.data()+0x18c+i*12,12);
   speed.z-=.004f*dt;a=a+speed*dt;std::memcpy(data.data()+0xc+i*12,&a,12);std::memcpy(data.data()+0x18c+i*12,&speed,12);}};
  emit(0);for(int frame=1;frame<=hz*4;++frame){clock.Prepare(data.data(),dt,frame,true);native();emit(frame);}
  float range=0;for(const auto& point:ReadCannon(data.data(),data.size()).positions)range=std::max(range,point.y);
  if(hz==15)referenceRange=range;CHECK(near(range,referenceRange,.002f)&&near(range,25.833333f,.002f));
  CHECK(clock.Matches(data.data()));
  for(int frame=hz*4+1;frame<=hz*6;++frame){clock.Prepare(data.data(),dt,frame,false);native();}
  const auto stopped=ReadCannon(data.data(),data.size());CHECK(std::all_of(stopped.active.begin(),stopped.active.end(),[](auto flag){return flag==0;}));
 }
 std::printf("Water monitor: %d checks passed; 3222 asset triangles, %u screen triangles\n",checks,roles[3]);
}

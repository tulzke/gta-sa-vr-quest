"""Production tests for lens framing, shutter review and detonator lifetime."""
from pathlib import Path
import os,re,subprocess,sys
R=Path(__file__).resolve().parents[1];O=R/'build/photo-detonator-tests';O.mkdir(parents=True,exist_ok=True)
def fn(file,name):
 s=(R/'native/src'/file).read_text(encoding='utf8');m=re.search(r'^(?:bool|void|int|float|CamBasis) '+name+r'\(',s,re.M);assert m,name
 a=m.start();i=s.index('{',a)+1;d=1
 while d:d+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[a:i]
code=r"""
#include "PairedWeapons.h"
#include "PhotoScreen.h"
#include "PhysicalWeapon.h"
#include "WeaponOptic.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#define LOGI(...) ((void)0)
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"FAIL %d %s\n",__LINE__,#e);std::exit(3);}}while(0)
struct V3{
 float x{},y{},z{};
 V3 operator+(V3 b)const{return {x+b.x,y+b.y,z+b.z};}
 V3 operator-(V3 b)const{return {x-b.x,y-b.y,z-b.z};}
 V3 operator*(float k)const{return {x*k,y*k,z*k};}
};
V3 Cross(V3 a,V3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float Dot(V3 v,V3 b){return v.x*b.x+v.y*b.y+v.z*b.z;}
V3 Normalized(V3 v){float n=std::sqrt(Dot(v,v));return n>1e-8f?V3{v.x/n,v.y/n,v.z/n}:V3{};}
bool Finite(V3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
struct CamBasis{V3 right,forward,up,pos;};
struct RwMat{float r[3]{},u[3]{},a[3]{},t[3]{};};
struct GameSymbols{using Vec3=V3;};
using namespace savr;
namespace scopeaim {
int hand=1;
float zoom=8;
int PhotoHand(){return hand;}
}
namespace calib {
weaponoptic::Profile SnapshotOptic(int){return weaponoptic::Defaults(43);}
}
namespace savr::physicalweapon {
float supportAngle{};
bool GetTwoHandTransformTracking(int, float* pivot, float* axis, float* angle,
                                 const TrackingPose* model) {
 CHECK(model && std::abs(model->position[1]-200.f)<1e-5f);
 if (supportAngle==0.f) return false;
 std::copy_n(model->position, 3, pivot);
 axis[0]=0.f;axis[1]=0.f;axis[2]=1.f;*angle=supportAngle;
 return true;
}
}
namespace photo {
std::atomic<bool> g_stereoActive{true};bool g_vrBaseValid=true;struct{bool valid=true;}g_origin;
bool tracked=true;
float handYaw=0;
bool HandWeaponMatrix(int hand,int type,RwMat& matrix) {
 if(!tracked || type!=43 || hand<0 || hand>1) return false;
 const float side = hand == 1 ? 1.f : -1.f;
 matrix={{std::sin(handYaw),std::cos(handYaw),0},
         {side*std::cos(handYaw),-side*std::sin(handYaw),0},{0,0,-1},{float(hand),200,30}};
 return true;
}
bool WeaponMatrixToTracking(const RwMat& matrix, physicalweapon::TrackingPose& pose) {
 std::copy_n(matrix.t,3,pose.position);std::copy_n(matrix.r,3,pose.right);
 std::copy_n(matrix.u,3,pose.forward);std::copy_n(matrix.a,3,pose.up);
 return true;
}
bool TrackingWeaponMatrix(const physicalweapon::TrackingPose& pose, RwMat& matrix) {
 std::copy_n(pose.position,3,matrix.t);std::copy_n(pose.right,3,matrix.r);
 std::copy_n(pose.forward,3,matrix.u);std::copy_n(pose.up,3,matrix.a);
 return true;
}
V3 RotateAroundAxis(V3 v,V3 axis,float angle) {
 return v*std::cos(angle)+Cross(axis,v)*std::sin(angle)+axis*(Dot(axis,v)*(1.f-std::cos(angle)));
}
constexpr float kVrFovX=105;
alignas(16) unsigned char camera[0x1200]{},placeable[0x40]{},scene[0x100]{};
int screen[4]{0,0,1920,1080};
void* SceneRwCamera(){return scene;}
float drawFov=70;int fireCalls=0,derivedCalls=0;bool fireResult=true;
namespace photo_render {
int requests{};
CamBasis queued{};
float queuedFov{};
void Request(const CamBasis& view,float fov,bool horizontal,bool vertical) {
 CHECK(horizontal && !vertical);++requests;queued=view;queuedFov=fov;
}
}
template<class T>void Put(void*p,unsigned off,T v){std::memcpy(static_cast<unsigned char*>(p)+off,&v,sizeof(v));}
template<class T>T Read(void*p,unsigned off){T v;std::memcpy(&v,static_cast<unsigned char*>(p)+off,sizeof(v));return v;}
CamBasis ReadCam(std::uintptr_t p){return {Read<V3>((void*)p,0x970),Read<V3>((void*)p,0x980),Read<V3>((void*)p,0x990),Read<V3>((void*)p,0x9a0)};}
void WriteCam(std::uintptr_t p,const CamBasis& b){Put((void*)p,0x970,b.right);Put((void*)p,0x980,b.forward);Put((void*)p,0x990,b.up);Put((void*)p,0x9a0,b.pos);}
void SetFov(float f,bool){drawFov=f;}void Derived(void*,bool,bool){++derivedCalls;}
bool NativeFire(void*,void*,V3*,V3*,void*,V3*,V3*);
struct{void* TheCamera=camera;void* RsGlobal=screen;bool(*CWeapon_Fire)(void*,void*,V3*,V3*,void*,V3*,V3*)=NativeFire;void(*CCamera_CalculateDerivedValues)(void*,bool,bool)=Derived;void(*CDraw_SetFOV)(float,bool)=SetFov;float* CDraw_ms_fFOV=&drawFov;}g;
float observedFov=0;
int NativeCommand(void*,int){unsigned a=camera[0x5f];observedFov=a<3?Read<float>(camera,a*0x228+0x204):-1;return 17;}
int(*g_origPhotoCommands)(void*,int)=NativeCommand;
"""
camera_source=(R/'native/src/VrCamera.cpp').read_text(encoding='utf8')
screen_start=camera_source.index('class ScopedScreenDimensions')
screen_end=camera_source.index('\n};',screen_start)+3
code=code.replace('int PhotoHand(){return hand;}', 'int PhotoHand(){return hand;}\n'
    + fn('ScopeAim.cpp','PhotoFovX').replace('gCameraZoom.load(std::memory_order_relaxed)', 'scopeaim::zoom'))
code+=camera_source[screen_start:screen_end]+fn('VrCamera.cpp','ApplyTwoHandRotation')+fn('WeaponOpticFrame.inl','BuildPhotoLens')+fn('VrCamera.cpp','BuildPhotoView')+fn('VrCamera.cpp','OnPhotoCommands')+fn('VrCamera.cpp','FirePhotograph')+r"""
bool NativeFire(void*,void*,V3* origin,V3*,void*,V3*,V3*){
 ++fireCalls;unsigned a=camera[0x5f];CHECK(Read<std::uint16_t>(camera,a*0x228+0x186)==46);
 float f=Read<float>(camera,a*0x228+0x204);CHECK(f<20&&std::abs(f-drawFov)<1e-5f);
 CamBasis want;CHECK(BuildPhotoView(scopeaim::hand,want));auto actual=ReadCam((std::uintptr_t)camera);
 CHECK(std::memcmp(&actual,&want,sizeof(want))==0);
 CHECK(std::memcmp(origin,&want.pos,sizeof(*origin))==0);
 CHECK(std::memcmp(placeable+0x10,&want.forward,12)==0&&std::memcmp(placeable+0x30,&want.pos,12)==0);
 CHECK(screen[2]==768&&screen[3]==576);
 CHECK(std::abs(Read<float>(scene,0x90)/Read<float>(scene,0x94)-photoscreen::Aspect)<1e-5f);
 return fireResult;
}
void Test(){
 Put(camera,0x18,static_cast<void*>(placeable));CamBasis native{{-1,0,0},{0,1,0},{0,0,1},{400,500,600}};WriteCam((std::uintptr_t)camera,native);
 std::memset(placeable,0x42,sizeof(placeable));int dummy=0;
 for(unsigned active=0;active<3;++active){camera[0x5f]=active;Put<float>(camera,active*0x228+0x204,70);Put<std::uint16_t>(camera,active*0x228+0x186,4);
  const std::vector<unsigned char> before(camera,camera+sizeof(camera));const std::vector<unsigned char> beforeP(placeable,placeable+sizeof(placeable));
  for(float z:{1.0f,1.8f,4.0f,8.0f}){scopeaim::zoom=z;CHECK(OnPhotoCommands(&dummy,0x801)==17);CHECK(std::abs(observedFov-scopeaim::PhotoFovX(105))<1e-5f);CHECK((observedFov<20)==(z==8));CHECK(std::memcmp(camera,before.data(),before.size())==0);}
  for(int hand:{0,1})for(float yaw:{0.0f,1.5707963f,3.1415926f,-1.5707963f})for(bool ok:{false,true}){
   scopeaim::zoom=8;scopeaim::hand=hand;handYaw=yaw;fireResult=ok;
   const int beforeRequests=photo_render::requests;
   CHECK(FirePhotograph(&dummy,&dummy,hand)==ok);
   CHECK(std::memcmp(camera,before.data(),before.size())==0&&std::memcmp(placeable,beforeP.data(),beforeP.size())==0&&drawFov==70);
   CHECK(screen[2]==1920&&screen[3]==1080&&Read<float>(scene,0x90)==0.f);
   CHECK(photo_render::requests==beforeRequests+(ok?1:0));
   CamBasis view;CHECK(BuildPhotoView(hand,view));
   CHECK(std::abs(view.forward.x-std::sin(yaw))<1e-5f);
   CHECK(view.up.z>.999f&&std::abs(view.pos.z-30.012f)<1e-5f);
   if(ok) CHECK(std::memcmp(&photo_render::queued,&view,sizeof(view))==0&&photo_render::queuedFov<20.f);
   CHECK(!BuildPhotoView(1-hand,view));
  }
  CHECK(OnPhotoCommands(&dummy,0x800)==17&&observedFov==70);
  handYaw=0;fireResult=true;
  physicalweapon::supportAngle=.45f;
  CHECK(FirePhotograph(&dummy,&dummy,scopeaim::hand));
  CHECK(std::abs(photo_render::queued.forward.x+std::sin(.45f))<1e-5f);
  CHECK(std::abs(photo_render::queued.forward.y-std::cos(.45f))<1e-5f);
  physicalweapon::supportAngle=0;
  scopeaim::hand=-1;OnPhotoCommands(&dummy,0x801);CHECK(observedFov==70);scopeaim::hand=1;
 }
 int n=fireCalls;tracked=false;CHECK(!FirePhotograph(&dummy,&dummy,1));tracked=true;camera[0x5f]=3;CHECK(!FirePhotograph(&dummy,&dummy,1));CHECK(fireCalls==n);
 using photoscreen::Sample;
 CHECK(Sample(50,0,-1,false).shutter==.5f);
 CHECK(Sample(200,0,-1,false).shutter==1.f);
 CHECK(Sample(250,0,200,true).photo&&Sample(250,0,200,true).shutter==.5f);
 CHECK(Sample(799,0,200,true).scale==1.f&&Sample(799,0,200,true).photo);
 CHECK(Sample(850,0,200,true).scale<1.f&&Sample(850,0,200,true).offset==0.f);
 CHECK(Sample(1000,0,200,true).offset>0.f&&!Sample(1100,0,200,true).photo);
 CHECK(!Sample(250,0,200,false).photo&&Sample(250,0,200,false).shutter==.5f);
 for(int turns=0;turns<4;++turns) {
  const auto motion=photoscreen::DisplayToMount({Sample(1000,0,200,true).offset,0},turns);
  const auto restored=photoscreen::DisplayToMount(motion,(4-turns)%4);
  CHECK(restored.x>0.f&&restored.y==0.f);
 }
 constexpr float wide=2.f, tall=.75f;
 const auto wideFit=photoscreen::ImageHalfExtents(wide);
 const auto tallFit=photoscreen::ImageHalfExtents(tall);
 CHECK(std::abs(wideFit.x/wideFit.y*wide-photoscreen::Aspect)<1e-5f);
 CHECK(std::abs(tallFit.x/tallFit.y*tall-photoscreen::Aspect)<1e-5f);
 std::puts("PASS: lens and two-hand steering, mission FOV, 4:3 framing, shutter timing and display orientation");
}
}
namespace charges {
constexpr int kProjectileCount=32,kHandCount=2,kWeaponSlotCount=13,kOffWeapons=0x730,kWeaponStride=0x20;
unsigned char info[32*0x38]{},ped[0xa00]{};struct{unsigned char* gaProjectileInfo=info;}g;
namespace throwable {
"""+fn('Throwable.cpp','HasLiveSatchels')+r"""
}
bool ValidSlot(int s){return s>0&&s<13;}
"""+fn('PhysicalWeapon.cpp','WeaponTypeInSlot')+r"""
int gHeldSlot[2]{12,12};struct DroppedWeapon{int slot=-1;};DroppedWeapon gDropped[2];
void ClearHeldLocked(int h){gHeldSlot[h]=-1;}bool OwnedConfiguredSlot(void*p,int s){return ValidSlot(s)&&WeaponTypeInSlot(p,s)!=0;}
"""+fn('PhysicalWeapon.cpp','ValidateInventoryLocked')+r"""
void Test(){
 int remote=40;std::memcpy(ped+0x730+12*0x20,&remote,4);int charge=39;
 CHECK(!throwable::HasLiveSatchels()&&WeaponTypeInSlot(ped,12)==0);
 for(int i:{0,7,31}){std::memcpy(info+i*0x38,&charge,4);info[i*0x38+0x1c]=1;CHECK(throwable::HasLiveSatchels()&&WeaponTypeInSlot(ped,12)==40);}
 ValidateInventoryLocked(ped);CHECK(gHeldSlot[0]==12&&gHeldSlot[1]==12);
 info[0x1c]=0;info[7*0x38+0x1c]=0;ValidateInventoryLocked(ped);CHECK(gHeldSlot[0]==12&&gHeldSlot[1]==12);
 info[31*0x38+0x1c]=0;gDropped[0].slot=12;ValidateInventoryLocked(ped);CHECK(gHeldSlot[0]==-1&&gHeldSlot[1]==-1&&gDropped[0].slot==-1&&WeaponTypeInSlot(ped,12)==0);
 info[0x1c]=1;CHECK(WeaponTypeInSlot(ped,12)==40); // new throw re-enables remote
 int grenade=16;std::memcpy(info,&grenade,4);CHECK(!throwable::HasLiveSatchels());
 g.gaProjectileInfo=nullptr;CHECK(throwable::HasLiveSatchels()); // unknown build fails open
 std::puts("PASS: zero/one/multiple live C4, first/middle/last slots, both hands and dropped remote removed only after last charge; spare inventory/stale projectile pointer cannot retain remote; new charge restores availability");
}
}
int main(){photo::Test();charges::Test();}
"""
(O/'test.cpp').write_text(code)
vswhere=Path(os.environ.get('ProgramFiles(x86)',r'C:\Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
vs=subprocess.check_output([str(vswhere),'-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
vc=Path(vs)/'VC/Auxiliary/Build/vcvars64.bat'
c=O/'run.cmd';c.write_text(f'@echo off\ncall "{vc}" >nul\ncl /nologo /std:c++20 /EHsc /W3 /D_CRT_SECURE_NO_WARNINGS /I"{R / "native/src"}" "{O / "test.cpp"}" /Fe:"{O / "test.exe"}" /Fo:"{O / "test.obj"}"\nif errorlevel 1 exit /b 1\n"{O / "test.exe"}"\n')
p=subprocess.run(['cmd','/c',str(c)],cwd=O,capture_output=True,text=True,encoding='utf8',errors='replace');print(p.stdout);print(p.stderr);sys.exit(p.returncode)

#include "RadioLabel.h"
#include "Driving.h"
#include "RadioKnobState.h"
#include "RadioRetune.h"
#include "IgnitionState.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <atomic>
#include <mutex>
namespace radio=savr::radio;
namespace driving=savr::driving;
int checks=0;
void Check(bool value,const char* message) {
    ++checks;if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
bool Near(float a,float b){return std::abs(a-b)<.00001f;}
using GLuint=unsigned;using GLint=int;using GLsizei=int;using GLboolean=bool;using GLenum=unsigned;
enum {GL_VERTEX_SHADER,GL_FRAGMENT_SHADER,GL_LINK_STATUS,GL_ARRAY_BUFFER,GL_ELEMENT_ARRAY_BUFFER,
      GL_STATIC_DRAW,GL_BLEND,GL_CULL_FACE,GL_BLEND_SRC_RGB,GL_BLEND_DST_RGB,GL_BLEND_SRC_ALPHA,
      GL_BLEND_DST_ALPHA,GL_DEPTH_WRITEMASK,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_FLOAT,
      GL_TRIANGLES,GL_UNSIGNED_SHORT};
constexpr bool GL_FALSE=false;
std::map<GLenum,int> state;
int draws=0,uploads=0;GLuint nextObject=1;
struct XrVector3f {float x,y,z;};
XrVector3f labelCenter{},labelRight{},labelUp{};
XrVector3f vadd(XrVector3f a,XrVector3f b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
XrVector3f vscale(XrVector3f a,float s){return {a.x*s,a.y*s,a.z*s};}
XrVector3f vcross(XrVector3f a,XrVector3f b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
XrVector3f vnorm(XrVector3f a){const float n=std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);return n>0?vscale(a,1/n):XrVector3f{};}
#define LOGE(...) ((void)0)
GLuint CompileHandShader(GLenum,const char*){return nextObject++;}
GLuint glCreateProgram(){return nextObject++;}
void glAttachShader(GLuint,GLuint){}void glLinkProgram(GLuint){}void glDeleteShader(GLuint){}void glDeleteProgram(GLuint){}
void glGetProgramiv(GLuint,GLenum,GLint* out){*out=1;}
GLint glGetUniformLocation(GLuint,const char* name){
    return std::string(name)=="center"?1:std::string(name)=="axisRight"?2:std::string(name)=="axisUp"?3:4;
}
void glGenBuffers(int,GLuint* out){*out=nextObject++;}
void glBindBuffer(GLenum,GLuint){}
void glBufferData(GLenum,std::size_t size,const void* data,GLenum){Check(size>0&&data,"mesh upload has data");++uploads;}
bool glIsEnabled(GLenum e){return state[e]!=0;}void glEnable(GLenum e){state[e]=1;}void glDisable(GLenum e){state[e]=0;}
void glGetIntegerv(GLenum e,GLint* out){*out=state[e];}void glGetBooleanv(GLenum e,GLboolean* out){*out=state[e]!=0;}
void glDepthMask(bool value){state[GL_DEPTH_WRITEMASK]=value;}
void glBlendFuncSeparate(int a,int b,int c,int d){state[GL_BLEND_SRC_RGB]=a;state[GL_BLEND_DST_RGB]=b;state[GL_BLEND_SRC_ALPHA]=c;state[GL_BLEND_DST_ALPHA]=d;}
void glBlendFunc(int a,int b){glBlendFuncSeparate(a,b,a,b);}
void glUseProgram(GLuint){}void glUniformMatrix4fv(int,int,bool,const float*){}
void glUniform3f(int location,float x,float y,float z){
    if(location==1)labelCenter={x,y,z};if(location==2)labelRight={x,y,z};if(location==3)labelUp={x,y,z};
}
void glUniform1f(int,float){}void glEnableVertexAttribArray(int){}void glDisableVertexAttribArray(int){}
void glVertexAttribPointer(int,int,GLenum,bool,std::size_t,const void*){}
void glDrawElements(GLenum,int count,GLenum,const void*){
    Check(count>0 && !state[GL_DEPTH_WRITEMASK] && state[GL_BLEND],"label blends without writing depth");++draws;
}
#include "RadioLabelRender.inl"
namespace integration {
using driving::WheelVisualState;
namespace ignition=savr::ignition;
#include "radio_runtime.inc"
namespace xr {
struct InputState{float grip[2]{};};
struct HandPose{bool valid{};float gripOri[4]{};};
struct Haptic{int hand;float amplitude,durationMs;};
std::vector<Haptic> haptics;
void TriggerHaptic(int hand,float amplitude,float,float durationMs){haptics.push_back({hand,amplitude,durationMs});}
}
std::array<std::uint8_t,0x400> car{},other{};
void* current=car.data();const void* bound=radio::VehicleRadioSettings(current);
int station=2,stationReads=0;
bool g_radioRetuneLayoutReady=false;
bool driver=true,transformReady=true;
V3 vehiclePosition{},cameraOffset{};
void* FindVehicle(int,bool){return current;}void* FindPed(int){return &driver;}
bool IsDriver(void*,void*){return driver;}
signed char ReadStation(void*){++stationReads;return static_cast<signed char>(station);}
void* BoundSettings(){return const_cast<void*>(bound);}
struct {
    void* AudioEngine=&driver;
    void* (*FindPlayerVehicle)(int,bool)=FindVehicle;
    void* (*FindPlayerPed)(int)=FindPed;
    bool (*CVehicle_IsDriver)(void*,void*)=IsDriver;
    signed char (*CAudioEngine_GetCurrentRadioStationID)(void*)=ReadStation;
    void* (*CAEVehicleAudioEntity_StaticGetPlayerVehicleAudioSettingsForRadio)()=BoundSettings;
    void* AERadioTrackManager{};
    void* AEUserRadioTrackManager{};
} g;
struct Calibration{bool radioPoseSet{};float radioPose[12]{};};
std::map<int,Calibration> g_modelCalibration;std::mutex g_modelMutex;
radio::KnobState g_radioKnob;int g_radioKnobHand=-1;void* g_radioKnobVehicle{};
double g_radioKnobLastTime{},testTime{};
double MonotonicSeconds(){testTime+=.02;return testTime;}
bool g_radioGripDown[2]{true,true};std::uint64_t g_radioGrabSequence{};
std::atomic<int> g_radioKnobNext{},g_radioKnobPrev{};
bool IsBicycleModel(int model){return model==481;}
V3 CockpitFingerPoint(const xr::HandPose&,int){return {};}
bool ReadIgnitionFrame(void*,bool,V3* r,V3* f,V3* u,V3* p){*r={1,0,0};*f={0,1,0};*u={0,0,1};*p=vehiclePosition;return transformReady;}
bool WorldPointToTracking(V3 world,V3* out){*out=world-cameraOffset;return transformReady;}
namespace vrcam {
bool TrackingPointToWorld(const float in[3],float out[3]){out[0]=in[0]+cameraOffset.x;out[1]=in[1]+cameraOffset.y;out[2]=in[2]+cameraOffset.z;return transformReady;}
}
#include "radio_update.inc"
void Tests() {
    WheelVisualState w;w.modelId=411;w.radius=.17f;w.right[0]=1;w.up[1]=1;w.normal[2]=-1;
    xr::InputState input;xr::HandPose hands[2]{};
    UpdateRadioKnob(w,current,input,hands,0,true);
    Check(w.radioVisible&&w.radioLocalPoseValid&&w.radioStation==2,"production update samples bound station and stores vehicle pose");
    station=7;UpdateRadioKnob(w,current,input,hands,0,true);
    Check(w.radioStation==7,"external/button station change refreshes without knob events");
    station=0;UpdateRadioKnob(w,current,input,hands,0,true);Check(w.radioStation==-1,"emergency ID is not mislabeled OFF");
    station=13;UpdateRadioKnob(w,current,input,hands,0,true);Check(w.radioVisible&&w.radioStation==13,"OFF keeps knob visible");
    g.CAudioEngine_GetCurrentRadioStationID=nullptr;UpdateRadioKnob(w,current,input,hands,0,true);
    Check(w.radioStation==-1,"temporarily unavailable reader clears old text");
    g.CAudioEngine_GetCurrentRadioStationID=ReadStation;station=3;UpdateRadioKnob(w,current,input,hands,0,true);
    Check(w.radioStation==3,"next successful update automatically recovers");
    current=other.data();const int reads=stationReads;UpdateRadioKnob(w,current,input,hands,0,true);
    Check(w.radioStation==-1&&stationReads==reads,"same-model vehicle change cannot read previous car's station");
    bound=radio::VehicleRadioSettings(current);station=5;UpdateRadioKnob(w,current,input,hands,0,true);
    Check(w.radioStation==5,"new vehicle binding restores current station");
    std::array<std::uint8_t,0x400> manager{};
    const int noAbsolute=-1,pending=2;
    std::memcpy(manager.data()+0x78,&noAbsolute,4);std::memcpy(manager.data()+0x7c,&noAbsolute,4);
    manager[0xad]=5;std::memcpy(manager.data()+0x6c,&pending,4);
    g.AERadioTrackManager=manager.data();g_radioRetuneLayoutReady=true;
    UpdateRadioKnob(w,current,input,hands,0,true);
    Check(w.radioStation==7&&station==5,"label shows pending target while previous station still plays");
    g_radioRetuneLayoutReady=false;
    UpdateRadioKnob(w,current,input,hands,0,true);
    Check(w.radioStation==5,"unrecognized retail ABI safely uses playing station");
    const V3 previous{w.radioCenter[0],w.radioCenter[1],w.radioCenter[2]};
    cameraOffset={1,2,3};vehiclePosition={10,0,0};RefreshRadioTracking(w);
    Check(w.radioVisible&&Near(w.radioCenter[0],previous.x+9)&&Near(w.radioCenter[1],previous.y-2)&&Near(w.radioCenter[2],previous.z-3),"late camera/vehicle changes reproject knob and label together");
    auto& cal=g_modelCalibration[411];cal.radioPoseSet=true;
    const float pose[]{.2f,.3f,.4f,0,1,0,0,0,1,-1,0,0};std::copy_n(pose,12,cal.radioPose);
    UpdateRadioKnob(w,current,input,hands,0,true);
    Check(Near(w.radioCenter[0],9.2f)&&Near(w.radioUp[2],1)&&!w.radioLabelBikeFrame,"debug translation/rotation controls whole assembly");
    current=car.data();RefreshRadioTracking(w);Check(!w.radioVisible,"late vehicle switch hides old assembly");
    car[0x247]=3;w.radioVisible=true;g_radioKnobNext=1;UpdateRadioKnob(w,current,input,hands,0,true);
    Check(!w.radioVisible&&g_radioKnobNext==0,"no-radio transport clears visual and queued physical event");
    car[0x247]=0;driver=false;UpdateRadioKnob(w,current,input,hands,0,true);Check(!w.radioVisible,"passenger cannot see driver control");
}
void HapticTests() {
    driver=true;cameraOffset={};vehiclePosition={};g_modelCalibration.clear();
    current=car.data();bound=radio::VehicleRadioSettings(current);g_radioKnobVehicle=current;
    for(int hand=0;hand<2;++hand) {
        g_radioKnob.Reset();g_radioKnobHand=hand;g_radioKnobNext=0;g_radioKnobPrev=0;
        xr::haptics.clear();xr::InputState input;input.grip[hand]=1;
        xr::HandPose hands[2]{};hands[hand].valid=true;
        WheelVisualState w;w.modelId=411;w.radius=.17f;w.right[0]=1;w.up[1]=1;w.normal[2]=-1;
        const auto turn=[&](float angle) {
            const float half=(-90-angle)*.01745329252f*.5f;
            hands[hand].gripOri[2]=std::sin(half);hands[hand].gripOri[3]=std::cos(half);
            UpdateRadioKnob(w,current,input,hands,0);
        };
        turn(0);Check(xr::haptics.empty(),"grabbing alone does not vibrate ratchet");
        turn(7);Check(xr::haptics.size()==1&&xr::haptics.back().amplitude==.14f&&g_radioKnobNext==0,"rotation sends tooth pulse without changing station");
        turn(23);Check(xr::haptics.size()==2&&xr::haptics.back().amplitude==.3f&&g_radioKnobNext==1,"station click has priority over tooth");
        turn(65);Check(xr::haptics.size()==3&&xr::haptics.back().amplitude==.85f&&xr::haptics.back().durationMs==75,"stop sends stronger longer pulse");
        Check(Near(w.radioAngle,1.04719755f),"rendered knob reaches sixty degrees");
        turn(80);Check(xr::haptics.size()==3&&g_radioKnobNext==1,"holding beyond stop repeats neither haptic nor station event");
        for(const auto& pulse:xr::haptics)Check(pulse.hand==hand,"only owning hand receives radio haptics");
    }
}
}
void RetuneTests() {
    std::array<std::uint8_t,0x400> manager{},user{};
    const auto write=[&](std::size_t offset,int value){std::memcpy(manager.data()+offset,&value,4);};
    write(0x78,-1);write(0x7c,-1);manager[0xad]=2;
    Check(radio::SelectedStation(nullptr,4,true)==4,"missing manager falls back to playing ID");
    write(0x6c,1);Check(radio::SelectedStation(manager.data(),2,true)==3,"next target appears before audio commits");
    write(0x6c,-1);Check(radio::SelectedStation(manager.data(),2,true)==1,"previous target appears before audio commits");
    manager[0xad]=11;write(0x6c,1);
    Check(radio::SelectedStation(manager.data(),11,false)==13,"skip unavailable user tracks forwards");
    Check(radio::SelectedStation(manager.data(),11,true)==12,"select available user tracks");
    manager[0xad]=13;write(0x6c,-1);
    Check(radio::SelectedStation(manager.data(),13,false)==11,"skip unavailable user tracks backwards");
    write(0x6c,1);Check(radio::SelectedStation(manager.data(),13,false)==1,"wrap OFF to first station");
    manager[0xad]=1;write(0x6c,-1);Check(radio::SelectedStation(manager.data(),1,false)==13,"wrap first station to OFF");
    write(0x7c,9);Check(radio::SelectedStation(manager.data(),1,false)==9,"absolute script selection overrides pending offset");
    write(0x7c,12);Check(radio::SelectedStation(manager.data(),1,false)==13,"unavailable absolute user tracks select OFF like native retune");
    write(0x7c,-1);write(0x78,4);Check(radio::SelectedStation(manager.data(),1,true)==4,"paused absolute request has correct preview");
    write(0x78,-1);write(0x6c,0);manager[0xad]=7;
    Check(radio::SelectedStation(manager.data(),13,false)==7,"loading new selected station does not flash playing OFF");
    write(0x6c,0x7fffffff);Check(radio::SelectedStation(manager.data(),3,true)==3,"invalid offset cannot overflow preview");
    Check(!radio::UserTracksAvailable(user.data()),"empty user catalogue detected");
    user[0xa]=1;Check(radio::UserTracksAvailable(user.data()),"nonempty catalogue detected");
    radio::KnobState knob;
    Check(knob.Step(true,0)==0,"start one continuous grip");
    manager[0xad]=2;write(0x6c,0);
    for(int i=1;i<=3;++i) {
        Check(knob.Step(true,22)==1,"another physical detent requires no release or audio acknowledgement");
        write(0x6c,i);
        Check(radio::SelectedStation(manager.data(),2,false)==2+i,"each detent advances pending target from same playing station");
        Check(knob.Step(true,0)==0 && knob.held,"return to center rearms while grip remains held");
    }
    std::array<std::uint32_t,0x18c/4> code{};
    code[0]=0x3943a408u;code[0x4c/4]=0xb9406e68u;code[0x100/4]=0xb9407e68u;code[0x108/4]=0x39c2b66au;
    for(const auto& patch:radio::kRetunePatches)code[patch.offset/4]=patch.before;
    Check(radio::RetuneLayoutMatches(code.data()),"verified stock timing layout accepted");
    for(const auto& patch:radio::kRetunePatches)code[patch.offset/4]=patch.after;
    Check(radio::RetuneLayoutMatches(code.data()),"installed timing patch is idempotent");
    Check(((code[0x15c/4]>>10)&0xfff)==350,"ARM64 input settle immediate is 350 milliseconds");
    for(auto offset:{0x16c,0x188})Check(((code[offset/4]>>5)&0xffff)==500,"both restart immediates are 500 milliseconds");
    code[0x15c/4]^=1;Check(!radio::RetuneLayoutMatches(code.data()),"unexpected retail instruction blocks patch");
    Check(!radio::RetuneLayoutMatches(nullptr),"missing symbol blocks patch safely");
}
int main(int argc,char** argv) {
    integration::Tests();
    integration::HapticTests();
    RetuneTests();
    Check(std::string(radio::StationName(13))=="RADIO OFF","retail OFF ID");
    for(int id:{-999,-1,0,14,127,255})Check(!*radio::StationName(id),"invalid/emergency stream is not OFF");
    for(unsigned type=0;type<256;++type)Check(radio::HasTunableRadio(type)==(type==0),"only civilian music radio is tunable");
    std::array<std::uint8_t,0x400> car{},other{};
    Check(!radio::VehicleHasTunableRadio(nullptr),"no vehicle has no radio");
    car[0x247]=3;Check(!radio::VehicleHasTunableRadio(car.data()),"emergency radio hides knob");
    car[0x247]=0;Check(radio::VehicleHasTunableRadio(car.data()),"civilian radio remains available independently of station");
    Check(radio::VehicleRadioSettings(car.data())!=radio::VehicleRadioSettings(other.data()),"different car instances have distinct bindings");
    for(int id=1;id<=13;++id) {
        const auto mesh=radio::BuildLabelMesh(id);
        Check(mesh.width<.24f && mesh.width>0 && mesh.height==.026f,"all names fit fixed pixel size");
        for(const char* c=radio::StationName(id);*c;++c) {
            const auto glyph=radio::LabelGlyph(*c);
            if(*c!=' ')Check(std::any_of(glyph.begin(),glyph.end(),[](auto row){return row!=0;}),"all station glyphs exist");
        }
        for(const auto& v:mesh.vertices)Check(std::abs(v.x)<=mesh.width*.5f+.00001f && std::abs(v.y)<=mesh.height*.5f+.00001f,"all pixels and backdrop remain inside plate");
        for(auto index:mesh.indices)Check(index<mesh.vertices.size(),"mesh indices are in bounds");
        for(std::size_t i=0;i<mesh.indices.size();i+=3) {
            const auto a=mesh.vertices[mesh.indices[i]],b=mesh.vertices[mesh.indices[i+1]],c=mesh.vertices[mesh.indices[i+2]];
            Check(std::abs((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x))>1e-10f,"all triangles have area");
        }
    }
    Check(radio::BuildLabelMesh(-1).vertices.empty(),"unknown state has no stale mesh");
    state={{GL_BLEND,0},{GL_CULL_FACE,1},{GL_DEPTH_WRITEMASK,1},{GL_BLEND_SRC_RGB,51},
        {GL_BLEND_DST_RGB,52},{GL_BLEND_SRC_ALPHA,53},{GL_BLEND_DST_ALPHA,54}};
    const auto before=state;
    driving::WheelVisualState w;w.radioVisible=true;w.radioStation=2;w.radioCenter[0]=1;
    w.radioCenter[1]=2;w.radioCenter[2]=3;w.radioRight[0]=1;w.radioUp[1]=1;w.radioNormal[2]=-1;
    float mvp[16]{};DrawRadioLabel(mvp,w);
    Check(Near(labelCenter.x,1)&&Near(labelCenter.y,2.08f)&&Near(labelCenter.z,3.002f),"car plate is above stationary base");
    Check(state==before,"blend/cull/depth write state restored");
    Check(draws==1 && uploads==2,"first eye uploads one station mesh");
    w.radioAngle=.5f;DrawRadioLabel(mvp,w);
    Check(draws==2 && uploads==2 && Near(labelUp.y,1),"other eye reuses mesh; turning knob does not rotate text");
    w.radioStation=-1;DrawRadioLabel(mvp,w);Check(draws==2,"temporary unknown hides stale station");
    w.radioStation=13;DrawRadioLabel(mvp,w);Check(draws==3&&uploads==4,"valid state recovers and OFF has own mesh");
    w.radioVisible=false;DrawRadioLabel(mvp,w);Check(draws==3,"invisible knob hides label");
    w.radioVisible=true;w.radioLabelBikeFrame=true;
    w.radioNormal[1]=0;w.radioNormal[2]=-1;w.radioUp[1]=1;
    DrawRadioLabel(mvp,w);Check(Near(labelUp.z,1)&&Near(labelCenter.z,3.08f),"default bike label stands above horizontal knob");
    w.radioLabelBikeFrame=false;w.radioUp[1]=0;w.radioUp[2]=1;
    DrawRadioLabel(mvp,w);Check(Near(labelUp.z,1),"custom debug pose supplies label up");
    if(argc>1) {
        const auto mesh=radio::BuildLabelMesh(10);
        FILE* out=std::fopen(argv[1],"w");Check(out!=nullptr,"open GPU mesh export");
        std::fprintf(out,"{\"vertices\":[");
        for(std::size_t i=0;i<mesh.vertices.size();++i){const auto v=mesh.vertices[i];std::fprintf(out,"%s%.9g,%.9g,%.9g",i?",":"",v.x,v.y,v.ink);}
        std::fprintf(out,"],\"indices\":[");
        for(std::size_t i=0;i<mesh.indices.size();++i)std::fprintf(out,"%s%u",i?",":"",unsigned(mesh.indices[i]));
        std::fprintf(out,"]}");std::fclose(out);
    }
    std::printf("PASS: %d radio label, glyph, geometry and renderer checks\n",checks);
}

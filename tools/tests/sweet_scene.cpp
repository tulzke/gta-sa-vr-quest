#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
namespace savr {GameSymbols g{};namespace modelsets {bool Applied(int){return false;}}}
using namespace savr;using namespace savr::missionscenes;
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"FAIL %d %s\n",__LINE__,#e);std::exit(3);}}while(0)
double mono=100;
double TestNow(){return mono;}
std::uint8_t thread[0x200]{},variables[1024*4]{},camera[0x100]{},pad[0x120]{};
std::uint32_t gameTime=5000;
bool paused=true;
int queries=0,actors=0;
unsigned char Loading(void*,unsigned char slot){CHECK(slot<2);++queries;return slot?0:1;}
bool Finished(void*,unsigned char slot){CHECK(slot<2);++queries;return slot==0;}
int Event(void*,unsigned char slot){CHECK(slot<2);++queries;return 29137+slot;}
void* Ped(int handle){++actors;CHECK(handle==10||handle==11);return thread;}
bool Talking(void* ped){CHECK(ped==thread);return true;}
void* Pad(int){return pad;}
GameSymbols::Vec3 Position(int){return {2770.1157f,-1915.8955f,11.651f};}
template<class T>void Put(void* p,unsigned offset,T value){std::memcpy(static_cast<unsigned char*>(p)+offset,&value,sizeof(value));}
void Local(unsigned i,int value){Put(variables,i*4,value);}
std::string Log(){std::ifstream f("vr_sweet5.log");return {std::istreambuf_iterator<char>(f),{}};}
void Step(double seconds){mono+=seconds;Update();}
int fallback=0,collects=0,compares=0;bool compared=false;
int params[2]{};
void Collect(void* s,short count){
 CHECK(count==2);++collects;auto* ip=Read<std::uint8_t*>(s,0x20);
 CHECK(ip[0]==3 && ip[3]==4);
 params[0]=Read<int>(variables,Read<std::uint16_t>(ip,1)*4);params[1]=ip[4];
 Put(s,0x20,ip+5);
}
void Compare(void*,unsigned char value){++compares;compared=value!=0;}
std::int8_t Original(void* s,int opcode){
 ++fallback;
 if(opcode==0x39){Collect(s,2);Compare(s,params[0]==params[1]);return 0;}
 return 7;
}
bool Gate(std::vector<std::uint8_t>& code,int scene,int line,int remaining,int next){
 Local(156,scene);Local(360,line);Local(362,remaining);Local(361,next);
 const int offset=scene==3?2399:2569;Put(thread,0x20,code.data()+offset+2);
 const auto beforeFallback=fallback,beforeCollects=collects,beforeCompares=compares;
 const std::vector<std::uint8_t> beforeLocals(variables,variables+sizeof(variables));
 CHECK(OnSweetCommands(thread,0x39)==0);
 CHECK(collects==beforeCollects+1 && compares==beforeCompares+1 && fallback==beforeFallback);
 CHECK(Read<std::uint8_t*>(thread,0x20)==code.data()+offset+7);
 CHECK(std::equal(beforeLocals.begin(),beforeLocals.end(),variables));
 return compared;
}
void CameraRegression(std::vector<std::uint8_t>& code){
 Local(135,1);collectParameters=Collect;updateCompareFlag=Compare;originalSweetCommands=Original;
 CHECK(!Gate(code,3,1,7,30));CHECK(!Gate(code,3,2,7,2));
 CHECK(Gate(code,3,3,6,3));CHECK(Gate(code,3,4,5,4));CHECK(Gate(code,3,8,1,8));
 CHECK(!Gate(code,3,0,7,30));CHECK(!Gate(code,3,0,0,30));
 CHECK(Gate(code,3,0,0,8)); // whole dialogue finished while waiting for the camera
 CHECK(!Gate(code,5,6,3,6));CHECK(Gate(code,5,7,2,7));CHECK(Gate(code,5,8,1,8));
 CHECK(Gate(code,5,0,0,8));CHECK(!Gate(code,5,0,1,8));
 // Replay the captured timing at the camera gates. The native timers stay in
 // charge: line 3 only exists before 7919 ms, so exact equality misses it.
 int scene=3,due=7919,originalScene=3;
 for(const auto sample:{std::pair<int,int>{6260,3},{7616,3},{7629,4},{9138,4},
                       {9178,5},{10988,6},{14233,7},{15978,8},{17424,0}}){
  const auto [timer,line]=sample;
  if(timer>7919 && line==3)originalScene=4;
  if(scene==3 && timer>due && Gate(code,3,line,line?1:0,line?line:8)){scene=4;due=timer+4000;}
  if(scene==4 && timer>due)scene=5;
  if(scene==5 && Gate(code,5,line,line?1:0,line?line:8))scene=21;
 }
 CHECK(originalScene==3 && scene==21); // old game sticks; fixed game reaches hang-up
 // Every other equality/opcode/thread/version follows the native handler.
 const int before=fallback;Local(156,4);Put(thread,0x20,code.data()+2401);OnSweetCommands(thread,0x39);
 CHECK(fallback==before+1);
 CHECK(OnSweetCommands(thread,0x38)==7 && fallback==before+2);
 Local(156,3);Put(thread,0x20,code.data()+2401);Local(135,2);OnSweetCommands(thread,0x39);CHECK(fallback==before+3);
 Local(135,1);Put(thread,0x20,code.data()+2401);Put<std::uint8_t>(thread,0x12c,0);OnSweetCommands(thread,0x39);CHECK(fallback==before+4);
 Put(thread,0x20,code.data()+2401);Put<std::uint8_t>(thread,0x12c,1);sweet.valid=false;OnSweetCommands(thread,0x39);CHECK(fallback==before+5);sweet.valid=true;
 Put(thread,0x20,code.data()+2401);code[2405]=4;OnSweetCommands(thread,0x39);CHECK(fallback==before+6);code[2405]=3;
 CHECK(Read<std::uint16_t>(code.data(),2399)==0x39 && Read<std::uint16_t>(code.data(),2569)==0x39);
 std::puts("PASS: captured missed-line race reproduces with equality, reaches hang-up with fix; exact/late/completed dialogue gates, native operand consumption, unchanged locals/bytecode and unrelated-handler fallback");
}
int main(){
 std::ifstream f("sweet5.scm",std::ios::binary);
 std::vector<std::uint8_t> code{std::istreambuf_iterator<char>(f),{}};CHECK(code.size()==15739);
 std::remove(kSweetLog);std::remove(kSweetPrevious);
 std::strcpy(reinterpret_cast<char*>(thread)+0x10,"SWEET5");
 Put(thread,0x18,code.data());Put(thread,0x20,code.data()+329);
 Put<std::uint8_t>(thread,0x12c,1);
 void* list=thread;scripts=&list;missionLocals=variables;
 audioEngine=thread;audioLoading=Loading;audioFinished=Finished;audioEvent=Event;getPed=Ped;pedTalking=Talking;
 g.CTimer_m_snTimeInMilliseconds=&gameTime;g.CTimer_m_UserPause=&paused;
 g.FindPlayerCoors=Position;g.CPad_GetPad=Pad;g.TheCamera=camera;
 Local(135,1);Local(156,5);Local(360,6);Local(363,2);Local(40,10000);Local(43,9000);
 Local(83,10);Local(51,11);Local(414,10);Local(412,0);
 const auto originalCode=code;
 std::vector<std::uint8_t> originalLocals(variables,variables+sizeof(variables));
 std::vector<std::uint8_t> originalThread(thread,thread+sizeof(thread));
 Update();CHECK(sweet.valid && queries==6 && actors==3);
 const auto first=Log();CHECK(first.find("scene=5 line=6 audio_phase=2")!=std::string::npos);
 CHECK(first.find("slot=1 event=29137 loading=1 finished=1")!=std::string::npos);
 CHECK(first.find("slot=2 event=29138 loading=0 finished=0")!=std::string::npos);
 CHECK(first.find("pause=1/-1")!=std::string::npos);
 Step(1.9);CHECK(queries==6 && Log()==first);
 Step(.2);CHECK(queries==12 && gameTime==5000); // heartbeat while paused game time is frozen
 CHECK(code==originalCode && std::equal(originalLocals.begin(),originalLocals.end(),variables));
 CHECK(std::equal(originalThread.begin(),originalThread.end(),thread));
 CameraRegression(code);CHECK(code==originalCode);
 Local(135,1);Local(156,5);Local(360,6);Local(363,2);
 Local(360,7);Step(.01);CHECK(queries==18); // dialogue change captured immediately
 Local(135,2);Step(.01);CHECK(queries==24); // return to gameplay once
 Local(360,8);Step(10);CHECK(queries==24); // later dialogue is outside opening scope
 list=nullptr;Update();CHECK(!sweet.valid && Log().find("END")!=std::string::npos);
 // Reject changed bytecode and non-mission threads without querying any actors/audio.
 code[500]^=1;list=thread;Step(1);CHECK(!sweet.valid && queries==24);
 CHECK(Log().find("UNSUPPORTED BYTECODE")!=std::string::npos);
 list=nullptr;Update();code[500]^=1;Put<std::uint8_t>(thread,0x12c,0);list=thread;Update();CHECK(queries==24);
 Put<std::uint8_t>(thread,0x12c,1);missionLocals=nullptr;Update();CHECK(queries==24);
 std::puts("PASS: retail SWEET5 fingerprint; native audio slots 0/1; opening transitions, wall-clock heartbeat during pause, no script/locals/thread writes; changed/unrelated scripts and missing locals rejected");
}

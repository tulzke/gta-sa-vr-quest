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

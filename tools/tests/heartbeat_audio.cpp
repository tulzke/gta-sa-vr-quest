#include "HeartbeatPcm.h"
#include <fstream>
#include <iterator>
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(c) do { if(!(c)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c);std::exit(1); } } while(false)
savr::heartbeataudio::detail::Pcm LoadPcm() {
    std::ifstream file("HEARTBEAT_PATH",std::ios::binary);
    std::vector<unsigned char> wav((std::istreambuf_iterator<char>(file)),{});
    auto pcm=savr::heartbeataudio::detail::Decode(wav.data(),wav.size());
    CHECK(pcm.rate==44100&&pcm.samples.size()==246556);
    CHECK(pcm.samples.front()==0&&pcm.samples.back()==0);
    CHECK(savr::heartbeataudio::detail::Decode(wav.data(),12).samples.empty());
    auto corrupt=wav;corrupt[0]='X';
    CHECK(savr::heartbeataudio::detail::Decode(corrupt.data(),corrupt.size()).samples.empty());
    CHECK(*std::max_element(pcm.samples.begin(),pcm.samples.end())>100);
    return pcm;
}
// PRODUCTION_AUDIO
using namespace savr::heartbeataudio;
void* ctx=reinterpret_cast<void*>(1);
unsigned nextSource=10,nextBuffer=20;
int plays{},stops{},uploads{},sourceCount{},bufferCount{},status{};
float gain{};
int main() {
    api.context=[]() { return ctx; };
    api.genSources=[](int n,unsigned* id) { CHECK(n==1);*id=++nextSource;++sourceCount; };
    api.genBuffers=[](int n,unsigned* id) { CHECK(n==1);*id=++nextBuffer;++bufferCount; };
    api.isSource=[](unsigned id)->char { return id==nextSource; };
    api.isBuffer=[](unsigned id)->char { return id==nextBuffer; };
    api.bufferData=[](unsigned id,int format,const void*,int size,int rate) {
        CHECK(id==nextBuffer&&format==0x1101&&size==493112&&rate==44100);++uploads;
    };
    api.sourcei=[](unsigned id,int param,int value) {
        CHECK(id==nextSource);
        if(param==0x1009) CHECK(value==static_cast<int>(nextBuffer));
        else CHECK((param==0x202||param==0x1007)&&value==1);
    };
    api.source3f=[](unsigned,int param,float x,float y,float z) { CHECK(param==0x1004&&x==0&&y==0&&z==0); };
    api.sourcef=[](unsigned,int param,float value) {
        if(param==0x100a) { CHECK(value>0&&value<=.4f);gain=value; }
        else CHECK((param==0x1003&&value==1)||(param==0x1021&&value==0));
    };
    api.getSourcei=[](unsigned,int param,int* out) { CHECK(param==0x1010);*out=status; };
    api.play=[](unsigned) { ++plays;status=0x1012; };
    api.stop=[](unsigned) { ++stops;status=0x1014; };
    api.ready=true;
    Update(0);CHECK(sourceCount==0);
    Update(.2f);CHECK(plays==1&&uploads==1&&sourceCount==1&&bufferCount==1);
    const float quiet=gain;Update(1);CHECK(gain>quiet&&plays==1&&uploads==1);
    Update(0);CHECK(stops==1);Update(0);CHECK(stops==1);
    Update(.5f);CHECK(plays==2&&sourceCount==1&&uploads==1);
    Update(std::numeric_limits<float>::quiet_NaN());CHECK(stops==2);
    ctx=nullptr;Update(1);CHECK(plays==2);
    ctx=reinterpret_cast<void*>(2);status=0;Update(1);
    CHECK(plays==3&&sourceCount==2&&uploads==2&&stops==2);
    // Native reset of a source is recovered without restarting on every frame.
    status=0x1014;Update(1);CHECK(plays==4&&sourceCount==2);
    Update(0);CHECK(stops==3);
    std::puts("PASS: actual BWF PCM24 decoding, mono conversion, loop edges and OpenAL ownership/start/stop/reset");
}

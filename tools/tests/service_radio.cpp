#include "ServiceRadio.h"
#include "ServiceRadioHand.h"
#include "ServiceRadioModel.h"
#include "ServiceRadioMission.h"
#include <fstream>
#include <iterator>
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace savr;
using namespace savr::serviceradio;
int checks{};
#define CHECK(e) do{++checks;if(!(e)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#e);std::exit(3);}}while(0)
int main(int argc,char** argv){
    CHECK(argc==2);
    std::vector<std::uint8_t> scriptSpace(90000),thread(0x140);
    void* active=thread.data();CHECK(!RetailReady(nullptr,&active));CHECK(!RetailReady(scriptSpace.data(),nullptr));
    CHECK(!ReadMission(scriptSpace.data(),&active).known);
    for(unsigned at:{83951u,84214u,84473u,84719u}){
        const std::uint8_t op[]{0x52,0x0a,4,70};std::memcpy(scriptSpace.data()+at,op,4);}
    CHECK(ReadMission(scriptSpace.data(),&active).known);
    thread[0x12c]=1;std::memcpy(thread.data()+0x10,"AMBULAN",8);
    auto observed=ReadMission(scriptSpace.data(),&active);CHECK(observed.active==Service::Medic&&observed.token==active);
    std::memcpy(thread.data()+0x10,"COPCAR\0",8);CHECK(ReadMission(scriptSpace.data(),&active).active==Service::Police);
    std::memcpy(thread.data()+0x10,"FIRETRU",8);CHECK(ReadMission(scriptSpace.data(),&active).active==Service::Fire);
    std::memcpy(thread.data()+0x10,"STORY\0\0",8);scriptSpace[0x660]=1;
    observed=ReadMission(scriptSpace.data(),&active);CHECK(observed.onMission&&observed.active==Service::None);
    thread[0x12c]=0;std::memcpy(thread.data()+0x10,"COPCAR\0",8);CHECK(ReadMission(scriptSpace.data(),&active).active==Service::None);
    scriptSpace[83951]=0;CHECK(!ReadMission(scriptSpace.data(),&active).known);
    tracking::SeatKey key{};key.vehicle=reinterpret_cast<void*>(1);key.ped=reinterpret_cast<void*>(2);key.model=416;key.seat=-1;key.generation=1;
    Mission idle{true,false,Service::None,nullptr},medic{true,true,Service::Medic,reinterpret_cast<void*>(3)};
    Hand hands[2]{{true,true,true,1,1},{true,true,false,1,1}};
    State s;
    s.Step(true,key,hands,idle,0);CHECK(s.owner==-1&&!s.pending); // held during entry
    for(auto& h:hands){h.grip=0;h.trigger=0;}
    s.Step(true,key,hands,idle,.01);hands[0].grip=1;hands[0].trigger=1;
    s.Step(true,key,hands,idle,.02);CHECK(s.owner==0&&!s.pending&&s.TriggerMask()==1);
    s.Step(true,key,hands,idle,.03);CHECK(!s.pending); // held at grab is not PTT
    hands[0].trigger=0;s.Step(true,key,hands,idle,.04);
    hands[0].trigger=1;s.Step(true,key,hands,idle,.05);CHECK(s.pending&&!s.cancel&&s.pressed);
    CHECK(!s.Widget(72,key,idle,.051));CHECK(s.Widget(70,key,idle,.052));CHECK(s.Widget(70,key,idle,.052));
    CHECK(s.Widget(70,key,idle,.10));CHECK(!s.Widget(70,key,idle,.18));CHECK(!s.pending);
    for(int i=0;i<120;++i)s.Step(true,key,hands,medic,.2+i*.01);
    CHECK(!s.pending); // holding after start never cancels
    hands[0].trigger=0;s.Step(true,key,hands,medic,2);
    hands[0].trigger=1;s.Step(true,key,hands,medic,2.01);CHECK(s.pending&&s.cancel);
    CHECK(!s.Widget(70,key,medic,2.02));CHECK(s.Widget(72,key,medic,2.03));CHECK(s.Widget(72,key,medic,2.05));
    // A replacement mission with the same name must not receive an old cancel.
    auto other=medic;other.token=reinterpret_cast<void*>(4);CHECK(!s.Widget(72,key,other,2.06));
    hands[0].trigger=0;s.Step(true,key,hands,idle,3);
    hands[0].trigger=1;s.Step(true,key,hands,idle,3.01);CHECK(s.pending);
    auto reused=key;++reused.generation;CHECK(!s.Widget(70,reused,idle,3.02));
    s.Step(true,reused,hands,idle,3.03);CHECK(s.owner==-1&&!s.pending);
    // Context loss + trigger held: no grab and no pedal until trigger release.
    hands[0].grip=0;s.Step(true,key,hands,idle,4);hands[0].grip=1;s.Step(true,key,hands,idle,4.01);
    CHECK(s.owner==0);s.Step(false,key,hands,idle,4.02);CHECK(s.owner==-1&&(s.TriggerMask()&1));
    float trigger[2]{1,0};s.Observe(trigger);CHECK(s.TriggerMask()&1);
    trigger[0]=0;s.Observe(trigger);CHECK(!s.TriggerMask());
    s.Step(true,key,hands,idle,4.03);CHECK(s.owner==-1);
    // Free-hand ownership and tracking gate.
    hands[0].grip=0;s.Step(true,key,hands,idle,5);hands[0].grip=1;hands[0].free=false;s.Step(true,key,hands,idle,5.01);CHECK(s.owner==-1);
    hands[0].free=true;hands[0].grip=0;s.Step(true,key,hands,idle,5.02);hands[0].grip=1;s.Step(true,key,hands,idle,5.03);CHECK(s.owner==0);
    hands[0].valid=false;s.Step(true,key,hands,idle,5.04);CHECK(s.owner==-1);
    hands[0].valid=true;hands[0].grip=0;hands[0].trigger=0;s.Step(true,key,hands,idle,6);
    hands[0].grip=1;s.Step(true,key,hands,idle,6.01);
    Mission story{true,true,Service::None,reinterpret_cast<void*>(9)};
    hands[0].trigger=1;s.Step(true,key,hands,story,6.02);CHECK(!s.pending);
    hands[0].trigger=0;s.Step(true,key,hands,idle,7);hands[0].trigger=1;s.Step(true,key,hands,idle,7.01);
    CHECK(s.pending);CHECK(!s.Widget(70,key,idle,7.3)); // never wait for a later availability window
    auto unsupported=key;unsupported.model=400;s.Step(true,unsupported,hands,idle,8);CHECK(s.owner==-1&&!s.pending);
    State lost;Hand tracked[2]{{true,true,true,0,0},{}};
    lost.Step(true,key,tracked,idle,9);tracked[0].grip=1;lost.Step(true,key,tracked,idle,9.01);
    tracked[0].trigger=1;lost.Step(true,key,tracked,idle,9.02);CHECK(lost.pending);
    tracked[0].valid=false;lost.Step(true,key,tracked,idle,9.03);CHECK(!lost.pending&&lost.owner==-1);
    CHECK(!lost.Widget(70,key,idle,9.04));
    State quick;Hand taps[2]{{true,true,true,0,0},{}};
    quick.Step(true,key,taps,idle,10);taps[0].grip=1;quick.Step(true,key,taps,idle,10.01);
    taps[0].trigger=1;quick.Step(true,key,taps,idle,10.02);CHECK(quick.Widget(70,key,idle,10.03));
    quick.Step(true,key,taps,medic,10.16);taps[0].trigger=0;quick.Step(true,key,taps,medic,10.17);
    taps[0].trigger=1;quick.Step(true,key,taps,medic,10.18);CHECK(quick.pending&&quick.cancel);
    // Both hands produce a rigid, proper (unmirrored) radio frame.
    for(int h=0;h<2;++h){xr::HandPose p{};p.valid=true;p.aimValid=true;
        for(auto& v:p.gripPos)v=.1f;
        const auto held=HeldFrame(p,h);CHECK(radio::Dot(handrender::Cross(held.right,held.forward),held.up)>.999f);
        Frame dock;dock.forward={0,1,0};dock.up={0,0,1};
        for(float t:{0.f,.25f,.5f,.75f,1.f}){const auto mixed=Blend(dock,held,t);
            CHECK(std::isfinite(mixed.center.x));CHECK(radio::Dot(handrender::Cross(mixed.right,mixed.forward),mixed.up)>.999f);}
    }
    std::ifstream file(argv[1],std::ios::binary);std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(file),{}};
    Model model;CHECK(model.Load(bytes.data(),bytes.size()));CHECK(model.indices.size()==8210*3);
    bool role[4]{};for(const auto& v:model.vertices)role[int(v.role)]=true;CHECK(role[0]&&role[1]&&role[2]&&role[3]);
    for(unsigned i:{0u,4u,8u,12u}){auto bad=bytes;bad[i]=255;CHECK(!model.Load(bad.data(),bad.size()));}
    auto bad=bytes;const float nan=std::numeric_limits<float>::quiet_NaN();std::memcpy(bad.data()+16,&nan,4);CHECK(!model.Load(bad.data(),bad.size()));
    CHECK(!model.Load(bytes.data(),bytes.size()-1));
    std::printf("PASS: %d service radio state, routing, both-hand frame and asset checks\n",checks);
}

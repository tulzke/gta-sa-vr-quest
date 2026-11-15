#include "VehicleLiftLever.h"
#include "VehicleLiftLeverModel.h"
#include "VehicleLiftLeverHand.h"
#include "CockpitSelection.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <iterator>
using namespace savr::liftlever;
namespace vehicledoors=savr::vehicledoors;
int checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
Point At(float angle,float x=0){const float a=angle*kRad;return {x,-kReach*std::sin(a),kReach*std::cos(a)};}
bool Near(float a,float b){return std::abs(a-b)<1e-5f;}
int main(int argc,char** argv){
    CHECK(argc==2);
    CHECK(Supported(486)&&Supported(525)&&Supported(530)&&Supported(531));
    CHECK(!Supported(400)&&!Supported(520)&&!Supported(432));
    for(int model:{486,525,530,531}){
        const int sign=model==525||model==531?-1:1;
        CHECK(RetailAxis(model,1)==128*sign);CHECK(RetailAxis(model,-1)==-128*sign);
        CHECK(RetailAxis(model,0,true)==0);CHECK(RetailAxis(model,.001f,true)==11*sign);
    }
    CHECK(Command(2*kRad)==0&&Near(Command(25*kRad),1)&&Near(Command(-25*kRad),-1));
    CHECK(Near(Command(14*kRad),.5f));
    {
        CHECK(kHandFit[0][1]==-.5f&&kHandFit[0][4]==-180.f&&kHandFit[0][5]==180.f);
        CHECK(kHandFit[1][1]==-.5f&&kHandFit[1][4]==-180.f&&kHandFit[1][5]==0.f);
        const vehicledoors::Basis lever{{0,1,0},{-1,0,0},{0,0,1},{4,7,2}};
        for(int hand:{0,1})for(float angle:{-25.f,0.f,25.f}) {
            const float a=angle*kRad;
            const auto movingF=lever.forward*std::cos(a)+lever.up*std::sin(a);
            const auto movingU=lever.up*std::cos(a)-lever.forward*std::sin(a);
            const auto expected=lever.position+lever.up*.05f+movingU*.18f+lever.right*.01f-movingF*.005f;
            const auto frame=HandFrame(lever,a,.01f,hand);
            const auto opening=frame.position+savr::vehicleclimb::GripPivotOffset(frame.forward,frame.up);
            CHECK(vehicledoors::Length(opening-expected)<1e-5f);
            CHECK(vehicledoors::Length(frame.forward-movingF)<1e-5f);
            CHECK(vehicledoors::Length(frame.up-movingU)<1e-5f);
            CHECK(vehicledoors::Length(frame.right-lever.right*(hand==0?1.f:-1.f))<1e-5f);
        }
    }
    Hand hands[2]{{At(3,.01f),1,true,true},{At(0),0,true,true}};
    State s;s.Step(hands,.01f,true);CHECK(s.owner==-1); // held on entry is not a grab
    hands[0].grip=0;s.Step(hands,.01f,true);hands[0].grip=.7f;s.Step(hands,.01f,true);
    CHECK(s.owner==0&&Near(s.angle,0)&&Near(s.Output(),0)); // preserve initial touch offset
    hands[0].point=At(28,.01f);s.Step(hands,.01f,true);CHECK(Near(s.angle,kTravel)&&Near(s.Output(),1));
    hands[1].grip=1;s.Step(hands,.01f,true);CHECK(s.owner==0); // no steal
    hands[0].grip=0;s.Step(hands,.01f,true);CHECK(s.owner==-1&&s.Output()==0&&s.angle>0);
    s.Step(hands,.1f,true);s.Step(hands,.02f,true);CHECK(Near(s.angle,0)&&s.owner==-1);
    // A held second hand cannot capture merely because ownership became free.
    CHECK(s.owner==-1);hands[1].grip=0;s.Step(hands,.01f,true);
    hands[1].grip=.7f;s.Step(hands,.01f,true);CHECK(s.owner==1);
    hands[1].point=At(-25);s.Step(hands,.01f,true);CHECK(Near(s.Output(),-1));
    hands[1].valid=false;s.Step(hands,.01f,true);CHECK(s.owner==-1&&s.Output()==0);
    hands[1].valid=true;s.Step(hands,.01f,true);CHECK(s.owner==-1); // new press required
    hands[1].grip=0;s.Step(hands,.1f,true);hands[1].point=At(0);
    hands[1].grip=1;s.Step(hands,.01f,true);CHECK(s.owner==1);
    hands[1].available=false;s.Step(hands,.01f,true);CHECK(s.owner==-1&&s.Output()==0);
    hands[1].available=true;s.Step(hands,.01f,true);CHECK(s.owner==-1);
    s.Step(hands,.01f,false);CHECK(s.owner==-1&&s.Output()==0&&s.angle==0);
    s.Step(hands,.01f,true);CHECK(s.owner==-1);
    hands[1].grip=0;s.Step(hands,.01f,true);hands[1].grip=1;s.Step(hands,.01f,true);
    CHECK(s.owner==1);hands[1].point.x=1;s.Step(hands,.01f,true);CHECK(s.owner==-1);
    CHECK(Distance({std::numeric_limits<float>::quiet_NaN(),0,0},0)==1e6f);
    CHECK(RetailAxis(530,std::numeric_limits<float>::quiet_NaN())==0);
    for(float dt:{1.f/36,1.f/72,1.f/90,1.f/120}){
        State t;Hand h[2]{{At(0),0,true,true},{}};t.Step(h,dt,true);h[0].grip=1;t.Step(h,dt,true);
        h[0].point=At(14);t.Step(h,dt,true);CHECK(Near(t.Output(),.5f));
    }
    float d[6]{1e6f,.09f,.08f,.07f,.06f,.03f};CHECK(savr::driving::SelectCockpitTarget(d,3)==5);
    std::ifstream file(argv[1],std::ios::binary);std::vector<char> bytes{std::istreambuf_iterator<char>(file),{}};
    Model model;CHECK(model.Load(bytes.data(),bytes.size()));CHECK(model.indices.size()==1156*3);
    bool fixed=false,moving=false,red=false;
    for(const auto& v:model.vertices){fixed|=v.moving==0;moving|=v.moving==1;red|=v.moving==1&&v.color[0]>.5f&&v.color[1]<.02f;}
    CHECK(fixed&&moving&&red);
    CHECK(!model.Load(bytes.data(),bytes.size()-1)&&model.vertices.empty());
    auto bad=bytes;bad[0]=0;CHECK(!model.Load(bad.data(),bad.size()));
    bad=bytes;std::uint16_t invalid=65535;std::memcpy(bad.data()+bad.size()-2,&invalid,2);CHECK(!model.Load(bad.data(),bad.size()));
    bad=bytes;float nan=std::numeric_limits<float>::quiet_NaN();std::memcpy(bad.data()+16,&nan,4);CHECK(!model.Load(bad.data(),bad.size()));
    std::printf("PASS: %d lever state, routing, arbitration and asset checks\n",checks);
}

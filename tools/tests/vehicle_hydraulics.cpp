#include "VehicleHydraulics.h"
#include "VehicleHydraulicsModel.h"
#include "VehicleHydraulicsHand.h"
#include "HydraulicFlick.h"
#include "Symbols.h"
#include "HandSkeleton.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <vector>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
using namespace savr::hydrocontrol;
namespace savr {GameSymbols g{};}
namespace savr::vrcam {bool IsStereoActive(){return true;}}
namespace savr::hydraulics {bool dancing{};bool DanceRunning(){return dancing;}}
namespace savr::cockpitclick {int clicks{};bool last{};void Click(bool on){++clicks;last=on;}}
namespace savr::xr {
int clicks{};float clickAmplitude{},clickFrequency{},clickDuration{};
void TriggerHaptic(int,float amplitude,float frequency,float duration){++clicks;clickAmplitude=amplitude;clickFrequency=frequency;clickDuration=duration;}
}
namespace savr::driving {
std::atomic<void*> g_hydroManagedVehicle{nullptr};std::atomic<bool> g_hydroToggle{};bool g_hydroHooksReady{};
constexpr int MODE_IMMERSIVE=1;int mode=MODE_IMMERSIVE,debug{};bool allowed=true,third=false;
void* controlled{};
int ModeForAppearance(int){return mode;}int GetVehicleDebugMode(){return debug;}
bool VrGameplayInputAllowed(){return allowed;}bool ThirdPersonViewActive(){return third;}
void* GetControlledVehicle(){return controlled;}
void* InstallVerifiedTrampoline(void*,void*,const std::uint32_t*,const char*){return nullptr;}
// COMPATIBILITY_FUNCTION
// EQUIPMENT_FUNCTION
#include "VehicleHydraulicsRuntime.inl"
int hornCalls{},toggleCalls{},controlCalls{};bool queryHorn=true,acceptToggle=true;
bool OriginalHorn(void*){++hornCalls;return true;}
void OriginalControl(void* vehicle){
    ++controlCalls;if(!queryHorn)return;
    if(OnHydraulicHorn(nullptr)&&acceptToggle){++toggleCalls;
        auto& timer=*reinterpret_cast<std::uint16_t*>(static_cast<char*>(vehicle)+0xa88);timer=timer>=500?60:500;}
    if(g_hydraulicHornScope)CHECK(!OnHydraulicHorn(nullptr)); // one edge per native call
}
void OriginalBeat(void*,void*){}
bool IsCar(int m){return m==400||m==567;}
}

Point Stroke(float x,float y){auto p=Grip(x,y);p.x*=kHandStroke;p.y*=kHandStroke;return p;}
void Capture(State& s,Hand h[2],bool four=false){
    h[0]={Grip(0,0),{},0,true,true};h[1]={};
    s.Step(h,.02f,true,four);h[0].grip=1;s.Step(h,.02f,true,four);CHECK(s.owner==0);
}
bool Cardinal(const State& s,Point input){return s.slot>0&&s.slot%2==1&&Dot(s.Command(),input)>.7f;}
void Detents(){
    for(int ray=1;ray<=8;++ray){State s;Hand h[2];Capture(s,h);const auto d=Direction(ray);
        h[0].point=Stroke(d.x*kTravel,d.y*kTravel);s.Step(h,.02f,true,false);CHECK(Cardinal(s,d));
        const int latched=s.slot;CHECK(Length(s.Command()-Direction(latched))<1e-6f);
        const auto n=Direction(ray==8?1:ray+1);
        h[0].point=Stroke(0,0);s.Step(h,.02f,true,false);CHECK(s.slot==0&&Length(s.Command())==0);
        h[0].point=Stroke(n.x*kTravel,n.y*kTravel);s.Step(h,.02f,true,false);CHECK(Cardinal(s,n));
        h[0].point=Stroke(n.x*(kTravel+.01f),n.y*(kTravel+.01f));s.Step(h,.02f,true,false);CHECK(Cardinal(s,n));
        h[0].grip=0;s.Step(h,.02f,true,false);CHECK(s.owner==-1&&s.slot==0&&Length(s.Command())==0);
        for(int i=0;i<80;++i)s.Step(h,.02f,true,false);CHECK(std::hypot(s.visualX,s.visualY)<1e-5f);
    }
    State s;Hand h[2];Capture(s,h);h[0].available=false;s.Step(h,.02f,true,false);CHECK(s.owner==-1);
    h[0].available=true;s.Step(h,.02f,true,false);CHECK(s.owner==-1);
    h[0].grip=0;s.Step(h,.02f,true,false);h[0].grip=1;s.Step(h,.02f,true,false);CHECK(s.owner==0);
    h[0].point.x=std::numeric_limits<float>::quiet_NaN();s.Step(h,.02f,true,false);CHECK(s.owner==-1&&s.slot==0);
    State held;Hand q[2]{{Grip(0,0),{},1,true,true},{}};held.Step(q,.02f,true,false);CHECK(held.owner==-1);
    State path;Hand points[2];Capture(path,points);points[0].point=Stroke(0,kTravel);
    for(int i=0;i<8;++i)path.Step(points,.01f,true,false);
    points[0].point=Stroke(0,0);path.Step(points,.005f,true,false);points[0].point=Stroke(kTravel,0);
    bool sawCenter=false;
    for(int i=0;i<12;++i){path.Step(points,.005f,true,false);
        if(std::hypot(path.visualX,path.visualY)<1e-6f)sawCenter=true;
        if(path.visualX>.001f)CHECK(sawCenter);}
    CHECK(sawCenter&&path.slot==3);
    std::puts("PASS cardinal policy: only four directions plus neutral for all eight input rays; no diagonal command; fixed amplitude; fresh grip/invalid hand");
}
void ArcChanges(){
    for(int hand=0;hand<2;++hand){
        State s;InputFrame frame{{},{1,0,0},{0,0,-1},{0,1,0},2.f};Hand h[2]{};
        h[hand]={frame.World(Grip(0,0)),{},0,true,true};s.StepTracked(h,frame,.02f,true,false,4);
        h[hand].grip=1;s.StepTracked(h,frame,.02f,true,false,4);const auto origin=h[hand].point;
        for(int i=0;i<720;++i){const float a=i*kRad;const Point d{std::sin(a),std::cos(a),0};
            h[hand].point=origin+frame.Vector(d)*.09f;s.StepTracked(h,frame,.02f,true,false,4);
            CHECK(s.owner==hand&&s.slot>0&&s.slot%2==1&&Dot(s.Command(),d)>.58f);
            CHECK(!s.recenterVisual&&s.boundaryError<1e-6f);
        }
        // Settled right sector, then jitter at the diagonal: do not chatter.
        h[hand].point=origin+frame.Vector(Direction(3))*.09f;s.StepTracked(h,frame,.02f,true,false,4);
        for(int i=0;i<20;++i){const float a=(i%2?44.f:46.f)*kRad;
            h[hand].point=origin+frame.Vector({std::sin(a),std::cos(a),0})*.09f;
            s.StepTracked(h,frame,.02f,true,false,4);CHECK(s.slot==3);}
        h[hand].point=origin+frame.Vector(Direction(1))*.09f;s.StepTracked(h,frame,.02f,true,false,4);CHECK(s.slot==1);
        h[hand].point=origin;s.StepTracked(h,frame,.02f,true,false,4);CHECK(s.slot==0);
    }
    std::puts("PASS free arc transitions: two full circles each hand without neutral lock; right-to-front and sector hysteresis; no boundary warning along valid arc");
}
void BoundaryFeedback(){
    using savr::cockpitboundary::Feedback;
    Feedback feedback;CHECK(!feedback.Step(0,true,.02f).play);
    float previous=0;int pulses=0;
    for(int i=1;i<=100;++i){const auto p=feedback.Step(i*.001f,true,.02f);
        CHECK(feedback.strength>=previous-1e-6f&&feedback.strength<=.65f);previous=feedback.strength;
        if(p.play){++pulses;CHECK(p.amplitude>0&&p.amplitude<=.65f&&p.durationMs<=30);}}
    CHECK(pulses>10&&previous>.6f);
    const float strong=feedback.strength;feedback.Step(.01f,true,.02f);CHECK(feedback.strength<strong);
    CHECK(!feedback.Step(0,true,.02f).play&&feedback.strength==0);
    CHECK(!feedback.Step(.09f,true,.02f,true).play);
    for(int i=0;i<3;++i)CHECK(!feedback.Step(.09f,true,.02f).play); // click priority
    feedback.Step(.09f,true,.02f);CHECK(!feedback.Step(.09f,false,.02f).play&&feedback.strength==0);
    const float scale=2.f;
    CHECK(BoundaryDistance(Grip(0,0),scale)==0);
    CHECK(BoundaryDistance({.05f,0,kReach},scale)==0);
    CHECK(BoundaryDistance({.065f,0,kReach},scale)>BoundaryDistance({.055f,0,kReach},scale));
    CHECK(BoundaryDistance({0,0,kReach+.01f},scale)==0);
    CHECK(BoundaryDistance({0,0,kReach+.04f},scale)>.03f);
    State s;InputFrame frame{{},{1,0,0},{0,0,-1},{0,1,0},2.f};Hand h[2]{{frame.World(Grip(0,0)),{},0,true,true},{}};
    s.StepTracked(h,frame,.02f,true,false,1);h[0].grip=1;s.StepTracked(h,frame,.02f,true,false,1);
    const auto origin=h[0].point;h[0].point=origin+frame.right*.15f;s.StepTracked(h,frame,.02f,true,false,1);CHECK(s.boundaryError>.04f);
    h[0].grip=0;s.StepTracked(h,frame,.02f,true,false,1);CHECK(s.owner==-1&&s.boundaryError==0);
    std::puts("PASS hot/cold boundary: silence inside, increasing capped amplitude, fast return, short pulses and click priority, radial/height distance and release cleanup");
}
void DirectBeatFlick(){
    savr::hydraulics::Flick f;f.Step(true,0,0,1,true);f.Step(true,1,0,1,true);CHECK(f.pending==4);
    f.Step(true,1,0,2,true);CHECK(f.pending==0); // holding cannot answer next beat
    f.Step(true,0,1,2,true);CHECK(f.pending==1); // physical sector hop is a fresh action
    f.Step(true,0,1,3,false);CHECK(f.pending==0);
    f.Step(true,-1,0,3,false);CHECK(f.pending==0); // legacy input retains neutral-flick rule
    f.Step(false,0,0,3,true);CHECK(f.pending==0&&f.neutral);
    std::puts("PASS mission flick: direct physical direction change can answer a beat; holding cannot; legacy gesture remains unchanged");
}
void Buttons(){
    Button b;b.Step({0,0,-.005f},true,true,.02f);CHECK(!b.clicked&&!b.pressed);
    b.Step({0,0,.03f},true,true,.1f);CHECK(b.hover==1&&!b.clicked);
    b.Step({0,0,-.003f},true,true,.02f);CHECK(!b.clicked&&b.depth<.45f);
    b.Step({0,0,-.005f},true,true,.02f);CHECK(b.clicked&&b.pressed);
    CHECK(b.depth==1.f); // cap snaps through with the click
    b.Step({0,0,-.005f},true,true,.02f);CHECK(!b.clicked&&b.pressed);
    b.Step({0,0,.02f},true,true,.02f);b.Step({0,0,-.005f},true,true,.02f);CHECK(b.clicked);
    b.Step({0,0,-.005f},false,true,.02f);b.Step({0,0,-.005f},true,true,.02f);CHECK(!b.clicked&&!b.pressed);
    b.Step({0,0,.03f},true,true,.02f);b.Step({.04f,0,.01f},true,true,.02f);b.Step({0,0,-.005f},true,true,.02f);CHECK(!b.clicked);
    b.Step({0,0,.03f},true,true,.02f);b.Step({0,0,-.005f},true,false,.02f);CHECK(!b.clicked);
    Button hover;hover.Step({.044f,0,.079f},true,true,.1f);CHECK(hover.pointing&&hover.hover==1);
    hover.Step({.050f,0,.085f},true,true,.02f);CHECK(hover.pointing&&hover.hover==1); // exit hysteresis
    hover.Step({0,0,-.006f},true,true,.02f);CHECK(hover.pointing&&hover.hover==1&&!hover.clicked);
    hover.Step({.065f,0,.02f},true,true,.1f);CHECK(!hover.pointing&&hover.hover==0);
    std::puts("PASS button: approach is gesture only; front arming, physical travel, withdrawal, no repeats, side/rear/tracking/occupied rejection");
    Button repeat;
    for(int i=0;i<20;++i){repeat.Step({0,0,.004f},true,true,.02f);CHECK(!repeat.clicked&&!repeat.pressed);
        repeat.Step({0,0,-.005f},true,true,.02f);CHECK(repeat.clicked&&repeat.pressed&&repeat.depth>.8f);
        repeat.Step({0,0,-.005f},true,true,.02f);CHECK(!repeat.clicked&&repeat.pressed);}
    repeat.Step({0,0,-.005f},false,true,.02f);repeat.Step({0,0,-.005f},true,true,.02f);
    CHECK(!repeat.clicked&&!repeat.pressed&&repeat.depth==0); // no fake animated press after tracking loss
    std::puts("PASS MODE repeats: 20 short withdrawals, one click per poke; unarmed contact has no fake cap travel");
}
void SpringReturn(){
    for(int hand=0;hand<2;++hand)for(bool four:{false,true}){
        State s;InputFrame frame{{.3f,.2f,-.1f},{1,0,0},{0,0,-1},{0,1,0},kModelScale};
        Hand h[2]{};h[hand]={frame.World(Grip(0,0)+Point{.017f,-.012f,.006f}),{},0,true,true};
        s.StepTracked(h,frame,.02f,true,four,1);h[hand].grip=1;s.StepTracked(h,frame,.02f,true,four,1);
        const auto origin=h[hand].point,grab=s.grabOffset;
        const auto unchanged=[&]{CHECK(Length(s.grabOffset-grab)<1e-7f);CHECK(Length(s.grabFrame.pivot-frame.pivot)<1e-7f);};
        const auto ball=GripFrame(frame,s.visualX,s.visualY);
        const savr::radio::HandPose tracked{{origin.x,origin.y,origin.z},{1,0,0},{0,1,0},{0,0,1}};
        savr::hydrocontrol::HandAttachment attachment;const auto initial=attachment.Grab(ball,hand,.045f);
        for(int cycle=0;cycle<128;++cycle){const int slot=cycle%8+1;const auto d=Direction(slot);
            const auto correct=[&]{return Cardinal(s,d);};
            h[hand].point=origin+frame.Vector(d)*.24f;s.StepTracked(h,frame,.02f,true,four,1);CHECK(s.owner==hand&&correct());unchanged();
            // Returning a short distance from overtravel is NOT a new centre.
            h[hand].point=origin+frame.Vector(d)*.176f;s.StepTracked(h,frame,.02f,true,four,1);CHECK(s.owner==hand&&correct());unchanged();
            // Neutral is always the same physical zone, including its edge.
            h[hand].point=origin+frame.Vector(d)*.04f;s.StepTracked(h,frame,.02f,true,four,1);CHECK(s.owner==hand&&s.slot==0);unchanged();
            h[hand].point=origin;
            for(int i=0;i<8;++i)s.StepTracked(h,frame,.02f,true,four,1);
            CHECK(s.owner==hand&&s.slot==0&&std::hypot(s.visualX,s.visualY)<1e-6f);unchanged();
            const auto final=attachment.Grab(GripFrame(frame,s.visualX,s.visualY),hand,.045f);
            CHECK(savr::radio::Dot(final.position-initial.position,final.position-initial.position)<1e-10f);
        }
        h[hand].point.y+=.06f;s.StepTracked(h,frame,.02f,true,four,1);CHECK(s.owner==hand&&s.slot==0);unchanged();
        h[hand].point=origin;h[hand].grip=0;s.StepTracked(h,frame,.02f,true,four,1);CHECK(s.owner==-1);
        const auto released=attachment.Free(tracked,.02f);CHECK(savr::radio::Dot(released.position-initial.position,released.position-initial.position)<1e-10f);
        for(int i=0;i<12;++i)attachment.Free(tracked,.02f);
        const auto free=attachment.Free(tracked,.02f);CHECK(savr::radio::Dot(free.position-tracked.position,free.position-tracked.position)<1e-10f);
    }
    std::puts("PASS fixed coordinate zones: 128 mixed cycles/overtravel per hand and mission mode; immutable centre/offset; same hand pose and release as initial grab");
}
void HandTravel(){
    for(int hand=0;hand<2;++hand)for(int slot=1;slot<=8;++slot){
        State s;InputFrame frame{{},{1,0,0},{0,0,-1},{0,1,0},kModelScale};Hand h[2]{};
        h[hand]={frame.World(Grip(0,0)),{},0,true,true};s.StepTracked(h,frame,.02f,true,false,3);
        h[hand].grip=1;s.StepTracked(h,frame,.02f,true,false,3);CHECK(s.owner==hand);
        const auto start=h[hand].point,d=Direction(slot);
        for(float metres:{.005f,.01f,.02f,.04f,.055f,.079f}){
            h[hand].point=start+frame.Vector(d)*metres;s.StepTracked(h,frame,.02f,true,false,3);
            CHECK(s.owner==hand&&s.slot==0&&!s.changed&&Length(s.Command())==0);
            CHECK(std::hypot(s.visualX,s.visualY)>.01f&&std::hypot(s.visualX,s.visualY)<=kCenterPlay+1e-6f);
            if(metres>=.04f)CHECK(std::abs(std::hypot(s.visualX,s.visualY)-9.f*kRad)<1e-5f);}
        h[hand].point=start;s.StepTracked(h,frame,.02f,true,false,3);CHECK(s.slot==0&&std::hypot(s.visualX,s.visualY)<1e-6f);
        h[hand].point=start+frame.Vector(d)*.082f;s.StepTracked(h,frame,.02f,true,false,3);CHECK(Cardinal(s,d));
    }
    std::puts("PASS neutral free play: continuous shaft/hand pose within 9 degrees with zero command and no clicks; returns to centre");
    std::puts("PASS measured hand travel: no activation at 5-79 mm, engagement at 82 mm at default 200%, both hands/eight rays");
}
void SharedCoordinates(){
    const Point root{.4f,-.2f,.7f};
    const auto r=Swing({1,0,0},.3f,-.2f),f=Swing({0,1,0},.3f,-.2f),u=Swing({0,0,1},.3f,-.2f);
    for(float scale:{.8f,2.f,4.f})for(int slot=0;slot<=8;++slot){
        const auto pivot=MountedFrame(root,r,f,u,scale);const auto d=Direction(slot);
        const float x=d.x*kTravel,y=d.y*kTravel;
        Vertex ball{};ball.position[2]=kReach;ball.role=1;ball=Model::Deform(ball,x,y,0);
        const Point mesh=root+(r*ball.position[0]+f*ball.position[1]+u*ball.position[2])*scale;
        const auto frame=GripFrame(pivot,x,y);
        CHECK(Length(mesh-Point{frame.center.x,frame.center.y,frame.center.z})<1e-6f);
        CHECK(Length(pivot.Local(pivot.World(Grip(x,y)))-Grip(x,y))<1e-6f);
    }
    std::puts("PASS shared coordinates: mesh, hand anchor and input frame coincide under all detents, rotated mount and 80/200/400 percent scale");
}
void HeightAndSweptReturn(){
    for(int hand=0;hand<2;++hand)for(int slot:{1,3,5,7}){
        const InputFrame cockpit{{.3f,.4f,-.2f},Swing({1,0,0},.7f,.8f),Swing({0,0,-1},.7f,.8f),Swing({0,1,0},.7f,.8f),2.f};
        State s;Hand h[2]{};h[hand]={cockpit.World(Grip(0,0)),{},0,true,true};
        s.StepTracked(h,cockpit,.02f,true,false,5);h[hand].grip=1;s.StepTracked(h,cockpit,.02f,true,false,5);CHECK(s.owner==hand);
        CHECK(std::abs(s.grabFrame.right.y)<1e-6f&&std::abs(s.grabFrame.forward.y)<1e-6f&&s.grabFrame.up.y==1);
        const auto origin=h[hand].point,dir=Direction(slot),motion=s.grabFrame.Vector(dir);const Point height{0,1,0};
        h[hand].point=origin+motion*.09f;s.StepTracked(h,cockpit,.02f,true,false,5);CHECK(Cardinal(s,dir));
        for(float rise:{.02f,.05f,.10f}){
            h[hand].point=origin+motion*.09f+height*rise;s.StepTracked(h,cockpit,.02f,true,false,5);CHECK(s.owner==hand&&Cardinal(s,dir));
        }
        h[hand].point=origin+height*.10f;s.StepTracked(h,cockpit,.02f,true,false,5);CHECK(s.owner==hand&&s.slot==0);
        h[hand].point=origin+motion*.09f+height*.10f;s.StepTracked(h,cockpit,.02f,true,false,5);CHECK(Cardinal(s,dir));
        // A single fast sample can cross all of neutral and end beyond it.
        h[hand].point=origin-motion*.09f+height*.10f;s.StepTracked(h,cockpit,.02f,true,false,5);CHECK(s.owner==hand&&s.slot==0&&Length(s.Command())==0);
        s.StepTracked(h,cockpit,.02f,true,false,5);CHECK(Cardinal(s,dir*-1.f));
        h[hand].point=origin-motion*.09f+height*.14f;s.StepTracked(h,cockpit,.02f,true,false,5);CHECK(s.owner==-1&&s.slot==0);
        h[hand].point=origin;s.StepTracked(h,cockpit,.02f,true,false,5);CHECK(s.owner==-1);
        h[hand].grip=0;s.StepTracked(h,cockpit,.02f,true,false,5);h[hand].grip=1;s.StepTracked(h,cockpit,.02f,true,false,5);CHECK(s.owner==hand);
    }
    std::puts("PASS height/swept return: pitched/rolled grab, 10 cm height changes do not latch input; fast centre crossing yields neutral; 14 cm withdrawal releases and requires fresh grip");
}
void MovingCockpit(){
    for(int hand=0;hand<2;++hand)for(int slot=1;slot<=8;++slot){
        State s;InputFrame start{{.3f,.2f,-.1f}},moving=start;
        Hand h[2]{};h[hand]={start.World(Grip(0,0)),{},0,true,true};
        s.StepTracked(h,start,.02f,true,false,7);h[hand].grip=1;s.StepTracked(h,start,.02f,true,false,7);CHECK(s.owner==hand);
        const auto d=Direction(slot);h[hand].point=start.World(Stroke(d.x*kTravel,d.y*kTravel));
        s.StepTracked(h,start,.02f,true,false,7);CHECK(Cardinal(s,d));
        for(int i=0;i<100;++i){const float angle=std::sin(i*.1f)*.8f;
            moving.pivot={.3f+std::sin(i*.3f),.2f+std::cos(i*.2f),-.1f+std::sin(i*.15f)*.6f};
            moving.right=Swing(start.right,angle,-angle*.4f);moving.forward=Swing(start.forward,angle,-angle*.4f);moving.up=Swing(start.up,angle,-angle*.4f);
            s.StepTracked(h,moving,.02f,true,false,7);CHECK(s.owner==hand&&Cardinal(s,d)&&s.boundaryError<1e-6f);}
        h[hand].point=start.World(Grip(0,0));s.StepTracked(h,moving,.02f,true,false,7);CHECK(s.slot==0&&s.owner==hand);
        h[hand].grip=0;s.StepTracked(h,moving,.02f,true,false,7);CHECK(s.owner==-1);
        for(int i=0;i<20;++i)s.StepTracked(h,moving,.02f,true,false,7);
        // A new grab uses the new cockpit, not the old frozen frame.
        h[hand].point=moving.World(Grip(0,0));h[hand].grip=1;s.StepTracked(h,moving,.02f,true,false,7);CHECK(s.owner==hand&&s.slot==0);
        s.StepTracked(h,moving,.02f,true,false,8);CHECK(s.owner==-1&&s.slot==0); // changed tracking origin
        s.StepTracked(h,moving,.02f,true,false,8);CHECK(s.owner==-1); // fresh squeeze required
    }
    std::puts("PASS frozen grab: both hands/eight detents under 1 m chassis motion and roll; neutral/release/regrab; tracking epoch reset");
}
void AttachedHand(){
    using namespace savr::radio;
    savr::hydrocontrol::HandAttachment s;HandPose tracked{{.2f,.3f,.4f},{1,0,0},{0,1,0},{0,0,1}};
    HandFrame frame{{.2f,.3f,.4f},{1,0,0},{0,1,0},{0,0,1}};
    const auto first=s.Grab(frame,0,.045f);const auto anchor=FixedGrip(frame,0,.045f);
    CHECK(Dot(first.position-anchor.position,first.position-anchor.position)<1e-10f);
    frame.center.x+=.4f;const auto attached=s.Grab(frame,0,.045f);CHECK(std::abs(attached.position.x-first.position.x-.4f)<1e-6f);
    const auto released=s.Free(tracked,.02f);CHECK(std::abs(released.position.x-attached.position.x)<1e-6f);
    float previous=released.position.x;
    for(int i=0;i<10;++i){const auto p=s.Free(tracked,.02f);CHECK(p.position.x<=previous+1e-6f);previous=p.position.x;}
    CHECK(std::abs(previous-tracked.position.x)<1e-6f);
    s.Reset();CHECK(std::abs(s.Free(tracked,.02f).position.x-tracked.position.x)<1e-6f);
    std::puts("PASS visual hand follows moving ball; release returns continuously to tracked wrist in 120 ms");
}
void Runtime(){
    using namespace savr;using namespace savr::driving;
    alignas(16) std::array<unsigned char,0xc68> vehicle{},other{};
    controlled=vehicle.data();g_hydroHooksReady=true;int model=400;std::memcpy(vehicle.data()+0x32,&model,2);
    static void* models[612]{};models[400]=vehicle.data();g.CModelInfo_ms_modelInfoPtrs=models;g.CModelInfo_IsCarModel=IsCar;
    auto& flags=*reinterpret_cast<std::uint32_t*>(vehicle.data()+0x4b8);
    flags=0;CHECK(HydraulicsCompatible(400));CHECK(!HydroEquipment(vehicle.data()));
    flags=0x10000;CHECK(HydraulicsCompatible(400)&&!HydroEquipment(vehicle.data())); // geometry without equipment
    flags=0x40000;CHECK(!HydraulicsCompatible(400));flags=0x20000;CHECK(HydroEquipment(vehicle.data()));
    flags=0x70000;CHECK(!HydroEquipment(vehicle.data()));flags=0x30000;CHECK(HydroEquipment(vehicle.data()));
    CHECK(!HydraulicsCompatible(567)&&!HydraulicsCompatible(399));
    g_originalHydraulicHorn=OriginalHorn;g_originalHydraulicControl=OriginalControl;
    CHECK(OnHydraulicHorn(nullptr)&&hornCalls==1);g_hydroManagedVehicle.store(controlled);
    OnHydraulicControl(controlled);CHECK(controlCalls==1&&toggleCalls==0&&hornCalls==1);
    g_hydroToggle=true;OnHydraulicControl(controlled);CHECK(toggleCalls==1&&!g_hydroToggle);
    OnHydraulicControl(controlled);CHECK(toggleCalls==1);
    OnHydraulicControl(other.data());CHECK(toggleCalls==2&&hornCalls==2); // ordinary caller still sees original horn
    debug=1;g_hydroToggle=true;OnHydraulicControl(controlled);CHECK(controlCalls==4&&!g_hydroToggle);debug=0;
    allowed=false;OnHydraulicControl(controlled);CHECK(controlCalls==4);allowed=true;
    HydroModeClick(0);CHECK(cockpitclick::clicks==1&&cockpitclick::last&&xr::clicks==1&&xr::clickAmplitude>=.6f&&xr::clickFrequency>=150.f&&xr::clickDuration<=20.f);
    Button button;
    for(int i=0;i<20;++i){
        const auto before=*reinterpret_cast<std::uint16_t*>(vehicle.data()+0xa88)>=500;
        button.Step({0,0,.004f},true,true,.02f);button.Step({0,0,-.005f},true,true,.02f);CHECK(button.clicked);
        g_hydroToggle=button.clicked;OnHydraulicControl(controlled);
        CHECK(!g_hydroToggle&&(*reinterpret_cast<std::uint16_t*>(vehicle.data()+0xa88)>=500)!=before);
    }
    queryHorn=false;g_hydroToggle=true;OnHydraulicControl(controlled);CHECK(g_hydroToggle);queryHorn=true;
    acceptToggle=false;OnHydraulicControl(controlled);CHECK(g_hydroToggle);acceptToggle=true;
    OnHydraulicControl(controlled);CHECK(!g_hydroToggle);
    alignas(16) std::array<unsigned char,0xd94> hardware{};std::array<unsigned char,0xac> output{};
    for(int i=0;i<20;++i){std::uint32_t time=999-i,key=11;std::memcpy(output.data()+i*8,&time,4);std::memcpy(output.data()+i*8+4,&key,4);}
    std::uint32_t last=16,index=42;std::memcpy(output.data()+0xa4,&last,4);std::memcpy(output.data()+0xa8,&index,4);
    std::memcpy(hardware.data()+0xce8,output.data(),output.size());g_originalHydraulicBeat=OriginalBeat;
    hydraulics::dancing=false;OnHydraulicBeat(hardware.data(),output.data());CHECK(*reinterpret_cast<std::uint32_t*>(output.data()+4)==11);
    hydraulics::dancing=true;OnHydraulicBeat(hardware.data(),output.data());CHECK(*reinterpret_cast<std::uint32_t*>(output.data()+4)==9);
    CHECK(*reinterpret_cast<std::uint32_t*>(hardware.data()+0xcec)==9);
    CHECK(*reinterpret_cast<std::uint32_t*>(output.data()+0xa4)==14);
    CHECK(*reinterpret_cast<std::uint32_t*>(output.data())==999&&*reinterpret_cast<std::uint32_t*>(output.data()+0xa8)==42);
    for(std::uint32_t key:{1u,2u,3u,4u,9u,10u,13u,14u})CHECK(MissionKey(key)==key);
    std::puts("PASS native adapter: compatibility vs equipment; scoped reverse; unrelated horn; preview/menu blocking; native/hint four-way beat windows with intact timing/index");
}
std::vector<unsigned char> Read(const char* path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void Geometry(const char* meshPath,const char* fixturePath){
    const auto bytes=Read(meshPath);Model model;CHECK(model.Load(bytes.data(),bytes.size()));CHECK(model.indices.size()/3==4912);
    int ball=0,labels=0;
    for(const auto& v:model.vertices){
        if(v.uv[0]>=0){++labels;CHECK(v.role==0&&std::abs(v.position[2]-.0148f)<1e-6f);}
        if(v.role==1&&std::abs(Length(Point{v.position[0],v.position[1],v.position[2]-.055f})-.0225f)<1e-6f){
            ++ball;CHECK(v.uv[0]<0&&v.uv[1]<0&&v.color[0]>.7f);}}
    CHECK(ball>300&&labels==24);
    const auto fixture=Read(fixturePath);CHECK(fixture.size()>12);std::uint32_t header[3];std::memcpy(header,fixture.data(),12);CHECK(header[0]==0x46525948);
    std::size_t offset=12;float worst=0;
    for(unsigned s=0;s<header[1];++s){float xy[2];std::memcpy(xy,fixture.data()+offset,8);offset+=8;
        for(unsigned i=0;i<header[2];++i){std::uint32_t index;float expected[3];std::memcpy(&index,fixture.data()+offset,4);std::memcpy(expected,fixture.data()+offset+4,12);offset+=16;
            CHECK(index<model.vertices.size());auto v=Model::Deform(model.vertices[index],xy[0],xy[1],0);
            for(int a=0;a<3;++a){const float error=std::abs(v.position[a]-expected[a]);worst=std::max(worst,error);CHECK(error<2e-6f);}
            float length=0;for(float n:v.normal)length+=n*n;CHECK(std::abs(length-1)<1e-4f);}}
    CHECK(offset==fixture.size());auto invalid=bytes;invalid[0]=0;CHECK(!model.Load(invalid.data(),invalid.size()));
    std::printf("PASS native deformation vs independent Blender evaluated vertices; worst error %.8f m\n",worst);
}
void PointPose(const char* path){
    using namespace savr;handskeleton::Library library;CHECK(library.Load(path));
    handskeleton::Palette fist{},point{},blend{},forced{};
    library.Evaluate({handanimation::Gesture::Fist,handanimation::Gesture::Fist,1,0},0,0,fist);
    library.Evaluate({handanimation::Gesture::Point,handanimation::Gesture::Point,1,0},0,0,point);
    handanimation::Selection override{handanimation::Gesture::Fist,handanimation::Gesture::Fist,1,0};
    override.pointBlend=1;library.Evaluate(override,0,0,forced);
    for(std::size_t j=0;j<library.joints.size();++j)for(int a=0;a<16;++a)CHECK(std::abs(forced[j][a]-point[j][a])<1e-5f);
    override.pointBlend=.5f;library.Evaluate(override,0,0,blend);
    handskeleton::Vec a{},b{},c{};CHECK(library.IndexTip(fist,a)&&library.IndexTip(point,b)&&library.IndexTip(blend,c));
    const auto distance=[](const auto& u,const auto& v){float s=0;for(int i=0;i<3;++i)s+=(u[i]-v[i])*(u[i]-v[i]);return std::sqrt(s);};
    CHECK(distance(a,b)>.01f&&distance(a,c)>.001f&&distance(b,c)>.001f);
    for(int hand=0;hand<2;++hand){xr::HandPose h{};h.valid=true;h.aimValid=true;
        h.gripPos[0]=.2f;h.gripPos[1]=-.1f;h.gripPos[2]=.4f;
        h.gripOri[1]=std::sin(.3f);h.gripOri[3]=std::cos(.3f);
        h.aimOri[1]=std::sin(.5f);h.aimOri[3]=std::cos(.5f);
        const auto pose=handrender::TrackedPose(h,hand);const auto tip=handrender::MeshPoint(pose,{b[0],b[1],b[2]});
        // Independent application of the renderer's mesh matrix to real GLB tip.
        const float m[16]{pose.forward.x,pose.forward.y,pose.forward.z,0,-pose.up.x,-pose.up.y,-pose.up.z,0,
            pose.right.x,pose.right.y,pose.right.z,0,pose.position.x-.055f*pose.forward.x,pose.position.y-.055f*pose.forward.y,pose.position.z-.055f*pose.forward.z,1};
        const float actual[3]{tip.x,tip.y,tip.z};
        for(int i=0;i<3;++i)CHECK(std::abs(actual[i]-(m[i]*b[0]+m[4+i]*b[1]+m[8+i]*b[2]+m[12+i]))<1e-6f);
    }
    std::puts("PASS automatic point pose: real hand asset, full override and continuous intermediate fingertip pose");
    handskeleton::Palette grip{};
    library.Evaluate({handanimation::Gesture::Grip,handanimation::Gesture::Grip,1,0},0,0,grip);
    const radio::HandFrame ballFrame{{},{1,0,0},{0,1,0},{0,0,1}};
    for(int hand=0;hand<2;++hand){
        const auto pose=FixedGrip(ballFrame,hand,.045f);int palm=0;
        std::ofstream obj(hand==0?"fixed-grip-left.obj":"fixed-grip-right.obj");
        for(const auto& vertex:library.vertices){radio::HandVector p{};
            for(int j=0;j<8;++j){const auto& m=grip[vertex.joints[j]];const float w=vertex.weights[j];
                p.x+=w*(m[0]*vertex.pos[0]+m[4]*vertex.pos[1]+m[8]*vertex.pos[2]+m[12]);
                p.y+=w*(m[1]*vertex.pos[0]+m[5]*vertex.pos[1]+m[9]*vertex.pos[2]+m[13]);
                p.z+=w*(m[2]*vertex.pos[0]+m[6]*vertex.pos[1]+m[10]*vertex.pos[2]+m[14]);}
            const auto world=handrender::MeshPoint(pose,p);
            if(p.x>.03f&&p.x<.08f){++palm;CHECK(radio::Dot(world,world)>.045f*.045f);}
            obj<<"v "<<world.x<<' '<<world.y<<' '<<world.z<<'\n';
        }
        CHECK(palm>100);
        for(std::size_t i=0;i<library.indices.size();i+=3)
            obj<<"f "<<library.indices[i]+1<<' '<<library.indices[i+1]+1<<' '<<library.indices[i+2]+1<<'\n';
        for(int slot=1;slot<=8;++slot){const auto d=Direction(slot);const float x=d.x*kTravel,y=d.y*kTravel;
            const auto swing=[&](radio::HandVector v){const auto p=Swing({v.x,v.y,v.z},x,y);return radio::HandVector{p.x,p.y,p.z};};
            radio::HandFrame moved{ swing({0,0,kReach*kModelScale})-radio::HandVector{0,0,kReach*kModelScale},
                swing(ballFrame.right),swing(ballFrame.up),swing(ballFrame.normal)};
            const auto fit=FixedGrip(moved,hand,.045f);const auto expected=moved.center+swing(pose.position);
            CHECK(radio::Dot(fit.position-expected,fit.position-expected)<1e-10f);
            const auto travel=fit.position-pose.position;const float stroke=std::sqrt(travel.x*travel.x+travel.y*travel.y);
            CHECK(stroke>.04f&&stroke<.065f);
        }
    }
    std::puts("PASS fixed ball grip: real skinned palms outside 90 mm sphere, both mirrored hands, 4-6.5 cm wrist arc across all detents");
}
void HandConfig(){
    using savr::hydrohand::Config;
    const char* path="hand-fit-test.ini";std::remove(path);
    Config config(path);CHECK(config.Get(0)==savr::hydrohand::Fit{}&&config.Saved());
    CHECK(config.Adjust(0,0,13)&&config.Adjust(0,3,-45)&&config.Adjust(1,1,25));
    Config read(path);CHECK(std::abs(read.Get(0)[0]-1.3f)<1e-5f&&read.Get(0)[3]==-45.f&&read.Get(1)[1]==2.5f);
    CHECK(read.Reset(0));Config reset(path);CHECK(reset.Get(0)==savr::hydrohand::Fit{}&&reset.Get(1)[1]==2.5f);
    std::ofstream invalid("hand-fit-invalid.ini");invalid<<"Left.Xcm=nan\nLeft.Zcm=10000\nRight.RotZDeg=-10000\nOther.Xcm=7\n";invalid.close();
    Config filtered("hand-fit-invalid.ini");CHECK(filtered.Get(0)[0]==0&&filtered.Get(0)[2]==25&&filtered.Get(1)[5]==-360);
    Config failed("hand-fit-missing-directory/no-config.ini");CHECK(failed.Get(0)==savr::hydrohand::Fit{}&&!failed.Saved());
    for(int h=0;h<2;++h){const savr::radio::HandFrame frame{{},{1,0,0},{0,1,0},{0,0,1}};
        const savr::hydrohand::Fit fit{1,2,3,45,-90,25};const auto pose=FixedGrip(frame,h,.045f,fit);
        const auto opening=savr::handrender::MeshPoint(pose,{.09869f,.01410f,0});
        const savr::radio::HandVector expected{.01f,-.037f+.02f,.012f+.03f};
        CHECK(savr::radio::Dot(opening-expected,opening-expected)<1e-10f);
        CHECK(std::abs(savr::radio::Dot(pose.up,pose.forward))<1e-6f);
    }
    savr::hydrohand::SetPreview(true);CHECK(savr::hydrohand::PreviewHand()==savr::hydrohand::Selected());
    savr::hydrohand::SetPreview(false);CHECK(savr::hydrohand::PreviewHand()==-1);
    std::puts("PASS hand config: separate left/right offsets and rotations; round trip/replace/reset; invalid data/save failure; rotations around grip opening; preview exit");
}
int main(int argc,char** argv){CHECK(argc==4);Detents();ArcChanges();BoundaryFeedback();DirectBeatFlick();Buttons();HandTravel();SpringReturn();SharedCoordinates();HeightAndSweptReturn();MovingCockpit();AttachedHand();Runtime();Geometry(argv[1],argv[2]);PointPose(argv[3]);HandConfig();}

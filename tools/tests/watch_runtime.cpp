#define _CRT_SECURE_NO_WARNINGS
#include "HandSkeleton.cpp"
#undef CGLTF_IMPLEMENTATION
#include "WatchModel.cpp"
#include "WatchLayout.h"
#include "WatchInteraction.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>

#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
int main(){
    using namespace savr;
    std::ifstream file(WATCH_FILE,std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{});
    watchmodel::Model model;
    CHECK(model.Load(bytes.data(),bytes.size()));
    CHECK(model.meshes.size()==9&&model.materials.size()==4&&model.images.size()==2);
    std::size_t triangles=0,displays=0;
    for(const auto& mesh:model.meshes){triangles+=mesh.indices.size()/3;displays+=mesh.display;}
    CHECK(triangles==2562&&displays==1);
    CHECK(std::abs(model.centre[0]-.0043426f)<.00001f);
    CHECK(std::abs(model.centre[2]-.0343966f)<.00001f);
    for(std::size_t length:{std::size_t(0),std::size_t(12),std::size_t(100),bytes.size()-4})
        CHECK(!model.Load(bytes.data(),length)&&model.meshes.empty());
    for(float maximum:{100.f,150.f,200.f}) {
        CHECK(watchhud::Fraction(maximum,maximum)==1.f);
        CHECK(watchhud::Fraction(maximum*.5f,maximum)==.5f);
        CHECK(watchhud::Fraction(0,maximum)==0.f);
        CHECK(watchhud::Fraction(maximum*2,maximum)==1.f);
    }
    CHECK(watchhud::Fraction(10,0)<0&&watchhud::Fraction(-1,100)<0);
    CHECK(watchhud::Fraction(std::numeric_limits<float>::quiet_NaN(),100)<0);
    for(int degrees=0;degrees<360;++degrees){
        const float angle=degrees*.01745329252f;
        float x=10000*std::sin(angle),y=10000*std::cos(angle);
        watchhud::Pin(x,y);
        // Visible sprite radius stays inside the tighter map inset.
        const float margin=48.f;
        CHECK(std::hypot(x,y)+margin<=watchhud::kMapRadius*watchhud::kMapPixels+.001f);
        CHECK(y-margin>=watchhud::kMapTop*watchhud::kMapPixels-.001f);
        CHECK(y+margin<=watchhud::kMapBottom*watchhud::kMapPixels+.001f);
    }
    float x=5,y=-10;watchhud::Pin(x,y);CHECK(x==5&&y==-10);
    CHECK(watchhud::LocalOpacity(0,0)==1);
    CHECK(watchhud::LocalOpacity(0,10000)==0&&watchhud::LocalOpacity(10000,0)==0);
    for(int degrees=0;degrees<360;++degrees){
        const float angle=degrees*.01745329252f;
        float x=10000*std::sin(angle),y=10000*std::cos(angle);
        CHECK(watchhud::ProjectedBlip(x,y,true)==1);
        CHECK(std::hypot(x,y)+24.f*1.414214f<=.5f*watchhud::kMapPixels+.001f);
    }
    {
        watchhud::Projection p;
        auto step=[&](float trigger,float radial=0.f,float depth=.005f,bool available=true,bool valid=true,std::uint64_t epoch=1,int hand=0){
            return p.Update(valid,epoch,hand,available,trigger,radial,depth,.023f,.02f);
        };
        CHECK(!step(1)&&!step(1)); // held input on startup is not a press
        CHECK(!step(0));CHECK(!step(1,.10f));CHECK(!step(1,0,-.03f));
        CHECK(step(1)&&p.open); // index contact from the face side
        for(int i=0;i<30;++i)CHECK(!step(1));
        CHECK(p.Amount()==1&&watchhud::kProjectionHeight*p.Amount()==.20f);
        CHECK(step(0)==false&&step(1)&&!p.open);
        for(int i=0;i<30;++i)CHECK(!step(1));
        CHECK(!p.Needed()&&p.Amount()==0);
        CHECK(!step(1,.10f));CHECK(step(1)&&p.open); // leave + re-touch with trigger held
        CHECK(!step(1,0,.005f,false));CHECK(!step(1)); // unavailable hand cannot queue a press
        CHECK(!step(0));CHECK(step(1)&&!p.open);
        CHECK(!step(1,0,.005f,true,false)&&!p.Needed());
        CHECK(!step(1)&&!step(1));CHECK(!step(0));CHECK(step(1));
        CHECK(!step(1,0,.005f,true,true,2)&&!p.Needed()); // recenter
        CHECK(!step(0,0,.005f,true,true,2,1)&&!p.Needed()); // changed wrist
    }
    {
        watchhud::ZoomSlider slider;
        CHECK(!slider.Update(true,1,0,0,0,1)&&!slider.dragging);
        CHECK(!slider.Update(true,0,0,0,0,1));
        CHECK(!slider.Update(true,1,.10f,0,0,1)&&!slider.dragging);
        CHECK(!slider.Update(true,1,0,0,0,1)&&slider.dragging);
        CHECK(std::abs(slider.zoom-1.25f)<.001f);
        slider.Update(true,1,0,.1f,0,slider.zoom);CHECK(slider.zoom==2);
        slider.Update(true,1,0,-.1f,0,slider.zoom);CHECK(slider.zoom==.5f);
        CHECK(slider.Update(true,0,0,0,0,slider.zoom)&&!slider.dragging);
        slider.Update(true,1,0,0,0,1);CHECK(slider.dragging);
        CHECK(slider.Update(false,1,0,0,0,slider.zoom)&&!slider.dragging);
        CHECK(!slider.Update(true,1,0,0,0,1)&&!slider.dragging);
    }
    {
        handskeleton::Library hand;CHECK(hand.Load(HAND_FILE)&&hand.indexTipJoint>=0);
        handskeleton::Vec first{},last{};
        for(float progress:{0.f,.25f,.5f,.75f,1.f}){
            handskeleton::Pose pose{};handskeleton::Palette palette{};
            hand.Sample(handanimation::Gesture::Point,progress,pose);hand.Matrices(pose,palette);
            handskeleton::Vec tip{};CHECK(hand.IndexTip(palette,tip));
            std::array<handskeleton::Mat,handskeleton::kMaxNodes> world{};
            for(int node:hand.order){const auto local=hand.nodes[node].fixedMatrix?hand.nodes[node].matrix:handskeleton::Matrix(pose[node]);
                world[node]=hand.nodes[node].parent<0?local:handskeleton::Multiply(world[hand.nodes[node].parent],local);}
            const auto& expected=world[hand.joints[hand.indexTipJoint]];
            for(int axis=0;axis<3;++axis)CHECK(std::abs(tip[axis]-expected[12+axis])<.000001f);
            if(progress==0)first=tip;if(progress==1)last=tip;
        }
        CHECK(std::hypot(first[0]-last[0],first[1]-last[1],first[2]-last[2])>.005f);
    }
    std::puts("PASS watch GLB, hand-relative fit, current-max vitals, clipped-edge mission markers");
    std::puts("PASS projection touch debounce, occupied hand, recovery, animation, circular pins and actual animated index tip");
}

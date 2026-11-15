#define _CRT_SECURE_NO_WARNINGS
#include "HandSkeleton.cpp"
#undef CGLTF_IMPLEMENTATION
#include "WatchModel.cpp"
#include "WatchLayout.h"
#include "WatchInteraction.h"
#include "RadarMapLayout.h"
#include "RadarTextureLeases.h"
#include "RadarTextureStreaming.h"
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
        CHECK(!slider.Update(true,1,0,0,0,1)&&!slider.dragging&&slider.touching);
        CHECK(!slider.Update(true,0,0,0,0,1));
        CHECK(!slider.Update(true,1,.10f,0,0,1)&&!slider.dragging&&!slider.touching);
        CHECK(!slider.Update(true,1,0,0,0,1)&&slider.dragging);
        CHECK(std::abs(slider.zoom-1.125f)<.001f);
        slider.Update(true,1,0,.1f,0,slider.zoom);CHECK(slider.zoom==2);
        slider.Update(true,1,0,-.1f,0,slider.zoom);CHECK(slider.zoom==.25f);
        CHECK(slider.Update(true,0,0,0,0,slider.zoom)&&!slider.dragging);
        slider.Update(true,1,0,0,0,1);CHECK(slider.dragging);
        CHECK(slider.Update(true,1,0,0,.10f,slider.zoom)&&!slider.dragging);
        CHECK(!slider.Update(true,1,0,0,0,slider.zoom)&&slider.dragging); // re-touch, trigger never released
        CHECK(slider.Update(true,1,0,.20f,0,slider.zoom)&&!slider.dragging);
        CHECK(!slider.Update(true,1,0,0,0,slider.zoom)&&slider.dragging);
        CHECK(slider.Update(false,1,0,0,0,slider.zoom)&&!slider.dragging&&!slider.touching);
        CHECK(!slider.Update(true,1,0,0,0,1)&&!slider.dragging);
    }
    {
        watchhud::Projection map;watchhud::ZoomSlider scale;
        map.Update(true,1,0,true,0,1,1,.023f,.02f);
        map.Update(true,1,0,true,0,1,1,.023f,.02f);
        CHECK(map.Update(true,1,0,true,1,0,.005f,.023f,.02f)&&map.open);
        scale.Update(true,0,0,0,0,1);
        scale.Update(true,1,0,0,0,1);CHECK(scale.dragging);
        // Match the renderer: a drag remains an available pointing hand.
        CHECK(!map.Update(true,1,0,true,1,0,.20f,.023f,.02f));
        CHECK(scale.Update(true,1,.10f,0,0,1)&&!scale.dragging);
        CHECK(map.Update(true,1,0,true,1,0,.005f,.023f,.02f)&&!map.open);
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
    {
        // Real tile enumerator: no gaps north of the player at wide zoom,
        // and no wrapped/clamped terrain outside the 6000x6000 game map.
        struct Tile{int x,y;radarmap::Rect rect;};
        for(float px:{-3500.f,-2999.f,0.f,2750.f,3500.f})
        for(float py:{-3500.f,-2999.f,-1500.f,0.f,2999.f,3500.f})
        for(float zoom:{.25f,.5f,1.f,2.f}){
            std::vector<Tile> tiles;
            radarmap::VisibleTiles(px,py,zoom,[&](int x,int y,radarmap::Rect r){
                CHECK(x>=0&&x<12&&y>=0&&y<12);tiles.push_back({x,y,r});
            });
            CHECK(tiles.size()<=144);
            for(int y=7;y<800;y+=23)for(int x=7;x<800;x+=23){
                const float wx=px+(x-400)/(400.f/300.f*zoom),wy=py-(y-400)/(400.f/300.f*zoom);
                bool covered=false;
                for(const auto& tile:tiles)if(x>=tile.rect.left&&x<=tile.rect.right&&y>=tile.rect.top&&y<=tile.rect.bottom)covered=true;
                const bool land=wx>-3000&&wx<3000&&wy>-3000&&wy<3000;
                // Half-pixel tile overlap can cover at most 1.5 world units at .25x.
                const bool farSea=wx<-3002||wx>3002||wy<-3002||wy>3002;
                CHECK(!land||covered);CHECK(!farSea||!covered);
            }
        }
        int count=0;
        radarmap::VisibleTiles(0,0,.25f,[&](int,int,radarmap::Rect){++count;});
        CHECK(count>9); // larger than the stock retained 3x3 neighbourhood
        CHECK(radarmap::kSea[0]==111&&radarmap::kSea[1]==137&&radarmap::kSea[2]==170&&radarmap::kSea[3]==255);
    }
    {
        struct Texture{int refs=2,retains=0,releases=0;}; // database + stock radar
        Texture far,near,replacement;
        radarmap::TextureLeases leases;
        const auto retain=[](void* raw){auto& t=*static_cast<Texture*>(raw);++t.refs;++t.retains;};
        const auto release=[](void* raw){auto& t=*static_cast<Texture*>(raw);--t.refs;++t.releases;};
        for(int frame=0;frame<100;++frame){
            leases.Begin();leases.Keep(17,&far,retain,release);
            leases.Keep(17,&far,retain,release); // both watch and projection
            leases.Keep(18,&near,retain,release);leases.Finish(release);
            CHECK(far.refs==3&&near.refs==3);
            --far.refs;CHECK(far.refs>1); // stock far-section removal cannot cancel streaming
            ++far.refs; // next RequestMapSection reacquires its stock reference
        }
        CHECK(far.retains==1&&near.retains==1&&far.releases==0);
        leases.Begin();leases.Keep(18,&near,retain,release);leases.Finish(release);
        CHECK(far.refs==2&&far.releases==1); // zoom/position removed the far tile
        leases.Begin();leases.Keep(18,&replacement,retain,release);leases.Finish(release);
        CHECK(near.refs==2&&replacement.refs==3);
        leases.Begin();leases.Finish(release); // no HUD maps active
        CHECK(replacement.refs==2&&replacement.releases==1);
        leases.Finish(release);CHECK(replacement.releases==1);
        leases.Keep(-1,&far,retain,release);leases.Keep(144,&far,retain,release);
        leases.Keep(1,nullptr,retain,release);CHECK(far.retains==1);
    }
    {
        std::array<unsigned char,8> texture{};
        std::array<unsigned char,64> raster{};
        std::array<unsigned char,31> entry{}; // retail packed TextureDatabaseEntry
        void* rasterPointer=raster.data();void* entryPointer=entry.data();
        std::memcpy(texture.data(),&rasterPointer,sizeof(rasterPointer));
        std::memcpy(raster.data()+0x38,&entryPointer,sizeof(entryPointer));
        std::uint16_t flags=0x1123;std::memcpy(entry.data()+0xc,&flags,2);
        int calls=0;
        const auto promote=[&](void* raw){CHECK(raw==entry.data());++calls;
            flags=0x2123;std::memcpy(entry.data()+0xc,&flags,2);};
        CHECK(radarmap::MarkVisibleTexture(texture.data(),promote)&&calls==1);
        CHECK(radarmap::StreamState(entry.data())==2);
        CHECK(!radarmap::MarkVisibleTexture(texture.data(),promote)&&calls==1);
        flags=0x3123;std::memcpy(entry.data()+0xc,&flags,2);
        CHECK(!radarmap::MarkVisibleTexture(texture.data(),promote));
        CHECK(!radarmap::MarkVisibleTexture(nullptr,promote));
        texture.fill(0);CHECK(!radarmap::MarkVisibleTexture(texture.data(),promote));
        CHECK(calls==1); // no duplicate queue inserts and no reload of full textures
    }
    std::puts("PASS visible-texture promotion from thumbnail queue; packed retail entry; full/pending/null cases");
    std::puts("PASS retained far-tile streaming across 100 stock unload/reacquire cycles; shared map passes and balanced releases");
    std::puts("PASS complete wide-zoom tile coverage, all world edges/corners and retail sea background");
    std::puts("PASS watch GLB, hand-relative fit, current-max vitals, clipped-edge mission markers");
    std::puts("PASS projection touch debounce, occupied hand, recovery, animation, circular pins and actual animated index tip");
}

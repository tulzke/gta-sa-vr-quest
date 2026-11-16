#include "Hydraulics.h"
#include "Symbols.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#e);std::exit(1);}}while(0)
namespace savr {GameSymbols g{};}
namespace savr::beachdance {bool playing{};bool Active(){return playing;}}
using namespace savr;
template<class T>void Put(void* bytes,int offset,T value){std::memcpy(static_cast<char*>(bytes)+offset,&value,sizeof(value));}
void Save(const char* directory,const char* name,const rhythm::Mesh& mesh){
    std::ofstream out(std::string(directory)+"/"+name,std::ios::binary);
    CHECK(out.good());out.write(reinterpret_cast<const char*>(mesh.vertices.data()),mesh.count*sizeof(rhythm::Vertex));
}
int main(int argc,char** argv){
    CHECK(argc==2);alignas(16) char beat[0xac]{},script[0x1010]{};
    Put<int>(beat,0xa0,1);Put<int>(beat,0xa8,40);
    Put<int>(beat,9*8,-180);Put<int>(beat,9*8+4,9);
    for(int i=10;i<14;++i){Put<int>(beat,i*8,63+(i-10)*500);Put<int>(beat,i*8+4,i==10?13:i==11?10:i==12?14:9);}
    const auto original=std::vector<char>(beat,beat+sizeof(beat));
    auto f=rhythm::Decode(beat,true);CHECK(f.active&&f.current==40&&f.count==5);
    CHECK(f.notes[0].ms==-180&&f.notes[0].id==39&&f.notes[0].direction==3);
    CHECK(f.notes[1].ms==63&&f.notes[1].id==40&&f.notes[1].direction==1);
    CHECK(f.notes[4].ms==1563&&f.notes[4].id==43&&f.notes[4].direction==3);
    CHECK(!rhythm::Decode(beat,false).active&&!rhythm::Decode(nullptr,true).active);
    Put<int>(beat,0xa0,0);CHECK(!rhythm::Decode(beat,true).active);Put<int>(beat,0xa0,1);
    CHECK(std::equal(original.begin(),original.end(),beat));
    CHECK(std::abs(rhythm::Position(63)-rhythm::kTargetX)<1e-6f);
    CHECK(std::abs(rhythm::Position(126)-(rhythm::kTargetX+rhythm::kHalfWindow))<1e-6f);
    CHECK(std::abs(rhythm::Position(0)-(rhythm::kTargetX-rhythm::kHalfWindow))<1e-6f);
    for(int ms=-300;ms<2200;++ms){
        const float x=rhythm::Position(static_cast<float>(ms));
        const bool inside=x>=rhythm::kTargetX-rhythm::kHalfWindow&&x<rhythm::kTargetX+rhythm::kHalfWindow;
        CHECK(inside==rhythm::InWindow(static_cast<float>(ms)));
        CHECK(rhythm::Position(static_cast<float>(ms-1))<x);
    }
    CHECK(rhythm::Build({},0).count==0&&rhythm::Build(f,251).count==0&&rhythm::Build(f,NAN).count==0);
    const auto mesh=rhythm::Build(f,0);CHECK(mesh.count>100&&mesh.count<static_cast<int>(mesh.vertices.size()));
    for(int i=0;i<mesh.count;++i){const auto& v=mesh.vertices[i];CHECK(std::isfinite(v.x)&&std::isfinite(v.y));CHECK(v.colour.a>=0&&v.colour.a<=1);}
    Save(argv[1],"hit.mesh",mesh);
    for(int i=0;i<f.count;++i)f.notes[i].ms+=400;
    f.current=39; // earlier native snapshot, before beat 39 has passed
    Save(argv[1],"approach.mesh",rhythm::Build(f,0));
    // Pure production glyphs for shape/contrast inspection, in all directions.
    rhythm::Mesh glyphs;for(int i=1;i<=4;++i)glyphs.Arrow(-.36f+(i-1)*.24f,i,1.f,{1,1,1,1});
    CHECK(glyphs.count==36);Save(argv[1],"arrows.mesh",glyphs);
    std::puts("PASS timeline: signed native past/future deltas, stable beat IDs, read-only decode, all four directions, exact 126 ms geometry, monotonic approach, stale/menu-empty snapshots");

    // Exercise the actual Hydraulics.cpp, not a duplicate of its axis policy.
    g.CTheScripts_ScriptSpace=script;hydraulics::g_beatInfo=reinterpret_cast<const std::uint8_t*>(beat);
    Put<int>(script,0x1004,1);Put<int>(beat,0x50,800);Put<int>(beat,0x54,10);
    hydraulics::SetStick(true,0,0,true);hydraulics::SetStick(true,1,0,true);
    CHECK(hydraulics::ScoringDirection()==0&&hydraulics::AxisLeftRight()==0);
    CHECK(hydraulics::AxisLeftRight(true)==127&&hydraulics::AxisUpDown(true)==0);
    auto published=hydraulics::GetRhythmFrame();CHECK(published.active&&published.current==40);
    Put<int>(beat,0x50,95);hydraulics::SetStick(true,1,0,true);
    CHECK(hydraulics::ScoringDirection()==4&&hydraulics::AxisLeftRight()==127&&hydraulics::AxisLeftRight(true)==127);
    Put<int>(beat,0xa8,41);Put<int>(beat,0x50,95);hydraulics::SetStick(true,1,0,true);
    CHECK(hydraulics::ScoringDirection()==0&&hydraulics::AxisLeftRight()==0&&hydraulics::AxisLeftRight(true)==127);
    Put<int>(beat,0x50,500);hydraulics::SetStick(true,0,-1,true);
    CHECK(hydraulics::ScoringDirection()==0&&hydraulics::AxisUpDown()==0&&hydraulics::AxisUpDown(true)==127);
    hydraulics::SetStick(false,0,0);CHECK(hydraulics::AxisLeftRight(true)==0&&hydraulics::AxisUpDown(true)==0);
    Put<int>(script,0x1004,0);hydraulics::SetStick(false,0,0);CHECK(!hydraulics::GetRhythmFrame().active);
    beachdance::playing=true;hydraulics::SetStick(false,0,0);CHECK(hydraulics::GetRhythmFrame().active);beachdance::playing=false;
    Put<int>(script,0x1004,1);Put<int>(beat,0xa0,0);hydraulics::SetStick(false,0,0);CHECK(!hydraulics::GetRhythmFrame().active);
    std::puts("PASS native input: live suspension between/on beats, script-only scoring window, no automatic next-beat answer from holding, release, radio isolation, shared beach timeline");
}

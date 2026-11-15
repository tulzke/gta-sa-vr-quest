#include "HandSkeleton.h"
#include "ReloadHintHand.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <limits>
#include <iomanip>

using namespace savr;
using namespace handskeleton;
template<class T> T Read(std::ifstream& in) {T v{};in.read(reinterpret_cast<char*>(&v),sizeof(v));assert(in);return v;}
float Distance(Vec a,Vec b){float s=0;for(int k=0;k<3;++k)s+=(a[k]-b[k])*(a[k]-b[k]);return std::sqrt(s);}
Vec Skin(const Vertex& v,const Palette& p) {
    Vec out{};
    for(int j=0;j<8;++j)for(int r=0;r<3;++r) {
        const auto& m=p[v.joints[j]];
        out[r]+=v.weights[j]*(m[12+r]+m[r]*v.pos[0]+m[4+r]*v.pos[1]+m[8+r]*v.pos[2]);
    }
    return {out[0],-out[2],out[1]}; // glTF -> canonical hand
}
int main(int argc,char** argv) {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    assert(argc==4);Library lib;
    if(!lib.Load(argv[1])) {std::fprintf(stderr,"GLB FAILED: %s\n",lib.error.c_str());return 1;}
    assert(lib.loaded && lib.joints.size()==25 && lib.vertices.size()==2437 && lib.indices.size()==13092);
    const auto pumpHand=reloadhint::CopyPumpHandModel(lib);
    assert(pumpHand && pumpHand->vertices.size()==lib.vertices.size()*3 && pumpHand->indices==lib.indices);
    Palette pumpPalette{};lib.Evaluate({handanimation::Gesture::GripShotgunOffhand,handanimation::Gesture::GripShotgunOffhand,1,0},0,0,pumpPalette);
    for(std::size_t i=0;i<lib.vertices.size();++i) {
        const auto actual=Skin(lib.vertices[i],pumpPalette);
        for(int k=0;k<3;++k)assert(std::abs(actual[k]-pumpHand->vertices[i*3+k])<.000001f);
    }
    reloadhint::VisualFrame hint;hint.items[0].model=pumpHand;hint.items[0].pumpHand=1;
    hint.items[0].pumpWrist.position={1,2,3};reloadhint::AddPumpHands(hint,pumpHand);
    assert(hint.items[2].model==pumpHand && !hint.items[3].model);
    assert(std::abs(hint.items[2].pose.position.y-1.945f)<.000001f);
    Pose rest{};for(std::size_t i=0;i<lib.nodes.size();++i)rest[i]=lib.nodes[i].rest;
    Palette p{};lib.Matrices(rest,p);
    float bindError=0;
    for(std::size_t i=0;i<lib.joints.size();++i)for(int k=0;k<16;++k)bindError=std::max(bindError,std::abs(p[i][k]-Identity()[k]));
    assert(bindError<2e-5f);
    std::ifstream ref(argv[2],std::ios::binary);
    assert(Read<unsigned>(ref)==lib.clips.size());assert(Read<unsigned>(ref)==lib.vertices.size());Read<unsigned>(ref);
    float globalMax=0;
    for(std::size_t clip=0;clip<lib.clips.size();++clip) {
        const auto count=Read<unsigned>(ref);float maximum=0;double sum=0;
        for(unsigned sample=0;sample<count;++sample) {
            const float progress=Read<float>(ref);Pose pose{};
            lib.Sample(static_cast<handanimation::Gesture>(clip),progress,pose);lib.Matrices(pose,p);
            for(const auto& v:lib.vertices) {
                Vec expected{};for(auto& x:expected)x=Read<float>(ref);
                for(int k=0;k<3;++k)Read<float>(ref); // evaluated geometric normals differ from LBS normals
                const float distance=Distance(expected,Skin(v,p));maximum=std::max(maximum,distance);sum+=distance;
            }
        }
        std::printf("PASS GLB %s: %u source/midpoint poses; max %.6f mm, mean %.6f mm\n",
            handanimation::kNames[clip],count,maximum*1000,sum/(count*lib.vertices.size())*1000);
        globalMax=std::max(maximum,globalMax);
        assert(maximum<.0003f); // bone-linear midpoints vs original Blender Bezier
    }
    // Quaternion antipodes, true spherical halfway, and normalized result.
    auto q=Slerp({0,0,0,1},{0,0,0,-1},.5f);assert(std::abs(q[3]-1)<1e-6f);
    q=Slerp({0,0,0,1},{0,0,1,0},.5f);assert(std::abs(q[2]-std::sqrt(.5f))<1e-6f);
    const Mat child=Multiply(Matrix({{1,0,0},{1,1,1},{0,0,std::sqrt(.5f),std::sqrt(.5f)}}),Matrix({{1,0,0}}));
    assert(std::abs(child[12]-1)<1e-5f && std::abs(child[13]-1)<1e-5f);
    // Eight-weight skinning is necessary: at least one vertex uses >4 joints.
    bool eight=false;for(const auto& v:lib.vertices)for(int j=4;j<8;++j)eight=eight||v.weights[j]>0;assert(eight);
    Pose a{},b{},blend{},expected{};
    lib.Sample(handanimation::Gesture::Grip,0,a);lib.Sample(handanimation::Gesture::Fist,1,b);
    lib.Blend(a,b,0,blend);assert(blend==a);
    lib.Blend(a,b,1,blend); // rotations may normalize by a few ulps
    for(std::size_t i=0;i<lib.nodes.size();++i)assert(Distance(blend[i].t,b[i].t)<1e-6f);
    // All analog extremes and the free-hand diagonal remain continuous in bone space.
    for(float grip:{-1.f,0.f,.25f,.5f,1.f,2.f,std::numeric_limits<float>::quiet_NaN()})
    for(float trigger:{-1.f,0.f,.25f,.5f,1.f,2.f,std::numeric_limits<float>::infinity()}) {
        lib.Evaluate(handanimation::FreeHand(grip,trigger),.5f,.3f,p);
        for(std::size_t i=0;i<lib.joints.size();++i)for(float v:p[i])assert(std::isfinite(v));
    }
    for(float progress:{.1f,.5f,.9f}) {
        Palette l{},r{};lib.Evaluate(handanimation::FreeHand(progress-1e-6f,progress),0,0,l);
        lib.Evaluate(handanimation::FreeHand(progress+1e-6f,progress),0,0,r);
        for(const auto& v:lib.vertices)assert(Distance(Skin(v,l),Skin(v,r))<1e-5f);
    }
    // STEP semantics including exact last key; LINEAR translation/scale channels.
    Library simple;simple.nodes.resize(1);simple.order={0};simple.joints={0};simple.inverseBind={Identity()};
    Track track;track.path=Path::Translation;track.step=true;track.times={0,1,2};track.values={Quat{0,0,0,0},Quat{1,0,0,0},Quat{2,0,0,0}};
    simple.clips[0].duration=2;simple.clips[0].tracks={track};
    for(float t:{0.f,.49f,.5f,.99f,1.f}) {simple.Sample(handanimation::Gesture::Grip,t,expected);assert(expected[0].t[0]==(t<.5f?0:t<1?1:2));}
    simple.clips[0].tracks[0].step=false;simple.Sample(handanimation::Gesture::Grip,.25f,expected);assert(expected[0].t[0]==.5f);
    simple.clips[0].tracks[0].path=Path::Scale;simple.Sample(handanimation::Gesture::Grip,.25f,expected);assert(expected[0].s[0]==.5f);
    int rejected=0;
    for(const auto& entry:std::filesystem::directory_iterator(argv[3])) {
        assert(!lib.Load(entry.path().string()));assert(lib.loaded && lib.vertices.size()==2437);
        std::printf("PASS rejected %s: %s\n",entry.path().filename().string().c_str(),lib.error.c_str());++rejected;
    }
    assert(rejected>=10);
    std::ofstream gpu(std::filesystem::path(argv[2]).parent_path()/"gpu.json");
    gpu<<std::setprecision(9)<<"{\"palettes\":[";
    int cases=0;
    const auto emit=[&](const Palette& palette) {
        if(cases++)gpu<<',';gpu<<'[';
        for(std::size_t i=0;i<lib.joints.size();++i)for(int k=0;k<16;++k) {if(i||k)gpu<<',';gpu<<palette[i][k];}
        gpu<<']';
    };
    for(std::size_t clip=0;clip<lib.clips.size();++clip)for(float t:{0.f,.5f,1.f}) {
        lib.Sample(static_cast<handanimation::Gesture>(clip),t,a);lib.Matrices(a,p);emit(p);
    }
    for(auto selection:{handanimation::FreeHand(.2f,.8f),handanimation::FreeHand(.8f,.2f)}) {
        lib.Evaluate(selection,.6f,.4f,p);emit(p);
    }
    gpu<<"]}";
    std::printf("PASS bind error %.9g; max Blender error %.6f mm; CPU bytes %zu (includes embedded PNG %zu)\n",
        bindError,globalMax*1000,lib.ResidentBytes(),lib.albedoPng.size());
}

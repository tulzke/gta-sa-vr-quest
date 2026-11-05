#include "HandAnimation.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>

using namespace savr::handanimation;
bool Near(float a,float b) {return std::abs(a-b)<1e-6f;}
int main(int argc,char** argv) {
    assert(argc==3);
    const auto zero=FreeHand(0,0);
    assert(zero.first==Gesture::Grip && zero.progress==0 && zero.blend==0);
    const auto grip=FreeHand(1,0);
    assert(grip.first==Gesture::Grip && grip.progress==1 && grip.blend==0);
    const auto point=FreeHand(0,1);
    assert(point.first==Gesture::Point && point.progress==1 && point.blend==0);
    const auto fist=FreeHand(1,1);
    assert(fist.second==Gesture::Fist && fist.progress==1 && fist.blend==1);
    const auto middle=FreeHand(.4f,.4f);
    assert(Near(middle.progress,.4f) && middle.blend==1);
    assert(FreeHand(std::numeric_limits<float>::quiet_NaN(),0).progress==0);
    for (int type=0;type<48;++type) assert(Pistol(type)==(type>=22 && type<=24));
    Context context;
    context.holdsWeapon=true;context.weaponType=22;
    assert(Select(1,.5f,context).first==Gesture::GripPistolShoot);
    assert(Near(Select(1,.5f,context).progress,.5f));
    context.weaponType=25;
    assert(Select(1,1,context).first==Gesture::Grip);
    context.support=true;context.weaponType=22;
    assert(Select(1,1,context).first==Gesture::Grip); // Offhand is not assigned.
    context.ball=true;
    assert(Select(1,1,context).first==Gesture::OpenPalm);
    context.steering=true;
    assert(Select(1,1,context).first==Gesture::Grip);
    context.pinchGrab=true;
    assert(Select(1,1,context).first==Gesture::Pinch);
    context.climb=true;
    assert(Select(1,1,context).first==Gesture::Grip);
    Library library;
    assert(library.Load(argv[1]));
    const auto& base=library.Get(Gesture::Grip);
    assert(base.uv.size()==2437 && base.indices.size()==13092);
    for (std::size_t n=0;n<kNames.size();++n) {
        const auto& clip=library.clips[n];
        assert(clip.Loaded() && clip.uv==base.uv && clip.indices==base.indices);
        for (std::size_t i=0;i<clip.frames.size();++i) {
            float progress=(clip.frames[i]-clip.frames.front())/(clip.frames.back()-clip.frames.front());
            const auto segment=clip.At(progress);
            for (std::size_t v=0;v<clip.uv.size();++v) {
                const auto actual=clip.Sample(segment,v);
                const auto& expected=clip.vertices[i*clip.uv.size()+v];
                for (int axis=0;axis<3;++axis) {
                    assert(Near(actual.pos[axis],expected.pos[axis]));
                    assert(std::abs(actual.normal[axis]-expected.normal[axis])<1e-5f);
                }
            }
        }
        // Interpolation uses source frame timing, including across block seams.
        const float frame=(clip.frames[3]+clip.frames[4])*.5f;
        const auto segment=clip.At((frame-clip.frames.front())/(clip.frames.back()-clip.frames.front()));
        assert(segment.first==3 && segment.last==4 && Near(segment.fraction,.5f));
        assert(clip.At(-1).first==0 && clip.At(2).first==clip.frames.size()-1);
        std::printf("PASS native sampler: %s (%zu frames)\n",kNames[n],clip.frames.size());
    }
    const auto& pistol=library.Get(Gesture::GripPistol);
    const auto& shoot=library.Get(Gesture::GripPistolShoot);
    assert(shoot.frames.front()==0 && shoot.frames.back()==5);
    for (std::size_t v=0;v<base.uv.size();++v) {
        auto held=pistol.Sample(pistol.At(1),v),start=shoot.Sample(shoot.At(0),v);
        for (int axis=0;axis<3;++axis) assert(Near(held.pos[axis],start.pos[axis]));
    }
    // A failed reload must keep the complete previously loaded library.
    assert(!library.Load(argv[2]) && library.Get(Gesture::Grip).Loaded());
    const auto folder=std::filesystem::path(argv[2])/"Grip";
    std::filesystem::create_directories(folder);
    {std::ofstream out(folder/"timing.txt");out<<"UXRA 1 2 24\n1 1\n";}
    Clip invalid;
    assert(!LoadClip(argv[2],Gesture::Grip,invalid));
    {std::ofstream out(folder/"timing.txt");out<<"UXRA 1 99999999 24\n";}
    assert(!LoadClip(argv[2],Gesture::Grip,invalid));
    {std::ofstream out(folder/"timing.txt");out<<"UXRA 1 2 24\n0 1\n";}
    {std::ofstream out(folder/"BigHandRight-000.uxrh",std::ios::binary);out<<"UXRH";}
    assert(!LoadClip(argv[2],Gesture::Grip,invalid));
    std::puts("PASS invalid/missing assets preserve fallback");
}

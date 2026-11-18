"""Exercise guide visibility, endpoints and draw state with synthetic poses."""

from pathlib import Path
import runpy
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / 'native/src/Xr.cpp').read_text(encoding='utf8')
compile_run = runpy.run_path(str(ROOT / 'tools/test-bike-steering.py'))['compile_run']

math = SOURCE[SOURCE.index('inline XrVector3f vadd('):SOURCE.index('XrVector3f RotateAroundAxis(')]
guide = SOURCE[SOURCE.index('struct WheelHandGuide {'):SOURCE.index('struct SkeletalHandFrame {')]
draw = SOURCE[SOURCE.index('void DrawWheelHandGuides('):SOURCE.index('#include "RadioLabelRender.inl"')]

test = r'''
#include "TrackingFrame.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <limits>
using savr::xr::HandPose;
struct XrVector3f {float x,y,z;};
#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#condition); std::exit(1); \
} } while (false)
bool Near(float a,float b) { return std::abs(a-b)<.00001f; }
''' + math + guide + r'''
struct { WheelHandGuide wheelGuide; } g_skeletalHands[2];
enum { GL_FALSE, GL_TRUE, GL_BLEND, GL_FUNC_ADD, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA };
bool blend=false, depthWrite=true;
int draws=0;
void glEnable(int mode) { CHECK(mode==GL_BLEND); blend=true; }
void glDisable(int mode) { CHECK(mode==GL_BLEND); blend=false; }
void glDepthMask(int mask) { depthWrite=mask==GL_TRUE; }
void glBlendEquation(int equation) { CHECK(equation==GL_FUNC_ADD); }
void glBlendFunc(int src,int dst) { CHECK(src==GL_SRC_ALPHA && dst==GL_ONE_MINUS_SRC_ALPHA); }
void DrawDrivingBoxes(const float*,const float* vertices,int count,const unsigned short* indices,
                      int indexCount,float,float,float,float alpha) {
    CHECK(blend && !depthWrite && count==7 && indexCount==9 && indices[8]==6);
    const auto& guide=g_skeletalHands[draws++].wheelGuide;
    const auto component=[](XrVector3f point,int i){return i==0?point.x:i==1?point.y:point.z;};
    for(int i=0;i<3;++i) {
        CHECK(Near((vertices[i]+vertices[i+3])*.5f,component(guide.controller,i)));
        CHECK(Near(vertices[18+i],component(guide.pinned,i)));
    }
    CHECK(Near(alpha,guide.opacity));
}
''' + draw + r'''
int main() {
    HandPose pose{};
    const XrVector3f far{.35f,0,0};
    pose.positionTracked=true;
    CHECK(MakeWheelHandGuide(pose,far).opacity==0);
    pose.valid=true;pose.positionTracked=false;
    CHECK(MakeWheelHandGuide(pose,far).opacity==0);
    pose.positionTracked=true;
    CHECK(MakeWheelHandGuide(pose,{.10f,0,0}).opacity==0);
    CHECK(MakeWheelHandGuide(pose,{.12f,0,0}).opacity>0);
    CHECK(MakeWheelHandGuide(pose,{0,0,std::numeric_limits<float>::infinity()}).opacity==0);
    CHECK(Near(MakeWheelHandGuide(pose,{0,.19f,0}).opacity,.115f));
    CHECK(Near(MakeWheelHandGuide(pose,{0,0,.28f}).opacity,.23f));
    CHECK(Near(MakeWheelHandGuide(pose,far).opacity,.23f));

    DrawWheelHandGuides(nullptr,{0,1,1});
    CHECK(draws==0 && !blend && depthWrite);
    g_skeletalHands[0].wheelGuide=MakeWheelHandGuide(pose,far);
    pose.gripPos[1]=-.3f;
    g_skeletalHands[1].wheelGuide=MakeWheelHandGuide(pose,{0,0,-.2f});
    DrawWheelHandGuides(nullptr,{0,1,1});
    CHECK(draws==2 && !blend && depthWrite);
    std::puts("PASS: wheel guide visibility, stereo arrow endpoints and draw state");
}
'''

with tempfile.TemporaryDirectory(prefix='savr-wheel-hand-guide-') as tmp:
    output = Path(tmp)
    cpp = output / 'wheel-hand-guide.cpp'
    cpp.write_text(test, encoding='utf8')
    compile_run(cpp, 'wheel-hand-guide', output)

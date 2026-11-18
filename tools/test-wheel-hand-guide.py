"""Exercise guide visibility, endpoints and draw state with synthetic poses."""

from pathlib import Path
import runpy
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / 'native/src/Xr.cpp').read_text(encoding='utf8')
helpers = runpy.run_path(str(ROOT / 'tools/test-bike-steering.py'))
compile_run = helpers['compile_run']
driving = helpers['SOURCE']

math = SOURCE[SOURCE.index('inline XrVector3f vadd('):SOURCE.index('XrVector3f RotateAroundAxis(')]
guide = SOURCE[SOURCE.index('struct WheelHandGuide {'):SOURCE.index('struct SkeletalHandFrame {')]
draw = SOURCE[SOURCE.index('void DrawWheelHandGuides('):SOURCE.index('#include "RadioLabelRender.inl"')]
setting = driving[driving.index('constexpr float kDefaultWheelHandGuideThresholdCm='):
                  driving.index('std::atomic<bool> g_highlights')]
format_line = next(line for line in driving.splitlines() if '"WheelHandGuideThresholdCm=%.1f' in line)
parse_start = driving.index('else if (std::sscanf(line, "WheelHandGuideThresholdCm=')
parse_line = driving[parse_start + len('else '):driving.index('else if', parse_start + len('else if'))]
get_threshold = helpers['function']('float GetWheelHandGuideThresholdCm()')
adjust_threshold = helpers['function']('void AdjustWheelHandGuideThreshold(')

test = r'''
#include "TrackingFrame.h"
#include <algorithm>
#include <atomic>
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
''' + setting + r'''
char savedThreshold[64]{};
void EnsureInit() {}
void Save() {
    FILE* file=std::tmpfile();
    CHECK(file);
''' + format_line + r'''
    std::rewind(file);
    CHECK(std::fgets(savedThreshold,sizeof(savedThreshold),file));
    std::fclose(file);
}
float LoadThreshold(const char* line) {
    float wheelHandGuideThresholdCm=kDefaultWheelHandGuideThresholdCm;
    float spatial{};
''' + parse_line + r'''
    return wheelHandGuideThresholdCm;
}
''' + get_threshold + adjust_threshold + math + guide + r'''
WheelHandGuide GuideAtCurrentThreshold(const HandPose& pose,XrVector3f pinned) {
    return MakeWheelHandGuide(pose,pinned,GetWheelHandGuideThresholdCm()/100.f);
}
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
    CHECK(GetWheelHandGuideThresholdCm()==7.5f);
    CHECK(LoadThreshold("WheelHandGuide=0")==7.5f);
    CHECK(LoadThreshold("WheelHandGuideThresholdCm=nan")==7.5f);
    CHECK(LoadThreshold("WheelHandGuideThresholdCm=-2")==1.f);
    CHECK(LoadThreshold("WheelHandGuideThresholdCm=40")==15.f);
    AdjustWheelHandGuideThreshold(1);
    CHECK(GetWheelHandGuideThresholdCm()==8.f && LoadThreshold(savedThreshold)==8.f);
    AdjustWheelHandGuideThreshold(-1);
    CHECK(GetWheelHandGuideThresholdCm()==7.5f && LoadThreshold(savedThreshold)==7.5f);

    HandPose pose{};
    const XrVector3f far{.35f,0,0};
    pose.positionTracked=true;
    CHECK(GuideAtCurrentThreshold(pose,far).opacity==0);
    pose.valid=true;pose.positionTracked=false;
    CHECK(GuideAtCurrentThreshold(pose,far).opacity==0);
    pose.positionTracked=true;
    CHECK(GuideAtCurrentThreshold(pose,{.075f,0,0}).opacity==0);
    CHECK(GuideAtCurrentThreshold(pose,{.09f,0,0}).opacity>0);
    CHECK(GuideAtCurrentThreshold(pose,{0,0,std::numeric_limits<float>::infinity()}).opacity==0);
    CHECK(Near(GuideAtCurrentThreshold(pose,{0,.1775f,0}).opacity,.115f));
    CHECK(Near(GuideAtCurrentThreshold(pose,{0,0,.28f}).opacity,.23f));
    CHECK(Near(GuideAtCurrentThreshold(pose,far).opacity,.23f));

    for (int i=0;i<30;++i) AdjustWheelHandGuideThreshold(-1);
    CHECK(GetWheelHandGuideThresholdCm()==1.f);
    CHECK(GuideAtCurrentThreshold(pose,{.009f,0,0}).opacity==0);
    CHECK(GuideAtCurrentThreshold(pose,{.02f,0,0}).opacity>0);
    for (int i=0;i<30;++i) AdjustWheelHandGuideThreshold(1);
    CHECK(GetWheelHandGuideThresholdCm()==15.f);
    CHECK(GuideAtCurrentThreshold(pose,{.15f,0,0}).opacity==0);
    CHECK(GuideAtCurrentThreshold(pose,{.16f,0,0}).opacity>0);

    DrawWheelHandGuides(nullptr,{0,1,1});
    CHECK(draws==0 && !blend && depthWrite);
    g_skeletalHands[0].wheelGuide=GuideAtCurrentThreshold(pose,far);
    pose.gripPos[1]=-.3f;
    g_skeletalHands[1].wheelGuide=GuideAtCurrentThreshold(pose,{0,0,-.2f});
    DrawWheelHandGuides(nullptr,{0,1,1});
    CHECK(draws==2 && !blend && depthWrite);
    std::puts("PASS: wheel guide setting, visibility, stereo arrow endpoints and draw state");
}
'''

with tempfile.TemporaryDirectory(prefix='savr-wheel-hand-guide-') as tmp:
    output = Path(tmp)
    cpp = output / 'wheel-hand-guide.cpp'
    cpp.write_text(test, encoding='utf8')
    compile_run(cpp, 'wheel-hand-guide', output)

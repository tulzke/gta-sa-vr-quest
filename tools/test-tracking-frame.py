"""Run the production XR pose readers against concurrent frame publications."""
from pathlib import Path
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/tracking-frame-tests'
OUT.mkdir(parents=True, exist_ok=True)
source = (ROOT / 'native/src/Xr.cpp').read_text(encoding='utf8')


def function(signature):
    start = source.index(signature)
    brace = source.index('{', start)
    end, depth = brace + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + '\n'


code = r'''
#include "TrackingFrame.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <mutex>
#include <thread>
using namespace savr::xr;
struct Pose { struct {float x,y,z;} position; struct {float x,y,z,w;} orientation; };
Pose g_headPose{},g_eyePose[2]{};
HandPose g_handPose[2]{};
bool g_headPoseValid{};
struct {std::uint64_t epoch{};} g_trackingRecovery;
std::uint64_t g_trackingSequence{};
std::int64_t g_trackingDisplayTime{};
std::mutex g_headPoseMutex;
bool GetHeadPose(float*,float*,std::uint64_t*);
'''
for sig in ('TrackingSnapshot CaptureTrackingSnapshot()',
            'void GetHandPoses(HandPose out[2])',
            'bool GetTrackingSnapshot(float p[3]',
            'bool GetHeadPose(float positionOut[3], float orientationOut[4])',
            'bool GetHeadPose(float positionOut[3], float orientationOut[4], std::uint64_t* epoch)',
            'bool GetEyePoses('):
    code += function(sig)
code += r'''
void Publish(int n,bool valid=true){
 std::lock_guard<std::mutex> lock(g_headPoseMutex);
 g_trackingSequence=n;g_trackingDisplayTime=n*1000;g_trackingRecovery.epoch=n/10;
 g_headPoseValid=valid;g_headPose.position.x=float(n);g_headPose.orientation.w=1;
 for(int h=0;h<2;++h){
  g_handPose[h].valid=valid;g_handPose[h].gripPos[0]=float(n+h);
  g_eyePose[h].position.x=float(n+h);
 }
}
void Check(int n,bool valid=true){
 const auto s=CaptureTrackingSnapshot();
 assert(s.valid==valid && s.sequence==unsigned(n) && s.epoch==unsigned(n/10));
 assert(s.displayTime==n*1000);
 float p[3]{},q[4]{},ep[2][3]{},eq[2][4]{};HandPose hands[2]{};std::uint64_t epoch{};
 assert(GetHeadPose(p,q,&epoch)==valid && epoch==unsigned(n/10));
 assert(GetHeadPose(p,q)==valid);
 assert(GetTrackingSnapshot(p,q,hands,&epoch)==valid && epoch==unsigned(n/10));
 if(valid)assert(p[0]==n && hands[0].gripPos[0]==n && hands[1].gripPos[0]==n+1);
 else assert(!hands[0].valid && !hands[1].valid);
 GetHandPoses(hands);assert(hands[0].valid==valid);
 assert(GetEyePoses(ep,eq)==valid);
 if(valid)assert(ep[1][0]==n+1);
}
int main(){
 Publish(11);Check(11);
 auto initial=CaptureTrackingSnapshot();
 {
  ScopedTrackingFrame frame(initial);
  initial.sequence=999; // Scope owns a value, never the caller's mutable sample.
  std::thread producer([]{Publish(22);Check(22);});producer.join();
  Check(11); // Camera, hands and eyes stay together even across a new epoch.
  {
   TrackingSnapshot invalid{};invalid.sequence=33;invalid.epoch=3;invalid.displayTime=33000;
   ScopedTrackingFrame nested(invalid);Check(33,false);
   std::thread recovery([]{Publish(44);Check(44);});recovery.join();
   Check(33,false); // No fallback to freshly recovered tracking mid-frame.
  }
  Check(11);
 }
 Check(44); // The next game frame can acquire the newest sample.
 Publish(55,false);Check(55,false);
 std::puts("PASS tracking frame: production readers, head/hands/eyes, epochs, invalidation, nesting, thread isolation");
}
'''
# Guard the integration point: ownership must be refreshed inside the same scope
# before the engine builds its camera/weapon matrices, exactly once per frame.
main = (ROOT / 'native/src/main.cpp').read_text(encoding='utf8')
frame = main[main.index('void OnDrawFrame('):main.index('// The present-only consumer thread.')]
assert frame.count('SendInputToGame(env, clazz)') == 1
assert frame.index('ScopedTrackingFrame trackingFrame') < frame.index('SendInputToGame(env, clazz)') < frame.index('g.implOnDrawFrame(env, clazz, deltaTime)')

test = OUT / 'tracking_frame.cpp'
test.write_text(code, encoding='utf8')
exe = OUT / ('test.exe' if os.name == 'nt' else 'test')
if os.name == 'nt':
    where = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    base = subprocess.check_output([str(where), '-latest', '-products', '*', '-requires',
        'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
    vc = Path(base) / 'VC/Auxiliary/Build/vcvars64.bat'
    script = OUT / 'test.cmd'
    script.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\n'
        f'cl /nologo /std:c++20 /EHsc /W4 /I"{ROOT / "native/src"}" "{test}" '
        f'/Fe:"{exe}" /Fo:"{OUT / "test.obj"}"\nif errorlevel 1 exit /b 1\n"{exe}"\n', encoding='utf8')
    subprocess.run(['cmd', '/c', str(script)], cwd=OUT, check=True)
else:
    subprocess.run([shutil.which('c++'), '-std=c++20', '-pthread', '-Wall', '-Wextra',
        '-I'+str(ROOT/'native/src'), str(test), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)

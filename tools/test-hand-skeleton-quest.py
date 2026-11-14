"""Run the GLB loader, Android PNG decoder and production skin shader on Quest.

Does not start the game or modify saves. Installs a temporary native diagnostic
under /data/local/tmp and reads the hands.glb installed by the full deploy.
"""
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
cfg=json.loads(Path(r'D:\development\SAVRBuild\tools.json').read_text(encoding='utf-8-sig'))
ndk=Path(cfg['AndroidSdk'])/'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64'
out=ROOT/'.cache/hand-skeletal'
source=(ROOT/'native/src/Xr.cpp').read_text(encoding='utf-8')
region=source.split('bool EnsureSkeletalRenderer() {',1)[1].split('void DrawSkeletalHands',1)[0]
vs,fs=re.findall(r'R"GLSL\((.*?)\)GLSL"',region,re.S)
code=r'''
#include "HandSkeleton.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <dlfcn.h>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <vector>
using namespace savr;
using namespace handskeleton;
const char* vs=R"SHADER(__VS__)SHADER";
const char* fs=R"SHADER(__FS__)SHADER";
GLuint Shader(GLenum type,const char* src) {
    const auto sh=glCreateShader(type);glShaderSource(sh,1,&src,nullptr);glCompileShader(sh);
    GLint ok=0;glGetShaderiv(sh,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[4096]{};glGetShaderInfoLog(sh,sizeof(log),nullptr,log);std::puts(log);}assert(ok);return sh;
}
int main() {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    Library lib;
    if(!lib.Load("/sdcard/Android/data/com.rockstargames.gtasa/files/vrhands/skeletal/hands.glb")) {
        std::printf("FAIL actual GLB: %s\n",lib.error.c_str());return 2;
    }
    void* graphics=dlopen("libjnigraphics.so",RTLD_NOW|RTLD_LOCAL);assert(graphics);
    const auto create=reinterpret_cast<int(*)(const void*,size_t,void**)>(dlsym(graphics,"AImageDecoder_createFromBuffer"));
    const auto decode=reinterpret_cast<int(*)(void*,void*,size_t,size_t)>(dlsym(graphics,"AImageDecoder_decodeImage"));
    const auto format=reinterpret_cast<int(*)(void*,int)>(dlsym(graphics,"AImageDecoder_setAndroidBitmapFormat"));
    const auto destroy=reinterpret_cast<void(*)(void*)>(dlsym(graphics,"AImageDecoder_delete"));
    assert(create&&decode&&format&&destroy);void* decoder=nullptr;
    assert(create(lib.albedoPng.data(),lib.albedoPng.size(),&decoder)==0 && decoder);
    std::vector<unsigned char> pixels(1024*1024*4);
    assert(format(decoder,1)==0 && decode(decoder,pixels.data(),4096,pixels.size())==0);
    destroy(decoder);dlclose(graphics);std::puts("PASS Android embedded GLB PNG decode");
    EGLDisplay display=eglGetDisplay(EGL_DEFAULT_DISPLAY);assert(display!=EGL_NO_DISPLAY);
    EGLint major=0,minor=0;assert(eglInitialize(display,&major,&minor));
    const EGLint attrib[]{EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_NONE};
    EGLConfig config{};EGLint n=0;assert(eglChooseConfig(display,attrib,&config,1,&n)&&n==1);
    const EGLint ctxAttrib[]{EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
    EGLContext context=eglCreateContext(display,config,EGL_NO_CONTEXT,ctxAttrib);assert(context!=EGL_NO_CONTEXT);
    const EGLint surfAttrib[]{EGL_WIDTH,8,EGL_HEIGHT,8,EGL_NONE};
    EGLSurface surface=eglCreatePbufferSurface(display,config,surfAttrib);assert(surface!=EGL_NO_SURFACE);
    assert(eglMakeCurrent(display,surface,surface,context));
    std::printf("GPU: %s / %s\n",glGetString(GL_RENDERER),glGetString(GL_VERSION));
    const auto program=glCreateProgram();glAttachShader(program,Shader(GL_VERTEX_SHADER,vs));glAttachShader(program,Shader(GL_FRAGMENT_SHADER,fs));
    const char* varyings[]{"gl_Position","vN","vUV"};glTransformFeedbackVaryings(program,3,varyings,GL_INTERLEAVED_ATTRIBS);glLinkProgram(program);
    GLint ok=0;glGetProgramiv(program,GL_LINK_STATUS,&ok);
    if(!ok){char log[4096]{};glGetProgramInfoLog(program,sizeof(log),nullptr,log);std::puts(log);}assert(ok);glUseProgram(program);
    GLuint vao=0,vbo=0,feedback=0,buffer=0,texture=0;
    glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,lib.vertices.size()*sizeof(Vertex),lib.vertices.data(),GL_STATIC_DRAW);
    for(int i=0;i<7;++i)glEnableVertexAttribArray(i);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,pos)));
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,normal)));
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,uv)));
    glVertexAttribIPointer(3,4,GL_UNSIGNED_SHORT,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,joints)));
    glVertexAttribIPointer(4,4,GL_UNSIGNED_SHORT,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,joints)+8));
    glVertexAttribPointer(5,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,weights)));
    glVertexAttribPointer(6,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,weights)+16));
    glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1024,1024,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glGenTransformFeedbacks(1,&feedback);glBindTransformFeedback(GL_TRANSFORM_FEEDBACK,feedback);
    glGenBuffers(1,&buffer);glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffer);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,lib.vertices.size()*9*sizeof(float),nullptr,GL_DYNAMIC_READ);glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer);
    const auto loc=[&](const char* name){return glGetUniformLocation(program,name);};
    const Mat identity=Identity();glUniformMatrix4fv(loc("uWrist"),1,GL_FALSE,identity.data());glUniform1i(loc("uLedge"),0);glUniform1i(loc("uTex"),0);
    glEnable(GL_RASTERIZER_DISCARD);float maximum=0;unsigned checks=0;
    for(int clip=0;clip<static_cast<int>(handanimation::Gesture::Count);++clip)
    for(float progress:{0.f,.5f,1.f})for(float side:{-1.f,1.f})for(float eye:{-1.f,1.f}) {
        Pose pose{};Palette palette{};lib.Sample(static_cast<handanimation::Gesture>(clip),progress,pose);lib.Matrices(pose,palette);
        Mat model=identity,mvp=identity;model[5]=side;mvp[12]=eye*.031f;
        glUniformMatrix4fv(loc("uModel"),1,GL_FALSE,model.data());glUniformMatrix4fv(loc("uMVP"),1,GL_FALSE,mvp.data());
        glUniformMatrix4fv(loc("uJoints[0]"),lib.joints.size(),GL_FALSE,palette[0].data());
        glBeginTransformFeedback(GL_POINTS);glDrawArrays(GL_POINTS,0,lib.vertices.size());glEndTransformFeedback();glFinish();
        const auto* actual=static_cast<const float*>(glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER,0,lib.vertices.size()*9*sizeof(float),GL_MAP_READ_BIT));assert(actual);
        for(std::size_t i=0;i<lib.vertices.size();++i) {
            const auto& v=lib.vertices[i];Vec p{};
            for(int j=0;j<8;++j)for(int r=0;r<3;++r) {const auto& m=palette[v.joints[j]];
                p[r]+=v.weights[j]*(m[r]*v.pos[0]+m[4+r]*v.pos[1]+m[8+r]*v.pos[2]+m[12+r]);}
            const float expected[]{p[0]+eye*.031f,-p[2]*side,p[1],1};
            for(int k=0;k<4;++k){assert(std::isfinite(actual[i*9+k]));maximum=std::max(maximum,std::abs(expected[k]-actual[i*9+k]));}
            for(int k=0;k<3;++k)assert(std::isfinite(actual[i*9+4+k]));
            for(int k=0;k<2;++k)assert(std::abs(v.uv[k]-actual[i*9+7+k])<1e-5f);
            ++checks;
        }
        assert(glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER));assert(glGetError()==GL_NO_ERROR);
    }
    assert(maximum<2e-5f);
    std::printf("PASS actual Quest GLB GPU skinning: %u vertex comparisons, max %.9g metres; all 14 clips, both hands/eyes; no fallback\n",checks,maximum);
    eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroySurface(display,surface);eglDestroyContext(display,context);eglTerminate(display);
}
'''
probe=out/'quest-gpu-probe.cpp'
probe.write_text(code.replace('__VS__',vs).replace('__FS__',fs),encoding='utf-8')
exe=out/'quest-gpu-probe'
subprocess.run([str(ndk/'bin/clang++.exe'),'--target=aarch64-linux-android26','--sysroot='+str(ndk/'sysroot'),
    '-std=c++20','-O2','-static-libstdc++','-I'+str(ROOT/'native/src'),str(probe),str(ROOT/'native/src/HandSkeleton.cpp'),
    '-lEGL','-lGLESv3','-ldl','-o',str(exe)],check=True)
remote='/data/local/tmp/savr-hand-skeleton-probe'
try:
    subprocess.run([cfg['Adb'],'push',str(exe),remote],check=True)
    subprocess.run([cfg['Adb'],'shell','chmod','700',remote],check=True)
    subprocess.run([cfg['Adb'],'shell',remote],check=True)
finally:
    subprocess.run([cfg['Adb'],'shell','rm','-f',remote],check=False)

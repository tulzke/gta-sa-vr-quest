#include "ReloadHint.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <map>
using GLuint=unsigned;using GLint=int;using GLsizei=int;using GLenum=unsigned;using GLboolean=unsigned char;
enum {GL_BLEND=1,GL_CULL_FACE,GL_DEPTH_TEST,GL_CURRENT_PROGRAM,GL_ARRAY_BUFFER_BINDING,
GL_ELEMENT_ARRAY_BUFFER_BINDING,GL_DEPTH_FUNC,GL_CULL_FACE_MODE,GL_DEPTH_WRITEMASK,GL_COLOR_WRITEMASK,
GL_BLEND_SRC_RGB,GL_BLEND_DST_RGB,GL_BLEND_SRC_ALPHA,GL_BLEND_DST_ALPHA,GL_BLEND_EQUATION_RGB,GL_BLEND_EQUATION_ALPHA,
GL_TRUE,GL_BACK,GL_FRONT,GL_LEQUAL,GL_ARRAY_BUFFER,GL_ELEMENT_ARRAY_BUFFER,GL_STATIC_DRAW,GL_FLOAT,GL_TRIANGLES,
GL_UNSIGNED_SHORT,GL_EQUAL,GL_FUNC_ADD,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_DEPTH_BUFFER_BIT};
constexpr int GL_FALSE=0;
int checks{},draws{},uploads{},clears{};
int frontCullDraws{};
void Check(bool v,const char* m){++checks;if(!v){std::fprintf(stderr,"FAIL: %s\n",m);std::exit(1);}}
std::map<unsigned,int> state;
std::array<GLboolean,4> colorMask{1,1,1,1};std::array<float,4> color{};
GLuint nextBuffer=100,g_markerProg=7;int g_markerMvpLoc=1,g_markerColLoc=2;
void EnsureMarkerProgram(){}
GLboolean glIsEnabled(GLenum e){return static_cast<GLboolean>(state[e]);}
void glEnable(GLenum e){state[e]=1;}void glDisable(GLenum e){state[e]=0;}
void glGetIntegerv(GLenum e,GLint* p){*p=state[e];}
void glGetBooleanv(GLenum e,GLboolean* p){if(e==GL_COLOR_WRITEMASK)std::copy(colorMask.begin(),colorMask.end(),p);else *p=static_cast<GLboolean>(state[e]);}
void glUseProgram(GLuint p){state[GL_CURRENT_PROGRAM]=p;}
void glCullFace(GLenum e){state[GL_CULL_FACE_MODE]=e;}void glDepthFunc(GLenum e){state[GL_DEPTH_FUNC]=e;}
void glDepthMask(GLboolean e){state[GL_DEPTH_WRITEMASK]=e;}
void glClear(GLenum e){Check(e==GL_DEPTH_BUFFER_BIT,"scope clears only private overlay depth");++clears;}
void glColorMask(GLboolean a,GLboolean b,GLboolean c,GLboolean d){colorMask={a,b,c,d};}
void glBlendEquationSeparate(GLenum a,GLenum b){state[GL_BLEND_EQUATION_RGB]=a;state[GL_BLEND_EQUATION_ALPHA]=b;}
void glBlendFuncSeparate(GLenum a,GLenum b,GLenum c,GLenum d){state[GL_BLEND_SRC_RGB]=a;state[GL_BLEND_DST_RGB]=b;state[GL_BLEND_SRC_ALPHA]=c;state[GL_BLEND_DST_ALPHA]=d;}
void glBindBuffer(GLenum e,GLuint id){state[e==GL_ARRAY_BUFFER?GL_ARRAY_BUFFER_BINDING:GL_ELEMENT_ARRAY_BUFFER_BINDING]=id;}
GLboolean glIsBuffer(GLuint p){return p>=100;}
void glGenBuffers(int,GLuint* p){*p=nextBuffer++;}
void glBufferData(GLenum,std::size_t bytes,const void* data,GLenum){Check(bytes>0 && data,"actual copied mesh is uploaded");++uploads;}
void glUniformMatrix4fv(int,int,int,const float* m){for(int i=0;i<16;++i)Check(std::isfinite(m[i]),"stereo model matrix remains finite");}
void glUniform4f(int,float r,float g,float b,float a){color={r,g,b,a};}
void glEnableVertexAttribArray(int){}void glDisableVertexAttribArray(int){}
void glVertexAttribPointer(int,int,int,int,std::size_t,const void*){}
void glDrawElements(GLenum,int count,GLenum,const void*){
    ++draws;Check(count==3,"draw uses current weapon triangles");
    if(state[GL_CULL_FACE_MODE]==GL_FRONT)++frontCullDraws;
    if(!colorMask[0])Check(state[GL_DEPTH_WRITEMASK] && !state[GL_BLEND] && state[GL_DEPTH_FUNC]==GL_LEQUAL,"depth-only pass precedes alpha color");
    else Check(!state[GL_DEPTH_WRITEMASK] && state[GL_BLEND] && state[GL_DEPTH_FUNC]==GL_EQUAL &&
               color[0]==1 && color[1]==1 && color[2]==0 && color[3]>0 && color[3]<=.2f,"nearest weapon surface is flat yellow with at most twenty percent opacity");
}
void MatMul(float* out,const float* a,const float* b){for(int c=0;c<4;++c)for(int r=0;r<4;++r){out[c*4+r]=0;for(int k=0;k<4;++k)out[c*4+r]+=a[k*4+r]*b[c*4+k];}}
namespace reloadhint=savr::reloadhint;
namespace arcadereload=savr::arcadereload;
#include "ReloadHintRender.inl"
int main(){
    state={{GL_BLEND,0},{GL_CULL_FACE,0},{GL_DEPTH_TEST,1},{GL_CURRENT_PROGRAM,3},
           {GL_ARRAY_BUFFER_BINDING,8},{GL_ELEMENT_ARRAY_BUFFER_BINDING,9},{GL_DEPTH_FUNC,77},
           {GL_CULL_FACE_MODE,78},{GL_DEPTH_WRITEMASK,1},{GL_BLEND_SRC_RGB,79},{GL_BLEND_DST_RGB,80},
           {GL_BLEND_SRC_ALPHA,81},{GL_BLEND_DST_ALPHA,82},{GL_BLEND_EQUATION_RGB,83},{GL_BLEND_EQUATION_ALPHA,84}};
    const auto saved=state;const auto savedMask=colorMask;
    auto model=std::make_shared<reloadhint::Model>();model->vertices={0,0,0,.2f,0,0,0,.1f,0};model->indices={0,1,2};model->extent=.2f;
    reloadhint::VisualFrame frame;frame.items[0].model=model;
    const std::array<float,16> identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    DrawReloadHintsForEye(identity.data(),frame,false);
    // Ignore zero-valued lookup entries materialized by the fake getter.
    for(const auto& [key,value]:saved)Check(state[key]==value,"all touched GL state is restored");
    Check(colorMask==savedMask && draws==2 && uploads==2 && clears==0,"one hint is two passes and does not clear game depth");
    DrawReloadHintsForEye(identity.data(),frame,false);Check(uploads==2 && draws==4,"second eye reuses immutable uploaded geometry");
    frame.items[1].model=model;DrawReloadHintsForEye(identity.data(),frame,true);
    Check(draws==8 && clears==1,"two hints share a nearest-surface pass even with scoped view");
    frame.items[1].pose.side={0,0,-1};frame.items[1].opacity=.10f;
    DrawReloadHintsForEye(identity.data(),frame,false);
    Check(draws==12 && frontCullDraws==2,"reflected live weapon pose uses the same front culling as the real weapon");
    frame.items[0].opacity=frame.items[1].opacity=0;
    DrawReloadHintsForEye(identity.data(),frame,false);Check(draws==12,"invisible reset phase draws neither color nor depth");
    frame={};DrawReloadHintsForEye(identity.data(),frame,false);Check(draws==12,"empty snapshot draws no stale hint");
    std::printf("PASS: %d translucent hint renderer checks\n",checks);
}

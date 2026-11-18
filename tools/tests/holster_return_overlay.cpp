#include "HolsterMarker.h"
#include <cassert>
#include <vector>

using namespace savr;
using GLboolean=bool;
constexpr bool GL_TRUE=true,GL_FALSE=false;
enum {GL_DEPTH_TEST,GL_DEPTH_WRITEMASK,GL_CULL_FACE,GL_ARRAY_BUFFER,
    GL_ELEMENT_ARRAY_BUFFER,GL_FLOAT,GL_DYNAMIC_DRAW,GL_TRIANGLES,
    GL_UNSIGNED_SHORT,GL_BLEND,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA};
bool depthTest=true,depthWrite=true,cull=true;
struct Draw {int count;bool depth,write;};
std::vector<Draw> draws;
bool glIsEnabled(int state){return state==GL_DEPTH_TEST?depthTest:state==GL_CULL_FACE?cull:false;}
void glGetBooleanv(int,GLboolean* value){*value=depthWrite;}
void glEnable(int state){if(state==GL_DEPTH_TEST)depthTest=true;if(state==GL_CULL_FACE)cull=true;}
void glDisable(int state){if(state==GL_DEPTH_TEST)depthTest=false;if(state==GL_CULL_FACE)cull=false;}
void glDepthMask(bool value){depthWrite=value;}
void glDrawElements(int,int count,int,const void*){draws.push_back({count,depthTest,depthWrite});}
void EnsureMarkerProgram(){}
template<class... T> void glUseProgram(T...){}
template<class... T> void glUniformMatrix4fv(T...){}
template<class... T> void glBindBuffer(T...){}
template<class... T> void glEnableVertexAttribArray(T...){}
template<class... T> void glDisableVertexAttribArray(T...){}
template<class... T> void glVertexAttribPointer(T...){}
template<class... T> void glBufferData(T...){}
template<class... T> void glUniform4f(T...){}
template<class... T> void glBlendFunc(T...){}
int g_markerProg{},g_markerMvpLoc{},g_markerVbo{},g_markerEbo{},g_markerColLoc{};
float g_markerVerts[8*8*3]{};
unsigned short g_markerIdx[8*36]{};

// PRODUCTION_RENDERER

int main(){
    holstermarker::Frame frame{};
    frame.count=3;frame.eye[2]=1;
    frame.markers[0].kind=holstermarker::Kind::Return;
    frame.markers[1].kind=holstermarker::Kind::Grab;
    frame.markers[2].kind=holstermarker::Kind::Empty;
    float mvp[16]{};
    DrawHolsterMarkers(mvp,frame);
    assert(draws.size()==2); // return waits for the pass after weapon rendering
    assert(draws[0].count==36&&draws[0].depth&&draws[0].write);
    assert(draws[1].count==144&&draws[1].depth&&!draws[1].write);
    assert(depthTest&&depthWrite&&cull);
    draws.clear();
    DrawHolsterMarkers(mvp,frame,true);
    assert(draws.size()==1&&draws[0].count==156);
    assert(!draws[0].depth&&!draws[0].write); // occupied models cannot occlude UI
    assert(depthTest&&depthWrite&&cull);
    depthTest=depthWrite=cull=false;draws.clear();
    DrawHolsterMarkers(mvp,frame,true);
    assert(draws.size()==1&&!draws[0].depth&&!draws[0].write);
    assert(!depthTest&&!depthWrite&&!cull);
    frame.count=0;draws.clear();DrawHolsterMarkers(mvp,frame,true);
    assert(draws.empty());
}

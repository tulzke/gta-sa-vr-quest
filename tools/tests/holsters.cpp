#include "HolsterBody.h"
#include "HolsterMarker.h"
#include <cstdio>
#include <cstdlib>
#include <limits>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
using namespace savr::holsterbody;
using savr::holstermarker::Visible;

static void Close(float a,float b,float tolerance=0.0001f) { CHECK(std::fabs(a-b)<=tolerance); }
static void Sample(Body& b,float yaw,double time,bool inner=false,bool outer=false) {
    b.Update({1,1.6f,2},yaw,true,true,true,1,0,inner,outer,time);
}
static float Follow(int rate,float target) {
    Body b; Sample(b,0,0);
    for(int i=1;i<=rate*3;++i) Sample(b,target,double(i)/rate);
    return b.yaw;
}
int main() {
    Body b; Sample(b,0,0);
    for(int i=1;i<=180;++i) Sample(b,Radians(15),i/90.0);
    Close(b.yaw,0); // an ordinary look does not turn the torso
    b={};Sample(b,0,0);
    for(int i=1;i<=45;++i) Sample(b,Radians(30),i/90.0);
    Close(b.yaw,0);
    for(int i=46;i<=180;++i) Sample(b,Radians(30),i/90.0);
    CHECK(b.yaw>Radians(15) && b.yaw<Radians(21));
    // Alternating glances must not accumulate a dwell in either direction.
    b={};Sample(b,0,0);
    for(int i=1;i<=90;++i) Sample(b,Radians((i/30)%2?30.0f:-30.0f),i/90.0);
    Close(b.yaw,0);
    Close(Follow(36,Radians(90)),Follow(90,Radians(90)),Radians(1));
    Close(Follow(72,Radians(30)),Follow(90,Radians(30)),Radians(1));
    Close(Follow(10,Radians(90)),Follow(90,Radians(90)),Radians(1));
    CHECK(Follow(90,Radians(90))>Radians(79));
    // The shortest arc across the discontinuity is a small look, not a full turn.
    b={};Sample(b,Radians(179),0);Sample(b,Radians(-179),0.01);
    Close(b.yaw,Radians(179));
    b={};Sample(b,Radians(160),0);
    for(int i=1;i<=90;++i) Sample(b,Radians(-120),i/90.0);
    CHECK(Wrap(b.yaw-Radians(160))>0 && Wrap(b.yaw-Radians(160))<Radians(81));
    // Entering reach freezes yaw while position remains live.
    b={};Sample(b,0,0);
    for(int i=1;i<=90;++i) Sample(b,Radians(90),i/90.0,true,true);
    Close(b.yaw,0);CHECK(b.frozen);
    b.Update({2,1,3},Radians(90),true,true,true,1,0,false,true,1.01);
    CHECK(b.frozen);Close(b.position.x,2);Close(b.position.y,1);
    for(int i=1;i<=10;++i) Sample(b,Radians(90),1.01+i/90.0,false,false);
    CHECK(b.frozen);Close(b.yaw,0);
    for(int i=11;i<=45;++i) Sample(b,Radians(90),1.01+i/90.0,false,false);
    CHECK(!b.frozen && b.yaw>0);
    // Tracking gaps, epoch changes, recenter and mode changes initialise once.
    Sample(b,Radians(-90),5);Close(b.yaw,Radians(-90));
    b.Update({0,1,0},Radians(45),true,true,true,2,0,false,false,5.01);
    Close(b.yaw,Radians(45));CHECK(b.epoch==2);
    b.Update({0,1,0},Radians(80),true,true,true,2,1,false,false,5.02);
    Close(b.yaw,Radians(80));
    b.Update({0,1,0},Radians(10),true,true,false,2,1,false,false,5.03);
    Close(b.yaw,Radians(10));CHECK(!b.bodyMode);
    b.Update({},0,false,true,true,2,1,false,false,5.04);CHECK(!b.valid);
    b.Update({},0,true,false,true,2,1,false,false,5.05);CHECK(!b.valid);
    b.Update({},Radians(30),true,true,true,2,1,false,false,5.06);CHECK(b.valid);
    b.Update({},Radians(90),true,false,true,2,1,false,false,5.07);
    Close(b.yaw,Radians(30)); // nearly vertical gaze retains its last reliable yaw
    b.Update({std::numeric_limits<float>::quiet_NaN(),0,0},0,true,true,true,2,1,false,false,5.08);
    CHECK(!b.valid);
    const auto p=Socket({1,1.6f,2},Radians(90),0.27f,-0.58f,0.07f);
    Close(p.x,0.93f);Close(p.y,1.02f);Close(p.z,1.73f);
    // Visibility table: owned socket, held return, dropped, hidden, off, preview.
    CHECK(Visible(true,false,true,true,false,false,true));
    CHECK(Visible(true,false,true,true,true,false,true));
    CHECK(!Visible(true,false,true,true,false,true,true));
    CHECK(!Visible(true,false,false,true,true,false,true));
    CHECK(!Visible(false,false,true,true,true,false,true));
    CHECK(!Visible(true,false,true,false,false,false,true));
    CHECK(!Visible(true,false,true,true,false,false,false));
    CHECK(Visible(true,false,true,true,true,false,false)); // held sidearm can return
    CHECK(Visible(true,true,false,false,false,false,false));
    CHECK(!Visible(false,true,true,true,true,false,true));
    savr::holstermarker::Frame batch;
    batch.count=8;batch.epoch=7;batch.markers[0].kind=savr::holstermarker::Kind::Return;
    CHECK(batch.Usable(true,7));CHECK(!batch.Usable(false,7));CHECK(!batch.Usable(true,8));
    const auto presented=batch;
    batch.count=0;batch.markers[0].kind=savr::holstermarker::Kind::Grab;
    CHECK(presented.count==8&&presented.markers[0].kind==savr::holstermarker::Kind::Return);
    // Return geometry stays finite, bounded, and nonempty at overhead/near views.
    const float origin[3]={1,2,3};
    const float eyes[][3]={{1,3,3},{1,2,3},{1,2,5},{-2,0,-3}};
    for(const auto& eye:eyes) {
        const auto mesh=savr::holstermarker::ReturnMesh(origin,eye);
        CHECK(mesh.vertexCount==104 && mesh.indexCount==156);
        for(int i=0;i<mesh.indexCount;++i) CHECK(mesh.indices[i]<mesh.vertexCount);
        for(int i=0;i<mesh.vertexCount;++i) {
            float radius=0;
            for(int axis=0;axis<3;++axis) {
                const float value=mesh.vertices[i*3+axis];CHECK(std::isfinite(value));
                radius+=(value-origin[axis])*(value-origin[axis]);
            }
            CHECK(radius<=0.0351f*0.0351f);
        }
    }
    std::puts("PASS: body dwell, hysteresis, reach freeze, translation, rates, wrap, recovery, modes; marker policy and geometry");
}

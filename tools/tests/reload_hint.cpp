#include "ReloadHintModel.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace savr;
int checks{};
void Check(bool value,const char* message) {++checks;if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
template<class T> void Put(void* p,int offset,T value) {std::memcpy(static_cast<unsigned char*>(p)+offset,&value,sizeof(value));}
void* atoms[2]{};
void* Iterate(void* clump,reloadhint::AtomicCallback cb,void* data) {for(auto* a:atoms)if(a)cb(a,data);return clump;}
int main() {
    using namespace reloadhint;using namespace arcadereload;
    Tracker tracker;
    tracker.Step(0,2,22,DOWN_FLICK,true,100,1);
    tracker.Step(0,2,22,DOWN_FLICK,true,3099,1);Check(!tracker.requests[0].visible,"no hint before three seconds");
    tracker.Step(0,2,22,DOWN_FLICK,true,3100,1);Check(tracker.requests[0].visible && tracker.requests[0].phaseMs==0,"hint appears exactly at three seconds");
    tracker.Step(0,2,22,DOWN_FLICK,true,4800,1);Check(tracker.requests[0].phaseMs==100,"animation loops while empty");
    tracker.Step(1,4,29,TWIST,true,4800,1);Check(!tracker.requests[1].visible,"second hand has its own waiting timer");
    tracker.Step(1,4,29,TWIST,true,7800,1);Check(tracker.requests[1].visible && tracker.requests[1].type==29 && tracker.requests[1].mode==TWIST,"second hint identifies its own model and gesture");
    tracker.Step(0,2,22,DOWN_FLICK,false,7800,1);Check(!tracker.requests[0].visible && tracker.requests[1].visible,"reload hides only the relevant hand");
    tracker.Step(1,5,30,TWIST,true,7801,1);Check(!tracker.requests[1].visible,"switching weapon resets timer");
    tracker.Step(1,5,30,TWIST,true,10801,2);Check(!tracker.requests[1].visible,"tracking epoch resets timer");
    tracker.Reset();Check(!tracker.requests[0].visible && !tracker.requests[1].visible,"menu/reset clears both hints");
    tracker.Step(0,2,22,DOWN_FLICK,true,0xfffffff0u,3);tracker.Step(0,2,22,DOWN_FLICK,true,2984,3);
    Check(tracker.requests[0].visible,"waiting timer supports native millisecond wrap");
    tracker.Step(0,2,22,AUTOMATIC,true,6000,3);Check(!tracker.requests[0].visible,"automatic weapons have no gesture tutorial");
    Check(kOpacity==.20f,"requested opacity is twenty percent");

    for(int mode:{DOWN_FLICK,TWIST,DOWN_UP})for(int fps:{36,72,90,120}) {
        Gesture gesture;int recognized=0;
        const Pose base=BuildPose({}, {},1,false,mode,0,.5f);
        for(int i=0;i<fps*2;++i) {
            const double time=double(i)/fps;const unsigned ms=static_cast<unsigned>(time*1000);
            const Pose pose=BuildPose({}, {},1,false,mode,ms,.5f);
            const Motion motion=Animation(mode,ms);
            const Vec axis=mode==TWIST?base.barrel:base.side;
            const float angle=mode==TWIST?motion.roll:-motion.down;
            const float sine=std::sin(angle*.5f);
            Sample sample{pose.position,pose.barrel,{axis.x*sine,axis.y*sine,axis.z*sine,std::cos(angle*.5f)}};
            recognized+=gesture.Update(sample,time,1,mode,1);
            Check(std::abs(Dot(pose.barrel,pose.top))<.001f && std::abs(Dot(pose.barrel,pose.barrel)-1)<.001f,"animated weapon has a stable orthonormal basis");
        }
        Check(recognized>=1,"tutorial motion is recognized by the actual assigned gesture detector");
    }
    for(int cm:{7,21,40,60})for(int degrees:{20,60}) {
        SetNodDistanceCm(cm);SetNodAngleDegrees(degrees);Gesture gesture;int recognized=0;
        const Pose base=BuildPose({}, {},1,false,DOWN_FLICK,0,.5f);
        for(int i=0;i<144;++i) {
            const double time=i/72.;const unsigned ms=static_cast<unsigned>(time*1000);
            const auto pose=BuildPose({}, {},1,false,DOWN_FLICK,ms,.5f);
            const auto motion=Animation(DOWN_FLICK,ms);const float sine=std::sin(-motion.down*.5f);
            Sample sample{pose.position,pose.barrel,{base.side.x*sine,base.side.y*sine,base.side.z*sine,std::cos(motion.down*.5f)}};
            recognized+=gesture.Update(sample,time,1,DOWN_FLICK,1);
        }
        Check(recognized>=1,"tutorial adapts to temporary distance and angle tuning");
    }
    SetNodDistanceCm(kDefaultNodCm);SetNodAngleDegrees(kDefaultNodDegrees);
    const auto left=BuildPose({0,1.7f,0},{},0,true,DOWN_FLICK,0,1.f);
    const auto right=BuildPose({0,1.7f,0},{},1,true,TWIST,0,1.f);
    Check(left.position.x<0 && right.position.x>0 && left.position.z<0,"two copies sit separately in front of the player");
    Check(left.scale==.38f,"long weapons are fitted to the tutorial space");

    alignas(16) unsigned char clump[64]{},root[256]{},child[256]{},a[128]{},b[128]{},geometry[192]{},morph[64]{};
    float vertices[]{0,0,0, .4f,0,0, 0,.2f,0};unsigned short triangles[]{0,1,2,0};
    Put<void*>(clump,8,root);Put<void*>(a,8,root);Put<void*>(b,8,child);Put<void*>(child,8,root);
    Put<void*>(a,0x30,geometry);Put<void*>(b,0x30,geometry);a[2]=b[2]=4;
    Put<int>(geometry,0x1c,3);Put<int>(geometry,0x18,1);Put<void*>(geometry,0x38,triangles);Put<void*>(geometry,0xa0,morph);
    Put<void*>(morph,0,geometry);Put<void*>(morph,0x18,vertices);
    std::array<float,16> local{0,1,0,0, -1,0,0,0, 0,0,1,0, 1,0,0,0};std::memcpy(child+0x20,local.data(),64);
    std::array<unsigned char,sizeof(child)> saved;std::memcpy(saved.data(),child,sizeof(child));
    atoms[0]=a;atoms[1]=b;auto mesh=CopyModel(clump,Iterate);
    Check(mesh && mesh->vertices.size()==18 && mesh->indices.size()==6,"all visible model atomics are copied");
    Check(mesh->vertices[9]==1.f && std::abs(mesh->vertices[13]-.4f)<.0001f,"child frame transforms are retained in object space");
    Check(mesh->indices[3]==3 && mesh->indices[5]==5,"indices are rebased for additional atomics");
    Check(std::memcmp(saved.data(),child,sizeof(child))==0,"copying never changes source frame state");
    VisualFrame published;published.items[0]={mesh,left};auto baked=published;published={};mesh.reset();
    Check(baked.items[0].model && baked.items[0].model->vertices.size()==18,"stereo snapshot owns immutable model memory");
    const auto matrix=ModelMatrix(*baked.items[0].model,left);
    const Vec centered=ModelReader::Transform(matrix.data(),baked.items[0].model->center);
    Check(std::abs(centered.x-left.position.x)<.001f && std::abs(centered.z-left.position.z)<.001f,"model bounds are centered on tutorial anchor");
    b[2]=0;mesh=CopyModel(clump,Iterate);Check(mesh && mesh->vertices.size()==9,"hidden atomics are not shown");
    vertices[0]=std::nanf("");Check(!CopyModel(clump,Iterate),"invalid vertices are rejected");vertices[0]=0;
    Put<int>(geometry,0x1c,60001);Check(!CopyModel(clump,Iterate),"oversized geometry is rejected before reading");Put<int>(geometry,0x1c,3);
    b[2]=4;Put<void*>(child,8,child);Check(!CopyModel(clump,Iterate),"invalid frame chain cannot loop indefinitely");
    Check(!CopyModel(nullptr,Iterate) && !CopyModel(clump,nullptr),"unavailable streamed model is skipped safely");
    std::printf("PASS: %d hint timing, geometry and animation checks\n",checks);
}

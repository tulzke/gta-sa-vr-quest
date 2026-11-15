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
    tracker.Step(0,3,25,PUMP,true,10000,4);tracker.Step(0,3,25,PUMP,true,13000,4);
    Check(tracker.requests[0].visible && tracker.requests[0].mode==PUMP,"empty shotgun requests its own support-hand tutorial");

    const Pose anchor{{0,1.2f,-.4f},{0,0,-1},{0,1,0},{1,0,0}};
    for(unsigned ms:{0u,340u,500u,800u,1300u}) {
        const auto pump=BuildPose(anchor,anchor.position,1,PUMP,ms);
        Check(pump.position.y==anchor.position.y && pump.barrel.z==anchor.barrel.z &&
              Animation(PUMP,ms).down==0,"shotgun hint leaves the weapon fixed while the separate hand pumps");
    }
    for(int mode:{DOWN_FLICK,TWIST,DOWN_UP})for(int fps:{36,72,90,120}) {
        Gesture gesture;int recognized=0;
        const Pose base=BuildPose(anchor,anchor.position,1,mode,0);
        for(int i=0;i<fps*2;++i) {
            const double time=double(i)/fps;const unsigned ms=static_cast<unsigned>(time*1000);
            const Pose pose=BuildPose(anchor,anchor.position,1,mode,ms);
            const Motion motion=Animation(mode,ms);
            if (motion.visibility==0) {gesture.Reset();continue;}
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
        const Pose base=BuildPose(anchor,anchor.position,1,DOWN_FLICK,0);
        for(int i=0;i<144;++i) {
            const double time=i/72.;const unsigned ms=static_cast<unsigned>(time*1000);
            const auto pose=BuildPose(anchor,anchor.position,1,DOWN_FLICK,ms);
            const auto motion=Animation(DOWN_FLICK,ms);const float sine=std::sin(-motion.down*.5f);
            Sample sample{pose.position,pose.barrel,{base.side.x*sine,base.side.y*sine,base.side.z*sine,std::cos(motion.down*.5f)}};
            recognized+=gesture.Update(sample,time,1,DOWN_FLICK,1);
        }
        Check(recognized>=1,"tutorial adapts to temporary distance and angle tuning");
    }
    SetNodDistanceCm(kDefaultNodCm);SetNodAngleDegrees(kDefaultNodDegrees);
    const Pose leftWeapon{{-.25f,1.1f,-.4f},{1,0,0},{0,1,0},{0,0,-1}};
    const Pose rightWeapon{{.20f,1.3f,-.5f},{0,0,-1},{0,1,0},{-1,0,0}};
    const Vec leftWrist{-.20f,1.15f,-.3f},rightWrist{.17f,1.25f,-.35f};
    const auto left=BuildPose(leftWeapon,leftWrist,0,DOWN_FLICK,0);
    const auto right=BuildPose(rightWeapon,rightWrist,1,TWIST,0);
    const Model emptyModel{};
    Check(ModelMatrix(left,emptyModel)==ModelMatrix(leftWeapon,emptyModel) && ModelMatrix(right,emptyModel)==ModelMatrix(rightWeapon,emptyModel),"hint starts at the live weapon pose before shell expansion");
    const auto rightMatrix=ModelMatrix(right,emptyModel);
    Check(std::abs(std::sqrt(rightMatrix[0]*rightMatrix[0]+rightMatrix[1]*rightMatrix[1]+rightMatrix[2]*rightMatrix[2])-1.05f)<.0001f,"hint shell is five percent larger than the live weapon");
    Check(Dot(right.barrel,Cross(right.top,right.side))<0,"live reflected handedness is retained");
    const auto rolled=BuildPose(rightWeapon,rightWrist,1,TWIST,800);
    const Vec before=Add(rightWeapon.position,Scale(rightWrist,-1)),after=Add(rolled.position,Scale(rightWrist,-1));
    Check(std::abs(Dot(before,before)-Dot(after,after))<.001f,"calibrated model offset rotates around the real wrist pivot");
    const Vec axis{0,1,0},delta{.4f,.2f,-.3f};const float turn=.7f;
    Pose moved=rightWeapon;moved.position=Add(Rotated(moved.position,axis,turn),delta);
    moved.barrel=Rotated(moved.barrel,axis,turn);moved.top=Rotated(moved.top,axis,turn);moved.side=Rotated(moved.side,axis,turn);
    const auto movedRoll=BuildPose(moved,Add(Rotated(rightWrist,axis,turn),delta),1,TWIST,800);
    const Vec expected=Add(Rotated(rolled.position,axis,turn),delta);
    const Vec difference=Add(movedRoll.position,Scale(expected,-1));
    Check(Dot(difference,difference)<.00001f && Dot(movedRoll.top,Rotated(rolled.top,axis,turn))>.999f,"moving and turning the hand carries the active animation with it");
    Check(Animation(TWIST,1000).roll==Animation(TWIST,600).roll && Animation(TWIST,1400).visibility==0,"side tutorial holds its outward pose and hides the reset instead of returning");

    Check(RifleLeverHint(33,DOWN_FLICK) && RifleLeverHint(34,DOWN_UP) && !RifleLeverHint(33,TWIST) &&
          !RifleLeverHint(30,TWIST) && !RifleLeverHint(22,DOWN_FLICK),"foregrip animation is limited to rifle downward gestures");
    for(int hand:{0,1})for(float pitch:{-.6f,0.f,.6f}) {
        Pose rifle=anchor;rifle.side=Scale(rifle.side,hand==0?-1.f:1.f);
        rifle=Pivoted(rifle,rifle.position,{1,0,0},pitch);
        const Vec support=Add(rifle.position,Scale(rifle.barrel,.42f));
        for(unsigned ms:{0u,340u,500u,800u,1300u,1600u}) {
            const auto lever=BuildRiflePose(rifle,rifle.position,support,DOWN_FLICK,ms);
            const Vec fixed=Add(lever.position,Scale(lever.barrel,.42f));
            const Vec error=Add(fixed,Scale(support,-1));
            Check(Dot(error,error)<.000001f,"rifle foregrip stays fixed throughout the animation for either hand and pitched weapons");
            Check(lever.position.y<=rifle.position.y+.0001f,"primary grip moves down rather than lifting or translating the whole rifle");
            Check(std::abs(Dot(lever.barrel,Cross(lever.top,lever.side))-Dot(rifle.barrel,Cross(rifle.top,rifle.side)))<.0001f,"lever motion preserves live reflected handedness");
        }
        const auto lever=BuildRiflePose(rifle,rifle.position,support,DOWN_FLICK,500);
        Check(lever.position.y<rifle.position.y-.15f,"rifle tutorial has a clear rear-grip downward stroke");
        const Vec shift{.3f,.1f,-.4f};Pose shifted=rifle;shifted.position=Add(shifted.position,shift);
        const auto carried=BuildRiflePose(shifted,shifted.position,Add(support,shift),DOWN_FLICK,500);
        const Vec error=Add(carried.position,Scale(Add(lever.position,shift),-1));
        Check(Dot(error,error)<.000001f,"moving the support anchor carries the lever animation with the hand");
    }

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
    const auto& copied=*baked.items[0].model;
    const auto matrix=ModelMatrix(left,copied);
    const Vec center=ModelReader::Transform(matrix.data(),copied.center);
    const Vec expectedCenter=Add(left.position,Add(Scale(left.barrel,copied.center.x),Add(Scale(left.top,copied.center.y),Scale(left.side,copied.center.z))));
    const Vec centerError=Add(center,Scale(expectedCenter,-1));
    Check(Dot(centerError,centerError)<.000001f,"expansion keeps an offset model center fixed instead of shifting the weapon");
    b[2]=0;mesh=CopyModel(clump,Iterate);Check(mesh && mesh->vertices.size()==9,"hidden atomics are not shown");
    vertices[0]=std::nanf("");Check(!CopyModel(clump,Iterate),"invalid vertices are rejected");vertices[0]=0;
    Put<int>(geometry,0x1c,60001);Check(!CopyModel(clump,Iterate),"oversized geometry is rejected before reading");Put<int>(geometry,0x1c,3);
    b[2]=4;Put<void*>(child,8,child);Check(!CopyModel(clump,Iterate),"invalid frame chain cannot loop indefinitely");
    Check(!CopyModel(nullptr,Iterate) && !CopyModel(clump,nullptr),"unavailable streamed model is skipped safely");
    std::printf("PASS: %d hint timing, geometry and animation checks\n",checks);
}

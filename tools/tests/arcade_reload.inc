void Pose(int hand,float y,float roll=0,float pitch=0) {
    auto& p=xr::tracked[hand];p={};p.gripPos[1]=y;
    const float angle=roll ? roll : pitch;
    p.gripOri[0]=roll?0:std::sin(angle*.5f);
    p.gripOri[2]=roll?-std::sin(angle*.5f):0;
    p.gripOri[3]=std::cos(angle*.5f);
}
int GestureRun(int mode,int fps,int path,float amplitude,float outDuration=.15f,
               float backDuration=.15f,bool returnMotion=true) {
    arcadereload::Gesture gesture;int count=0;
    const auto step=[&](float amount,double now) {
        arcadereload::Sample s;
        if(path==0)s.position.y=-amount;
        if(path==3)s.position.y=amount*.05f;
        const float angle=path ? amount : 0;
        s.orientation={path==1 || path==3?std::sin(angle*.5f):0,0,path==2?-std::sin(angle*.5f):0,std::cos(angle*.5f)};
        s.barrel=arcadereload::Rotate(s.orientation,{0,0,-1});
        count+=gesture.Update(s,now,150,mode,1);
    };
    for(int i=0;i<fps*2;++i) {
        const double t=double(i)/fps;
        float amount=0;
        if(t>=.3 && t<.3+outDuration) amount=amplitude*float((t-.3)/outDuration);
        else if(t>=.3+outDuration && (!returnMotion || t<.3+outDuration+backDuration))
            amount=returnMotion?amplitude*(1-float((t-.3-outDuration)/backDuration)):amplitude;
        step(amount,t);
    }
    return count;
}
arcadereload::Quat Compose(arcadereload::Quat a,arcadereload::Quat b) {
    return arcadereload::Delta(a,{-b.x,-b.y,-b.z,b.w});
}
int FlickRun(int fps,int kind,float duration=.18f,float startRoll=0,float amplitude=.65f,float travel=1.f) {
    using namespace arcadereload;
    Gesture gesture;int count=0;
    for(int i=0;i<fps*2;++i) {
        const double t=double(i)/fps;
        const float p=std::clamp(float((t-.02)/duration),0.f,1.f);
        Sample s;
        float pitch=(kind==2 || kind==3 || kind==5)?0:-amplitude*p;
        if(kind==4)pitch=-pitch;
        const float yaw=1.1f+(kind==1 || kind==2 ? 1.2f*p : 0);
        const float roll=startRoll+(kind==1 || kind==3 ? 1.4f*p : 0);
        s.orientation=Compose(Compose({0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)},
                                     {std::sin(pitch*.5f),0,0,std::cos(pitch*.5f)}),
                                     {0,0,std::sin(roll*.5f),std::cos(roll*.5f)});
        s.barrel=Rotate(s.orientation,{0,0,-1});
        if(kind==1 || kind==5) {
            s.position={travel*.18f*std::sin(p*1.2f),-travel*.16f*std::sin(p*1.1f),travel*.20f*(1-std::cos(p*1.2f))};
        }
        if(kind==2)s.position={.20f*std::sin(p),0,.20f*(1-std::cos(p))};
        count+=gesture.Update(s,t,kind+1,DOWN_FLICK,1);
    }
    return count;
}
void ArcadeTests() {
    using namespace arcadereload;
    for(int fps:{36,72,90,120}) {
        Check(GestureRun(TWIST,fps,2,1.f)==1,"right twist and return recognized once");
        Check(GestureRun(TWIST,fps,2,-1.f)==1,"left twist and return recognized once");
        Check(GestureRun(TWIST,fps,2,1.f,.15f,.15f,false)==1,"outward roll reloads without any return");
        Check(GestureRun(TWIST,fps,2,1.f,1.f,.5f)==0,"slow inspection roll cannot reload");
        Check(GestureRun(TWIST,fps,2,.2f)==0,"small aim roll cannot reload");
        Check(GestureRun(TWIST,fps,1,-.7f)==0,"pitch cannot masquerade as barrel twist");
        Check(GestureRun(AUTOMATIC,fps,0,.12f)==0,"automatic mode ignores gestures");
        Check(FlickRun(fps,0)==0,"old small wrist nod no longer bypasses the tripled amplitude");
        Check(FlickRun(fps,0,.18f,0,1.15f)==1,"larger downward wrist nod reloads without return or braking");
        Check(FlickRun(fps,1,.18f,0,1.15f)==1,"downward arc with simultaneous yaw and roll reloads once");
        Check(FlickRun(fps,1,.18f,1.1f,1.15f)==1,"rolled starting grip and curved motion remain valid");
        Check(FlickRun(fps,5)==0,"old short curved arm stroke is below the new distance");
        Check(FlickRun(fps,5,.18f,0,.65f,4.5f)==1,"longer curved arm stroke still works without wrist rotation");
        Check(GestureRun(DOWN_FLICK,fps,0,.49f)==0,"stroke below the approved fifty centimetres cannot reload");
        Check(GestureRun(DOWN_FLICK,fps,0,.55f)==1,"stroke above the approved fifty centimetres reloads");
        Check(FlickRun(fps,0,1.2f)==0,"slow wrist lowering cannot trigger downward flick");
        Check(FlickRun(fps,1,1.2f)==0,"slow curved holstering with rotations cannot trigger flick");
        Check(FlickRun(fps,2)==0,"sideways arc and yaw alone do not count as downward flick");
        Check(FlickRun(fps,3)==0,"barrel roll alone does not count as downward flick");
        Check(FlickRun(fps,4)==0,"upward nod does not count as downward flick");
        Check(GestureRun(DOWN_FLICK,fps,0,.012f)==0,"tracking jitter cannot trigger downward flick");
    }
    Gesture jump;Sample jumping;
    jump.Update(jumping,0,1,DOWN_FLICK,1);jump.Update(jumping,.06,1,DOWN_FLICK,1);
    jumping.position.y=-1;
    Check(!jump.Update(jumping,.07,1,DOWN_FLICK,1),"position discontinuity is rejected as a downward flick");
    Check(!jump.Update(jumping,.13,1,DOWN_FLICK,1),"no stale history survives the tracking jump");
    Gesture interrupted;
    Sample s;
    interrupted.Update(s,1.,10,DOWN_FLICK,1);interrupted.Update(s,1.1,10,DOWN_FLICK,1);
    interrupted.Update(s,1.2,10,DOWN_FLICK,1);s.position.y=-.3f;
    interrupted.Update(s,1.3,10,DOWN_FLICK,1);s.position.y=0;
    Check(!interrupted.Update(s,1.4,10,DOWN_FLICK,2),"recenter cancels half-completed gesture");
    s.position.y=-.3f;interrupted.Update(s,1.6,10,DOWN_FLICK,2);s.position.y=0;
    Check(!interrupted.Update(s,1.7,11,DOWN_FLICK,2),"hand transfer cancels half-completed gesture");
    s.orientation.w=std::nanf("");
    Check(!interrupted.Update(s,1.8,11,DOWN_FLICK,2) && !interrupted.valid,"invalid tracking clears history");
    Check(FlickRun(90,0,.18f,0,.84f)==0 && FlickRun(90,0,.18f,0,.90f)==1,"fixed fifty-degree threshold rejects smaller rotations and accepts larger ones");

}

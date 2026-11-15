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
int FlickRun(int fps,int kind,float duration=.18f,float startRoll=0) {
    using namespace arcadereload;
    Gesture gesture;int count=0;
    for(int i=0;i<fps*2;++i) {
        const double t=double(i)/fps;
        const float p=std::clamp(float((t-.02)/duration),0.f,1.f);
        Sample s;
        float pitch=(kind==2 || kind==3 || kind==5)?0:-.65f*p;
        if(kind==4)pitch=-pitch;
        const float yaw=1.1f+(kind==1 || kind==2 ? 1.2f*p : 0);
        const float roll=startRoll+(kind==1 || kind==3 ? 1.4f*p : 0);
        s.orientation=Compose(Compose({0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)},
                                     {std::sin(pitch*.5f),0,0,std::cos(pitch*.5f)}),
                                     {0,0,std::sin(roll*.5f),std::cos(roll*.5f)});
        s.barrel=Rotate(s.orientation,{0,0,-1});
        if(kind==1 || kind==5) {
            s.position={.18f*std::sin(p*1.2f),-.16f*std::sin(p*1.1f),.20f*(1-std::cos(p*1.2f))};
        }
        if(kind==2)s.position={.20f*std::sin(p),0,.20f*(1-std::cos(p))};
        count+=gesture.Update(s,t,kind+1,DOWN_FLICK,1);
    }
    return count;
}
void ArcadeTests() {
    using namespace arcadereload;
    for(int fps:{36,72,90,120}) {
        Check(GestureRun(DOWN_UP,fps,0,.12f)==1,"down/up translation recognized once at each frame rate");
        Check(GestureRun(DOWN_UP,fps,1,-.7f)==1,"wrist dip and return recognized without translating arm");
        Check(GestureRun(DOWN_UP,fps,3,-.7f)==1,"small incidental hand translation does not hide a wrist dip");
        Check(GestureRun(TWIST,fps,2,1.f)==1,"right twist and return recognized once");
        Check(GestureRun(TWIST,fps,2,-1.f)==1,"left twist and return recognized once");
        Check(GestureRun(DOWN_UP,fps,0,.12f,.15f,.15f,false)==0,"lowering without return cannot reload");
        Check(GestureRun(TWIST,fps,2,1.f,.15f,.15f,false)==0,"rolling without return cannot reload");
        Check(GestureRun(DOWN_UP,fps,0,.12f,1.f,.5f)==0,"slow holster movement cannot reload");
        Check(GestureRun(TWIST,fps,2,1.f,1.f,.5f)==0,"slow inspection roll cannot reload");
        Check(GestureRun(DOWN_UP,fps,0,.02f)==0,"small tracking movement cannot reload");
        Check(GestureRun(TWIST,fps,2,.2f)==0,"small aim roll cannot reload");
        Check(GestureRun(TWIST,fps,1,-.7f)==0,"pitch cannot masquerade as barrel twist");
        Check(GestureRun(AUTOMATIC,fps,0,.12f)==0,"automatic mode ignores gestures");
        Check(FlickRun(fps,0)==1,"downward wrist nod reloads without a return or long arming delay");
        Check(FlickRun(fps,1)==1,"downward arc with simultaneous yaw and roll reloads once");
        Check(FlickRun(fps,1,.18f,1.1f)==1,"rolled starting grip and curved motion remain valid");
        Check(FlickRun(fps,5)==1,"curved downward arm movement works without wrist rotation");
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
    interrupted.Update(s,1.,10,DOWN_UP,1);interrupted.Update(s,1.1,10,DOWN_UP,1);
    interrupted.Update(s,1.2,10,DOWN_UP,1);s.position.y=-.12f;
    interrupted.Update(s,1.3,10,DOWN_UP,1);s.position.y=0;
    Check(!interrupted.Update(s,1.4,10,DOWN_UP,2),"recenter cancels half-completed gesture");
    s.position.y=-.12f;interrupted.Update(s,1.6,10,DOWN_UP,2);s.position.y=0;
    Check(!interrupted.Update(s,1.7,11,DOWN_UP,2),"hand transfer cancels half-completed gesture");
    s.orientation.w=std::nanf("");
    Check(!interrupted.Update(s,1.8,11,DOWN_UP,2) && !interrupted.valid,"invalid tracking clears history");

    // Actual Update/UpdateHand/OnWeaponUpdate, with raw tracking and retail
    // CWeapon calls mocked by the dual-fire harness.
    ResetDirectTrigger();pairedweapon::Reset();SetMode(DOWN_UP);
    Setup(0,2,22,1,40,34,2000,0x800);Setup(1,4,28,20,90,100,1300,0x800);
    triggers[0]=triggers[1]=1;Pose(0,0);Pose(1,0);tick=10000;Update(false);
    triggers[0]=triggers[1]=0;tick=14000;Update(false);
    Check(Field(2,8)==0 && Field(2,4)==2,"empty gun waits past native automatic reload deadline");
    const auto frame=[] {tick+=10;Update(false);};
    for(int i=0;i<25;++i)frame();
    for(int i=1;i<=12;++i){Pose(0,-i*.01f);frame();}
    Check(Field(2,8)==0 && reloadSounds[0]==0,"downward half alone does not reload or play a sound");
    for(int i=11;i>=0;--i){Pose(0,-i*.01f);frame();}
    Check(Field(2,8)==17 && Field(2,4)==0,"return instantly fills magazine and makes weapon ready");
    Check(Field(2,16)<=tick,"gesture leaves no reload deadline to wait for");
    Check(reloadSounds[0]==1 && lastReloadEvent==147,"completed gesture plays bolt confirmation once");
    Check(Field(2,12)==39,"instant reload preserves total ammunition");
    int left=shots[0],right=shots[1];triggers[0]=triggers[1]=1;tick+=10;Update(false);
    Check(shots[0]==left+1 && shots[1]==right+1,"both guns can shoot immediately after gesture");
    triggers[0]=triggers[1]=0;frame();
    const int pulses=xr::haptics[0];tick+=10;Update(false);
    Check(xr::haptics[0]==pulses && reloadSounds[0]==1,"no delayed second pulse or sound");
    const unsigned total=Field(2,12);Field(2,8)=5;Field(2,4)=0;
    SetMode(TWIST);SetSound(MAGAZINE);for(int i=0;i<25;++i)frame();
    for(int i=1;i<=10;++i){Pose(0,0,i*.1f);frame();}
    Check(Field(2,8)==5 && reloadSounds[0]==1,"twist alone cannot reload or play sound");
    for(int i=9;i>=0;--i){Pose(0,0,i*.1f);frame();}
    Check(Field(2,8)==17 && Field(2,4)==0,"twist-return instantly reloads partial magazine");
    Check(reloadSounds[0]==2 && lastReloadEvent==146,"magazine click selection uses its own event");
    Check(Field(2,8)==17 && Field(2,12)==total,"partial reload never discards or duplicates ammo");

    // The mirror's update also runs from PhysicalWeapon while holstered.
    ResetDirectTrigger();pairedweapon::Reset();SetMode(DOWN_UP);
    Setup(0,2,22,5,20,34,2000,0x800);held[1]=15;
    unsigned capacities[13]{};capacities[2]=17;
    pairedweapon::Refresh(player,1u<<2,capacities,tick);
    auto* mirror=pairedweapon::copies[2].bytes;
    pairedweapon::Field(mirror,8)=0;pairedweapon::Field(mirror,4)=2;pairedweapon::Field(mirror,16)=tick-1;
    pairedweapon::Refresh(player,1u<<2,capacities,tick+5000);
    Check(pairedweapon::Clip(player,15)==0,"holstered empty mirror also waits for gesture");
    Pose(0,0);Pose(1,0);for(int i=0;i<25;++i)frame();
    for(int i=1;i<=12;++i){Pose(1,-i*.01f);frame();}
    for(int i=11;i>=0;--i){Pose(1,-i*.01f);frame();}
    Check(pairedweapon::Clip(player,15)==15 && ::Field(2,8)==5,"mirror gesture instantly fills only its magazine");
    Check(pairedweapon::Clip(player,15)==15 && ::Field(2,8)==5 && ::Field(2,12)==20,
          "mirror refill excludes other magazine without creating shared ammo");
    SetMode(AUTOMATIC);pairedweapon::Field(mirror,8)=0;pairedweapon::Field(mirror,4)=2;
    pairedweapon::Refresh(player,1u<<2,capacities,tick+4000);
    Check(pairedweapon::Clip(player,15)==15,"switching back restores automatic mirror reload");

    // Engine focus and trigger gates must cancel a gesture before its return.
    pairedweapon::Refresh(player,0,capacities,tick);
    ResetDirectTrigger();SetMode(DOWN_UP);held[1]=-1;::Field(2,8)=1;::Field(2,4)=0;
    Pose(0,0);for(int i=0;i<25;++i)frame();
    for(int i=1;i<=12;++i){Pose(0,-i*.01f);frame();}
    xr::focused=false;frame();xr::focused=true;
    for(int i=11;i>=0;--i){Pose(0,-i*.01f);frame();}
    Check(::Field(2,8)==1,"focus loss prevents completing an old gesture");
    for(int i=0;i<25;++i)frame();
    for(int i=1;i<=12;++i){Pose(0,-i*.01f);frame();}
    triggers[0]=.3f;frame();triggers[0]=0;
    for(int i=11;i>=0;--i){Pose(0,-i*.01f);frame();}
    Check(::Field(2,8)==1,"held trigger prevents completing an old gesture");
    SetSound(SILENT);for(int i=0;i<25;++i)frame();
    const int mutedSounds=reloadSounds[0],mutedPulses=xr::haptics[0];
    for(int i=1;i<=12;++i){Pose(0,-i*.01f);frame();}
    for(int i=11;i>=0;--i){Pose(0,-i*.01f);frame();}
    Check(::Field(2,8)==17 && ::Field(2,4)==0,"silent mode still instantly reloads");
    Check(reloadSounds[0]==mutedSounds && xr::haptics[0]==mutedPulses+1,"silent mode keeps haptics and suppresses sound");
    Check(xr::hapticAmplitude[0]>=.6f && xr::hapticDuration[0]>=45 && xr::hapticDuration[0]<=65,
          "reload confirmation is a distinct short pulse independent of recoil setting");
    SetMode(DOWN_FLICK);::Field(2,8)=1;::Field(2,4)=0;Pose(0,0);
    for(int i=0;i<6;++i)frame();
    const int nodPulses=xr::haptics[0];
    for(int i=1;i<=12;++i){Pose(0,0,0,-i*.05f);frame();}
    Check(::Field(2,8)==17 && ::Field(2,4)==0,"down-only nod instantly reloads in the production firing loop");
    Check(xr::haptics[0]==nodPulses+1,"down-only nod gives exactly one short haptic confirmation");
    unsigned state=2,clip=0,deadline=tick+2000;
    Check(Reload(22,state,clip,3,deadline,tick,17) && clip==3 && state==0 && deadline==tick,
          "small reserve instantly fills only available rounds");
    Check(!Reload(22,state,clip,3,deadline,tick,17),"no reserve cannot produce a false reload");
    clip=17;Check(!Reload(22,state,clip,40,deadline,tick,17),"full magazine cannot produce a false reload");
    Check(!Manual(35) && !Manual(38) && !Manual(41),"launchers, minigun and continuous tools retain native cycle");
}

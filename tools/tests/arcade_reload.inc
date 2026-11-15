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
    }
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
    Check(!guns[2].requested,"downward half alone does not start reload");
    for(int i=11;i>=0;--i){Pose(0,-i*.01f);frame();}
    Check(guns[2].requested && Field(2,8)==0,"return starts reload without instantly filling magazine");
    Check(Field(2,16)>tick && Field(2,16)<=tick+2000,"gesture uses single-gun skill reload duration");
    int right=shots[1];triggers[1]=1;tick+=100;Update(false);
    Check(shots[1]==right+1 && Field(2,8)==0,"other hand keeps firing during gesture reload");
    triggers[1]=0;tick=Field(2,16)+1;Update(false);
    Check(Field(2,8)==17 && Field(2,12)==39,"gesture reload preserves reserve and single-gun capacity");
    Check(!guns[2].requested,"completed reload releases request for next empty magazine");
    const int pulses=xr::haptics[0];tick+=10;Update(false);
    Check(xr::haptics[0]==pulses,"completion pulse is not repeated");
    const unsigned total=Field(2,12);Field(2,8)=5;Field(2,4)=0;
    SetMode(TWIST);for(int i=0;i<25;++i)frame();
    for(int i=1;i<=10;++i){Pose(0,0,i*.1f);frame();}
    Check(!guns[2].requested,"twist alone cannot start reload");
    for(int i=9;i>=0;--i){Pose(0,0,i*.1f);frame();}
    Check(guns[2].requested && Field(2,8)==5,"twist-return starts partial-magazine reload");
    tick=Field(2,16)+1;Update(false);
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
    Check(guns[15].requested && !guns[2].requested,"mirror gesture targets its own reload request");
    pairedweapon::Refresh(player,1u<<2,capacities,tick+2001);
    Check(pairedweapon::Clip(player,15)==15 && ::Field(2,8)==5 && ::Field(2,12)==20,
          "mirror refill excludes other magazine without creating shared ammo");
    SetMode(AUTOMATIC);pairedweapon::Field(mirror,8)=0;pairedweapon::Field(mirror,4)=2;
    pairedweapon::Refresh(player,1u<<2,capacities,tick+4000);
    Check(pairedweapon::Clip(player,15)==15,"switching back restores automatic mirror reload");

    // Engine focus and trigger gates must cancel a gesture before its return.
    ResetDirectTrigger();SetMode(DOWN_UP);held[1]=-1;::Field(2,8)=1;::Field(2,4)=0;
    Pose(0,0);for(int i=0;i<25;++i)frame();
    for(int i=1;i<=12;++i){Pose(0,-i*.01f);frame();}
    xr::focused=false;frame();xr::focused=true;
    for(int i=11;i>=0;--i){Pose(0,-i*.01f);frame();}
    Check(!guns[2].requested,"focus loss prevents completing an old gesture");
    for(int i=0;i<25;++i)frame();
    for(int i=1;i<=12;++i){Pose(0,-i*.01f);frame();}
    triggers[0]=.3f;frame();triggers[0]=0;
    for(int i=11;i>=0;--i){Pose(0,-i*.01f);frame();}
    Check(!guns[2].requested,"held trigger prevents completing an old gesture");
    Check(!Manual(35) && !Manual(38) && !Manual(41),"launchers, minigun and continuous tools retain native cycle");
}

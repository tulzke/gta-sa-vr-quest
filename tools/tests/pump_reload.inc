void PumpReloadTests() {
    using namespace savr::pumpreload;
    for(int fps:{36,72,90,120})for(float yaw:{0.f,.7f,2.1f})for(int primaryHand:{0,1}) {
        Pump pump;int backs=0,fronts=0;
        for(int i=0;i<fps*16/10;++i) {
            const double now=double(i)/fps;const unsigned ms=static_cast<unsigned>(now*1000);
            const float angle=yaw+static_cast<float>(now)*.5f;
            const Quat q{0,std::sin(angle*.5f),0,std::cos(angle*.5f)};
            const Vec origin{static_cast<float>(now)*.2f,1.2f,-.3f};
            const float travel=HintTravel(ms);
            const Vec off=Add(origin,Rotate(q,{primaryHand==0?-.06f:.06f,0,-.4f+travel}));
            const Sample main{origin,Rotate(q,{0,0,-1}),q};
            const auto event=pump.Update(main,off,3,1-primaryHand,1,now,true);
            backs+=event==Rear;fronts+=event==Front;
            Check(pump.travel>=0 && pump.travel<=kStroke,"pump wrist stays between both axial stops");
            if(fronts==0)Check(event!=Front || backs==1,"forward stop only completes after rear stop");
        }
        Check(backs==1 && fronts==1,"hint's complete pump stroke reaches both checkpoints once at every supported frame rate");
    }
    Pump pump;Sample main{{0,1,0},{0,0,-1},{}};
    auto step=[&](float z,bool empty,double now){return pump.Update(main,{0,1,z},3,1,1,now,empty);};
    Check(step(-.4f,false,0)==None && step(-.2f,false,.1)==None && pump.travel==0,"loaded magazine locks visible pump forward and emits no clicks");
    Check(step(-.2f,true,.11)==None && pump.travel==0,"unlocking does not replay motion performed while loaded");
    Check(step(-.08f,true,.2)==Rear && pump.travel==kStroke,"pull reaches the hard rear stop");
    Check(step(.02f,true,.21)==None && pump.travel==kStroke,"overpull is clamped without repeating rear feedback");
    Check(step(-.10f,true,.3)==Front && pump.travel==0,"return from an overpull does not require invisible extra travel");
    Check(step(-.10f,true,.31)==None,"holding the front stop cannot create another cycle");
    Check(step(-.04f,true,.4)==None && step(-.10f,true,.5)==None,"partial pull and return cannot reload");
    pump.Reset();step(-.4f,true,0);
    for(int i=1;i<=10;++i) {
        Check(pump.Update(main,{i*.02f,1,-.4f},3,1,1,i*.01,true)==None && pump.travel<.000001f,"sideways controller motion cannot advance the pump rail");
    }
    pump.Reset();step(-.4f,true,0);
    for(int i=1;i<=10;++i) {
        const float a=i*.1f;main.orientation={std::sin(a*.5f),0,0,std::cos(a*.5f)};
        main.barrel=Rotate(main.orientation,{0,0,-1});
        Check(pump.Update(main,{0,1,-.4f},3,1,1,i*.01,true)==None && pump.travel==0,"rotating the primary wrist alone cannot perform the pump gesture");
    }
    main.orientation={};main.barrel={0,0,-1};
    pump.Reset();step(-.4f,true,0);Check(step(-.28f,true,.1)==Rear,"rear checkpoint can arm a cycle");
    Check(step(-.4f,true,.5)==None && !pump.rear,"tracking gap cancels an incomplete cycle");
    step(-.28f,true,.6);
    Check(pump.Update(main,{0,1,-.4f},3,1,2,.7,true)==None && !pump.rear,"tracking epoch change cancels the return checkpoint");
    step(-.28f,true,.8);
    Check(pump.Update(main,{0,1,-.4f},3,0,1,.9,true)==None && !pump.rear,"support hand transfer starts in the forward position");
    main.position.x=std::nanf("");
    Check(pump.Update(main,{0,1,-.4f},3,1,1,1,true)==None && !pump.valid,"invalid controller data cannot reload");
}

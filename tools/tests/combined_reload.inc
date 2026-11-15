void SetPose(int hand,float y=0,float roll=0) {
    auto& p=xr::tracked[hand];p={};p.gripPos[1]=y;
    p.gripOri[2]=-std::sin(roll*.5f);p.gripOri[3]=std::cos(roll*.5f);
}
void Frame() {tick+=10;Update(false);}
void HoldStill() {for(int i=0;i<25;++i)Frame();}
void Nod(int hand) {for(int i=1;i<=12;++i){SetPose(hand,-i*.05f);Frame();}}
void Roll(int hand,bool back=false) {
    for(int i=1;i<=10;++i){SetPose(hand,0,(back?10-i:i)*.1f);Frame();}
}
int main(){
    using namespace arcadereload;
    SetType(TYPE_AUTOMATIC);
    for(int type:{22,23,24,25,26,27,28,29,30,31,32,33,34,35,38,41})
        Check(ModeForWeapon(type)==AUTOMATIC && !Manual(type),"automatic selector disables every weapon gesture");
    SetType(TYPE_ARCADE);
    for(int type:{22,23,24,26,28,32,33,34})Check(ModeForWeapon(type)==DOWN_FLICK,"arcade sidearms and rifles use the final downward nod");
    for(int type:{27,29,30,31})Check(ModeForWeapon(type)==TWIST,"arcade long guns use lateral roll");
    Check(ModeForWeapon(25)==PUMP,"arcade shotgun requires the support-hand pump cycle");
    for(int type:{38,35,41})Check(ModeForWeapon(type)==AUTOMATIC,"unsupported families retain their native cycle");
    player[0x8dc]=7;
    Setup(0,2,22,1,40,34,2000,0x800);Setup(1,4,29,1,90,30,1300);
    SetPose(0);SetPose(1);triggers[0]=triggers[1]=1;Update(false);
    Check(shots[0]==1 && shots[1]==1,"both hands fire with separate physical rays in one frame");
    Check(player[0x8dc]==7,"engine active slot restored after each hand");
    Check(Field(2,8)==0 && Field(4,8)==0,"each physical magazine is consumed independently");
    triggers[0]=triggers[1]=0;tick=5000;Update(false);HoldStill();
    Check(Field(2,8)==0 && Field(4,8)==0,"both fixed gesture families wait past native reload deadlines");
    Nod(0);
    Check(Field(2,8)==17 && Field(2,4)==0,"sidearm reloads immediately on downward motion without brake or return");
    Check(Field(4,8)==0,"sidearm nod leaves long gun empty");
    Check(reloadSounds[0]==1 && reloadSounds[1]==0 && lastReloadEvent==147,"configured bolt sound plays once on the correct hand");
    Check(xr::hapticAmplitude[0]>=.6f && xr::hapticDuration[0]>=45 && xr::hapticDuration[0]<=65,"short distinct confirmation pulse is retained");
    Nod(1);
    Check(Field(4,8)==0 && reloadSounds[1]==0,"long gun cannot be reloaded by the sidearm nod");
    SetPose(1);HoldStill();Roll(1);
    Check(Field(4,8)==30 && Field(4,4)==0,"lateral roll alone instantly reloads before returning");
    const int rollSounds=reloadSounds[1],rollPulses=xr::haptics[1];
    Roll(1,true);
    Check(Field(4,8)==30 && reloadSounds[1]==rollSounds && xr::haptics[1]==rollPulses,"return is optional and creates no duplicate confirmation");
    Check(Field(2,12)==39 && Field(4,12)==89,"reload neither discards nor duplicates total ammo");
    int a=shots[0],b=shots[1];triggers[0]=triggers[1]=1;Frame();
    Check(shots[0]==a+1 && shots[1]==b+1,"both guns shoot immediately after their own reload gestures");
    triggers[0]=triggers[1]=0;Frame();
    std::swap(held[0],held[1]);tick+=1000;Update(false);
    Check(Field(2,8)==16 && Field(4,8)==29,"transfer preserves physical magazines");
    a=shots[0];b=shots[1];triggers[0]=triggers[1]=1;Frame();
    Check(shots[0]==a+1 && shots[1]==b+1,"transferred guns use the new hands' independent rays");
    Update(true);Check(!g_directTriggerDown[0] && !g_directTriggerDown[1],"blocked gameplay resets both trigger and gesture owners");

    // Two identical pistols share reserve, but only the gesturing copy reloads.
    ResetDirectTrigger();pairedweapon::Reset();Setup(0,2,22,5,20,34,2000,0x800);held[1]=15;
    unsigned capacities[13]{};capacities[2]=17;
    pairedweapon::Refresh(player,1u<<2,capacities,tick);
    auto* mirror=pairedweapon::copies[2].bytes;
    pairedweapon::Field(mirror,8)=1;
    triggers[0]=triggers[1]=1;tick+=1000;Update(false);
    Check(Field(2,8)==4 && pairedweapon::Clip(player,15)==0 && Field(2,12)==18,"identical pistols consume exactly two rounds from shared total");
    triggers[0]=triggers[1]=0;tick+=4000;Update(false);SetPose(0);SetPose(1);HoldStill();
    pairedweapon::Refresh(player,1u<<2,capacities,tick);
    Check(pairedweapon::Clip(player,15)==0,"empty mirror cannot refill in the holster without gesture");
    Nod(1);
    Check(pairedweapon::Clip(player,15)==14 && Field(2,8)==4 && Field(2,12)==18,"mirror nod loads available reserve excluding the other magazine");
    Check(pairedweapon::Available(player,15)==14 && pairedweapon::Available(player,2)==4,"both counters exclude the opposite magazine");
    pairedweapon::Refresh(player,0,capacities,tick);
    Check(Field(2,12)==18,"disabling a pair preserves all original ammo");

    // Minigun/native cycle and remaining weapons keep their existing routing.
    ResetDirectTrigger();pairedweapon::Reset();Setup(0,7,38,1,50,50,1200);held[1]=-1;
    triggers[0]=1;triggers[1]=0;tick+=1000;Update(false);
    Check(Field(7,4)==2 && Field(7,16)==tick+1200,"minigun retains its native reload timer");
    triggers[0]=0;tick=Field(7,16)+1;Update(false);
    Check(Field(7,8)==49 && Field(7,4)==0,"minigun still completes automatically");
    SetType(TYPE_AUTOMATIC);Check(!Manual(33) && !Manual(34) && !Manual(22) && !Manual(31) && !Manual(25),"automatic selection includes sidearms, rifles and the pump shotgun");
    SetType(TYPE_ARCADE);Check(Manual(33) && Manual(34) && !Manual(38) && !Manual(35) && !Manual(41),"fixed arcade assignments preserve unsupported native families");

    // The final bolt sound and short confirmation pulse no longer need tuning.
    Setup(0,2,22,1,20,34,2000,0x800);SetPose(0);HoldStill();
    int sounds=reloadSounds[0];Nod(0);
    Check(lastReloadEvent==147 && reloadSounds[0]==sounds+1,"arcade reload always uses the approved bolt sound");
    Field(2,8)=1;SetPose(0);HoldStill();sounds=reloadSounds[0];const int pulses=xr::haptics[0];Nod(0);
    Check(Field(2,8)==17 && reloadSounds[0]==sounds+1 && xr::haptics[0]==pulses+1,"fixed feedback retains one sound and one readiness pulse");
    unsigned state=2,clip=0,deadline=tick+2000;
    Check(Reload(22,state,clip,3,deadline,tick,17) && state==0 && clip==3 && deadline==tick,"small reserve fills instantly without a future deadline");
    Check(!Reload(22,state,clip,3,deadline,tick,17),"no reserve produces no false reload");
    clip=17;Check(!Reload(22,state,clip,40,deadline,tick,17),"full magazine produces no false reload");
    state=2;clip=0;AdvancePhysicalGun(state,clip,20,0xfffffff0u,5,17);
    Check(state==0 && clip==17,"native automatic deadline still supports timer wrap");
    Check(PhysicalMagazineCapacity(2,0x800)==1,"single sawn-off uses half twin capacity");
    Check(!pairedweapon::Allowed(22,1,0x800) && pairedweapon::Allowed(22,2,0x800),"paired weapon skill restrictions remain intact");

    // Pump-shotgun reload uses only an empty magazine and the support wrist.
    ResetDirectTrigger();pairedweapon::Reset();Setup(0,3,25,1,30,7,1200);held[1]=-1;
    physicalweapon::supportHands[0]=1;SetPose(0);SetPose(1);
    xr::tracked[1].gripPos[2]=-.4f;triggers[0]=triggers[1]=0;Frame();
    sounds=reloadSounds[0];
    for(int i=1;i<=12;++i){xr::tracked[1].gripPos[2]=-.4f+i*.01f;Frame();}
    Check(g_pumpTravel[0]==0 && reloadSounds[0]==sounds && Field(3,8)==1,"loaded shotgun locks the pump forward without reload or checkpoint feedback");
    Field(3,8)=0;xr::tracked[1].gripPos[2]=-.4f;ResetReloadGesture(0);Frame();
    for(int i=1;i<=12;++i){xr::tracked[1].gripPos[2]=-.4f+i*.01f;Frame();}
    Check(Field(3,8)==0 && reloadSounds[0]==sounds+1 && g_pumpTravel[0]>.119f,"rear checkpoint clicks once but does not reload");
    HoldStill();Check(reloadSounds[0]==sounds+1,"rear stop held still never repeats its click");
    for(int i=1;i<=12;++i){xr::tracked[1].gripPos[2]=-.28f-i*.01f;Frame();}
    Check(Field(3,8)==7 && Field(3,4)==0 && Field(3,16)<=tick && Field(3,12)==30,"full support-hand cycle reloads immediately without changing total ammunition");
    Check(reloadSounds[0]==sounds+2 && g_pumpTravel[0]==0,"forward checkpoint clicks once and locks the loaded wrist forward");
    triggers[0]=1;Frame();Check(Field(3,8)==6,"shotgun fires immediately after the completed pump cycle");triggers[0]=0;
    Field(3,8)=0;ResetReloadGesture(0);xr::tracked[1].gripPos[2]=-.4f;Frame();
    for(int i=1;i<=12;++i){xr::tracked[1].gripPos[2]=-.4f+i*.01f;Frame();}
    Update(true);xr::tracked[1].gripPos[2]=-.4f;Frame();
    Check(Field(3,8)==0 && g_pumpTravel[0]==0,"menu interrupts the pump cycle instead of completing it on resume");
    physicalweapon::supportHands[0]=-1;Frame();Nod(0);
    Check(Field(3,8)==0 && g_pumpTravel[0]==0,"shotgun cannot reload by primary-hand nod or without a support grip");
    Field(3,12)=0;physicalweapon::supportHands[0]=1;ResetReloadGesture(0);SetPose(0);xr::tracked[1].gripPos[2]=-.4f;Frame();
    sounds=reloadSounds[0];
    for(int i=1;i<=12;++i){xr::tracked[1].gripPos[2]=-.4f+i*.01f;Frame();}
    Check(Field(3,8)==0 && reloadSounds[0]==sounds && g_pumpTravel[0]==0,"no reserve keeps the pump locked and silent");
    physicalweapon::supportHands[0]=-1;ResetDirectTrigger();
    SetType(TYPE_AUTOMATIC);
    for(int type:{22,25,29,33,34}) {
        Setup(0,type==22?2:type==25?3:type==29?4:6,type,0,30,type==22?17:7,1200);
        const int slot=held[0];Field(slot,4)=2;Field(slot,16)=tick+500;
        physicalweapon::supportHands[0]=1;SetPose(0);SetPose(1);sounds=reloadSounds[0];
        Frame();Check(Field(slot,8)==0 && g_pumpTravel[0]==0,"automatic reload waits for its native timer and never moves the pump");
        tick+=501;Frame();
        Check(Field(slot,8)>0 && reloadSounds[0]==sounds,"all supported families reload automatically without arcade feedback");
    }
    physicalweapon::supportHands[0]=-1;SetType(TYPE_ARCADE);ResetDirectTrigger();
    std::printf("PASS: %d production dual-fire checks\n",checks);
}

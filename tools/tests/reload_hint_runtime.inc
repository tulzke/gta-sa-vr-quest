void ReloadHintRuntimeTests() {
    using namespace arcadereload;
    ResetDirectTrigger();pairedweapon::Reset();g_reloadHints.Reset();SetMode(DOWN_FLICK);SetSound(BOLT);
    Setup(0,2,22,0,40,34,2000,0x800);Setup(1,4,29,0,90,30,1300);
    triggers[0]=triggers[1]=0;SetPose(0);SetPose(1);tick=20000;Update(false);
    tick=22999;Update(false);Check(!g_reloadHints.requests[0].visible,"production timer waits three seconds after empty magazine");
    tick=23000;Update(false);
    Check(g_reloadHints.requests[0].visible && g_reloadHints.requests[1].visible,"two empty guns show independent hints after timeout");
    Check(g_reloadHints.requests[0].type==22 && g_reloadHints.requests[0].mode==DOWN_FLICK &&
          g_reloadHints.requests[1].type==29 && g_reloadHints.requests[1].mode==TWIST,"hint carries the actual weapon type and fixed gesture");
    HoldStill();Nod(0);
    Check(Field(2,8)==17 && !g_reloadHints.requests[0].visible && g_reloadHints.requests[1].visible,"successful gesture removes matching hint in the same update");
    Update(true);Check(!g_reloadHints.requests[0].visible && !g_reloadHints.requests[1].visible,"menu/control block removes all hint requests");
    tick+=5000;Update(false);Check(!g_reloadHints.requests[1].visible,"return from blocked gameplay restarts delay");
    tick+=3000;Update(false);Check(g_reloadHints.requests[1].visible,"empty weapon can show hint again after unblock");
    xr::focused=false;Update(false);xr::focused=true;Check(!g_reloadHints.requests[1].visible,"focus loss clears hint");
    tick+=5000;Update(false);tick+=3000;Update(false);
    xr::tracked[1].valid=false;Update(false);xr::tracked[1].valid=true;
    Check(!g_reloadHints.requests[1].visible,"controller tracking loss clears hint");
    Field(4,12)=0;tick+=5000;Update(false);tick+=5000;Update(false);
    Check(!g_reloadHints.requests[1].visible,"no reserve ammunition produces no misleading reload hint");
    Field(4,12)=90;tick+=100;Update(false);tick+=3000;Update(false);
    held[1]=-1;Update(false);Check(!g_reloadHints.requests[1].visible,"holstering immediately clears the corresponding hint");
    Setup(1,4,29,1,90,30,1300);tick+=5000;Update(false);
    Check(!g_reloadHints.requests[1].visible,"partially loaded magazine does not nag the player to reload");
    Setup(1,7,38,0,90,50,1300);tick+=5000;Update(false);tick+=5000;Update(false);
    Check(!g_reloadHints.requests[1].visible,"native automatic family has no gesture tutorial");
    Setup(1,4,29,0,90,30,1300);tick+=5000;Update(false);tick+=3000;Update(false);
    Check(g_reloadHints.requests[1].visible,"normal physical weapon can show tutorial again");
    g.FindPlayerVehicle=[](int,bool)->void*{return player;};Update(false);
    Check(!g_reloadHints.requests[1].visible,"vehicle mode with physical guns disabled does not display a reload gesture");
    g.FindPlayerVehicle=NoVehicle;
}

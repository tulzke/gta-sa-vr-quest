#include "BikeSteering.h"
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace savr::bikesteering;
int checks=0;
#define CHECK(c) do { ++checks; if (!(c)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); std::exit(1); } } while(false)
bool Near(float a,float b,float eps=0.0001f) {return std::abs(a-b)<eps;}

Vec3 Chord(float roll,float yaw=0.0f,float span=0.62f) {
    return {span*std::cos(roll)*std::cos(yaw),span*std::sin(roll),
            -span*std::cos(roll)*std::sin(yaw)};
}
float Settle(State& state,Vec3 chord,bool two,float speed,float yaw,double& now,
             int sensitivity=100,int hz=90,float duration=1.0f) {
    float output=0;
    for (int i=0;i<static_cast<int>(duration*hz);++i) {
        now+=1.0/hz;
        output=state.Update({},chord,two,speed,yaw,sensitivity,now).steering;
    }
    return output;
}
int main() {
    float angle=0;
    for(float yaw:{-2.8f,-0.5f,0.0f,1.2f,3.0f}) {
        const auto chord=Chord(20*kRadians,yaw);
        CHECK(HandRoll({},chord,angle)); CHECK(Near(angle,20*kRadians));
        const Vec3 left{10,1.7f,-3};
        CHECK(HandRoll(left,{left.x+chord.x,left.y+chord.y,left.z+chord.z},angle));
        CHECK(Near(angle,20*kRadians));
    }
    CHECK(!HandRoll({},{},angle));
    CHECK(!HandRoll({},{0.05f,0,0},angle));
    CHECK(!HandRoll({},{2,0,0},angle));
    CHECK(!HandRoll({},{0,0.5f,0},angle));
    CHECK(!HandRoll({},{NAN,0,0},angle));
    CHECK(!HandRoll({},{INFINITY,0,0},angle));
    CHECK(Near(LeanWeight(-1),0)); CHECK(Near(LeanWeight(0.04f),0));
    CHECK(Near(LeanWeight(0.08f),0.5f)); CHECK(Near(LeanWeight(0.12f),1));
    CHECK(Near(LeanWeight(NAN),0)); CHECK(Near(LeanWeight(INFINITY),0));
    CHECK(LeanWeight(0.04001f)<0.0001f); CHECK(LeanWeight(0.11999f)>0.9999f);
    CHECK(LeanCommand(1.9f*kRadians,100)==0);
    CHECK(Near(LeanCommand(25*kRadians,100),1));
    CHECK(Near(LeanCommand(-25*kRadians,100),-1));
    CHECK(Near(LeanCommand(12.5f*kRadians,200),1));
    CHECK(Near(LeanCommand(NAN,100),0));
    CHECK(Near(LeanCommand(25*kRadians,0),LeanCommand(25*kRadians,25)));

    State state; double now=0;
    state.Update({},Chord(0),true,0.2f,0,100,now);
    CHECK(Settle(state,Chord(25*kRadians),true,0.2f,0,now)>0.99f);
    // Holding a lean must not re-zero it after a long curve.
    CHECK(Settle(state,Chord(25*kRadians),true,0.2f,0,now,100,90,8)>0.99f);
    CHECK(std::abs(Settle(state,Chord(0),true,0.2f,0,now))<0.001f);
    CHECK(Settle(state,Chord(-25*kRadians),true,0.2f,0,now)<-0.99f);
    // Vertical translation of both hands (wheelie) never introduces roll.
    state.Reset(); now=0;
    state.Update({0,0.2f,0},{0.62f,0.2f,0},true,0.2f,0,100,now);
    for(int i=0;i<90;++i) {
        now+=1.0/90;
        CHECK(Near(state.Update({0,0.5f,0},{0.62f,0.5f,0},true,0.2f,0,100,now).steering,0));
    }
    // Regrip captures the current hand angle, rather than lurching toward it.
    state.Reset(); now=0;
    state.Update({},Chord(20*kRadians),true,0.2f,0,100,now);
    CHECK(std::abs(Settle(state,Chord(20*kRadians),true,0.2f,0,now))<0.001f);
    CHECK(Settle(state,Chord(45*kRadians),true,0.2f,0,now)>0.99f);
    const float before=state.output;
    now+=1.0/90;
    auto result=state.Update({},Chord(45*kRadians),false,0.2f,-0.4f,100,now);
    CHECK(std::abs(result.steering-before)<0.3f); CHECK(!state.referenceValid);
    CHECK(Near(Settle(state,Chord(45*kRadians),false,0.2f,-0.4f,now),-0.4f));
    CHECK(std::abs(Settle(state,Chord(45*kRadians),true,0.2f,0,now))<0.001f);
    // Parking/reverse and a missing speed preserve handlebar intent.
    CHECK(Near(Settle(state,Chord(20*kRadians),true,0.02f,0.7f,now),0.7f));
    CHECK(Near(Settle(state,Chord(20*kRadians),true,-0.2f,-0.6f,now),-0.6f));
    CHECK(Near(Settle(state,Chord(20*kRadians),true,NAN,0.3f,now),0.3f));
    // Bad geometry must relinquish authority and recapture after recovery.
    result=state.Update({}, {},true,0.2f,0,100,now+0.01);
    CHECK(!result.twoHands&&result.weight==0&&!state.referenceValid); now+=0.01;
    CHECK(std::abs(Settle(state,Chord(-20*kRadians),true,0.2f,0,now))<0.001f);
    result=state.Update({},Chord(0),true,0.2f,0,100,now+1.0);
    CHECK(Near(result.roll,0)&&Near(result.steering,0)); now+=1.0;
    result=state.Update({},Chord(20*kRadians),true,0.2f,0,100,now-1.0);
    CHECK(Near(result.roll,0)&&Near(result.steering,0));
    result=state.Update({},Chord(0),true,0.2f,0,100,NAN);
    CHECK(!state.clockValid&&Near(result.steering,0));
    state.Reset(); CHECK(!state.referenceValid&&!state.clockValid&&state.output==0);
    now=0;state.Update({},Chord(0),true,0.2f,0,100,now);
    Settle(state,Chord(0),true,0.2f,0,now);
    now+=1.0/90;
    result=state.Update({},Chord(60*kRadians),true,0.2f,0,100,now);
    CHECK(Near(result.roll,0)&&Near(result.steering,0));
    CHECK(std::abs(Settle(state,Chord(60*kRadians),true,0.2f,0,now))<0.001f);

    // Once takeover has settled, response over equal time is frame-rate invariant.
    float outputs[3]{}; int index=0;
    for(int hz:{45,72,120}) {
        State s; double time=0;
        s.Update({},Chord(0),true,0.2f,0,100,time);
        Settle(s,Chord(0),true,0.2f,0,time,100,hz);
        outputs[index++]=Settle(s,Chord(25*kRadians),true,0.2f,0,time,100,hz,1.0f);
    }
    CHECK(Near(outputs[0],outputs[1])); CHECK(Near(outputs[1],outputs[2]));
    // The handover itself also behaves consistently at different frame rates.
    index=0;
    for(int hz:{45,72,120}) {
        State s; double time=0;
        s.Update({},Chord(0),true,0.2f,0,100,time);
        outputs[index++]=Settle(s,Chord(25*kRadians),true,0.2f,0,time,100,hz,0.2f);
    }
    CHECK(std::abs(outputs[0]-outputs[1])<0.04f);
    CHECK(std::abs(outputs[1]-outputs[2])<0.04f);
    std::printf("PASS: %d lean geometry, axes, blend, input bounds, tracking and handover checks\n",checks);
}

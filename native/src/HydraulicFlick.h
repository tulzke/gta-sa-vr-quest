#pragma once
#include <cmath>

namespace savr::hydraulics {
// One intentional physical input can answer a beat. Holding the same sector
// cannot answer later beats; an actual sector change can be a fresh input.
struct Flick {
    int pending{},beat{-1},lastDirection{};bool neutral{true};
    void Suspend(int beatIndex){
        // A missing pose is not a deliberate return through neutral. Preserve
        // the edge history, while still expiring an answer when the beat ends.
        if(beatIndex!=beat){beat=beatIndex;pending=0;}
    }
    void Step(bool active,float x,float y,int beatIndex,bool directChanges){
        Suspend(beatIndex);
        const float mag=std::hypot(x,y);
        const int direction=mag>=.5f?(std::abs(x)>=std::abs(y)?(x>0?4:3):(y>0?1:2)):0;
        if(!active){pending=0;neutral=true;lastDirection=0;return;}
        if(mag<.35f)neutral=true;
        else if(mag>=.5f&&(neutral||(directChanges&&lastDirection&&direction!=lastDirection))){
            neutral=false;pending=direction;
        }
        lastDirection=direction;
    }
};
}

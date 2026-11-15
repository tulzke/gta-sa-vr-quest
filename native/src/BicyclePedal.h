#pragma once

#include <algorithm>
#include <cmath>

namespace savr::bicyclepedal {

struct Input { bool active{},held{},justPressed{}; int accelerate{}; };
inline thread_local Input current{};

// Sample once per CBmx physics update, rather than consuming edges in pad
// queries (the native sprint controller can poll them more than once).
struct State {
    void* bike{};
    bool down{},clockValid{};
    double previous{},driveUntil{};
    void Reset() { *this={}; }
    Input Update(void* vehicle,float trigger,double now) {
        if (!vehicle||!std::isfinite(trigger)||!std::isfinite(now)) {
            Reset(); return {};
        }
        trigger=std::clamp(trigger,0.0f,1.0f);
        const bool fresh=bike!=vehicle||!clockValid||now<previous||now-previous>.25;
        bool edge=false;
        if (fresh) {
            // Boarding/resume with R2 held pedals normally but invents no tap.
            down=trigger>=.65f;driveUntil=0;
        } else if (!down&&trigger>=.65f) {
            down=true;edge=true;
        } else if (down&&trigger<=.30f) {
            down=false;driveUntil=now+.20;
        }
        bike=vehicle;previous=now;clockValid=true;
        // Bridge the brief release between taps. A deliberate release coasts
        // after 200 ms; this is drive, never a synthetic extra sprint edge.
        const float drive=(down||now<driveUntil)?1.0f:trigger;
        return {true,down,edge,static_cast<int>(drive*255.0f)};
    }
};

class Scope {
    Input previous_;
public:
    explicit Scope(Input input):previous_(current) { current=input; }
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
    ~Scope() { current=previous_; }
};

} // namespace savr::bicyclepedal

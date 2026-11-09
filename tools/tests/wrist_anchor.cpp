using XrVector3f=vehicledoors::Vec;
struct XrQuaternionf {float x{},y{},z{},w{1};};
struct TwoHandVisualState {
 bool enabled{},supportAnchorValid{},supportBasisValid{};int primaryHand{},supportHand=1;
 float supportAnchor[3]{},supportRight[3]{1,0,0},supportUp[3]{0,0,1},supportForward[3]{0,1,0};
};
bool TwoHandStateUsable(const TwoHandVisualState& s){return s.enabled;}
XrVector3f vadd(XrVector3f a,XrVector3f b){return a+b;}
XrVector3f vsub(XrVector3f a,XrVector3f b){return a-b;}
XrVector3f vscale(XrVector3f a,float f){return a*f;}
float vdot(XrVector3f a,XrVector3f b){return vehicledoors::Dot(a,b);}
XrVector3f vcross(XrVector3f a,XrVector3f b){return vehicledoors::Cross(a,b);}
XrVector3f vnorm(XrVector3f a){float n=vehicledoors::Length(a);return n>.0001f?a*(1.f/n):XrVector3f{};}
XrVector3f QuatRotate(XrQuaternionf q,XrVector3f v){XrVector3f u{q.x,q.y,q.z};return v+(vcross(u,v)*q.w+vcross(u,vcross(u,v)))*2.f;}
XrVector3f TwoHandRotatePoint(const TwoHandVisualState&,XrVector3f a){return a+XrVector3f{10,20,30};}
XrVector3f TwoHandRotateVector(const TwoHandVisualState&,XrVector3f a){return a;}
constexpr float kWristPanelBack=.07f,kWristPanelLift=.035f;
// ANCHOR
void TestWristHud(){
 for(int hand=0;hand<2;++hand){
  HandPose raw;raw.gripPos[0]=.3f;raw.gripPos[1]=.4f;raw.gripPos[2]=1.2f;
  XrVector3f base,along,across,lift,right,up,normal;
  assert(BuildHandPanelAnchor(raw,hand,{},base,along,across,lift,right,up,normal));
  const auto expected=base,expectedAlong=along;
  HandPose pinned=raw;pinned.wristAnchorValid=true;std::copy_n(raw.gripPos,3,pinned.wristGripPos);
  pinned.gripPos[0]=100;pinned.gripPos[1]=-100;pinned.gripPos[2]=50;
  TwoHandVisualState weapon;weapon.enabled=true;
  assert(BuildHandPanelAnchor(pinned,hand,weapon,base,along,across,lift,right,up,normal));
  assert(vehicledoors::Length(base-expected)<.00001f&&vehicledoors::Length(along-expectedAlong)<.00001f);
  pinned.gripOri[1]=.70710678f;pinned.gripOri[3]=.70710678f;
  assert(BuildHandPanelAnchor(pinned,hand,weapon,base,along,across,lift,right,up,normal));
  assert(vehicledoors::Length(along-expectedAlong)>1.f); // live wrist rotation survives palm locking
 }
}

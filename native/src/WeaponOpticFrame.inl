// Included after the tracked weapon matrix helpers. No engine state changes.
struct WeaponOpticFrame {
    CamBasis surface{}, camera{};
    float halfSize{}, length{};
};
bool BuildWeaponOpticFrame(int type, const RwMat& weapon, WeaponOpticFrame& out) {
    if(!weaponoptic::Supported(type)) return false;
    const auto profile=calib::SnapshotOptic(type);
    const int* v=profile.value;
    const V3 barrel=Normalized(V3{weapon.r[0],weapon.r[1],weapon.r[2]});
    const V3 top=Normalized(V3{weapon.u[0],weapon.u[1],weapon.u[2]});
    const V3 side=Normalized(V3{weapon.a[0],weapon.a[1],weapon.a[2]});
    const V3 pivot{weapon.t[0],weapon.t[1],weapon.t[2]};
    const V3 position=pivot+barrel*(v[0]*.001f)+top*(v[1]*.001f)+side*(v[2]*.001f);
    constexpr float rad=.001745329252f;
    Mat34 local{barrel,top,side,position};
    local=MatMul34(local,SetRotate34(v[3]*rad,v[4]*rad,v[5]*rad));
    const V3 forward=Normalized(local.right);
    const V3 up=Normalized(local.fwd-forward*Dot(local.fwd,forward));
    const V3 left=Normalized(Cross(up,forward));
    if(!Finite(position)||!Finite(forward)||Dot(forward,forward)<.9f||Dot(left,left)<.9f) return false;
    out.halfSize=v[6]*.0005f;
    out.length=v[7]*.001f;
    out.surface={left,forward,up,position};
    out.camera=out.surface;
    out.camera.pos=position+forward*out.length;
    return true;
}

// Included after the tracked weapon matrix helpers. No engine state changes.
struct WeaponOpticFrame {
    CamBasis surface{}, camera{};
    float halfSize{}, length{};
};

bool BuildPhotoLens(const RwMat& weapon, CamBasis& camera) {
    const V3 forward = Normalized(V3{weapon.r[0], weapon.r[1], weapon.r[2]});
    const V3 top{weapon.u[0], weapon.u[1], weapon.u[2]};
    // The camera model's second axis is along the body, not the top of the photo.
    const V3 up = Normalized(Cross(top, forward));
    const V3 left = Normalized(Cross(up, forward));
    const V3 pivot{weapon.t[0], weapon.t[1], weapon.t[2]};
    if (!Finite(pivot) || Dot(forward, forward) < .9f || Dot(left, left) < .9f)
        return false;
    camera = {left, forward, up, pivot + forward * .075f + up * .012f};
    return true;
}

bool BuildWeaponOpticFrame(int type, const RwMat& weapon, WeaponOpticFrame& out) {
    if(!weaponoptic::Calibratable(type)) return false;
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
    V3 forward=Normalized(local.right);
    V3 up=Normalized(local.fwd-forward*Dot(local.fwd,forward));
    if (type == weaponoptic::PhotoCamera) {
        if (!profile.legacyPhotoMount)
            up = Normalized(Cross(up, forward));
        const V3 right = Cross(forward, up);
        const auto screenVector = [&](V3 v) {
            return forward*v.x + up*v.y + right*v.z;
        };
        const auto r = profile.photoRotation;
        const Q turn{r.x, r.y, r.z, r.w};
        const V3 rotatedForward = screenVector(Rotate(turn, {1, 0, 0}));
        up = screenVector(Rotate(turn, {0, 1, 0}));
        forward = rotatedForward;
    }
    const V3 left=Normalized(Cross(up,forward));
    if(!Finite(position)||!Finite(forward)||Dot(forward,forward)<.9f||Dot(left,left)<.9f) return false;
    out.halfSize=v[6]*.0005f;
    out.length=v[7]*.001f;
    out.surface={left,forward,up,position};
    if(type==weaponoptic::PhotoCamera) return BuildPhotoLens(weapon,out.camera);
    out.camera=out.surface;
    out.camera.pos=position+forward*out.length;
    return true;
}

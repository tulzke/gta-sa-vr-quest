bool HydraulicsCompatible(int model){
    if(model<400||model>611||!g.CModelInfo_IsCarModel||!g.CModelInfo_IsCarModel(model)||!g.CModelInfo_ms_modelInfoPtrs||!g.CModelInfo_ms_modelInfoPtrs[model])return false;
    void* v=GetControlledVehicle();
    if(!v||*reinterpret_cast<const std::uint16_t*>(static_cast<const char*>(v)+0x32)!=model)return false;
    return (*reinterpret_cast<const std::uint32_t*>(static_cast<const char*>(v)+0x4b8)&0x40000u)==0;
}
void SetHydraulicsPose(int model,const float pose[12]){
    if(!HydraulicsCompatible(model)||!pose)return;
    for(int i=0;i<12;++i)if(!std::isfinite(pose[i])||std::abs(pose[i])>10.f)return;
    const V3 r{pose[3],pose[4],pose[5]},f{pose[6],pose[7],pose[8]},u{pose[9],pose[10],pose[11]};
    if(std::abs(Length(r)-1.f)>.01f||std::abs(Length(f)-1.f)>.01f||std::abs(Length(u)-1.f)>.01f||Dot(Cross(r,f),u)<.99f)return;
    std::lock_guard<std::mutex> lock(g_modelMutex);auto& c=g_modelCalibration[model];std::copy_n(pose,12,c.hydroPose);c.hydroPoseSet=true;
}
float GetHydraulicsScale(){const int model=GetActiveVehicleModelId();std::lock_guard<std::mutex> lock(g_modelMutex);const auto it=g_modelCalibration.find(model);return it==g_modelCalibration.end()?1.f:it->second.hydroScale;}
void AdjustHydraulicsScale(int direction){const int model=GetActiveVehicleModelId();if(!HydraulicsCompatible(model))return;
    {std::lock_guard<std::mutex> lock(g_modelMutex);auto& c=g_modelCalibration[model];c.hydroScale=std::clamp(c.hydroScale+direction*.05f,.4f,2.f);}Save();ResetInteraction();}
void ResetHydraulicsPose(){const int model=GetActiveVehicleModelId();
    {std::lock_guard<std::mutex> lock(g_modelMutex);auto& c=g_modelCalibration[model];c.hydroPoseSet=false;c.hydroScale=1.f;}Save();ResetInteraction();}
bool GetPhysicalHydraulics(float* x,float* y){
    void* v=g_hydroManagedVehicle.load(std::memory_order_acquire);
    if(!v||v!=GetControlledVehicle())return false;
    if(x)*x=g_hydroCommandX.load();if(y)*y=g_hydroCommandY.load();return true;
}

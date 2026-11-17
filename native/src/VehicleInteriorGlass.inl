bool PlayerInteriorGlassAtomic(void* atomic){
 if(!atomic||g_renderingStereoEyeIndex<0||!g_stereoActive.load(std::memory_order_relaxed)||
    !driving::IsInteriorGlassHidden()||driving::ThirdPersonViewActive()||!g.FindPlayerVehicle||!g.CVehicle_GetVehicleAppearance)return false;
 void* vehicle=g.FindPlayerVehicle(-1,false);if(!vehicle)return false;
 const int appearance=g.CVehicle_GetVehicleAppearance(vehicle);if(appearance!=1&&appearance!=3&&appearance!=5)return false;
 void* playerClump{};void* clump{};std::memcpy(&playerClump,static_cast<char*>(vehicle)+0x20,8);std::memcpy(&clump,static_cast<char*>(atomic)+0x58,8);
 return playerClump&&playerClump==clump;
}
bool NamedInteriorWindow(void* atomic){
 void* frame{};std::memcpy(&frame,static_cast<char*>(atomic)+8,8);
 return frame&&g.GetFrameNodeName&&interiorglass::WindowAtomic(g.GetFrameNodeName(frame));
}
struct ScopedInteriorGlass {
 struct Entry{std::uint8_t* alpha;std::uint8_t saved;};Entry entries[128];int count{};
 static void* Material(void* material,void* context){
  if(!material)return material;auto& self=*static_cast<ScopedInteriorGlass*>(context);
  auto* bytes=static_cast<std::uint8_t*>(material);void* texture{};std::memcpy(&texture,material,8);
  if(!interiorglass::WindowMaterial(texture?static_cast<char*>(texture)+0x20:nullptr,bytes[11],bytes[8],bytes[9],bytes[10]))return material;
  for(int i=0;i<self.count;++i)if(self.entries[i].alpha==bytes+11)return material;
  if(self.count<128){self.entries[self.count++]={bytes+11,bytes[11]};bytes[11]=0;}
  return material;
 }
 void Apply(void* atomic){
  if(!g.RpGeometryForAllMaterials)return;void* geometry{};std::memcpy(&geometry,static_cast<char*>(atomic)+0x30,8);
  if(geometry)g.RpGeometryForAllMaterials(geometry,&Material,this);
 }
 ~ScopedInteriorGlass(){for(int i=0;i<count;++i)*entries[i].alpha=entries[i].saved;}
};

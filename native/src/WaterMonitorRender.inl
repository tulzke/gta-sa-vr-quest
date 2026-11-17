// Same producer queue as the scene camera and both eyes; no shared GL texture.

extern "C" const unsigned char savr_water_monitor_mesh[],savr_water_monitor_mesh_end[];
namespace water_monitor_render {
using Vec=watermonitor::Vec;
watermonitor::Visual visual{};
watermonitor::Model model{};
watermonitor::Frame camera{};
CamBasis body{},dock{};
bool ready{};
V3 V(Vec v){return {v.x,v.y,v.z};}
Vec W(V3 v){return {v.x,v.y,v.z};}
bool WorldFrame(const watermonitor::Frame& source,CamBasis& out){
 const auto point=[](Vec p,V3& v){float a[3]{p.x,p.y,p.z},b[3]{};
  if(!vrcam::TrackingPointToWorld(a,b))return false;v={b[0],b[1],b[2]};return Finite(v);};
 V3 r{},f{},u{};
 if(!point(source.center,out.pos)||!point(source.center+source.right,r)||!point(source.center+source.forward,f)||!point(source.center+source.up,u))return false;
 out.right=Normalized(r-out.pos);out.forward=Normalized(f-out.pos);out.up=Normalized(u-out.pos);
 return Dot(out.right,out.right)>.9f&&Dot(out.forward,out.forward)>.9f&&Dot(out.up,out.up)>.9f;
}
bool NeedsRaster(){watermonitor::Visual v{};return driving::GetWaterMonitorVisual(&v)&&v.cameraActive&&!weapon_optic_render::optics[2].raster;}
void RenderEffects(void* sc){
 using RenderFx=void(*)(void*,void*,unsigned char);
 static RenderFx fx=[](){void* h=dlopen("libGame.so",RTLD_NOW|RTLD_NOLOAD);if(!h)return RenderFx{};
  auto p=reinterpret_cast<RenderFx>(dlsym(h,"_ZN11FxManager_c6RenderEP8RwCamerah"));
  if(reinterpret_cast<std::uintptr_t>(p)!=g.LoadBase+0x4dfae4||reinterpret_cast<std::uintptr_t>(dlsym(h,"g_fxMan"))!=g.LoadBase+0xa5bcf8)p=nullptr;
  dlclose(h);return p;}();
 if(fx){auto* manager=reinterpret_cast<unsigned char*>(g.LoadBase+0xa5bcf8);const auto flag=manager[0x110];fx(manager,sc,0);manager[0x110]=flag;}
}
void Prepare(){
 ready=false;
 if(!driving::GetWaterMonitorVisual(&visual)||PhotoViewActive()||FormalCutsceneRunning()||ScriptedCutsceneRunning()||
    !rear_mirror_render::Resolve()||!WorldFrame(visual.body,body)||!WorldFrame(visual.dock,dock))return;
 static const bool loaded=model.Load(savr_water_monitor_mesh,static_cast<std::size_t>(savr_water_monitor_mesh_end-savr_water_monitor_mesh));
 if(!loaded)return;
 ready=true;
 auto& optic=weapon_optic_render::optics[2];
 // Camera position is attached to the nozzle; its orientation follows the
 // physical screen immediately. The native nozzle orientation is independent.
 const V3 forward=visual.owner>=0?body.forward:dock.forward;
 V3 up{0,0,1};up=Normalized(up-forward*Dot(up,forward));
 if(Dot(up,up)<.5f)up=body.up;
 const V3 right=Normalized(Cross(forward,up));up=Normalized(Cross(right,forward));
 watermonitor::CameraSettings settings{};std::copy_n(visual.cameraSettings,6,settings.begin());
 camera=watermonitor::CameraFrame(visual.nozzleWorld,{{},W(right),W(forward),W(up)},settings);
 if(!visual.cameraActive)return;
 weapon_optic_render::remoteEffects=&RenderEffects;
 optic.remote=true;optic.type=-113;optic.excluded=visual.vehicle;
 optic.width=512;optic.height=384;optic.farClip=110.f;optic.tanFov=.70020754f;
 // Native CCamera's first axis points left, unlike our authored model axes.
 optic.frame.camera={V(camera.right)*-1.f,V(camera.forward),V(camera.up),V(camera.center)};
 optic.visible=true;
 if(!optic.raster)optic.raster=g.RwRasterCreate(optic.width,optic.height,32,5);
}
V3 Point(const CamBasis& frame,Vec p){return frame.pos+frame.right*p.x+frame.forward*p.y+frame.up*p.z;}
std::uint32_t Color(float r,float green,float b,float alpha=1.f){
 const auto byte=[](float c){return static_cast<std::uint32_t>(std::clamp(c,0.f,1.f)*255.f+.5f);};
 return byte(r)|(byte(green)<<8)|(byte(b)<<16)|(byte(alpha)<<24);
}
void DrawSurface(){
 if(!ready||!g.RwIm3DTransform||!g.RwRenderStateGet)return;
 constexpr int states[]{1,6,8,9,10,11,12,14,20};std::uintptr_t saved[std::size(states)]{};
 for(std::size_t i=0;i<std::size(states);++i)g.RwRenderStateGet(states[i],&saved[i]);
 g.RwRenderStateSet(6,reinterpret_cast<void*>(1));g.RwRenderStateSet(8,reinterpret_cast<void*>(1));
 g.RwRenderStateSet(9,reinterpret_cast<void*>(2));g.RwRenderStateSet(12,nullptr);
 g.RwRenderStateSet(14,nullptr);g.RwRenderStateSet(20,reinterpret_cast<void*>(1));
 const auto draw=[](std::vector<SkyBackdropVertex>& v,std::vector<unsigned short>& ix,void* texture){
  if(ix.empty())return;g.RwRenderStateSet(1,texture);
  if(g.RwIm3DTransform(v.data(),static_cast<int>(v.size()),nullptr,1u|8u|16u)){
   g.RwIm3DRenderIndexedPrimitive(3,ix.data(),static_cast<int>(ix.size()));g.RwIm3DEnd();}
 };
 static std::vector<SkyBackdropVertex> vertices;static std::vector<unsigned short> indices,glass;
 vertices.clear();indices.clear();glass.clear();vertices.reserve(model.vertices.size());
 const float angle=visual.fire?-.20943951f:0.f,c=std::cos(angle),s=std::sin(angle);
 for(const auto& v:model.vertices){
  Vec p{v.position[0],v.position[1],v.position[2]},n{v.normal[0],v.normal[1],v.normal[2]};
  if(v.role==2){const float y=p.y-.018f,z=p.z-.103f;p.y=.018f+c*y-s*z;p.z=.103f+s*y+c*z;
   const float ny=n.y;n.y=c*ny-s*n.z;n.z=s*ny+c*n.z;}
  const auto& frame=v.role==0?dock:body;
  const V3 normal=frame.right*n.x+frame.forward*n.y+frame.up*n.z;
  const float light=.64f+.36f*std::max(0.f,Dot(normal,Normalized(V3{-.4f,-.4f,1.f})));
  const auto channel=[&](float value){return std::pow(value,1.f/2.2f)*light;};
  const auto color=v.role==3?Color(.008f,.009f,.012f):v.role==4?
   Color(visual.cameraActive?.85f:.11f,visual.cameraActive?.40f:.06f,.025f):Color(channel(v.color[0]),channel(v.color[1]),channel(v.color[2]));
  vertices.push_back({Point(frame,p),normal,color,v.uv[0],v.uv[1]});
 }
 // Original glass stays black behind the collapsing image.
 indices.assign(model.indices.begin(),model.indices.end());draw(vertices,indices,nullptr);
 const auto& optic=weapon_optic_render::optics[2];
 if(visual.screen.light>0&&optic.imageReady&&optic.raster){
  for(std::size_t i=0;i<model.indices.size();i+=3)
   if(model.vertices[model.indices[i]].role==3)for(int n=0;n<3;++n)glass.push_back(model.indices[i+n]);
  for(std::size_t i=0;i<model.vertices.size();++i){const auto& v=model.vertices[i];if(v.role!=3)continue;
   const float x=v.position[0]*visual.screen.x,z=(v.position[2]-.173f)*visual.screen.y;
   const float y=-.059f+.0033f*((x/.061f)*(x/.061f)+(z/.04575f)*(z/.04575f))-.00012f;
   const float vignette=1.f-.14f*std::clamp((v.position[0]/.061f)*(v.position[0]/.061f)+((v.position[2]-.173f)/.04575f)*((v.position[2]-.173f)/.04575f),0.f,1.f);
   const float light=visual.screen.light*vignette;vertices[i].position=Point(body,{x,y,z+.173f});vertices[i].color=Color(light,light,light);
  }
  draw(vertices,glass,optic.raster);
  // Subtle neutral scan lines. No monochrome tint or full-screen postprocess.
  vertices.clear();indices.clear();
  const auto vertex=[&](float x,float z,std::uint32_t color){
   x*=visual.screen.x;z*=visual.screen.y;
   const float y=-.059f+.0033f*((x/.061f)*(x/.061f)+(z/.04575f)*(z/.04575f))-.00030f;
   return SkyBackdropVertex{Point(body,{x,y,z+.173f}),{},color,0,0};};
  const auto quad=[&](float x0,float z0,float x1,float z1,std::uint32_t color){
   const auto start=static_cast<unsigned short>(vertices.size());
   vertices.push_back(vertex(x0,z0,color));vertices.push_back(vertex(x1,z0,color));vertices.push_back(vertex(x1,z1,color));vertices.push_back(vertex(x0,z1,color));
   for(int i:{0,1,2,0,2,3})indices.push_back(start+i);
  };
  for(int row=0;row<64;++row){const float z=-.0454f+row*.00142f;for(int col=0;col<2;++col)quad(-.0604f+col*.0604f,z,col*.0604f,z+.00025f,Color(0,0,0,.16f));}
  // Desired crosshair is fixed; the real nozzle circle follows the native
  // direction, including limits and inertia. Off-screen directions pin to edge.
  const auto circle=watermonitor::Circle(visual.actualWorld,camera);
  const float cx=circle.x*.061f,cz=circle.y*.04575f,rad=.0037f,thick=.00045f;
  const auto white=Color(.95f,.94f,.88f,visual.screen.light);
  for(int i=0;i<32;++i){const float a=i*6.283185307f/32,b=(i+1)*6.283185307f/32;
   const auto start=static_cast<unsigned short>(vertices.size());
   for(auto p:{std::pair<float,float>{a,rad}, {b,rad},{b,rad-thick},{a,rad-thick}})
    vertices.push_back(vertex(cx+std::cos(p.first)*p.second,cz+std::sin(p.first)*p.second,white));
   for(int n:{0,1,2,0,2,3})indices.push_back(start+n);
  }
  const auto target=watermonitor::Circle(W(visual.owner>=0?body.forward:dock.forward),camera);const float tx=target.x*.061f,tz=target.y*.04575f;
  quad(tx-.003f,tz-.00024f,tx+.003f,tz+.00024f,white);quad(tx-.00024f,tz-.003f,tx+.00024f,tz+.003f,white);
  g.RwRenderStateSet(8,nullptr);g.RwRenderStateSet(10,reinterpret_cast<void*>(5));g.RwRenderStateSet(11,reinterpret_cast<void*>(6));g.RwRenderStateSet(12,reinterpret_cast<void*>(1));
  draw(vertices,indices,nullptr);
 }
 for(std::size_t i=0;i<std::size(states);++i)g.RwRenderStateSet(states[i],reinterpret_cast<void*>(saved[i]));
}
}

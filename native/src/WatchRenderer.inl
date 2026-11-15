// Included in Xr.cpp. Interaction/animation updates once before either eye.
extern "C" {
extern const unsigned char savr_watch_glb[],savr_watch_glb_end[];
extern const char savr_watch_attribution[];
}
watchmodel::Model g_watchModel;
struct WatchGpuMesh { GLuint vao=0,vbo=0,ebo=0; GLsizei count=0;int material=0;bool display=false; };
std::vector<WatchGpuMesh> g_watchMeshes;
std::vector<GLuint> g_watchImages;
GLuint g_watchProgram=0;
bool g_watchTried=false;
enum WatchUniform { WATCH_MVP,WATCH_MODEL,WATCH_OPACITY,WATCH_HEADING,WATCH_WATCH,WATCH_MONEY,WATCH_WANTED,WATCH_COLOR,WATCH_METAL,WATCH_MAP,WATCH_HASMAP,WATCH_HASCOLOR,WATCH_HASMETAL,WATCH_COLORFACTOR,WATCH_MATERIAL,WATCH_DISPLAY,WATCH_MIRRORFACE,WATCH_PROJECTION,WATCH_EFFECT,WATCH_TIME,WATCH_UNIFORM_COUNT };
GLint g_watchUniforms[WATCH_UNIFORM_COUNT]{};
const char* const kWatchUniformNames[WATCH_UNIFORM_COUNT]={"uMVP","uModel","uOpacity","uHeading","uWatch","uMoney","uWanted","uColor","uMetal","uMap","uHasMap","uHasColor","uHasMetal","uColorFactor","uMaterial","uDisplay","uMirrorFace","uProjection","uEffect","uTime"};
watchhud::Projection g_watchProjection;
watchhud::ZoomSlider g_watchZoomSlider;
float g_watchEffectTime=0;
handskeleton::Mat g_watchFrameModel{};
bool g_watchFrameValid=false;
std::atomic<bool> g_watchProjectionNeeded{false};
bool WatchProjectionNeeded(){return g_watchProjectionNeeded.load(std::memory_order_relaxed);}
std::atomic<std::uint64_t> g_watchTriggerUntil[2]{};
bool WatchConsumesTrigger(int hand){
    return hand>=0&&hand<2&&IsSessionFocused()&&!IsMenuVisible()&&hud::ShouldRenderWristHud()&&
        MonotonicNowNs()<g_watchTriggerUntil[hand].load(std::memory_order_relaxed);
}
XrVector3f WatchNative(const std::array<float,3>& v){return {v[0],-v[2],v[1]};}
XrVector3f WatchPoint(const handskeleton::Mat& m,XrVector3f p){
    return {m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12],m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13],m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14]};
}
XrVector3f WatchVector(const handskeleton::Mat& m,XrVector3f p){return vsub(WatchPoint(m,p),WatchPoint(m,{0,0,0}));}
void WatchReadingAxes(const handskeleton::Mat& m,XrVector3f& right,XrVector3f& up){
    const XrVector3f x{m[0],m[1],m[2]},y{m[4],m[5],m[6]},z{m[8],m[9],m[10]};
    right=vscale(vnorm(WatchVector(m,WatchNative(g_watchModel.right))),vdot(vcross(x,y),z)<0?1.f:-1.f);
    up=vscale(vnorm(WatchVector(m,WatchNative(g_watchModel.up))),-1.f);
}

struct WatchGlBindings {
    GLint program{},vao{},vbo{},active{},textures[3]{},samplers[3]{};
    WatchGlBindings() {
        glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&vbo);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
        for(int i=0;i<3;++i){glActiveTexture(GL_TEXTURE0+i);glGetIntegerv(GL_TEXTURE_BINDING_2D,&textures[i]);glGetIntegerv(GL_SAMPLER_BINDING,&samplers[i]);}
    }
    ~WatchGlBindings() {
        for(int i=0;i<3;++i){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,textures[i]);glBindSampler(i,samplers[i]);}
        glActiveTexture(active);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glUseProgram(program);
    }
};
bool EnsureWatchModel() {
    if(g_watchTried)return g_watchProgram!=0;
    g_watchTried=true;
    if(!g_watchModel.Load(savr_watch_glb,savr_watch_glb_end-savr_watch_glb)) {
        LOGE("[watch] GLB failed: %s",g_watchModel.error.c_str());return false;
    }
    char defines[240];std::snprintf(defines,sizeof(defines),
        "#define MAP_RADIUS %.6f\n#define MAP_TOP %.6f\n#define MAP_BOTTOM %.6f\n#define MAP_SCALE %.6f\n",
        watchhud::kMapRadius,watchhud::kMapTop,watchhud::kMapBottom,watchhud::kTextureScale);
    std::string fragment=watchhud::kFragmentShader;
    fragment.insert(fragment.find('\n')+1,defines);
    const GLuint vertex=CompileHandShader(GL_VERTEX_SHADER,watchhud::kVertexShader);
    const GLuint pixel=CompileHandShader(GL_FRAGMENT_SHADER,fragment.c_str());
    if(!vertex||!pixel){if(vertex)glDeleteShader(vertex);if(pixel)glDeleteShader(pixel);return false;}
    GLuint program=glCreateProgram();glAttachShader(program,vertex);glAttachShader(program,pixel);
    glLinkProgram(program);glDeleteShader(vertex);glDeleteShader(pixel);
    GLint linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);
    if(!linked){char message[1024]{};glGetProgramInfoLog(program,sizeof(message),nullptr,message);LOGE("[watch] shader: %s",message);glDeleteProgram(program);return false;}
    for(int i=0;i<WATCH_UNIFORM_COUNT;++i)g_watchUniforms[i]=glGetUniformLocation(program,kWatchUniformNames[i]);
    void* library=dlopen("libjnigraphics.so",RTLD_NOW|RTLD_LOCAL);
    const auto sym=[&](const char* name){return library?dlsym(library,name):nullptr;};
    const auto create=reinterpret_cast<int(*)(const void*,size_t,void**)>(sym("AImageDecoder_createFromBuffer"));
    const auto header=reinterpret_cast<const void*(*)(const void*)>(sym("AImageDecoder_getHeaderInfo"));
    const auto width=reinterpret_cast<int(*)(const void*)>(sym("AImageDecoderHeaderInfo_getWidth"));
    const auto height=reinterpret_cast<int(*)(const void*)>(sym("AImageDecoderHeaderInfo_getHeight"));
    const auto format=reinterpret_cast<int(*)(void*,int)>(sym("AImageDecoder_setAndroidBitmapFormat"));
    const auto decode=reinterpret_cast<int(*)(void*,void*,size_t,size_t)>(sym("AImageDecoder_decodeImage"));
    const auto destroy=reinterpret_cast<void(*)(void*)>(sym("AImageDecoder_delete"));
    bool ok=create&&header&&width&&height&&format&&decode&&destroy;
    g_watchImages.resize(g_watchModel.images.size());
    for(std::size_t i=0;ok&&i<g_watchModel.images.size();++i) {
        const auto& encoded=g_watchModel.images[i];void* decoder=nullptr;
        ok=create(encoded.data(),encoded.size(),&decoder)==0&&decoder;
        if(ok){const void* info=header(decoder);const int w=width(info),h=height(info);
            ok=w>0&&h>0&&w<=2048&&h<=2048;
            if(ok){std::vector<unsigned char> rgba(static_cast<std::size_t>(w)*h*4);
                ok=format(decoder,1)==0&&decode(decoder,rgba.data(),w*4,rgba.size())==0;
                if(ok){glActiveTexture(GL_TEXTURE0);glGenTextures(1,&g_watchImages[i]);glBindTexture(GL_TEXTURE_2D,g_watchImages[i]);
                    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
                    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);}}
        }
        if(decoder&&destroy)destroy(decoder);
    }
    if(library)dlclose(library);
    if(!ok){for(GLuint image:g_watchImages)if(image)glDeleteTextures(1,&image);g_watchImages.clear();glDeleteProgram(program);LOGE("[watch] embedded image decode failed");return false;}
    for(const auto& mesh:g_watchModel.meshes) {
        WatchGpuMesh gpu;gpu.count=static_cast<GLsizei>(mesh.indices.size());gpu.material=mesh.material;gpu.display=mesh.display;
        glGenVertexArrays(1,&gpu.vao);glBindVertexArray(gpu.vao);
        glGenBuffers(1,&gpu.vbo);glBindBuffer(GL_ARRAY_BUFFER,gpu.vbo);
        glBufferData(GL_ARRAY_BUFFER,mesh.vertices.size()*sizeof(watchmodel::Vertex),mesh.vertices.data(),GL_STATIC_DRAW);
        glGenBuffers(1,&gpu.ebo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,gpu.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,mesh.indices.size()*sizeof(std::uint16_t),mesh.indices.data(),GL_STATIC_DRAW);
        for(int a=0;a<3;++a)glEnableVertexAttribArray(a);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(watchmodel::Vertex),reinterpret_cast<void*>(offsetof(watchmodel::Vertex,position)));
        glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(watchmodel::Vertex),reinterpret_cast<void*>(offsetof(watchmodel::Vertex,normal)));
        glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(watchmodel::Vertex),reinterpret_cast<void*>(offsetof(watchmodel::Vertex,uv)));
        g_watchMeshes.push_back(gpu);
    }
    g_watchProgram=program;
    // Also retains the complete attribution in the distributed native payload.
    LOGI("[watch] GLB ready: %zu meshes; %s",g_watchMeshes.size(),savr_watch_attribution);
    return true;
}
bool BuildWatchModel(unsigned weaponHands,const TwoHandVisualState& twoHand,bool calibrating,handskeleton::Mat& result) {
    const int hand=hud::DeviceHand(hud::WATCH);
    const auto& frame=g_skeletalHands[hand];if(!frame.valid)return false;
    const auto native=[](const std::array<float,3>& v){return XrVector3f{v[0],-v[2],v[1]};};
    const XrVector3f centre=native(g_watchModel.centre),right=native(g_watchModel.right),up=native(g_watchModel.up),normal=native(g_watchModel.normal);
    const auto translation=[](XrVector3f p){auto m=handskeleton::Identity();m[12]=p.x;m[13]=p.y;m[14]=p.z;return m;};
    const auto rotation=[](XrVector3f axis,float degrees){
        const float angle=degrees*.01745329251994f,c=std::cos(angle),s=std::sin(angle),t=1-c;
        const float x=axis.x,y=axis.y,z=axis.z;
        return handskeleton::Mat{t*x*x+c,t*x*y+s*z,t*x*z-s*y,0,t*x*y-s*z,t*y*y+c,t*y*z+s*x,0,
            t*x*z+s*y,t*y*z-s*x,t*z*z+c,0,0,0,0,1};
    };
    const bool support=TwoHandStateUsable(twoHand)&&hand==twoHand.supportHand;
    const bool preview=calibrating&&g_hudPage.load(std::memory_order_relaxed)==4;
    const int slot=preview||((weaponHands&(1u<<hand))&&!support)?hud::WRIST_SLOT_WEAPON:hud::WRIST_SLOT_HAND;
    const auto settings=hud::GetWristSettings(hud::WATCH,slot,-1);
    auto adjust=handskeleton::Multiply(rotation(normal,settings.rollDeg),
        handskeleton::Multiply(rotation(right,settings.pitchDeg),rotation(up,settings.yawDeg)));
    const float scale=settings.scaleTenths*.1f;
    auto size=handskeleton::Identity();size[0]=size[5]=size[10]=scale;
    adjust=handskeleton::Multiply(translation(centre),handskeleton::Multiply(adjust,
        handskeleton::Multiply(size,translation(vscale(centre,-1.f)))));
    if(hud::WatchInside())adjust=handskeleton::Multiply(rotation({1,0,0},180),adjust);
    adjust=handskeleton::Multiply(translation({-settings.alongMm*.001f,-settings.liftMm*.001f,settings.acrossMm*.001f}),adjust);
    result=handskeleton::Multiply(frame.ledge?frame.wrist:frame.model,adjust);return true;
}
float WatchFaceRadius(){
    float radius=0;
    for(const auto& mesh:g_watchModel.meshes)if(mesh.display)for(const auto& v:mesh.vertices){
        const auto p=vsub(WatchNative({v.position[0],v.position[1],v.position[2]}),WatchNative(g_watchModel.centre));
        radius=std::max(radius,std::sqrt(vdot(p,p)));
    }
    return radius;
}
void UpdateWatchFrame(const HandPose hp[2],const TwoHandVisualState& twoHand,
                      const driving::WheelVisualState& wheel,const BallHandLock& ball,unsigned weaponHands,
                      const float pinchAmounts[2],bool trackingValid,std::uint64_t epoch,bool scoped) {
    static std::uint64_t previousTime=0;
    const auto now=MonotonicNowNs();const float dt=previousTime?float(now-previousTime)*1e-9f:0;previousTime=now;
    g_watchEffectTime=float((now/1000000)%100000)*.001f;
    const int wrist=hud::DeviceHand(hud::WATCH),other=1-wrist;
    const bool calibrating=g_hudActive.load(std::memory_order_relaxed);
    g_watchFrameValid=false;
    if(!scoped&&trackingValid&&IsSessionFocused()&&(!IsMenuVisible()||calibrating)&&
       (hud::ShouldRenderWristHud()||calibrating)&&(hud::GetElementSettings(hud::WATCH).enabled||calibrating)){
        WatchGlBindings bindings;
        if(EnsureWatchModel())g_watchFrameValid=BuildWatchModel(weaponHands,twoHand,calibrating,g_watchFrameModel);
    }
    const auto& m=g_watchFrameModel;
    const auto centre=WatchPoint(m,WatchNative(g_watchModel.centre));
    const auto right=vnorm(WatchVector(m,WatchNative(g_watchModel.right)));
    const auto up=vnorm(WatchVector(m,WatchNative(g_watchModel.up)));
    const auto normal=vnorm(WatchVector(m,WatchNative(g_watchModel.normal)));
    const float scale=std::sqrt(vdot(WatchVector(m,{1,0,0}),WatchVector(m,{1,0,0})));
    const float radius=WatchFaceRadius()*scale;
    const bool available=g_watchFrameValid&&!calibrating&&hp[other].valid&&g_skeletalHands[other].valid&&
        !(weaponHands&(1u<<other))&&!(TwoHandStateUsable(twoHand)&&other==twoHand.supportHand)&&
        !hp[other].climbGrip&&!hp[other].ledgeGrip&&!ball.locked[other]&&!wheel.grabbed[other]&&
        !wheel.radioGrabbed[other]&&!wheel.ignitionGrabbed[other]&&pinchAmounts[other]<=0&&hp[other].grip<.35f;
    float radial=1,depth=1;XrVector3f tipWorld{};
    if(available){
        auto& frame=g_skeletalHands[other];
        // FreeHand already selects the authored Point clip with trigger down.
        // Preserve that extended index finger; no contextual pinch override.
        handskeleton::Vec tip{};
        if(g_handSkeleton.IndexTip(frame.palette,tip)){
            tipWorld=WatchPoint(frame.model,WatchNative(tip));
            const auto relative=vsub(tipWorld,centre);
            radial=std::hypot(vdot(relative,right),vdot(relative,up));depth=vdot(relative,normal);
        }
    }
    XrVector3f readingRight{},readingUp{};WatchReadingAxes(m,readingRight,readingUp);
    const auto sliderCentre=vadd(centre,vadd(vscale(normal,watchhud::kProjectionHeight),vscale(readingRight,watchhud::kSliderAcross)));
    const auto sliderTip=vsub(tipWorld,sliderCentre);
    const bool sliderEnabled=available&&g_watchProjection.open&&g_watchProjection.Amount()>.98f&&
        g_watchProjection.epoch==epoch&&g_watchProjection.wrist==wrist;
    const bool ended=g_watchZoomSlider.Update(sliderEnabled,hp[other].trigger,
        vdot(sliderTip,readingRight),vdot(sliderTip,readingUp),vdot(sliderTip,normal),hud::WatchMapZoom());
    if(g_watchZoomSlider.dragging)hud::SetWatchMapZoom(g_watchZoomSlider.zoom,false);
    if(ended)hud::SetWatchMapZoom(hud::WatchMapZoom(),true);
    // A scale drag is still this UI's valid pointing hand, not tracking/context
    // loss. The two contact planes are 20 cm apart, so hits cannot overlap.
    g_watchProjection.Update(g_watchFrameValid&&!calibrating,epoch,wrist,available,
        hp[other].trigger,radial,depth,radius,dt);
    g_watchProjectionNeeded.store(g_watchProjection.Needed(),std::memory_order_relaxed);
    // Reserve only this free hand's trigger near the dial. Keep the reservation
    // until release, so tapping a watch cannot also accelerate/brake a vehicle.
    const bool near=(radial<=radius+.025f&&depth>=-.03f&&depth<=.05f)||
        (sliderEnabled&&std::abs(vdot(sliderTip,readingRight))<.03f&&
         std::abs(vdot(sliderTip,readingUp))<.065f&&std::abs(vdot(sliderTip,normal))<.04f)||g_watchZoomSlider.dragging;
    const bool claimed=available&&(near||(hp[other].trigger>=.30f&&
        now<g_watchTriggerUntil[other].load(std::memory_order_relaxed)));
    g_watchTriggerUntil[wrist].store(0,std::memory_order_relaxed);
    g_watchTriggerUntil[other].store(claimed?now+150000000:0,std::memory_order_relaxed);
}
#include "WatchEffects.inl"
void DrawWatchForEye(const float* mvp,unsigned int mapTexture,unsigned int projectionTexture,float heading,
                     const watchhud::Readout& readout) {
    if(!g_watchFrameValid)return;
    WatchGlBindings bindings;
    const auto& model=g_watchFrameModel;
    const auto centre=WatchNative(g_watchModel.centre);
    // The watch is worn geometry: use the hand's visibility, without HUD
    // distance/gaze fading when the player extends or turns their arm.
    const float opacity=1.f;
    glUseProgram(g_watchProgram);
    glUniformMatrix4fv(g_watchUniforms[WATCH_MVP],1,GL_FALSE,mvp);glUniformMatrix4fv(g_watchUniforms[WATCH_MODEL],1,GL_FALSE,model.data());
    const XrVector3f x{model[0],model[1],model[2]},y{model[4],model[5],model[6]},z{model[8],model[9],model[10]};
    glUniform1i(g_watchUniforms[WATCH_MIRRORFACE],vdot(vcross(x,y),z)<0);
    glUniform1i(g_watchUniforms[WATCH_PROJECTION],0);
    glUniform1i(g_watchUniforms[WATCH_EFFECT],0);
    glUniform1f(g_watchUniforms[WATCH_OPACITY],opacity);glUniform1f(g_watchUniforms[WATCH_HEADING],heading);
    const int time=readout.time;
    glUniform4f(g_watchUniforms[WATCH_WATCH],time<0?-1.f:float(time/60),time<0?-1.f:float(time%60),
        readout.health,readout.armour);
    glUniform1i(g_watchUniforms[WATCH_MONEY],readout.money);
    glUniform1i(g_watchUniforms[WATCH_WANTED],readout.wanted);
    glUniform1i(g_watchUniforms[WATCH_COLOR],0);glUniform1i(g_watchUniforms[WATCH_METAL],1);glUniform1i(g_watchUniforms[WATCH_MAP],2);
    for(int i=0;i<3;++i)glBindSampler(i,0);
    glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,mapTexture);glUniform1i(g_watchUniforms[WATCH_HASMAP],mapTexture!=0);
    for(const auto& mesh:g_watchMeshes)if(opacity>0){
        const auto& material=g_watchModel.materials[mesh.material];
        const auto image=[](int id){return id<0?0u:g_watchImages[id];};
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,image(material.image));
        glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,image(material.metallicImage));
        glUniform1i(g_watchUniforms[WATCH_HASCOLOR],material.image>=0);glUniform1i(g_watchUniforms[WATCH_HASMETAL],material.metallicImage>=0);
        glUniform4fv(g_watchUniforms[WATCH_COLORFACTOR],1,material.color.data());glUniform2f(g_watchUniforms[WATCH_MATERIAL],material.metallic,material.roughness);
        glUniform1i(g_watchUniforms[WATCH_DISPLAY],mesh.display);
        glBindVertexArray(mesh.vao);glDrawElements(GL_TRIANGLES,mesh.count,GL_UNSIGNED_SHORT,nullptr);
    }
    const float amount=g_watchProjection.Amount();
    if(amount>0&&projectionTexture){
        DrawWatchEffect(model,amount,false);
        auto projection=model;
        const float scale=std::sqrt(vdot(x,x));
        const float enlargement=1.f+amount*(watchhud::kProjectionDiameter/(2.f*WatchFaceRadius()*scale)-1.f);
        const auto normal=vnorm(WatchVector(model,WatchNative(g_watchModel.normal)));
        const auto target=vadd(WatchPoint(model,centre),vscale(normal,watchhud::kProjectionHeight*amount));
        for(int c=0;c<3;++c)for(int r=0;r<3;++r)projection[c*4+r]*=enlargement;
        const auto offset=vsub(target,WatchVector(projection,centre));
        projection[12]=offset.x;projection[13]=offset.y;projection[14]=offset.z;
        glUniformMatrix4fv(g_watchUniforms[WATCH_MODEL],1,GL_FALSE,projection.data());
        glUniform1i(g_watchUniforms[WATCH_EFFECT],0);
        glUniform1i(g_watchUniforms[WATCH_PROJECTION],1);
        glUniform1f(g_watchUniforms[WATCH_OPACITY],.5f*amount);
        glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,projectionTexture);
        glUniform1i(g_watchUniforms[WATCH_HASMAP],1);
        // Translucent map tests against hands/world without hiding later
        // transparent overlays behind an opaque depth silhouette.
        glDepthMask(GL_FALSE);
        for(const auto& mesh:g_watchMeshes)if(mesh.display){
            glUniform1i(g_watchUniforms[WATCH_DISPLAY],1);
            glBindVertexArray(mesh.vao);glDrawElements(GL_TRIANGLES,mesh.count,GL_UNSIGNED_SHORT,nullptr);
        }
        DrawWatchEffect(model,amount,true);
        glDepthMask(GL_TRUE);
    }
}

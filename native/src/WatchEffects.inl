// Animated translucent cone and the physical scale rail, in the same depth
// pass as the watch. Geometry uses tracking metres, independent of wrist size.
GLuint g_watchEffectVao=0,g_watchEffectVbo=0;
void DrawWatchEffect(const handskeleton::Mat& model,float amount,bool slider) {
    if(!g_watchEffectVao){
        glGenVertexArrays(1,&g_watchEffectVao);glBindVertexArray(g_watchEffectVao);
        glGenBuffers(1,&g_watchEffectVbo);glBindBuffer(GL_ARRAY_BUFFER,g_watchEffectVbo);
        for(int i=0;i<3;++i)glEnableVertexAttribArray(i);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(watchmodel::Vertex),nullptr);
        glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(watchmodel::Vertex),reinterpret_cast<void*>(offsetof(watchmodel::Vertex,normal)));
        glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(watchmodel::Vertex),reinterpret_cast<void*>(offsetof(watchmodel::Vertex,uv)));
    }
    XrVector3f right{},up{};WatchReadingAxes(model,right,up);
    const auto normal=vnorm(WatchVector(model,WatchNative(g_watchModel.normal)));
    const auto base=WatchPoint(model,WatchNative(g_watchModel.centre));
    const auto top=vadd(base,vscale(normal,watchhud::kProjectionHeight*amount));
    const auto scaleVector=WatchVector(model,{1,0,0});
    const float lower=WatchFaceRadius()*std::sqrt(vdot(scaleVector,scaleVector));
    const float upper=lower+amount*(watchhud::kProjectionDiameter*.5f-lower);
    // Both eyes and both effect passes reuse this storage on the XR thread.
    static std::vector<watchmodel::Vertex> vertices;
    vertices.clear();vertices.reserve(480);
    const auto vertex=[&](XrVector3f p,float u,float v){
        vertices.push_back({{p.x,p.z,-p.y},{0,1,0},{u,v}});
    };
    if(!slider){
        const auto point=[&](float u,bool high){const float theta=u*6.2831853f;
            return vadd(high?top:base,vscale(vadd(vscale(right,std::cos(theta)),vscale(up,std::sin(theta))),high?upper:lower));};
        for(int i=0;i<64;++i){const float a=i/64.f,b=(i+1)/64.f;
            vertex(point(a,false),a,0);vertex(point(b,false),b,0);vertex(point(b,true),b,1);
            vertex(point(a,false),a,0);vertex(point(b,true),b,1);vertex(point(a,true),a,1);
        }
    }else{
        const auto centre=vadd(top,vscale(right,watchhud::kSliderAcross));
        const auto point=[&](float x,float y){return vadd(centre,vadd(vscale(right,x*amount),vscale(up,y*amount)));};
        const auto rect=[&](float x,float y,float hx,float hy){
            vertex(point(x-hx,y-hy),0,0);vertex(point(x+hx,y-hy),0,0);vertex(point(x+hx,y+hy),0,0);
            vertex(point(x-hx,y-hy),0,0);vertex(point(x+hx,y+hy),0,0);vertex(point(x-hx,y+hy),0,0);
        };
        rect(0,0,.0008f,watchhud::kSliderHalfHeight);
        for(int i=0;i<5;++i)rect(0,-watchhud::kSliderHalfHeight+i*watchhud::kSliderHalfHeight*.5f,.003f,.0006f);
        const float y=-watchhud::kSliderHalfHeight+2*watchhud::kSliderHalfHeight*
            ((hud::WatchMapZoom()-watchhud::kZoomMin)/(watchhud::kZoomMax-watchhud::kZoomMin));
        for(int i=0;i<24;++i){const float a=i*6.2831853f/24,b=(i+1)*6.2831853f/24;
            vertex(point(0,y),1,0);vertex(point(.006f*std::cos(a),y+.006f*std::sin(a)),1,0);
            vertex(point(.006f*std::cos(b),y+.006f*std::sin(b)),1,0);
        }
    }
    const auto identity=handskeleton::Identity();
    glUniformMatrix4fv(g_watchUniforms[WATCH_MODEL],1,GL_FALSE,identity.data());
    glUniform1i(g_watchUniforms[WATCH_EFFECT],slider?2:1);
    glUniform1f(g_watchUniforms[WATCH_TIME],g_watchEffectTime);
    glUniform1f(g_watchUniforms[WATCH_OPACITY],amount*(slider?.85f:1.f));
    glBindVertexArray(g_watchEffectVao);glBindBuffer(GL_ARRAY_BUFFER,g_watchEffectVbo);
    glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(watchmodel::Vertex),vertices.data(),GL_STREAM_DRAW);
    glDepthMask(GL_FALSE);glDrawArrays(GL_TRIANGLES,0,static_cast<GLsizei>(vertices.size()));
}

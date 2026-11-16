extern "C" const unsigned char savr_service_radio_mesh[],savr_service_radio_mesh_end[];
GLuint g_serviceProgram{},g_serviceVbo{},g_serviceEbo{};
GLsizei g_serviceIndices{};GLint g_serviceMvp{},g_serviceDock{},g_serviceBody{},g_serviceTint{},g_servicePress{};
bool EnsureServiceRadio(){
    if(g_serviceProgram)return true;static bool failed=false;if(failed)return false;
    serviceradio::Model model;
    if(!model.Load(savr_service_radio_mesh,savr_service_radio_mesh_end-savr_service_radio_mesh)){failed=true;LOGE("[service.radio] invalid mesh");return false;}
    const char* vs=R"GLSL(#version 300 es
layout(location=0) in vec3 p;layout(location=1) in vec3 n;
layout(location=2) in vec3 color;layout(location=3) in float role;
uniform mat4 mvp;uniform vec3 dock[4],body[4],tint;uniform float press;
out vec3 shade;
void main(){vec3 pos=p;if(role>2.5)pos.x+=.0015*press;
 vec3 c=role<.5?dock[0]:body[0],r=role<.5?dock[1]:body[1],f=role<.5?dock[2]:body[2],u=role<.5?dock[3]:body[3];
 vec3 normal=normalize(r*n.x+f*n.y+u*n.z);
 float light=.42+.58*max(0.,dot(normal,normalize(vec3(-.4,.7,.5))));
 shade=pow(max(role>1.5&&role<2.5?tint:color,vec3(0.)),vec3(1./2.2))*light;
 gl_Position=mvp*vec4(c+r*pos.x+f*pos.y+u*pos.z,1.);})GLSL";
    const char* fs=R"GLSL(#version 300 es
precision mediump float;in vec3 shade;out vec4 outColor;void main(){outColor=vec4(shade,1.);})GLSL";
    GLuint v=CompileHandShader(GL_VERTEX_SHADER,vs),f=CompileHandShader(GL_FRAGMENT_SHADER,fs);
    GLuint p=glCreateProgram();glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);glDeleteShader(v);glDeleteShader(f);
    GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);if(!ok){glDeleteProgram(p);failed=true;LOGE("[service.radio] shader link failed");return false;}
    g_serviceProgram=p;g_serviceMvp=glGetUniformLocation(p,"mvp");g_serviceDock=glGetUniformLocation(p,"dock[0]");
    g_serviceBody=glGetUniformLocation(p,"body[0]");g_serviceTint=glGetUniformLocation(p,"tint");g_servicePress=glGetUniformLocation(p,"press");
    glGenBuffers(1,&g_serviceVbo);glGenBuffers(1,&g_serviceEbo);
    glBindBuffer(GL_ARRAY_BUFFER,g_serviceVbo);glBufferData(GL_ARRAY_BUFFER,model.vertices.size()*40,model.vertices.data(),GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,g_serviceEbo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,model.indices.size()*2,model.indices.data(),GL_STATIC_DRAW);
    g_serviceIndices=static_cast<GLsizei>(model.indices.size());return true;
}
void DrawServiceRadio(const float* mvp,const driving::WheelVisualState& w,const HandPose hands[2]){
    if(!w.serviceVisible||!EnsureServiceRadio())return;
    const auto vec=[](const float* v){return serviceradio::Vec{v[0],v[1],v[2]};};
    const serviceradio::Frame dock{vec(w.serviceCenter),vec(w.serviceRight),vec(w.serviceForward),vec(w.serviceUp)};
    auto body=dock;
    if(w.serviceOwner>=0&&hands[w.serviceOwner].valid)body=serviceradio::HeldFrame(hands[w.serviceOwner],w.serviceOwner);
    else if(w.serviceReturnWeight>0&&w.serviceReturnHand>=0&&hands[w.serviceReturnHand].valid)
        body=serviceradio::Blend(dock,serviceradio::HeldFrame(hands[w.serviceReturnHand],w.serviceReturnHand),w.serviceReturnWeight);
    glUseProgram(g_serviceProgram);glUniformMatrix4fv(g_serviceMvp,1,GL_FALSE,mvp);
    static_assert(sizeof(serviceradio::Frame)==48);
    glUniform3fv(g_serviceDock,4,&dock.center.x);glUniform3fv(g_serviceBody,4,&body.center.x);
    if(w.serviceKind==2)glUniform3f(g_serviceTint,.78f,.80f,.79f);
    else if(w.serviceKind==3)glUniform3f(g_serviceTint,.65f,.022f,.018f);
    else glUniform3f(g_serviceTint,.018f,.12f,.64f);
    glUniform1f(g_servicePress,w.serviceOwner>=0?w.servicePress:0.f);
    glBindBuffer(GL_ARRAY_BUFFER,g_serviceVbo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,g_serviceEbo);
    for(int i=0;i<4;++i){glEnableVertexAttribArray(i);glVertexAttribPointer(i,i==3?1:3,GL_FLOAT,GL_FALSE,40,reinterpret_cast<void*>(std::uintptr_t(i*12)));}
    glDrawElements(GL_TRIANGLES,g_serviceIndices,GL_UNSIGNED_SHORT,nullptr);
    for(int i=0;i<4;++i)glDisableVertexAttribArray(i);
    glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);glUseProgram(0);
}

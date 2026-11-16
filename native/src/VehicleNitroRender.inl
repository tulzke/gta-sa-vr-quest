extern "C" const unsigned char savr_nitro_mesh[],savr_nitro_mesh_end[],savr_nitro_alpha[],savr_nitro_alpha_end[];
GLuint g_nitroProgram{},g_nitroVbo{},g_nitroEbo{},g_nitroTexture{};
GLsizei g_nitroIndexCount{};
GLint g_nitroMvp{},g_nitroCenter{},g_nitroRight{},g_nitroForward{},g_nitroUp{},g_nitroTurn{},g_nitroState{},g_nitroAtlas{};
bool EnsureNitroModel(){
    if(g_nitroProgram)return true;static bool failed=false;if(failed)return false;
    nitro::Model model;
    if(!model.Load(savr_nitro_mesh,static_cast<std::size_t>(savr_nitro_mesh_end-savr_nitro_mesh))||savr_nitro_alpha_end-savr_nitro_alpha!=1024*512){failed=true;LOGE("[nitro] invalid embedded model");return false;}
    const char* vs=R"GLSL(#version 300 es
layout(location=0) in vec3 p;layout(location=1) in vec3 n;
layout(location=2) in vec3 color;layout(location=3) in vec2 uv;layout(location=4) in float role;
uniform mat4 mvp;uniform vec3 center,axisRight,axisForward,axisUp;
uniform vec3 turn;uniform highp vec4 state;
out mediump vec3 shade;out mediump vec2 texcoord;out mediump float visible;
vec3 rotateX(vec3 v){return vec3(v.x,v.y*turn.x-v.z*turn.y,v.y*turn.y+v.z*turn.x);}
void main(){vec3 pos=p,normal=n,c=color;texcoord=uv;visible=1.;
 if(role>.5&&role<1.5){pos=rotateX(p)+vec3(0.,.018,.048);normal=rotateX(n);}
 if(role>=10.&&role<20.)c=role-10.<ceil(clamp(state.x,0.,1.)*10.)?vec3(.015,.36,.95):vec3(.045,.060,.075);
 if(role>=20.&&role<23.){int value=int(ceil(clamp(state.x,0.,1.)*100.));int place=int(role)-20;int d=place==0?value/100:(place==1?(value/10)%10:value%10);
   if(place==0&&value<100)visible=0.;int cell=d+4;vec2 origin=vec2(float(cell%8)*128.+32.,float(cell/8)*128.+20.);texcoord=(origin+uv*vec2(64.,88.))/vec2(1024.,512.);}
 if(role>29.5)c=state.y>.5?vec3(1.,.42,.025):vec3(.045,.038,.026);
 float light=.42+.58*max(0.,dot(normalize(normal),normalize(vec3(-.4,-.5,.8))));
 shade=pow(max(c,vec3(0.)),vec3(1./2.2))*light;
 if(state.z>.5)shade=mix(shade,vec3(1.,.85,.12),.20);
 gl_Position=mvp*vec4(center+(axisRight*pos.x+axisForward*pos.y+axisUp*pos.z)*turn.z,1.);
})GLSL";
    const char* fs=R"GLSL(#version 300 es
precision mediump float;in vec3 shade;in vec2 texcoord;in float visible;
uniform sampler2D atlas;uniform highp vec4 state;out vec4 outColor;
void main(){float a=visible*(state.z>.5?.45:1.);if(texcoord.x>=0.)a*=texture(atlas,texcoord).r;
 if(a<.01)discard;outColor=vec4(shade,a);})GLSL";
    const GLuint v=CompileHandShader(GL_VERTEX_SHADER,vs),f=CompileHandShader(GL_FRAGMENT_SHADER,fs);
    GLuint program=glCreateProgram();glAttachShader(program,v);glAttachShader(program,f);glLinkProgram(program);glDeleteShader(v);glDeleteShader(f);
    GLint ok=0;glGetProgramiv(program,GL_LINK_STATUS,&ok);if(!ok){glDeleteProgram(program);failed=true;LOGE("[nitro] shader link failed");return false;}
    g_nitroProgram=program;
    g_nitroMvp=glGetUniformLocation(program,"mvp");g_nitroCenter=glGetUniformLocation(program,"center");
    g_nitroRight=glGetUniformLocation(program,"axisRight");g_nitroForward=glGetUniformLocation(program,"axisForward");g_nitroUp=glGetUniformLocation(program,"axisUp");
    g_nitroTurn=glGetUniformLocation(program,"turn");g_nitroState=glGetUniformLocation(program,"state");g_nitroAtlas=glGetUniformLocation(program,"atlas");
    glGenBuffers(1,&g_nitroVbo);glGenBuffers(1,&g_nitroEbo);glBindBuffer(GL_ARRAY_BUFFER,g_nitroVbo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,g_nitroEbo);
    glBufferData(GL_ARRAY_BUFFER,model.vertices.size()*sizeof(nitro::Vertex),model.vertices.data(),GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,model.indices.size()*2,model.indices.data(),GL_STATIC_DRAW);g_nitroIndexCount=static_cast<GLsizei>(model.indices.size());
    glGenTextures(1,&g_nitroTexture);glBindTexture(GL_TEXTURE_2D,g_nitroTexture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_R8,1024,512,0,GL_RED,GL_UNSIGNED_BYTE,savr_nitro_alpha);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    return true;
}
void DrawNitroLever(const float* mvp,const driving::WheelVisualState& visual){
    if(!visual.nitroVisible)return;
    GLint active{},texture{},srcRgb{},dstRgb{},srcAlpha{},dstAlpha{};GLboolean depthWrite{};
    const GLboolean blend=glIsEnabled(GL_BLEND);
    glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);
    glGetIntegerv(GL_BLEND_SRC_RGB,&srcRgb);glGetIntegerv(GL_BLEND_DST_RGB,&dstRgb);glGetIntegerv(GL_BLEND_SRC_ALPHA,&srcAlpha);glGetIntegerv(GL_BLEND_DST_ALPHA,&dstAlpha);glGetBooleanv(GL_DEPTH_WRITEMASK,&depthWrite);
    if(EnsureNitroModel()){
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);if(visual.nitroGhost)glDepthMask(GL_FALSE);
        glUseProgram(g_nitroProgram);glUniformMatrix4fv(g_nitroMvp,1,GL_FALSE,mvp);
        glUniform3fv(g_nitroCenter,1,visual.nitroCenter);glUniform3fv(g_nitroRight,1,visual.nitroRight);glUniform3fv(g_nitroForward,1,visual.nitroForward);glUniform3fv(g_nitroUp,1,visual.nitroUp);
        glUniform3f(g_nitroTurn,std::cos(visual.nitroAngle),std::sin(visual.nitroAngle),visual.nitroScale);
        glUniform4f(g_nitroState,visual.nitroReserve,visual.nitroFlow?1.f:0.f,visual.nitroGhost?1.f:0.f,0.f);
        glUniform1i(g_nitroAtlas,0);glBindTexture(GL_TEXTURE_2D,g_nitroTexture);
        glBindBuffer(GL_ARRAY_BUFFER,g_nitroVbo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,g_nitroEbo);
        for(int a=0;a<5;++a)glEnableVertexAttribArray(a);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,48,reinterpret_cast<void*>(0));glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,48,reinterpret_cast<void*>(12));
        glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,48,reinterpret_cast<void*>(24));glVertexAttribPointer(3,2,GL_FLOAT,GL_FALSE,48,reinterpret_cast<void*>(36));glVertexAttribPointer(4,1,GL_FLOAT,GL_FALSE,48,reinterpret_cast<void*>(44));
        glDrawElements(GL_TRIANGLES,g_nitroIndexCount,GL_UNSIGNED_SHORT,nullptr);
        for(int a=0;a<5;++a)glDisableVertexAttribArray(a);
        glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);glUseProgram(0);
    }
    glDepthMask(depthWrite);glBlendFuncSeparate(srcRgb,dstRgb,srcAlpha,dstAlpha);if(!blend)glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D,texture);glActiveTexture(active);
}

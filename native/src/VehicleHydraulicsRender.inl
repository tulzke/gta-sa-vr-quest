extern "C" const unsigned char savr_hydraulics_mesh[],savr_hydraulics_mesh_end[],savr_hydraulics_alpha[],savr_hydraulics_alpha_end[];
GLuint g_hydroProgram{},g_hydroVbo{},g_hydroEbo{},g_hydroTexture{};
GLsizei g_hydroIndexCount{};
hydrocontrol::Model g_hydroModel;
std::vector<hydrocontrol::Vertex> g_hydroDeformed;
float g_hydroLastX=999,g_hydroLastY=999,g_hydroLastPress=999;
bool EnsureHydraulicsModel(){
    if(g_hydroProgram)return true;static bool failed=false;if(failed)return false;
    if(!g_hydroModel.Load(savr_hydraulics_mesh,static_cast<std::size_t>(savr_hydraulics_mesh_end-savr_hydraulics_mesh))||
       savr_hydraulics_alpha_end-savr_hydraulics_alpha!=1024*256){failed=true;LOGE("[hydraulics] invalid embedded model");return false;}
    const char* vs=R"GLSL(#version 300 es
layout(location=0) in vec3 p;layout(location=1) in vec3 n;
layout(location=2) in vec3 color;layout(location=3) in vec2 uv;layout(location=4) in float role;
uniform mat4 mvp;uniform vec3 center,axisRight,axisForward,axisUp;uniform float scale;
uniform highp vec4 state;
out mediump vec3 shade;out mediump vec2 texcoord;out mediump float visible;
void main(){vec3 c=color;texcoord=uv;visible=1.;
 if(role>3.5&&role<5.5){bool on=role<4.5?state.x<.5:state.x>.5;c=on?vec3(.95,.38,.035):vec3(.055,.045,.03);}
 if(role>=10.){int slot=int(role)-9;if(state.y>.5&&(slot%2)==0)visible=0.;c=int(state.z)==slot?vec3(1.,.5,.07):vec3(.30,.20,.10);}
 float light=.42+.58*max(0.,dot(normalize(n),normalize(vec3(-.4,-.5,.8))));
 shade=pow(max(c,vec3(0.)),vec3(1./2.2))*light;
 if(state.w>.5)shade=mix(shade,vec3(1.,.85,.12),.20);
 gl_Position=mvp*vec4(center+(axisRight*p.x+axisForward*p.y+axisUp*p.z)*scale,1.);
})GLSL";
    const char* fs=R"GLSL(#version 300 es
precision mediump float;in vec3 shade;in vec2 texcoord;in float visible;
uniform sampler2D atlas;uniform highp vec4 state;out vec4 outColor;
void main(){float a=visible*(state.w>.5?.45:1.);if(texcoord.x>=0.)a*=texture(atlas,texcoord).r;
 if(a<.01)discard;outColor=vec4(shade,a);})GLSL";
    const GLuint v=CompileHandShader(GL_VERTEX_SHADER,vs),f=CompileHandShader(GL_FRAGMENT_SHADER,fs);
    GLuint program=glCreateProgram();glAttachShader(program,v);glAttachShader(program,f);glLinkProgram(program);glDeleteShader(v);glDeleteShader(f);
    GLint ok=0;glGetProgramiv(program,GL_LINK_STATUS,&ok);if(!ok){glDeleteProgram(program);failed=true;LOGE("[hydraulics] shader link failed");return false;}
    g_hydroProgram=program;g_hydroDeformed=g_hydroModel.vertices;
    glGenBuffers(1,&g_hydroVbo);glGenBuffers(1,&g_hydroEbo);glBindBuffer(GL_ARRAY_BUFFER,g_hydroVbo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,g_hydroEbo);
    glBufferData(GL_ARRAY_BUFFER,g_hydroDeformed.size()*sizeof(hydrocontrol::Vertex),g_hydroDeformed.data(),GL_DYNAMIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,g_hydroModel.indices.size()*2,g_hydroModel.indices.data(),GL_STATIC_DRAW);
    g_hydroIndexCount=static_cast<GLsizei>(g_hydroModel.indices.size());
    glGenTextures(1,&g_hydroTexture);glBindTexture(GL_TEXTURE_2D,g_hydroTexture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_R8,1024,256,0,GL_RED,GL_UNSIGNED_BYTE,savr_hydraulics_alpha);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    return true;
}
void DrawHydraulicsJoystick(const float* mvp,const driving::WheelVisualState& v){
    if(!v.hydroVisible)return;
    GLint active{},texture{},srcRgb{},dstRgb{},srcAlpha{},dstAlpha{};GLboolean depthWrite{};
    const GLboolean blend=glIsEnabled(GL_BLEND);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);
    glGetIntegerv(GL_BLEND_SRC_RGB,&srcRgb);glGetIntegerv(GL_BLEND_DST_RGB,&dstRgb);glGetIntegerv(GL_BLEND_SRC_ALPHA,&srcAlpha);glGetIntegerv(GL_BLEND_DST_ALPHA,&dstAlpha);glGetBooleanv(GL_DEPTH_WRITEMASK,&depthWrite);
    if(EnsureHydraulicsModel()){
        glBindBuffer(GL_ARRAY_BUFFER,g_hydroVbo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,g_hydroEbo);
        if(v.hydroX!=g_hydroLastX||v.hydroY!=g_hydroLastY||v.hydroPress!=g_hydroLastPress){
            for(std::size_t i=0;i<g_hydroModel.vertices.size();++i)g_hydroDeformed[i]=hydrocontrol::Model::Deform(g_hydroModel.vertices[i],v.hydroX,v.hydroY,v.hydroPress);
            glBufferSubData(GL_ARRAY_BUFFER,0,g_hydroDeformed.size()*sizeof(hydrocontrol::Vertex),g_hydroDeformed.data());
            g_hydroLastX=v.hydroX;g_hydroLastY=v.hydroY;g_hydroLastPress=v.hydroPress;
        }
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);if(v.hydroGhost)glDepthMask(GL_FALSE);
        glUseProgram(g_hydroProgram);
        glUniformMatrix4fv(glGetUniformLocation(g_hydroProgram,"mvp"),1,GL_FALSE,mvp);
        glUniform3fv(glGetUniformLocation(g_hydroProgram,"center"),1,v.hydroCenter);
        glUniform3fv(glGetUniformLocation(g_hydroProgram,"axisRight"),1,v.hydroRight);
        glUniform3fv(glGetUniformLocation(g_hydroProgram,"axisForward"),1,v.hydroForward);
        glUniform3fv(glGetUniformLocation(g_hydroProgram,"axisUp"),1,v.hydroUp);
        glUniform1f(glGetUniformLocation(g_hydroProgram,"scale"),v.hydroScale);
        glUniform4f(glGetUniformLocation(g_hydroProgram,"state"),v.hydroHigh?1.f:0.f,v.hydroFour?1.f:0.f,static_cast<float>(v.hydroSlot),v.hydroGhost?1.f:0.f);
        glUniform1i(glGetUniformLocation(g_hydroProgram,"atlas"),0);glBindTexture(GL_TEXTURE_2D,g_hydroTexture);
        for(int a=0;a<5;++a)glEnableVertexAttribArray(a);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,56,reinterpret_cast<void*>(0));glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,56,reinterpret_cast<void*>(12));
        glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,56,reinterpret_cast<void*>(24));glVertexAttribPointer(3,2,GL_FLOAT,GL_FALSE,56,reinterpret_cast<void*>(36));glVertexAttribPointer(4,1,GL_FLOAT,GL_FALSE,56,reinterpret_cast<void*>(44));
        glDrawElements(GL_TRIANGLES,g_hydroIndexCount,GL_UNSIGNED_SHORT,nullptr);
        for(int a=0;a<5;++a)glDisableVertexAttribArray(a);
        glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);glUseProgram(0);
    }
    glDepthMask(depthWrite);glBlendFuncSeparate(srcRgb,dstRgb,srcAlpha,dstAlpha);if(!blend)glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D,texture);glActiveTexture(active);
}

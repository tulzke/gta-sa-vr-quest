// Native SCM sprites are authored in mobile pixel space and do not survive
// the VR HUD crop. This small stereo strip shares the native beat timeline.
GLuint g_rhythmProgram{},g_rhythmVbo{};
GLint g_rhythmMvp{-1},g_rhythmOrigin{-1},g_rhythmRight{-1},g_rhythmUp{-1};
bool EnsureRhythmHud(){
    if(g_rhythmProgram)return true;
    const char* vs=R"GLSL(#version 300 es
layout(location=0) in vec2 aPosition;
layout(location=1) in vec4 aColour;
uniform mat4 uMvp; uniform vec3 uOrigin,uRight,uUp;
out vec4 colour;
void main(){ colour=aColour; gl_Position=uMvp*vec4(uOrigin+uRight*aPosition.x+uUp*aPosition.y,1.0); }
)GLSL";
    const char* fs=R"GLSL(#version 300 es
precision mediump float;
in vec4 colour; out vec4 fragment;
void main(){ fragment=colour; }
)GLSL";
    const GLuint v=CompileHandShader(GL_VERTEX_SHADER,vs),f=CompileHandShader(GL_FRAGMENT_SHADER,fs);
    if(!v||!f){if(v)glDeleteShader(v);if(f)glDeleteShader(f);return false;}
    const GLuint program=glCreateProgram();glAttachShader(program,v);glAttachShader(program,f);glLinkProgram(program);
    glDeleteShader(v);glDeleteShader(f);GLint ok{};glGetProgramiv(program,GL_LINK_STATUS,&ok);
    if(!ok){glDeleteProgram(program);return false;}
    g_rhythmProgram=program;glGenBuffers(1,&g_rhythmVbo);
    g_rhythmMvp=glGetUniformLocation(program,"uMvp");g_rhythmOrigin=glGetUniformLocation(program,"uOrigin");
    g_rhythmRight=glGetUniformLocation(program,"uRight");g_rhythmUp=glGetUniformLocation(program,"uUp");
    return true;
}
void DrawRhythmHud(const float* mvp,const XrPosef& head){
    if(!IsSessionFocused()||IsMenuVisible())return;
    rhythm::Frame frame;std::uint64_t time{};
    {std::lock_guard<std::mutex> lock(g_rhythmHudMutex);frame=g_rhythmHud;time=g_rhythmHudTimeNs;}
    if(!frame.active)return;
    const auto now=MonotonicNowNs();if(now<time)return;
    const auto mesh=rhythm::Build(frame,static_cast<float>(now-time)/1.e6f);
    if(!mesh.count||!EnsureRhythmHud())return;
    const auto forward=QuatRotate(head.orientation,{0,0,-1}),right=QuatRotate(head.orientation,{1,0,0}),up=QuatRotate(head.orientation,{0,1,0});
    const auto origin=vadd(vadd(head.position,vscale(forward,1.8f)),vscale(up,-.36f));
    const auto depth=glIsEnabled(GL_DEPTH_TEST),blend=glIsEnabled(GL_BLEND),cull=glIsEnabled(GL_CULL_FACE);
    GLboolean depthWrite{};glGetBooleanv(GL_DEPTH_WRITEMASK,&depthWrite);
    GLint srcRgb{},dstRgb{},srcAlpha{},dstAlpha{};
    glGetIntegerv(GL_BLEND_SRC_RGB,&srcRgb);glGetIntegerv(GL_BLEND_DST_RGB,&dstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA,&srcAlpha);glGetIntegerv(GL_BLEND_DST_ALPHA,&dstAlpha);
    glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(g_rhythmProgram);glUniformMatrix4fv(g_rhythmMvp,1,GL_FALSE,mvp);
    glUniform3f(g_rhythmOrigin,origin.x,origin.y,origin.z);glUniform3f(g_rhythmRight,right.x,right.y,right.z);glUniform3f(g_rhythmUp,up.x,up.y,up.z);
    glBindBuffer(GL_ARRAY_BUFFER,g_rhythmVbo);glBufferData(GL_ARRAY_BUFFER,mesh.count*sizeof(rhythm::Vertex),mesh.vertices.data(),GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);glEnableVertexAttribArray(1);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(rhythm::Vertex),nullptr);
    glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,sizeof(rhythm::Vertex),reinterpret_cast<const void*>(2*sizeof(float)));
    glDrawArrays(GL_TRIANGLES,0,mesh.count);
    glDisableVertexAttribArray(0);glDisableVertexAttribArray(1);glBindBuffer(GL_ARRAY_BUFFER,0);glUseProgram(0);
    glBlendFuncSeparate(srcRgb,dstRgb,srcAlpha,dstAlpha);glDepthMask(depthWrite);
    if(depth)glEnable(GL_DEPTH_TEST);if(cull)glEnable(GL_CULL_FACE);if(!blend)glDisable(GL_BLEND);
}

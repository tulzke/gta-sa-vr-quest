// Included in Xr.cpp; the immutable local mesh is shared by both eye draws.
GLuint g_radioLabelProgram{},g_radioLabelVbo{},g_radioLabelEbo{};
GLint g_radioLabelMvp{},g_radioLabelCenter{},g_radioLabelRight{},g_radioLabelUp{},g_radioLabelOff{};
GLsizei g_radioLabelIndexCount{};
int g_radioLabelStation=radio::kUnknownStation;
bool EnsureRadioLabelProgram() {
    if(g_radioLabelProgram)return true;
    static bool failed=false;if(failed)return false;
    const char* vs=R"GLSL(#version 300 es
layout(location=0) in vec3 vertex;
uniform mat4 mvp;uniform vec3 center,axisRight,axisUp;
out float ink;
void main(){ink=vertex.z;gl_Position=mvp*vec4(center+axisRight*vertex.x+axisUp*vertex.y,1.);}
)GLSL";
    const char* fs=R"GLSL(#version 300 es
precision mediump float;
in float ink;uniform float radioOff;out vec4 color;
void main(){vec3 text=mix(vec3(.949,.949,.925),vec3(.761,.769,.776),radioOff);
 color=vec4(mix(vec3(.090,.098,.110),text,ink),mix(.75,1.,ink));}
)GLSL";
    const GLuint v=CompileHandShader(GL_VERTEX_SHADER,vs),f=CompileHandShader(GL_FRAGMENT_SHADER,fs);
    GLuint program=glCreateProgram();glAttachShader(program,v);glAttachShader(program,f);
    glLinkProgram(program);glDeleteShader(v);glDeleteShader(f);
    GLint ok=0;glGetProgramiv(program,GL_LINK_STATUS,&ok);
    if(!ok){glDeleteProgram(program);failed=true;LOGE("[radio.label] shader link failed");return false;}
    g_radioLabelProgram=program;
    g_radioLabelMvp=glGetUniformLocation(program,"mvp");g_radioLabelCenter=glGetUniformLocation(program,"center");
    g_radioLabelRight=glGetUniformLocation(program,"axisRight");g_radioLabelUp=glGetUniformLocation(program,"axisUp");
    g_radioLabelOff=glGetUniformLocation(program,"radioOff");
    glGenBuffers(1,&g_radioLabelVbo);glGenBuffers(1,&g_radioLabelEbo);return true;
}
void DrawRadioLabel(const float* mvp,const driving::WheelVisualState& w) {
    if(!w.radioVisible || !*radio::StationName(w.radioStation))return;
    if(!EnsureRadioLabelProgram())return;
    glBindBuffer(GL_ARRAY_BUFFER,g_radioLabelVbo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,g_radioLabelEbo);
    if(g_radioLabelStation!=w.radioStation) {
        const auto mesh=radio::BuildLabelMesh(w.radioStation);
        glBufferData(GL_ARRAY_BUFFER,mesh.vertices.size()*sizeof(radio::LabelVertex),mesh.vertices.data(),GL_STATIC_DRAW);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,mesh.indices.size()*sizeof(std::uint16_t),mesh.indices.data(),GL_STATIC_DRAW);
        g_radioLabelIndexCount=static_cast<GLsizei>(mesh.indices.size());g_radioLabelStation=w.radioStation;
    }
    const XrVector3f right=vnorm({w.radioRight[0],w.radioRight[1],w.radioRight[2]});
    // Default bike knobs lie in the horizontal handlebar frame. Lift the label
    // along vehicle-up and stand it toward the rider; custom poses use their up.
    const float* upField=w.radioLabelBikeFrame?w.radioNormal:w.radioUp;
    const XrVector3f up=vscale(vnorm({upField[0],upField[1],upField[2]}),w.radioLabelBikeFrame?-1.f:1.f);
    const XrVector3f normal=vnorm(vcross(right,up));
    const auto center=vadd({w.radioCenter[0],w.radioCenter[1],w.radioCenter[2]},
        vadd(vscale(up,radio::kLabelLift),vscale(normal,.002f)));
    GLint srcRgb{},dstRgb{},srcAlpha{},dstAlpha{};GLboolean write{};
    const bool blend=glIsEnabled(GL_BLEND),cull=glIsEnabled(GL_CULL_FACE);
    glGetIntegerv(GL_BLEND_SRC_RGB,&srcRgb);glGetIntegerv(GL_BLEND_DST_RGB,&dstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA,&srcAlpha);glGetIntegerv(GL_BLEND_DST_ALPHA,&dstAlpha);
    glGetBooleanv(GL_DEPTH_WRITEMASK,&write);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);
    glUseProgram(g_radioLabelProgram);glUniformMatrix4fv(g_radioLabelMvp,1,GL_FALSE,mvp);
    glUniform3f(g_radioLabelCenter,center.x,center.y,center.z);
    glUniform3f(g_radioLabelRight,right.x,right.y,right.z);glUniform3f(g_radioLabelUp,up.x,up.y,up.z);
    glUniform1f(g_radioLabelOff,w.radioStation==radio::kOffStation?1.f:0.f);
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(radio::LabelVertex),nullptr);
    glDrawElements(GL_TRIANGLES,g_radioLabelIndexCount,GL_UNSIGNED_SHORT,nullptr);
    glDisableVertexAttribArray(0);glUseProgram(0);glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
    glDepthMask(write);glBlendFuncSeparate(srcRgb,dstRgb,srcAlpha,dstAlpha);
    if(!blend)glDisable(GL_BLEND);if(cull)glEnable(GL_CULL_FACE);
}

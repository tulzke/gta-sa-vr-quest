// Included in Xr.cpp's renderer namespace. One immutable VBO/EBO for both
// groups; the shader rotates only pivot-local moving vertices.
extern "C" const unsigned char savr_lift_lever_mesh[],savr_lift_lever_mesh_end[];
GLuint g_liftProgram{},g_liftVbo{},g_liftEbo{};
GLsizei g_liftIndexCount{};
GLint g_liftMvp{},g_liftCenter{},g_liftRight{},g_liftForward{},g_liftUp{},g_liftTurn{},g_liftHighlight{};
bool EnsureLiftLeverModel() {
    if(g_liftProgram)return true;
    static bool failed=false;if(failed)return false;
    liftlever::Model model;
    if(!model.Load(savr_lift_lever_mesh,static_cast<std::size_t>(savr_lift_lever_mesh_end-savr_lift_lever_mesh))){
        failed=true;LOGE("[lift] invalid embedded mesh");return false;
    }
    const char* vs=R"GLSL(#version 300 es
layout(location=0) in vec3 p;
layout(location=1) in vec3 n;
layout(location=2) in vec3 color;
layout(location=3) in float moving;
uniform mat4 mvp;
uniform vec3 center,axisRight,axisForward,axisUp;
uniform vec2 turn;
uniform float highlight;
out vec3 shade;
vec3 rotateX(vec3 v){return vec3(v.x,v.y*turn.x-v.z*turn.y,v.y*turn.y+v.z*turn.x);}
void main(){
  vec3 pos=p,normal=n;
  if(moving>.5){pos=rotateX(pos)+vec3(0.,0.,.05);normal=rotateX(normal);}
  gl_Position=mvp*vec4(center+axisRight*pos.x+axisForward*pos.y+axisUp*pos.z,1.);
  float light=.38+.62*max(0.,dot(normalize(normal),normalize(vec3(-.4,-.5,.8))));
  shade=pow(max(color,vec3(0.)),vec3(1./2.2))*light+vec3(highlight);
})GLSL";
    const char* fs=R"GLSL(#version 300 es
precision mediump float;
in vec3 shade;
out vec4 outColor;
void main(){outColor=vec4(shade,1.);})GLSL";
    const GLuint vert=CompileHandShader(GL_VERTEX_SHADER,vs),frag=CompileHandShader(GL_FRAGMENT_SHADER,fs);
    const GLuint program=glCreateProgram();glAttachShader(program,vert);glAttachShader(program,frag);
    glLinkProgram(program);glDeleteShader(vert);glDeleteShader(frag);
    GLint ok=0;glGetProgramiv(program,GL_LINK_STATUS,&ok);
    if(!ok){glDeleteProgram(program);failed=true;LOGE("[lift] shader link failed");return false;}
    g_liftProgram=program;
    g_liftMvp=glGetUniformLocation(program,"mvp");g_liftCenter=glGetUniformLocation(program,"center");
    g_liftRight=glGetUniformLocation(program,"axisRight");g_liftForward=glGetUniformLocation(program,"axisForward");
    g_liftUp=glGetUniformLocation(program,"axisUp");g_liftTurn=glGetUniformLocation(program,"turn");
    g_liftHighlight=glGetUniformLocation(program,"highlight");
    glGenBuffers(1,&g_liftVbo);glGenBuffers(1,&g_liftEbo);
    glBindBuffer(GL_ARRAY_BUFFER,g_liftVbo);
    glBufferData(GL_ARRAY_BUFFER,model.vertices.size()*sizeof(liftlever::Vertex),model.vertices.data(),GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,g_liftEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,model.indices.size()*2,model.indices.data(),GL_STATIC_DRAW);
    g_liftIndexCount=static_cast<GLsizei>(model.indices.size());
    glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
    LOGI("[lift] mesh ready vertices=%zu triangles=%zu",model.vertices.size(),model.indices.size()/3);
    return true;
}
void DrawLiftLever(const float* mvp,const driving::WheelVisualState& visual) {
    if(!visual.liftVisible||!EnsureLiftLeverModel())return;
    glUseProgram(g_liftProgram);glUniformMatrix4fv(g_liftMvp,1,GL_FALSE,mvp);
    glUniform3fv(g_liftCenter,1,visual.liftCenter);glUniform3fv(g_liftRight,1,visual.liftRight);
    glUniform3fv(g_liftForward,1,visual.liftForward);glUniform3fv(g_liftUp,1,visual.liftUp);
    glUniform2f(g_liftTurn,std::cos(visual.liftAngle),std::sin(visual.liftAngle));
    glUniform1f(g_liftHighlight,visual.interactionTarget[0]==5||visual.interactionTarget[1]==5?.055f:0.f);
    glBindBuffer(GL_ARRAY_BUFFER,g_liftVbo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,g_liftEbo);
    for(int a=0;a<4;++a)glEnableVertexAttribArray(a);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(liftlever::Vertex),reinterpret_cast<void*>(0));
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(liftlever::Vertex),reinterpret_cast<void*>(12));
    glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(liftlever::Vertex),reinterpret_cast<void*>(24));
    glVertexAttribPointer(3,1,GL_FLOAT,GL_FALSE,sizeof(liftlever::Vertex),reinterpret_cast<void*>(36));
    glDrawElements(GL_TRIANGLES,g_liftIndexCount,GL_UNSIGNED_SHORT,nullptr);
    for(int a=0;a<4;++a)glDisableVertexAttribArray(a);
    glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);glUseProgram(0);
}

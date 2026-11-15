// Present-thread untextured copy. Model memory is immutable and belongs to the
// same stereo-ring slot as the pose, so neither eye reads live engine objects.
struct ReloadHintGpu {
    GLuint vertices{},indices{};
    std::shared_ptr<const reloadhint::Model> uploaded;
};
ReloadHintGpu g_reloadHintGpu[4];
void DrawReloadHintsForEye(const float* viewProjection,const reloadhint::VisualFrame& frame,bool clearDepth) {
    if (!frame.Visible()) return;
    EnsureMarkerProgram();if (!g_markerProg) return;
    GLint oldProgram{},oldArray{},oldElements{},oldDepthFunc{},oldCullMode{};
    GLint oldSrcRgb{},oldDstRgb{},oldSrcAlpha{},oldDstAlpha{},oldEqRgb{},oldEqAlpha{};
    GLboolean oldDepthMask{},oldColorMask[4]{};
    const bool oldBlend=glIsEnabled(GL_BLEND),oldCull=glIsEnabled(GL_CULL_FACE),oldDepth=glIsEnabled(GL_DEPTH_TEST);
    glGetIntegerv(GL_CURRENT_PROGRAM,&oldProgram);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&oldArray);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING,&oldElements);glGetIntegerv(GL_DEPTH_FUNC,&oldDepthFunc);
    glGetIntegerv(GL_CULL_FACE_MODE,&oldCullMode);glGetBooleanv(GL_DEPTH_WRITEMASK,&oldDepthMask);
    glGetBooleanv(GL_COLOR_WRITEMASK,oldColorMask);
    glGetIntegerv(GL_BLEND_SRC_RGB,&oldSrcRgb);glGetIntegerv(GL_BLEND_DST_RGB,&oldDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA,&oldSrcAlpha);glGetIntegerv(GL_BLEND_DST_ALPHA,&oldDstAlpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB,&oldEqRgb);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&oldEqAlpha);
    glUseProgram(g_markerProg);glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);glCullFace(GL_BACK);
    glDepthMask(GL_TRUE);glDepthFunc(GL_LEQUAL);glDisable(GL_BLEND);
    if (clearDepth) glClear(GL_DEPTH_BUFFER_BIT); // scope pixels have a different projection
    std::array<float,16> matrices[4];bool ready[4]{};
    for(int h=0;h<4;++h) {
        const auto& visual=frame.items[h];if (!visual.model || visual.opacity<=0) continue;
        auto& gpu=g_reloadHintGpu[h];
        if (!gpu.vertices || !glIsBuffer(gpu.vertices)) {
            glGenBuffers(1,&gpu.vertices);glGenBuffers(1,&gpu.indices);gpu.uploaded.reset();
        }
        glBindBuffer(GL_ARRAY_BUFFER,gpu.vertices);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,gpu.indices);
        if (gpu.uploaded!=visual.model) {
            const auto& mesh=*visual.model;
            glBufferData(GL_ARRAY_BUFFER,mesh.vertices.size()*sizeof(float),mesh.vertices.data(),GL_STATIC_DRAW);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,mesh.indices.size()*sizeof(unsigned short),mesh.indices.data(),GL_STATIC_DRAW);
            gpu.uploaded=visual.model;
        }
        const auto model=reloadhint::ModelMatrix(visual.pose,*visual.model);
        MatMul(matrices[h].data(),viewProjection,model.data());ready[h]=true;
    }
    const auto draw=[&] {
        for(int h=0;h<4;++h)if(ready[h]) {
            const auto& pose=frame.items[h].pose;
            const float determinant=arcadereload::Dot(pose.barrel,arcadereload::Cross(pose.top,pose.side));
            glCullFace(determinant<0?GL_FRONT:GL_BACK);
            glUniform4f(g_markerColLoc,1.f,1.f,0.f,frame.items[h].opacity);
            glUniformMatrix4fv(g_markerMvpLoc,1,GL_FALSE,matrices[h].data());
            glBindBuffer(GL_ARRAY_BUFFER,g_reloadHintGpu[h].vertices);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,g_reloadHintGpu[h].indices);
            glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),nullptr);
            glDrawElements(GL_TRIANGLES,static_cast<GLsizei>(frame.items[h].model->indices.size()),GL_UNSIGNED_SHORT,nullptr);
            glDisableVertexAttribArray(0);
        }
    };
    // Draw only the nearest ghost surface at exactly 20% opacity. Without the
    // depth-only pass, overlapping surfaces would accumulate into a solid blob.
    glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);draw();
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_FALSE);glDepthFunc(GL_EQUAL);
    glEnable(GL_BLEND);glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    draw();
    glColorMask(oldColorMask[0],oldColorMask[1],oldColorMask[2],oldColorMask[3]);
    glDepthMask(oldDepthMask);glDepthFunc(oldDepthFunc);glCullFace(oldCullMode);
    if(oldBlend)glEnable(GL_BLEND);else glDisable(GL_BLEND);
    if(oldCull)glEnable(GL_CULL_FACE);else glDisable(GL_CULL_FACE);
    if(oldDepth)glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);
    glBlendFuncSeparate(oldSrcRgb,oldDstRgb,oldSrcAlpha,oldDstAlpha);glBlendEquationSeparate(oldEqRgb,oldEqAlpha);
    glBindBuffer(GL_ARRAY_BUFFER,oldArray);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,oldElements);glUseProgram(oldProgram);
}

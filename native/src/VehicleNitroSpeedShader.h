#pragma once
// Shared by the FXAA and plain/graded copy paths. No history texture or extra
// render target: only peripheral pixels take the eight additional samples.
namespace savr::nitro {
inline constexpr const char* kSpeedBlurGlsl=R"GLSL(
uniform vec3 uNitro; // strength, optical centre U/V for this eye
vec3 NitroSpeedBlur(vec3 colour,vec2 uv){
    if(uNitro.x<=.001)return colour;
    vec2 radial=uv-uNitro.yz;
    // Cover the main field of view, retaining a small sharp reading region.
    // The central 18% diameter stays untouched; full blur starts at 55%.
    float edge=smoothstep(.18,.55,length(radial*2.));
    if(edge<=0.)return colour;
    vec2 stepUv=radial*(.18*uNitro.x*edge);
    vec2 halfPixel=.5/vec2(textureSize(uTexture,0));
    vec3 blur=colour*.12;
    for(int tap=1;tap<=8;++tap)
        blur+=texture(uTexture,clamp(uv-stepUv*(float(tap)*.125),halfPixel,1.-halfPixel)).rgb*.11;
    return mix(colour,blur,.98*uNitro.x*edge);
}
)GLSL";
}

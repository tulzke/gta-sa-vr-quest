#pragma once
namespace savr::watchhud {
inline constexpr const char* kVertexShader=R"GLSL(#version 300 es
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
uniform mat4 uMVP,uModel;
out vec2 vUV;out vec3 vNormal;
void main(){
    vec3 p=vec3(aPos.x,-aPos.z,aPos.y);
    vec3 n=vec3(aNormal.x,-aNormal.z,aNormal.y);
    vNormal=normalize(mat3(uModel)*n);vUV=aUV;
    gl_Position=uMVP*uModel*vec4(p,1.0);
})GLSL";
inline constexpr const char* kFragmentShader=R"GLSL(#version 300 es
precision highp float;precision highp int;
in vec2 vUV;in vec3 vNormal;
uniform sampler2D uColor,uMetal,uMap;
uniform bool uDisplay,uHasColor,uHasMetal,uHasMap,uMirrorFace,uProjection;
uniform vec4 uColorFactor,uWatch;
uniform vec2 uMaterial;
uniform float uOpacity,uHeading;
uniform int uMoney,uWanted;
uniform int uEffect;
uniform float uTime;
out vec4 o;
float glyph(vec2 p,int n){
    const int rows[84]=int[84](
    14,17,19,21,25,17,14,4,12,4,4,4,4,14,14,17,1,2,4,8,31,
    30,1,1,14,1,1,30,2,6,10,18,31,2,2,31,16,16,30,1,1,30,
    14,16,16,30,17,17,14,31,1,2,4,8,8,8,14,17,17,14,17,17,14,
    14,17,17,15,1,1,14,4,15,20,14,5,30,4,0,4,4,0,4,4,0);
    ivec2 c=ivec2(floor(p*vec2(6,9)));
    if(p.x<0.0||p.y<0.0||c.x>4||c.y<1||c.y>7||n<0||n>11)return 0.0;
    return float((rows[n*7+c.y-1]>>(4-c.x))&1);
}
float heart(vec2 q){q.y=-q.y;float a=dot(q,q)-.8;return step(a*a*a-q.x*q.x*q.y*q.y*q.y,0.0);}
float shield(vec2 q){float d=max(abs(q.x)-.72,max(-q.y-.8,q.y-.95));d=max(d,abs(q.x)+.75*q.y-.83);return 1.0-step(0.0,d);}
void main(){
    if(uEffect>0){
        if(uEffect==1){
            float meridian=smoothstep(.975,.999,cos(vUV.x*62.831853));
            float pulse=pow(.5+.5*cos((vUV.y-uTime*.32)*18.849556),48.0);
            float fade=sin(clamp(vUV.y,0.0,1.0)*3.14159265);
            o=vec4(.18,.78,1.0,uOpacity*(.018+.13*meridian+.38*pulse)*fade);
        }else o=vec4(mix(mix(vec3(.55,.62,.65),vec3(.18,.78,1.0),vUV.x),vec3(.65,.95,1.0),vUV.y),uOpacity);
        return;
    }
    if(!uDisplay){
        vec3 base=uColorFactor.rgb*(uHasColor?pow(texture(uColor,vUV).rgb,vec3(2.2)):vec3(1));
        vec3 packed=uHasMetal?texture(uMetal,vUV).rgb:vec3(1);
        float metal=uMaterial.x*packed.b,rough=max(.1,uMaterial.y*packed.g);
        vec3 n=normalize(vNormal),l=normalize(vec3(-.2,.7,.55));
        float diffuse=.42+.58*max(dot(n,l),0.0);
        float shine=pow(max(dot(n,normalize(l+vec3(0,0,1))),0.0),mix(96.0,4.0,rough));
        vec3 col=base*diffuse+vec3(.035+.20*shine)*mix(.15,1.0,metal);
        o=vec4(pow(max(col,vec3(0)),vec3(1.0/2.2)),uOpacity);return;
    }
    vec2 faceUV=vec2(uMirrorFace?1.0-vUV.x:vUV.x,vUV.y);
    // The authored display UV axes point opposite to the wrist's reading
    // direction. Turn the interface, preserving the fitted physical model.
    faceUV=1.0-faceUV;
    vec2 p=faceUV-.5;float r=length(p);
    if(uProjection){
        if(r>.5||!uHasMap)discard;
        float c=cos(uHeading),s=sin(uHeading);vec2 q=p*MAP_SCALE;
        o=vec4(texture(uMap,vec2(.5)+vec2(c*q.x+s*q.y,-s*q.x+c*q.y)).rgb,uOpacity);
        return;
    }
    vec3 col=vec3(.024,.033,.03);
    // The same limits are supplied by WatchLayout.h to avoid a hidden band
    // containing a mission target beyond either straight map edge.
    if(r<MAP_RADIUS && p.y>=MAP_TOP && p.y<=MAP_BOTTOM && uHasMap){
        float c=cos(uHeading),s=sin(uHeading);
        vec2 q=p*MAP_SCALE;
        col=texture(uMap,vec2(.5)+vec2(c*q.x+s*q.y,-s*q.x+c*q.y)).rgb;
    }
    float angle=atan(abs(p.x),-p.y);
    if(r>.43&&r<.485){
        float value=p.x<0.0?uWatch.z:uWatch.w;
        col=vec3(.12,.14,.13);
        if(value>=0.0&&angle<=clamp(value,0.0,1.0)*3.14159265)
            col=p.x<0.0?vec3(.88,.12,.19):vec3(.65,.68,.66);
    }
    vec2 h=(p-vec2(-.155,-.415))/.040;
    vec2 a=(p-vec2(.155,-.415))/.040;
    if(abs(h.x)<1.0&&abs(h.y)<1.0)col=mix(vec3(.88,.12,.19),vec3(.035),heart(h));
    if(abs(a.x)<1.0&&abs(a.y)<1.0)col=mix(vec3(.65,.68,.66),vec3(.035),shield(a));
    vec2 t=vec2((faceUV.x-.33)*(5.0/.34),(faceUV.y-.12)/.085);
    int ti=int(floor(t.x)),g=-1;
    if(uWatch.x>=0.0&&uWatch.y>=0.0){int hour=int(uWatch.x),minute=int(uWatch.y);
        if(ti==0)g=hour/10;if(ti==1)g=hour%10;if(ti==2)g=11;if(ti==3)g=minute/10;if(ti==4)g=minute%10;}
    if(t.x>=0.0&&ti<5)col=mix(col,vec3(.94),glyph(vec2(fract(t.x),t.y),g));
    if(uWanted>0)for(int i=0;i<6;++i){
        vec2 q=p-vec2(-.275+.110*float(i),.240);
        // Intersect the radial ray with an outer-to-inner straight edge of
        // a five-point star (linear radius interpolation curves the edges).
        float theta=abs(mod(atan(q.x,-q.y)+.62831853,1.25663706)-.62831853);
        float bound=(.054*.024*.58778525)/
            (.024*.58778525*cos(theta)+(.054-.024*.80901699)*sin(theta));
        float star=1.0-smoothstep(bound-.001,bound+.001,length(q));
        col=mix(col,i<uWanted?vec3(1,.8,.25):vec3(.18),star);
    }
    vec2 cash=vec2((faceUV.x-.27)*(9.0/.46),(faceUV.y-.79)/.075);
    int digitIndex=int(floor(cash.x)),digit=-1;
    if(uMoney>=0&&cash.x>=0.0&&digitIndex<9){
        if(digitIndex==0)digit=10;else{
            int k=8-digitIndex;
            int divisor=k==7?10000000:k==6?1000000:k==5?100000:k==4?10000:k==3?1000:k==2?100:k==1?10:1;
            digit=(uMoney/divisor)%10;}
        col=mix(col,vec3(.84,.93,.85),glyph(vec2(fract(cash.x),cash.y),digit));
    }
    o=vec4(col,uOpacity);
})GLSL";
}

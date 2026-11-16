"""Exercise the production GLES skin shader in headless WebGL2 (ANGLE).

Requires Edge on Windows and test-hand-skeleton.py fixtures. Uses transform
feedback to verify both hands/eyes, eight weights, normals and ledge attachment.
Also creates a contact sheet from the actual shader, without UXRH fallback.
"""
import base64
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
out = ROOT / '.cache/hand-skeletal'
source = (ROOT / 'native/src/Xr.cpp').read_text(encoding='utf-8')
shader_region = source.split('bool EnsureSkeletalRenderer() {', 1)[1].split('void DrawSkeletalHands', 1)[0]
vs, fs = re.findall(r'R"GLSL\((.*?)\)GLSL"', shader_region, re.S)
glb = base64.b64encode((ROOT / 'assets/vrhands/skeletal/hands.glb').read_bytes()).decode()
palettes = json.loads((out/'gpu.json').read_text())['palettes']
html = r'''<!doctype html><meta charset="utf-8"><style>
body{background:#17212c;color:#e9edf1;font:15px sans-serif}h1{font-size:22px}
canvas{width:1120px;height:1600px}pre{white-space:pre-wrap}</style>
<h1>GLB skeletal hands — production shader / left and right</h1><pre id="result">RUNNING</pre>
<canvas id="canvas" width="1120" height="1600"></canvas><script>
const vertexSource=__VS__,fragmentSource=__FS__,palettes=__PALETTES__;
async function run(){
 const bytes=Uint8Array.from(atob('__GLB__'),c=>c.charCodeAt(0));
 const dv=new DataView(bytes.buffer),jsonLength=dv.getUint32(12,true);
 const doc=JSON.parse(new TextDecoder().decode(bytes.slice(20,20+jsonLength))),bin=28+jsonLength;
 const acc=(id)=>{const a=doc.accessors[id],v=doc.bufferViews[a.bufferView];
  const offset=bin+(v.byteOffset||0)+(a.byteOffset||0),n=a.count*({SCALAR:1,VEC2:2,VEC3:3,VEC4:4,MAT4:16}[a.type]);
  return a.componentType===5126?new Float32Array(bytes.buffer,offset,n):new Uint16Array(bytes.buffer,offset,n);};
 const prim=doc.meshes[0].primitives[0],attr=prim.attributes;
 const pos=acc(attr.POSITION),normal=acc(attr.NORMAL),uv=acc(attr.TEXCOORD_0),ids=[acc(attr.JOINTS_0),acc(attr.JOINTS_1)],weights=[acc(attr.WEIGHTS_0),acc(attr.WEIGHTS_1)];
 const count=pos.length/3,verts=new ArrayBuffer(count*80),view=new DataView(verts);
 for(let i=0;i<count;i++){
  for(let k=0;k<3;k++){view.setFloat32(i*80+k*4,pos[i*3+k],true);view.setFloat32(i*80+12+k*4,normal[i*3+k],true);}
  for(let k=0;k<2;k++)view.setFloat32(i*80+24+k*4,uv[i*2+k],true);
  for(let k=0;k<8;k++){view.setUint16(i*80+32+k*2,ids[k>>2][i*4+k%4],true);view.setFloat32(i*80+48+k*4,weights[k>>2][i*4+k%4],true);}
 }
 const gl=canvas.getContext('webgl2',{preserveDrawingBuffer:true,antialias:false});if(!gl)throw Error('WebGL2 unavailable');
 function shader(type,src){let s=gl.createShader(type);gl.shaderSource(s,src);gl.compileShader(s);if(!gl.getShaderParameter(s,gl.COMPILE_STATUS))throw Error(gl.getShaderInfoLog(s));return s;}
 const program=gl.createProgram();gl.attachShader(program,shader(gl.VERTEX_SHADER,vertexSource));gl.attachShader(program,shader(gl.FRAGMENT_SHADER,fragmentSource));
 gl.transformFeedbackVaryings(program,['gl_Position','vN','vUV'],gl.INTERLEAVED_ATTRIBS);gl.linkProgram(program);
 if(!gl.getProgramParameter(program,gl.LINK_STATUS))throw Error(gl.getProgramInfoLog(program));gl.useProgram(program);
 const loc=n=>gl.getUniformLocation(program,n),identity=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1];
 const vao=gl.createVertexArray();gl.bindVertexArray(vao);const vb=gl.createBuffer();gl.bindBuffer(gl.ARRAY_BUFFER,vb);gl.bufferData(gl.ARRAY_BUFFER,verts,gl.STATIC_DRAW);
 for(let k=0;k<7;k++)gl.enableVertexAttribArray(k);
 gl.vertexAttribPointer(0,3,gl.FLOAT,false,80,0);gl.vertexAttribPointer(1,3,gl.FLOAT,false,80,12);gl.vertexAttribPointer(2,2,gl.FLOAT,false,80,24);
 gl.vertexAttribIPointer(3,4,gl.UNSIGNED_SHORT,80,32);gl.vertexAttribIPointer(4,4,gl.UNSIGNED_SHORT,80,40);
 gl.vertexAttribPointer(5,4,gl.FLOAT,false,80,48);gl.vertexAttribPointer(6,4,gl.FLOAT,false,80,64);
 const ib=gl.createBuffer();gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER,ib);gl.bufferData(gl.ELEMENT_ARRAY_BUFFER,acc(prim.indices),gl.STATIC_DRAW);
 const feedback=gl.createTransformFeedback();gl.bindTransformFeedback(gl.TRANSFORM_FEEDBACK,feedback);
 const fb=gl.createBuffer();gl.bindBuffer(gl.TRANSFORM_FEEDBACK_BUFFER,fb);gl.bufferData(gl.TRANSFORM_FEEDBACK_BUFFER,count*9*4,gl.DYNAMIC_READ);gl.bindBufferBase(gl.TRANSFORM_FEEDBACK_BUFFER,0,fb);
 const values=new Float32Array(count*9),norm=v=>{const n=Math.hypot(...v);return v.map(x=>x/n)},cross=(a,b)=>[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]];
 const transform=(m,p,w=1)=>[0,1,2,3].map(r=>m[r]*p[0]+m[4+r]*p[1]+m[8+r]*p[2]+m[12+r]*w);
 const model=side=>[0,1,0,0, side,0,0,0, 0,0,1,0, 0,-.11,0,1];
 const wrist=side=>[.3,.953939,0,0, side*.953939,-side*.3,0,0, 0,0,1,0, .01,-.12,.02,1];
 let maxPosition=0,maxNormal=0,checks=0;
 gl.enable(gl.RASTERIZER_DISCARD);
 for(const palette of palettes)for(const side of [-1,1])for(const eye of [-1,1])for(const ledge of [false,true]){
  const m=model(side),w=wrist(side),mvp=identity.slice();mvp[12]=eye*.031;
  gl.uniformMatrix4fv(loc('uJoints[0]'),false,palette);gl.uniformMatrix4fv(loc('uModel'),false,m);gl.uniformMatrix4fv(loc('uWrist'),false,w);gl.uniformMatrix4fv(loc('uMVP'),false,mvp);gl.uniform1i(loc('uLedge'),ledge);
  gl.beginTransformFeedback(gl.POINTS);gl.drawArrays(gl.POINTS,0,count);gl.endTransformFeedback();
  gl.getBufferSubData(gl.TRANSFORM_FEEDBACK_BUFFER,0,values);
  for(let i=0;i<count;i+=17){
   const skin=new Array(16).fill(0);for(let k=0;k<8;k++){const j=ids[k>>2][i*4+k%4],weight=weights[k>>2][i*4+k%4];for(let a=0;a<16;a++)skin[a]+=palette[j*16+a]*weight;}
   let p=transform(skin,[...pos.slice(i*3,i*3+3)]);p=[p[0],-p[2],p[1]];
   // Cofactor columns = inverse-transpose * determinant.
   const a=skin.slice(0,3),b=skin.slice(4,7),c=skin.slice(8,11),ca=cross(b,c),cb=cross(c,a),cc=cross(a,b),det=a.reduce((s,v,k)=>s+v*ca[k],0);
   let n=norm([0,1,2].map(k=>(ca[k]*normal[i*3]+cb[k]*normal[i*3+1]+cc[k]*normal[i*3+2])/det));n=[n[0],-n[2],n[1]];
   let world=transform(m,p),wn=norm(transform(m,n,0).slice(0,3));
   if(ledge){let t=Math.min(1,Math.max(0,(p[0]-.015)/.05));t=t*t*(3-2*t);const wp=transform(w,p),nn=norm(transform(w,n,0).slice(0,3));world=world.map((v,k)=>wp[k]*(1-t)+v*t);wn=norm(wn.map((v,k)=>nn[k]*(1-t)+v*t));}
   const clip=transform(mvp,world);for(let k=0;k<4;k++)maxPosition=Math.max(maxPosition,Math.abs(clip[k]-values[i*9+k]));
   for(let k=0;k<3;k++)maxNormal=Math.max(maxNormal,Math.abs(wn[k]-values[i*9+4+k]));
   for(let k=0;k<2;k++)if(Math.abs(uv[i*2+k]-values[i*9+7+k])>1e-6)throw Error('UV mismatch');checks++;
  }
 }
 gl.disable(gl.RASTERIZER_DISCARD);gl.bindTransformFeedback(gl.TRANSFORM_FEEDBACK,null);
 if(!Number.isFinite(maxPosition)||maxPosition>2e-5||maxNormal>2e-4)throw Error(JSON.stringify({maxPosition,maxNormal}));
 if(gl.getError()!==gl.NO_ERROR)throw Error('GL error during feedback');
 // Contact sheet: actual production albedo and shader, all clip endpoints.
 const image=doc.images[0],imageView=doc.bufferViews[image.bufferView];
 const bitmap=await createImageBitmap(new Blob([bytes.slice(bin+imageView.byteOffset,bin+imageView.byteOffset+imageView.byteLength)],{type:'image/png'}),{premultiplyAlpha:'none',colorSpaceConversion:'none'});
 const texture=gl.createTexture();gl.activeTexture(gl.TEXTURE0);gl.bindTexture(gl.TEXTURE_2D,texture);gl.texImage2D(gl.TEXTURE_2D,0,gl.RGBA,gl.RGBA,gl.UNSIGNED_BYTE,bitmap);
 gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MIN_FILTER,gl.LINEAR);gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MAG_FILTER,gl.LINEAR);gl.uniform1i(loc('uTex'),0);gl.uniform1f(loc('uDarkSkin'),0);gl.uniform1i(loc('uLedge'),false);
 gl.clearColor(.12,.16,.2,1);gl.clear(gl.COLOR_BUFFER_BIT|gl.DEPTH_BUFFER_BIT);gl.enable(gl.DEPTH_TEST);gl.disable(gl.CULL_FACE);
 for(let action=0;action<doc.animations.length;action++)for(let hand=0;hand<2;hand++){
  gl.viewport((action%2)*560+hand*280,1400-Math.floor(action/2)*200,280,200);
  const mvp=identity.slice();mvp[0]=6;mvp[5]=6;mvp[10]=-6;
  gl.uniformMatrix4fv(loc('uMVP'),false,mvp);gl.uniformMatrix4fv(loc('uModel'),false,model(hand?1:-1));gl.uniformMatrix4fv(loc('uJoints[0]'),false,palettes[action*3+2]);
  gl.drawElements(gl.TRIANGLES,doc.accessors[prim.indices].count,gl.UNSIGNED_SHORT,0);
 }
 gl.finish();if(gl.getError()!==gl.NO_ERROR)throw Error('Contact sheet GL error');
 result.textContent='PASS GPU: '+checks+' comparisons; position max '+maxPosition+' m; normal max '+maxNormal+'; '+gl.getParameter(gl.RENDERER)+'\nRows (two clips per row; L/R per clip): '+doc.animations.map(a=>a.name).join(', ');
 document.title='PASS GPU';
}
run().catch(e=>{result.textContent='FAIL '+e.stack;document.title='FAIL GPU'});
</script>'''
html = html.replace('__VS__',json.dumps(vs)).replace('__FS__',json.dumps(fs)).replace('__PALETTES__',json.dumps(palettes)).replace('__GLB__',glb)
page=out/'gpu-test.html'
page.write_text(html,encoding='utf-8')
edge=Path(r'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe')
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
run=subprocess.run([str(edge),'--headless','--no-sandbox','--no-first-run','--disable-gpu-sandbox',
    '--use-gl=angle','--use-angle=swiftshader','--enable-unsafe-swiftshader',
    '--user-data-dir='+str(out/'edge-profile'),'--virtual-time-budget=20000','--window-size=1180,1580',
    '--screenshot='+str(out/'gpu-contact-sheet.png'),'--dump-dom',page.as_uri()],
    capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=90,startupinfo=startup)
(out/'gpu-result.html').write_text(run.stdout,encoding='utf-8')
match=re.search(r'<pre id="result">(.*?)</pre>',run.stdout,re.S)
if not match or not match[1].startswith('PASS GPU:'):
    raise RuntimeError((match[1] if match else run.stderr[-3000:]))
print(match[1])
print(out/'gpu-contact-sheet.png')

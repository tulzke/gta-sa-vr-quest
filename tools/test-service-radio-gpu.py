"""Run the production radio shader/vertex format through WebGL2 transform feedback."""
import base64
import json
from pathlib import Path
import re
import subprocess

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/service-radio/gpu';OUT.mkdir(parents=True,exist_ok=True)
vs,fs=re.findall(r'R"GLSL\((.*?)\)GLSL"',(ROOT/'native/src/ServiceRadioRender.inl').read_text(),re.S)
vs=vs.replace('out vec3 shade;','out vec3 shade;out vec4 qaPosition;')
at=vs.rfind('}');vs=vs[:at]+'qaPosition=gl_Position;'+vs[at:]
data=base64.b64encode((ROOT/'assets/service-radio/runtime/service-radio.mesh').read_bytes()).decode()
html=r'''<!doctype html><pre id="result">RUNNING</pre><canvas id="canvas"></canvas><script>
try{
const gl=canvas.getContext('webgl2');if(!gl)throw Error('No WebGL2');
const compile=(type,text)=>{const s=gl.createShader(type);gl.shaderSource(s,text);gl.compileShader(s);if(!gl.getShaderParameter(s,gl.COMPILE_STATUS))throw Error(gl.getShaderInfoLog(s));return s;};
const p=gl.createProgram();gl.attachShader(p,compile(gl.VERTEX_SHADER,__VS__));gl.attachShader(p,compile(gl.FRAGMENT_SHADER,__FS__));
gl.transformFeedbackVaryings(p,['qaPosition','shade'],gl.INTERLEAVED_ATTRIBS);gl.linkProgram(p);
if(!gl.getProgramParameter(p,gl.LINK_STATUS))throw Error(gl.getProgramInfoLog(p));gl.useProgram(p);
const bytes=Uint8Array.from(atob('__MESH__'),c=>c.charCodeAt(0)),view=new DataView(bytes.buffer),nv=view.getUint32(8,true),v=new Float32Array(bytes.buffer,16,nv*10);
const buffer=gl.createBuffer();gl.bindBuffer(gl.ARRAY_BUFFER,buffer);gl.bufferData(gl.ARRAY_BUFFER,v,gl.STATIC_DRAW);
for(let i=0;i<4;i++){gl.enableVertexAttribArray(i);gl.vertexAttribPointer(i,i===3?1:3,gl.FLOAT,false,40,i*12);}
gl.uniformMatrix4fv(gl.getUniformLocation(p,'mvp'),false,new Float32Array([1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]));
gl.uniform3fv(gl.getUniformLocation(p,'dock[0]'),new Float32Array([0,0,0,1,0,0,0,1,0,0,0,1]));
gl.uniform3fv(gl.getUniformLocation(p,'body[0]'),new Float32Array([.2,.3,.4,0,1,0,-1,0,0,0,0,1]));
const output=gl.createBuffer();gl.bindBuffer(gl.TRANSFORM_FEEDBACK_BUFFER,output);gl.bufferData(gl.TRANSFORM_FEEDBACK_BUFFER,nv*7*4,gl.STREAM_READ);gl.bindBufferBase(gl.TRANSFORM_FEEDBACK_BUFFER,0,output);
gl.enable(gl.RASTERIZER_DISCARD);let checks=0,base=null;
for(const tint of [[.018,.12,.64],[.78,.80,.79],[.65,.022,.018]])for(const press of [0,1]){
 gl.uniform3fv(gl.getUniformLocation(p,'tint'),tint);gl.uniform1f(gl.getUniformLocation(p,'press'),press);
 gl.beginTransformFeedback(gl.POINTS);gl.drawArrays(gl.POINTS,0,nv);gl.endTransformFeedback();
 const actual=new Float32Array(nv*7);gl.getBufferSubData(gl.TRANSFORM_FEEDBACK_BUFFER,0,actual);
 for(let i=0;i<nv;i++){
  const role=v[i*10+9],x=v[i*10]+(role===3?.0015*press:0),y=v[i*10+1],z=v[i*10+2];
  const expected=role===0?[x,y,z]:[.2-y,.3+x,.4+z];
  for(let j=0;j<3;j++){checks++;if(Math.abs(actual[i*7+j]-expected[j])>2e-6)throw Error('Wrong dock/body/PTT transform at '+i);}
  if(base&&role!==2)for(let j=4;j<7;j++)if(Math.abs(actual[i*7+j]-base[i*7+j])>1e-5)throw Error('Service color changed a non-accent material');
  if(role===2){const n=[-v[i*10+4],v[i*10+3],v[i*10+5]],length=Math.hypot(...n),l=(n[0]*-.4+n[1]*.7+n[2]*.5)/length/Math.sqrt(.9),light=.42+.58*Math.max(0,l);
   for(let j=0;j<3;j++){checks++;if(Math.abs(actual[i*7+4+j]-Math.pow(tint[j],1/2.2)*light)>.003)throw Error('Wrong isolated accent color');}}
 }
 if(!base)base=actual;
}
if(gl.getError()!==gl.NO_ERROR)throw Error('GL error');
result.textContent='PASS GPU: '+checks+' production radio shader checks; dock/body separation, PTT travel, three isolated service tints';
}catch(e){result.textContent='FAIL: '+e.stack}
</script>'''.replace('__VS__',json.dumps(vs)).replace('__FS__',json.dumps(fs)).replace('__MESH__',data)
page=OUT/'test.html';page.write_text(html,encoding='utf8')
edge=Path(r'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe')
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
run=subprocess.run([str(edge),'--headless','--no-first-run','--no-sandbox','--disable-gpu-sandbox',
    '--use-gl=angle','--use-angle=swiftshader','--enable-unsafe-swiftshader','--user-data-dir='+str(OUT/'profile'),
    '--virtual-time-budget=3000','--dump-dom',page.as_uri()],capture_output=True,text=True,encoding='utf8',errors='replace',timeout=60,startupinfo=startup)
(OUT/'result.html').write_text(run.stdout,encoding='utf8')
match=re.search(r'<pre id="result">(.*?)</pre>',run.stdout,re.S)
if not match or not match[1].startswith('PASS GPU:'):raise RuntimeError(match[1] if match else run.stderr[-2000:])
print(match[1])

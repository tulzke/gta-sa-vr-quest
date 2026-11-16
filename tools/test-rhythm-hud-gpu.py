"""Replay the production rhythm mesh/shaders and inspect four directional glyphs."""
from pathlib import Path
import base64
import json
import re
import subprocess

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/rhythm-tests'
shaders=re.findall(r'R"GLSL\((.*?)\)GLSL"',(ROOT/'native/src/RhythmHudRender.inl').read_text(encoding='utf8'),re.S)
assert len(shaders)==2
meshes={name:base64.b64encode((OUT/(name+'.mesh')).read_bytes()).decode() for name in ['approach','hit','arrows']}
page=r'''<!doctype html><meta charset="utf-8"><style>
body{margin:28px;background:#111a20;color:#eaf0f2;font:18px system-ui}h2{font-size:22px;margin:0 0 20px}
label{display:block;margin:18px 0 8px;color:#b4c5ce}canvas{display:block;width:1200px;height:190px}#arrows{height:130px}pre{font:13px monospace;color:#9dc7b4}
</style><h2>Ритм лоурайдеров · игровой меш и шейдер</h2>
<label>Приближение: стрелки движутся справа к неподвижной янтарной зоне</label><canvas id="approach" width="1200" height="190"></canvas>
<label>Момент движения: метка стрелки внутри зоны зачёта</label><canvas id="hit" width="1200" height="190"></canvas>
<label>Четыре направления — отдельный наконечник и длинный хвост</label><canvas id="arrows" width="1200" height="130"></canvas><pre id="result">RUNNING</pre>
<script>
try {
 const shaders=__SHADERS__,meshes=__MESHES__,assert=(c,m)=>{if(!c)throw Error(m)};
 const setup=(name)=>{
  const view=document.getElementById(name),gl=view.getContext('webgl2',{alpha:true,antialias:false,preserveDrawingBuffer:true});assert(gl,'WebGL2');
  const compile=(type,s)=>{const h=gl.createShader(type);gl.shaderSource(h,s);gl.compileShader(h);assert(gl.getShaderParameter(h,gl.COMPILE_STATUS),gl.getShaderInfoLog(h));return h};
  const p=gl.createProgram();gl.attachShader(p,compile(gl.VERTEX_SHADER,shaders[0]));gl.attachShader(p,compile(gl.FRAGMENT_SHADER,shaders[1]));gl.linkProgram(p);assert(gl.getProgramParameter(p,gl.LINK_STATUS),gl.getProgramInfoLog(p));gl.useProgram(p);
  const raw=Uint8Array.from(atob(meshes[name]),x=>x.charCodeAt(0)),vertices=new Float32Array(raw.buffer);
  gl.bindBuffer(gl.ARRAY_BUFFER,gl.createBuffer());gl.bufferData(gl.ARRAY_BUFFER,vertices,gl.STATIC_DRAW);
  gl.enableVertexAttribArray(0);gl.vertexAttribPointer(0,2,gl.FLOAT,false,24,0);gl.enableVertexAttribArray(1);gl.vertexAttribPointer(1,4,gl.FLOAT,false,24,8);
  const u=s=>gl.getUniformLocation(p,s);
  gl.uniformMatrix4fv(u('uMvp'),false,new Float32Array([1/.52,0,0,0,0,1/.09,0,0,0,0,1,0,0,0,0,1]));
  gl.uniform3f(u('uOrigin'),0,0,0);gl.uniform3f(u('uRight'),1,0,0);gl.uniform3f(u('uUp'),0,1,0);
  gl.viewport(0,0,view.width,view.height);gl.enable(gl.BLEND);gl.blendFuncSeparate(gl.SRC_ALPHA,gl.ONE_MINUS_SRC_ALPHA,gl.ONE,gl.ONE_MINUS_SRC_ALPHA);
  gl.clearColor(name==='arrows'?0:.35,name==='arrows'?0:.45,name==='arrows'?0:.5,name==='arrows'?0:1);gl.clear(gl.COLOR_BUFFER_BIT);
  gl.drawArrays(gl.TRIANGLES,0,vertices.length/6);assert(gl.getError()===gl.NO_ERROR,'production shader draw');
  return (x,y)=>{const c=new Uint8Array(4);gl.readPixels(Math.round((x/.52+1)*view.width/2),Math.round((y/.09+1)*view.height/2),1,1,gl.RGBA,gl.UNSIGNED_BYTE,c);return c};
 };
 const approach=setup('approach'),hit=setup('hit'),glyph=setup('arrows');
 assert(approach(0,.07)[0]<50,'dark track must separate the notes from the world');
 assert(hit(-.32,.028)[0]>220&&hit(-.32,.028)[1]>140,'on-time UP arrow at gate');
 assert(hit(-.345,-.065)[0]>80,'fixed acceptance band visible');
 for(let dir=1;dir<=4;dir++){
  const cx=-.36+(dir-1)*.24;
  const sample=(x,y)=>{if(dir===2){x=-x;y=-y}else if(dir===3){[x,y]=[-y,x]}else if(dir===4){[x,y]=[y,-x]}return glyph(cx+x,y)};
  assert(sample(0,.028)[3]===255,'arrow '+dir+' tip');assert(sample(0,-.025)[3]===255,'arrow '+dir+' long tail');
  assert(sample(.020,.006)[3]===255,'arrow '+dir+' broad arrowhead');assert(sample(.020,-.025)[3]===0,'arrow '+dir+' narrower tail');
 }
 result.textContent='PASS GPU: production strip shaders/meshes, exact fixed gate, moving notes and four distinct shafted arrows';
}catch(e){result.textContent='FAIL: '+e.stack}
</script>'''.replace('__SHADERS__',json.dumps(shaders)).replace('__MESHES__',json.dumps(meshes))
html=OUT/'gpu.html';html.write_text(page,encoding='utf8')
edge=Path(r'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe')
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
result=subprocess.run([str(edge),'--headless','--no-first-run','--no-sandbox','--disable-gpu-sandbox',
    '--use-gl=angle','--use-angle=swiftshader','--enable-unsafe-swiftshader',
    '--user-data-dir='+str(OUT/'edge-profile'),'--virtual-time-budget=2500','--window-size=1280,830',
    '--screenshot='+str(OUT/'rhythm.png'),'--dump-dom',html.as_uri()],capture_output=True,text=True,encoding='utf8',errors='replace',timeout=60,startupinfo=startup)
(OUT/'gpu-result.html').write_text(result.stdout,encoding='utf8')
match=re.search(r'<pre id="result">(.*?)</pre>',result.stdout,re.S)
if not match or not match[1].startswith('PASS GPU:'):
    raise RuntimeError(match[1] if match else result.stderr[-2000:])
print(match[1]);print(OUT/'rhythm.png')

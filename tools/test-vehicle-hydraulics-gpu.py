"""Render the production hydraulics shaders/mesh: opaque ball and alpha-only labels."""
import base64
import json
from pathlib import Path
import re
import subprocess

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/hydraulics-gpu-tests';OUT.mkdir(parents=True,exist_ok=True)
source=(ROOT/'native/src/VehicleHydraulicsRender.inl').read_text(encoding='utf8')
shaders=re.findall(r'R"GLSL\((.*?)\)GLSL"',source,re.S)
assert len(shaders)==2
payload=[base64.b64encode((ROOT/'assets/hydraulics/runtime'/name).read_bytes()).decode()
         for name in ['hydraulics.mesh','hydraulics.alpha']]
html=r'''<!doctype html><meta charset="utf-8"><style>
body{background:#222;color:#eee;font:16px monospace}canvas{width:512px;height:512px;background:#444}
</style><h3>Hydraulics: production native shader and mesh</h3><canvas id="view" width="512" height="512"></canvas><pre id="result">RUNNING</pre><script>
try {
const shaders=__SHADERS__,payload=__PAYLOAD__;
const assert=(c,m)=>{if(!c)throw Error(m)};
const bytes=s=>Uint8Array.from(atob(s),c=>c.charCodeAt(0));
const raw=bytes(payload[0]),header=new Uint32Array(raw.buffer,0,4);
const vertices=new Float32Array(raw.buffer,16,header[2]*14);
const indices=new Uint16Array(raw.buffer,16+header[2]*56,header[3]);
const gl=view.getContext('webgl2',{alpha:true,preserveDrawingBuffer:true,antialias:false});assert(gl,'WebGL2');
const compile=(type,text)=>{const s=gl.createShader(type);gl.shaderSource(s,text);gl.compileShader(s);assert(gl.getShaderParameter(s,gl.COMPILE_STATUS),gl.getShaderInfoLog(s));return s};
const p=gl.createProgram();gl.attachShader(p,compile(gl.VERTEX_SHADER,shaders[0]));gl.attachShader(p,compile(gl.FRAGMENT_SHADER,shaders[1]));gl.linkProgram(p);assert(gl.getProgramParameter(p,gl.LINK_STATUS),gl.getProgramInfoLog(p));gl.useProgram(p);
const sphere=[],labels=[],markers=[];
for(let i=0;i<indices.length;i+=3){const tri=indices.subarray(i,i+3);
 if([...tri].every(v=>{const a=v*14;return vertices[a+11]===1&&Math.abs(Math.hypot(vertices[a],vertices[a+1],vertices[a+2]-.055)-.0225)<1e-6}))sphere.push(...tri);
 if([...tri].every(v=>vertices[v*14+9]>=0))labels.push(...tri);
 if([...tri].every(v=>vertices[v*14+11]>=10))markers.push(...tri);
}
assert(sphere.length>1000&&labels.length===36,'ball and label geometry');
// Production neutral deformation for the rigid lever; rings are covered by
// the independent Blender/C++ test. The input payload stays unchanged.
for(let i=0;i<header[2];++i){const a=i*14;if(vertices[a+11]===1){vertices[a]-=.032;vertices[a+1]+=.008;vertices[a+2]+=.018}}
gl.bindBuffer(gl.ARRAY_BUFFER,gl.createBuffer());gl.bufferData(gl.ARRAY_BUFFER,vertices,gl.STATIC_DRAW);
for(const [a,n,offset] of [[0,3,0],[1,3,12],[2,3,24],[3,2,36],[4,1,44]]){gl.enableVertexAttribArray(a);gl.vertexAttribPointer(a,n,gl.FLOAT,false,56,offset)}
gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER,gl.createBuffer());
const uniform=n=>gl.getUniformLocation(p,n);
gl.uniformMatrix4fv(uniform('mvp'),false,new Float32Array([10,0,0,0,0,12,0,0,0,0,-10,0,0,0,0,1]));
for(const [n,v] of [['center',[0,0,0]],['axisRight',[1,0,0]],['axisForward',[0,1,0]],['axisUp',[0,0,1]]])gl.uniform3fv(uniform(n),v);
gl.uniform1f(uniform('scale'),1);gl.uniform4f(uniform('state'),0,1,0,0);
gl.activeTexture(gl.TEXTURE0);gl.bindTexture(gl.TEXTURE_2D,gl.createTexture());
gl.texImage2D(gl.TEXTURE_2D,0,gl.R8,1024,256,0,gl.RED,gl.UNSIGNED_BYTE,bytes(payload[1]));
gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MIN_FILTER,gl.LINEAR);gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MAG_FILTER,gl.LINEAR);
gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_S,gl.CLAMP_TO_EDGE);gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_T,gl.CLAMP_TO_EDGE);gl.uniform1i(uniform('atlas'),0);
gl.viewport(0,0,512,512);gl.enable(gl.DEPTH_TEST);gl.clearColor(0,0,0,0);
const draw=list=>{gl.clear(gl.COLOR_BUFFER_BIT|gl.DEPTH_BUFFER_BIT);const solid=[],decal=[];
 for(let i=0;i<list.length;i+=3)(vertices[list[i]*14+9]>=0?decal:solid).push(...list.slice(i,i+3));
 gl.bufferData(gl.ELEMENT_ARRAY_BUFFER,new Uint16Array([...solid,...decal]),gl.STATIC_DRAW);
 gl.drawElements(gl.TRIANGLES,solid.length,gl.UNSIGNED_SHORT,0);
 gl.enable(gl.POLYGON_OFFSET_FILL);gl.polygonOffset(-1,-4);gl.drawElements(gl.TRIANGLES,decal.length,gl.UNSIGNED_SHORT,solid.length*2);gl.disable(gl.POLYGON_OFFSET_FILL);
 assert(gl.getError()===gl.NO_ERROR,'draw error')};
const sample=(x,y)=>{const a=new Uint8Array(4);gl.readPixels(Math.round((x*10+1)*256),Math.round((y*12+1)*256),1,1,gl.RGBA,gl.UNSIGNED_BYTE,a);return a};
draw(sphere);let samples=0;
for(let x=-3;x<=3;++x)for(let y=-3;y<=3;++y){if(x*x+y*y>9)continue;const c=sample(-.032+x*.004,.008+y*.004);assert(c[3]===255&&Math.min(c[0],c[1],c[2])>80&&Math.max(c[0],c[1],c[2])-Math.min(c[0],c[1],c[2])<35,'ball must be opaque ivory: '+c);samples++}
draw(markers);
for(let role=10;role<=17;role++){
 const xy=[];for(let v=0;v<header[2];v++)if(vertices[v*14+11]===role)xy.push([vertices[v*14],vertices[v*14+1]]);
 assert(role%2===1?xy.length===0:xy.length>0,'diagonal divisions must be absent from the mesh: '+role);
 const a=(role-10)*Math.PI/4,c=sample(-.032+.032*Math.sin(a),.008+.032*Math.cos(a));
 assert(role%2===1?c[3]===0:c[3]===255,'only four cardinal marker positions may be visible: '+role+' alpha '+c[3]);
}
draw(labels);const pixels=new Uint8Array(512*512*4);gl.readPixels(0,0,512,512,gl.RGBA,gl.UNSIGNED_BYTE,pixels);
let ink=0,clear=0;for(let i=3;i<pixels.length;i+=4){if(pixels[i]===0)clear++;else ink++}
assert(ink>200&&clear>250000,'label alpha must remain visible and cut out');
draw(indices);
const full=new Uint8Array(pixels.length);gl.readPixels(0,0,512,512,gl.RGBA,gl.UNSIGNED_BYTE,full);
for(const [name,x,y,w,h] of [['HYDRAULICS',-.027,-.044,.066,.0065],['MODE',.047,-.040,.021,.006],['LOW',.032,.029,.014,.005],['HIGH',.059,.029,.017,.005],['FRONT',-.032,.050,.021,.0045],['BACK',-.032,-.033,.017,.0045]]){
 let total=0,white=0;
 for(let py=0;py<512;py++)for(let px=0;px<512;px++){
  const wx=(px/256-1)/10,wy=(py/256-1)/12;if(Math.abs(wx-x)>w/2||Math.abs(wy-y)>h/2)continue;
  const at=(py*512+px)*4;if(pixels[at+3]>245){total++;if(Math.min(full[at],full[at+1],full[at+2])>225)white++;}}
 assert(total>5&&white/total>.9,name+' must remain uniformly white over deck: '+white+'/'+total);
}
result.textContent='PASS GPU: four cardinal divisions, diagonals absent from mesh; '+samples+' opaque ivory ball samples; all six legends uniformly white over deck; transparent label atlas preserved ('+ink+' ink pixels)';
}catch(e){result.textContent='FAIL: '+e.stack}
</script>'''.replace('__SHADERS__',json.dumps(shaders)).replace('__PAYLOAD__',json.dumps(payload))
page=OUT/'gpu-test.html';page.write_text(html,encoding='utf8')
edge=Path(r'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe')
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
result=subprocess.run([str(edge),'--headless','--no-first-run','--no-sandbox','--disable-gpu-sandbox',
    '--use-gl=angle','--use-angle=swiftshader','--enable-unsafe-swiftshader',
    '--user-data-dir='+str(OUT/'edge-profile'),'--virtual-time-budget=3000','--window-size=800,800',
    '--screenshot='+str(OUT/'hydraulics.png'),'--dump-dom',page.as_uri()],
    capture_output=True,text=True,encoding='utf8',errors='replace',timeout=60,startupinfo=startup)
(OUT/'gpu-result.html').write_text(result.stdout,encoding='utf8')
match=re.search(r'<pre id="result">(.*?)</pre>',result.stdout,re.S)
if not match or not match[1].startswith('PASS GPU:'):
    raise RuntimeError(match[1] if match else result.stderr[-2000:])
print(match[1]);print(OUT/'hydraulics.png')

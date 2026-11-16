"""Check radio label geometry/state and render its production shaders on WebGL2."""
import json
import os
from pathlib import Path
import re
import subprocess
import struct
import sys

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/radio-label-tests'
OUT.mkdir(parents=True,exist_ok=True)
driving=(ROOT/'native/src/Driving.cpp').read_text(encoding='utf8')
def function(signature):
    start=driving.index(signature);brace=driving.index('{',start);end=brace+1;depth=1
    while depth:
        depth+=(driving[end]=='{')-(driving[end]=='}');end+=1
    return driving[start:end]+'\n'
(OUT/'radio_runtime.inc').write_text(driving[driving.index('struct V3 {'):driving.index('double MonotonicSeconds()')]+
    function('float PlanarAngle(')+function('bool NormalizeQuaternion('),encoding='utf8')
(OUT/'radio_update.inc').write_text(function('void UpdateRadioKnob(')+function('void RefreshRadioTracking('),encoding='utf8')
where=Path(os.environ.get('ProgramFiles(x86)',r'C:\Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
vs=subprocess.check_output([str(where),'-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
cmd=OUT/'test.cmd'
cmd.write_text(f'@echo off\ncall "{vs}/VC/Auxiliary/Build/vcvars64.bat" >nul\nif errorlevel 1 exit /b 1\n'
    f'cl /nologo /std:c++20 /EHsc /W4 /D_CRT_SECURE_NO_WARNINGS /I"{ROOT}/native/src" /I"{OUT}" "{ROOT}/tools/tests/radio_label.cpp" /Fe:"{OUT}/test.exe" /Fo:"{OUT}/test.obj"\n'
    f'if errorlevel 1 exit /b 1\n"{OUT}/test.exe" "{OUT}/mesh.json"\nif errorlevel 1 exit /b 1\n'
    f'cl /nologo /std:c++20 /EHsc /W4 /I"{ROOT}/native/src" "{ROOT}/tools/tests/radio_knob_state.cpp" /Fe:"{OUT}/knob.exe" /Fo:"{OUT}/knob.obj"\n'
    f'if errorlevel 1 exit /b 1\n"{OUT}/knob.exe"\n',encoding='utf8')
subprocess.run(['cmd','/c',str(cmd)],cwd=OUT,check=True)
# Check the same digit rows, not a similar-looking replacement font.
source=(ROOT/'native/src/Xr.cpp').read_text(encoding='utf8')
ammo=[int(v) for v in re.search(r'const int rows\[70\]=int\[70\]\((.*?)\);',source)[1].split(',')]
header=(ROOT/'native/src/RadioLabel.h').read_text(encoding='utf8')
digits=[int(v) for v in re.findall(r'\d+',re.search(r'Glyph digits\[\]\{(.*?)\};',header,re.S)[1])]
assert digits==ammo,'radio digits must exactly match the ammo counter'
assert 'PublishRadioText' not in source,'old transient HUD message must be removed'
if '--retail' in sys.argv:
    # Targeted ABI verification for the new code patch; no APK/game-data audit.
    data=(ROOT/'build/vehicle-lift-lever/retail.so').read_bytes()
    phoff=struct.unpack_from('<Q',data,32)[0];phsize,phnum=struct.unpack_from('<HH',data,54)
    segments=[struct.unpack_from('<IIQQQQQQ',data,phoff+i*phsize) for i in range(phnum)]
    def word(va):
        for kind,flags,offset,address,physical,filesz,memsz,align in segments:
            if kind==1 and address<=va<address+filesz:return struct.unpack_from('<I',data,offset+va-address)[0]
        raise AssertionError(hex(va))
    for offset,expected in [(0,0x3943a408),(0x4c,0xb9406e68),(0x100,0xb9407e68),(0x108,0x39c2b66a),
                            (0x15c,0x11177129),(0x16c,0x5281f40a),(0x188,0x5280fa09)]:
        assert word(0x3889a8+offset)==expected,(hex(offset),hex(word(0x3889a8+offset)))
    print('PASS ABI: native target/delta fields and original 1500/4000/2000 ms instructions match')
if '--runtime-only' in sys.argv:
    raise SystemExit(0)
shaders=re.findall(r'R"GLSL\((.*?)\)GLSL"',(ROOT/'native/src/RadioLabelRender.inl').read_text(encoding='utf8'),re.S)
mesh=json.loads((OUT/'mesh.json').read_text())
html=r'''<!doctype html><meta charset="utf-8"><canvas id="view" width="1000" height="240"></canvas><pre id="result">RUNNING</pre><script>
try {
const shaders=__SHADERS__,mesh=__MESH__;
const check=(v,m)=>{if(!v)throw Error(m)};
const gl=view.getContext('webgl2',{alpha:true,antialias:false,preserveDrawingBuffer:true});check(gl,'WebGL2');
const compile=(t,s)=>{let a=gl.createShader(t);gl.shaderSource(a,s);gl.compileShader(a);check(gl.getShaderParameter(a,gl.COMPILE_STATUS),gl.getShaderInfoLog(a));return a};
const p=gl.createProgram();gl.attachShader(p,compile(gl.VERTEX_SHADER,shaders[0]));gl.attachShader(p,compile(gl.FRAGMENT_SHADER,shaders[1]));gl.linkProgram(p);check(gl.getProgramParameter(p,gl.LINK_STATUS),gl.getProgramInfoLog(p));gl.useProgram(p);
gl.bindBuffer(gl.ARRAY_BUFFER,gl.createBuffer());gl.bufferData(gl.ARRAY_BUFFER,new Float32Array(mesh.vertices),gl.STATIC_DRAW);
gl.enableVertexAttribArray(0);gl.vertexAttribPointer(0,3,gl.FLOAT,false,12,0);
gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER,gl.createBuffer());gl.bufferData(gl.ELEMENT_ARRAY_BUFFER,new Uint16Array(mesh.indices),gl.STATIC_DRAW);
const u=n=>gl.getUniformLocation(p,n);
gl.uniformMatrix4fv(u('mvp'),false,new Float32Array([8,0,0,0,0,100/3,0,0,0,0,1,0,0,0,0,1]));
gl.uniform3f(u('center'),0,0,0);gl.uniform3f(u('axisRight'),1,0,0);gl.uniform3f(u('axisUp'),0,1,0);gl.uniform1f(u('radioOff'),0);
gl.viewport(0,0,1000,240);gl.enable(gl.DEPTH_TEST);gl.enable(gl.BLEND);gl.blendFunc(gl.SRC_ALPHA,gl.ONE_MINUS_SRC_ALPHA);gl.depthMask(false);gl.clearColor(0,0,0,0);
const draw=()=>gl.drawElements(gl.TRIANGLES,mesh.indices.length,gl.UNSIGNED_SHORT,0);
const pixels=()=>{let b=new Uint8Array(1000*240*4);gl.readPixels(0,0,1000,240,gl.RGBA,gl.UNSIGNED_BYTE,b);return b};
gl.clear(gl.COLOR_BUFFER_BIT|gl.DEPTH_BUFFER_BIT);draw();let a=pixels(),white=0,backdrop=0,clear=0;
for(let i=0;i<a.length;i+=4){if(a[i]>225&&a[i+3]===255)white++;if(a[i+3]>130&&a[i+3]<180)backdrop++;if(a[i+3]===0)clear++}
check(white>1000&&backdrop>1000&&clear>150000,'opaque white pixels and translucent compact backdrop');
gl.uniform1f(u('radioOff'),1);gl.clear(gl.COLOR_BUFFER_BIT);draw();let off=pixels(),gray=0;for(let i=0;i<off.length;i+=4)if(off[i]>185&&off[i]<205&&off[i+3]===255)gray++;check(gray>1000,'OFF text is readable gray');
// A foreground depth plane must fully occlude both text and backdrop.
gl.uniform1f(u('radioOff'),0);gl.depthMask(true);gl.clearDepth(.25);gl.clear(gl.COLOR_BUFFER_BIT|gl.DEPTH_BUFFER_BIT);gl.depthMask(false);draw();let hidden=pixels();check(hidden.every(v=>v===0),'foreground depth occludes label');
gl.depthMask(true);gl.clearDepth(1);gl.clear(gl.COLOR_BUFFER_BIT|gl.DEPTH_BUFFER_BIT);gl.depthMask(false);draw();
check(gl.getError()===gl.NO_ERROR,'no GL errors');result.textContent='PASS GPU: native shaders, white/gray pixel text, translucent backdrop and world-depth occlusion';
}catch(e){result.textContent='FAIL: '+e.stack}
</script>'''.replace('__SHADERS__',json.dumps(shaders)).replace('__MESH__',json.dumps(mesh))
page=OUT/'gpu-test.html';page.write_text(html,encoding='utf8')
edge=Path(r'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe')
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
result=subprocess.run([str(edge),'--headless','--no-first-run','--no-sandbox','--disable-gpu-sandbox',
    '--use-gl=angle','--use-angle=swiftshader','--enable-unsafe-swiftshader',
    '--user-data-dir='+str(OUT/'edge-profile'),'--virtual-time-budget=2000','--window-size=1100,400',
    '--screenshot='+str(OUT/'radio-label.png'),'--dump-dom',page.as_uri()],capture_output=True,text=True,encoding='utf8',errors='replace',timeout=60,startupinfo=startup)
(OUT/'gpu-result.html').write_text(result.stdout,encoding='utf8')
match=re.search(r'<pre id="result">(.*?)</pre>',result.stdout,re.S)
if not match or not match[1].startswith('PASS GPU:'):raise RuntimeError(match[1] if match else result.stderr[-2000:])
print(match[1]);print(OUT/'radio-label.png')

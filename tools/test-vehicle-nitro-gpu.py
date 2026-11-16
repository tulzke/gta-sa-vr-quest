"""Render the production eye-copy shaders: sharp centre, radial edges, AA on/off."""
import json
from pathlib import Path
import re
import subprocess

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/nitro-gpu-tests';OUT.mkdir(parents=True,exist_ok=True)
source=(ROOT/'native/src/Xr.cpp').read_text(encoding='utf8')
vertex=re.search(r'const char\* kVertexShader = R"\((.*?)\)";',source,re.S)[1]
common=re.search(r'R"GLSL\((.*?)\)GLSL"',(ROOT/'native/src/VehicleNitroSpeedShader.h').read_text(),re.S)[1]
shaders=[]
for name,next_name in [('kFxaaFragmentShader','kUnderwaterCopyFragmentShader'),('kUnderwaterCopyFragmentShader','kHudCompositeFragmentShader')]:
    start=source.index('const std::string '+name)
    block=source[start:source.index(next_name,start+1)]
    pieces=re.findall(r'R"\((.*?)\)"',block,re.S)
    assert len(pieces)==2,name
    shaders.append(pieces[0]+common+pieces[1])
html=(ROOT/'tools/tests/vehicle_nitro_gpu.html').read_text()
html=html.replace('__VS__',json.dumps(vertex)).replace('__SHADERS__',json.dumps(shaders))
page=OUT/'gpu-test.html';page.write_text(html,encoding='utf8')
edge=Path(r'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe')
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
result=subprocess.run([str(edge),'--headless','--no-first-run','--no-sandbox','--disable-gpu-sandbox',
    '--use-gl=angle','--use-angle=swiftshader','--enable-unsafe-swiftshader',
    '--user-data-dir='+str(OUT/'edge-profile'),'--virtual-time-budget=3000','--window-size=1050,850',
    '--screenshot='+str(OUT/'nitro-speed.png'),'--dump-dom',page.as_uri()],
    capture_output=True,text=True,encoding='utf8',errors='replace',timeout=60,startupinfo=startup)
(OUT/'gpu-result.html').write_text(result.stdout,encoding='utf8')
match=re.search(r'<pre id="result">(.*?)</pre>',result.stdout,re.S)
if not match or not match[1].startswith('PASS GPU:'):
    raise RuntimeError(match[1] if match else result.stderr[-2000:])
print(match[1]);print(OUT/'nitro-speed.png')

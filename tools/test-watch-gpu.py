"""Compile/render the exact watch GLES shader on the exported display mesh."""
import json
from pathlib import Path
import re
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'.cache/watch-runtime';out.mkdir(parents=True,exist_ok=True)
source=(ROOT/'native/src/WatchShaders.h').read_text()
vs,fs=re.findall(r'R"GLSL\((.*?)\)GLSL"',source,re.S)
layout=(ROOT/'native/src/WatchLayout.h').read_text()
constants={name:float(value) for name,value in re.findall(r'(kMapRadius|kMapTop|kMapBottom|kTextureScale)=(-?\.?\d+(?:\.\d*)?)f',layout)}
defines=''.join(f'#define {target} {constants[name]:.6f}\n' for name,target in [('kMapRadius','MAP_RADIUS'),('kMapTop','MAP_TOP'),('kMapBottom','MAP_BOTTOM'),('kTextureScale','MAP_SCALE')])
first,rest=fs.split('\n',1);fs=first+'\n'+defines+rest
data=(ROOT/'assets/watches/watches.glb').read_bytes()
length=struct.unpack_from('<I',data,12)[0];doc=json.loads(data[20:20+length]);binary=28+length
primitive=doc['meshes'][next(n['mesh'] for n in doc['nodes'] if n['name']=='WatchshellTop_RIS_Shader:PxrDisney2_0')]['primitives'][0]
def accessor(index):
    a=doc['accessors'][index];v=doc['bufferViews'][a['bufferView']]
    components={'SCALAR':1,'VEC2':2,'VEC3':3}[a['type']];kind={5123:'H',5125:'I',5126:'f'}[a['componentType']]
    offset=binary+v.get('byteOffset',0)+a.get('byteOffset',0)
    stride=v.get('byteStride',struct.calcsize('<'+kind*components))
    return [x for i in range(a['count']) for x in struct.unpack_from('<'+kind*components,data,offset+i*stride)]
html=(ROOT/'tools/tests/watch_gpu.html').read_text()
for key,value in {'VS':vs,'FS':fs,'POINTS':accessor(primitive['attributes']['POSITION']),'UV':accessor(primitive['attributes']['TEXCOORD_0']),'INDICES':accessor(primitive['indices'])}.items():
    html=html.replace('__'+key+'__',json.dumps(value))
page=out/'gpu-test.html';page.write_text(html,encoding='utf-8')
edge=Path(r'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe')
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
result=subprocess.run([str(edge),'--headless','--no-first-run','--no-sandbox','--disable-gpu-sandbox','--use-gl=angle','--use-angle=swiftshader','--enable-unsafe-swiftshader','--user-data-dir='+str(out/'edge-profile'),'--virtual-time-budget=5000','--window-size=1100,840','--screenshot='+str(out/'watch-contact-sheet.png'),'--dump-dom',page.as_uri()],capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=60,startupinfo=startup)
(out/'gpu-result.html').write_text(result.stdout,encoding='utf-8')
match=re.search(r'<pre id="result">(.*?)</pre>',result.stdout,re.S)
if not match or not match[1].startswith('PASS GPU:'):
    raise RuntimeError(match[1] if match else result.stderr[-2000:])
print(match[1]);print(out/'watch-contact-sheet.png')

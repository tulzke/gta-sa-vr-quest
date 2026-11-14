"""Compile/test the production GLB loader and pose sampler; fallback cannot pass.

First run export_hand_glb.py in Blender on a working copy. Fixtures compare all
source frames and their midpoints against evaluated Blender geometry.
"""
from pathlib import Path
import copy
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
GLB = ROOT / 'assets/vrhands/skeletal/hands.glb'
sys.path.insert(0,str(ROOT/'tools'))
from radio_png import rgba_png
width, height, pixels = rgba_png((ROOT/'assets/vrhands/BigHandsAlbedo.png').read_bytes())
assert (width,height)==(1024,1024)
assert len(pixels)==1024*1024*4
with tempfile.TemporaryDirectory(prefix='hand-skeleton-', dir=ROOT / '.cache', ignore_cleanup_errors=True) as temporary:
    output = Path(temporary)
    malformed = output / 'invalid'
    malformed.mkdir()
    data = GLB.read_bytes()
    length, kind = struct.unpack_from('<II', data, 12)
    original = json.loads(data[20:20+length])
    original_bin = data[28+length:]

    def fixture(name, edit):
        doc = copy.deepcopy(original)
        binary = bytearray(original_bin)
        edit(doc, binary)
        doc['buffers'][0]['byteLength']=len(binary)
        binary += b'\0' * (-len(binary)%4)
        encoded = json.dumps(doc, separators=(',', ':')).encode()
        encoded += b' ' * (-len(encoded) % 4)
        glb = struct.pack('<III', 0x46546c67, 2, 28+len(encoded)+len(binary))
        glb += struct.pack('<II', len(encoded), 0x4e4f534a)+encoded
        glb += struct.pack('<II', len(binary), 0x004e4942)+binary
        (malformed / (name+'.glb')).write_bytes(glb)

    def attribute(doc, name):
        return doc['accessors'][doc['meshes'][0]['primitives'][0]['attributes'][name]]

    def binary_edit(doc, binary, name, fmt, value):
        acc = attribute(doc, name)
        view = doc['bufferViews'][acc['bufferView']]
        struct.pack_into(fmt, binary, view.get('byteOffset', 0)+acc.get('byteOffset', 0), value)

    fixture('nan-position', lambda d,b: binary_edit(d,b,'POSITION','<f',float('nan')))
    fixture('bad-joint', lambda d,b: binary_edit(d,b,'JOINTS_0','<H',65535))
    fixture('negative-weight', lambda d,b: binary_edit(d,b,'WEIGHTS_0','<f',-1))
    fixture('weight-sum', lambda d,b: binary_edit(d,b,'WEIGHTS_0','<f',.1234))
    fixture('bounds', lambda d,b: d['bufferViews'][0].update(byteLength=999999999))
    fixture('overflow', lambda d,b: d['accessors'][0].update(count=999999999999999))
    fixture('sparse', lambda d,b: d['accessors'][0].update(sparse={'count': 1, 'indices': {'bufferView': 0, 'componentType': 5123}, 'values': {'bufferView': 0}}))
    fixture('cycle', lambda d,b: d['nodes'][1].update(children=[1]))
    def cubic(d,b):
        s=d['animations'][0]['samplers'][0]
        output=d['accessors'][s['output']]
        count=d['accessors'][s['input']]['count']*3
        view=len(d['bufferViews'])
        d['bufferViews'].append({'buffer':0,'byteOffset':len(b),'byteLength':count*3*4})
        b.extend(bytes(count*3*4))
        output.update(bufferView=view,count=count)
        s['interpolation']='CUBICSPLINE'
    fixture('cubic',cubic)
    fixture('missing-clip', lambda d,b: d['animations'].pop())
    fixture('duplicate-clip', lambda d,b: d['animations'][1].update(name='Grip'))
    fixture('external-buffer', lambda d,b: d['buffers'][0].update(uri='external.bin'))

    def bad_time(d,b):
        a=d['accessors'][d['animations'][0]['samplers'][0]['input']]
        v=d['bufferViews'][a['bufferView']]
        struct.pack_into('<f',b,v['byteOffset']+4,0)
    fixture('duplicate-time',bad_time)
    def accessor_edit(d,b,index,value,fmt='<f'):
        a=d['accessors'][index]
        v=d['bufferViews'][a['bufferView']]
        struct.pack_into(fmt,b,v.get('byteOffset',0)+a.get('byteOffset',0),value)
    fixture('inverse-bind',lambda d,b:accessor_edit(d,b,d['skins'][0]['inverseBindMatrices'],0))
    fixture('triangle-index',lambda d,b:accessor_edit(d,b,d['meshes'][0]['primitives'][0]['indices'],65535,'<H'))
    fixture('animation-scale',lambda d,b:accessor_edit(d,b,d['animations'][0]['samplers'][2]['output'],-1))
    (malformed/'truncated.glb').write_bytes(data[:100])
    exe = output / ('test.exe' if os.name == 'nt' else 'test')
    sources = [ROOT/'tools/tests/hand_skeleton.cpp', ROOT/'native/src/HandSkeleton.cpp']
    args = [str(GLB), str(ROOT/'.cache/hand-skeletal/reference.bin'), str(malformed)]
    if os.name == 'nt':
        where = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        base = subprocess.check_output([str(where), '-latest', '-products', '*', '-requires',
            'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
        vc = Path(base) / 'VC/Auxiliary/Build/vcvars64.bat'
        script = output / 'test.cmd'
        script.write_text(f'@echo off\ncall "{vc}" >nul\nif errorlevel 1 exit /b 1\n'
            f'cl /nologo /std:c++20 /EHsc /W4 /D_CRT_SECURE_NO_WARNINGS /I"{ROOT / "native/src"}" '
            + ' '.join(f'"{s}"' for s in sources)+f' /Fe:"{exe}"\n'
            'if errorlevel 1 exit /b 1\n'+f'"{exe}" '+' '.join(f'"{a}"' for a in args)+'\n', encoding='utf-8')
        subprocess.run(['cmd','/c',str(script)],cwd=output,check=True)
    else:
        subprocess.run([os.environ.get('CXX') or shutil.which('c++'),'-std=c++20','-Wall','-Wextra',
                        '-I'+str(ROOT/'native/src'),*(str(s) for s in sources),'-o',str(exe)],check=True)
        subprocess.run([str(exe),*args],check=True)

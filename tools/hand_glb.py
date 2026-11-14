"""Small offline reader for hand asset QA. Runtime uses HandSkeleton.cpp."""
import json
from pathlib import Path
import struct

ACTIONS = ('Grip', 'Pinch', 'Point', 'OpenPalm', 'Beckon', 'GripPistol',
           'GripPistolOffhand', 'Fist', 'GripPistolShoot', 'GripDeagle',
           'GripDeagleOffhand', 'GripRifle', 'GripShotgun', 'GripShotgunOffhand')


def action_key(name):
    return ''.join(c.lower() for c in name if c.isalnum())


def uv_key(uv):
    return tuple(round(float(x), 5) for x in uv)


class HandGlb:
    def __init__(self, path):
        data = Path(path).read_bytes()
        assert struct.unpack_from('<III',data)==(0x46546c67,2,len(data)), 'Not a GLB (run git lfs pull)'
        size,kind=struct.unpack_from('<II',data,12)
        assert kind==0x4e4f534a
        self.doc=json.loads(data[20:20+size])
        self.binary=data[28+size:]

    def accessor(self, index):
        a=self.doc['accessors'][index];view=self.doc['bufferViews'][a['bufferView']]
        width={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}[a['type']]
        code={5121:'B',5123:'H',5125:'I',5126:'f'}[a['componentType']]
        fmt='<'+str(width)+code;stride=view.get('byteStride',struct.calcsize(fmt))
        start=view.get('byteOffset',0)+a.get('byteOffset',0)
        return [struct.unpack_from(fmt,self.binary,start+i*stride) for i in range(a['count'])]

    def end_positions(self, clip_name):
        """CPU endpoint skinning for geometry-dependent interaction tests."""
        nodes=[dict(n) for n in self.doc['nodes']]
        clip=next(c for c in self.doc['animations'] if c['name']==clip_name)
        for channel in clip['channels']:
            sampler=clip['samplers'][channel['sampler']]
            nodes[channel['target']['node']][channel['target']['path']]=self.accessor(sampler['output'])[-1]
        parent={c:i for i,n in enumerate(nodes) for c in n.get('children',[])}
        def mul(a,b):
            return [sum(a[k*4+r]*b[c*4+k] for k in range(4)) for c in range(4) for r in range(4)]
        def matrix(n):
            if 'matrix' in n:return n['matrix']
            x,y,z,w=n.get('rotation',(0,0,0,1));s=n.get('scale',(1,1,1));t=n.get('translation',(0,0,0))
            m=[1-2*(y*y+z*z),2*(x*y+z*w),2*(x*z-y*w),0,
               2*(x*y-z*w),1-2*(x*x+z*z),2*(y*z+x*w),0,
               2*(x*z+y*w),2*(y*z-x*w),1-2*(x*x+y*y),0,*t,1]
            for c in range(3):
                for r in range(3):m[c*4+r]*=s[c]
            return m
        world={}
        def global_matrix(i):
            if i not in world:world[i]=mul(global_matrix(parent[i]),matrix(nodes[i])) if i in parent else matrix(nodes[i])
            return world[i]
        skin=self.doc['skins'][0];bind=self.accessor(skin['inverseBindMatrices'])
        palette=[mul(global_matrix(j),b) for j,b in zip(skin['joints'],bind)]
        attr=self.doc['meshes'][0]['primitives'][0]['attributes']
        pos=self.accessor(attr['POSITION'])
        joints=[self.accessor(attr[f'JOINTS_{i}']) for i in range(2)]
        weights=[self.accessor(attr[f'WEIGHTS_{i}']) for i in range(2)]
        result=[]
        for i,v in enumerate(pos):
            p=[0.,0.,0.]
            for group in range(2):
                for j,w in zip(joints[group][i],weights[group][i]):
                    m=palette[j]
                    for r in range(3):p[r]+=w*(m[r]*v[0]+m[4+r]*v[1]+m[8+r]*v[2]+m[12+r])
            result.append((p[0],-p[2],p[1]))
        return result

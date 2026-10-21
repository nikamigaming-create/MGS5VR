"""Pack the existing MIT WebXR Touch Plus models for the native headset editor."""
import hashlib,json,math,pathlib,struct
ROOT=pathlib.Path(__file__).resolve().parents[1]
SOURCE=ROOT/'tools/field-guide/assets'
DEST=ROOT/'assets/controls'

def mul(a,b):return [[sum(a[r][k]*b[k][c] for k in range(4)) for c in range(4)] for r in range(4)]
def matrix(node):
    if 'matrix' in node:return [[node['matrix'][c*4+r] for c in range(4)] for r in range(4)]
    x,y,z,w=node.get('rotation',[0,0,0,1]);s=node.get('scale',[1,1,1]);t=node.get('translation',[0,0,0])
    m=[[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w),t[0]],
       [2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w),t[1]],
       [2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y),t[2]],[0,0,0,1]]
    for r in range(3):
        for c in range(3):m[r][c]*=s[c]
    return m
def transform(m,v,point=True):return [sum(m[r][k]*v[k] for k in range(3))+(m[r][3] if point else 0) for r in range(3)]
def unit(v):n=math.sqrt(sum(x*x for x in v));return [x/n for x in v]
def cross(a,b):return [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]

DEST.mkdir(parents=True,exist_ok=True)
records=[]
for hand in ('left','right'):
    file=SOURCE/f'meta-quest-touch-plus-{hand}.glb';data=file.read_bytes()
    assert data[:4]==b'glTF' and struct.unpack_from('<I',data,4)[0]==2
    length,kind=struct.unpack_from('<II',data,12);assert kind==0x4e4f534a
    doc=json.loads(data[20:20+length]);binary=data[28+length:]
    worlds={}
    def visit(index,parent):
        node=doc['nodes'][index];worlds[index]=mul(parent,matrix(node))
        for child in node.get('children',[]):visit(child,worlds[index])
    identity=[[int(r==c) for c in range(4)] for r in range(4)]
    for node in doc['scenes'][doc.get('scene',0)]['nodes']:visit(node,identity)
    def access(index):
        a=doc['accessors'][index];v=doc['bufferViews'][a['bufferView']]
        fmt={5126:'f',5125:'I',5123:'H',5121:'B'}[a['componentType']]
        count={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']];size=struct.calcsize(fmt)*count
        offset=v.get('byteOffset',0)+a.get('byteOffset',0);stride=v.get('byteStride',size)
        return [struct.unpack_from('<'+fmt*count,binary,offset+n*stride) for n in range(a['count'])]
    geometry=[]
    for index,world in worlds.items():
        node=doc['nodes'][index]
        if 'mesh' not in node:continue
        for primitive in doc['meshes'][node['mesh']]['primitives']:
            assert primitive.get('mode',4)==4
            a=primitive['attributes'];p=access(a['POSITION']);normal=access(a['NORMAL']);uv=access(a['TEXCOORD_0'])
            geometry.append((node['name'],[transform(world,v) for v in p],
                [unit(transform(world,v,False)) for v in normal],uv,[v[0] for v in access(primitive['indices'])]))
    body=next(g[1] for g in geometry if g[0]=='controller_mesh')
    center=[(min(v[c] for v in body)+max(v[c] for v in body))/2 for c in range(3)]
    front=unit([-.025 if hand=='left' else .025,.779,-.624]);right=unit(cross([0,-.624,-.779],front));up=unit(cross(front,right))
    def orient(v):return [sum(a*b for a,b in zip(axis,v)) for axis in (right,up,front)]
    def positioned(p):return orient([a-b for a,b in zip(p,center)])
    masks={'a_button':(1<<0)|(1<<21),'b_button':(1<<1)|(1<<22),'x_button':(1<<2)|(1<<23),'y_button':(1<<3)|(1<<24),
        'squeeze':1<<(7 if hand=='left' else 8),'trigger':(1<<(9 if hand=='left' else 10))|(1<<(27 if hand=='left' else 28)),
        'thumbstick':(1<<(5 if hand=='left' else 6))|(1<<(25 if hand=='left' else 26))|(15<<(11 if hand=='left' else 15))}
    parts=[]
    for name,p,n,uv,indices in geometry:
        vertices=[tuple(positioned(a)+orient(b)+list(c)) for a,b,c in zip(p,n,uv)]
        parts.append((masks.get(name,0),vertices,indices))
    anchors=[(1<<(19 if hand=='left' else 20),next(transform(worlds[i],[0,0,0]) for i,n in enumerate(doc['nodes']) if n['name']=='thumbrest_pressed_value'))]
    if hand=='left':anchors.append((1<<4,json.loads((SOURCE/'controller-spatial-anchors.json').read_text())['left_menu']['gltf_world_position']))
    for mask,p in anchors:
        p=positioned(p);vertices=[tuple(p[:2]+[p[2]+.004]+[0,0,1]+[.5,.5])]
        for i in range(17):
            a=i*math.tau/16;vertices.append((p[0]+math.cos(a)*.003,p[1]+math.sin(a)*.003,p[2]+.004,0,0,1,.5,.5))
        parts.append((mask,vertices,[i for n in range(1,17) for i in (0,n,n+1)]))
    packed=bytearray(b'M5CT'+struct.pack('<II',1,len(parts)))
    for mask,vertices,indices in parts:
        packed+=struct.pack('<III',mask,len(vertices),len(indices))
        for vertex in vertices:packed+=struct.pack('<8f',*vertex)
        packed+=struct.pack('<'+'I'*len(indices),*indices)
    (DEST/f'{hand}.mesh').write_bytes(packed)
    image=doc['images'][0];view=doc['bufferViews'][image['bufferView']];start=view.get('byteOffset',0)
    (DEST/f'{hand}.png').write_bytes(binary[start:start+view['byteLength']])
    records.append({'hand':hand,'source':str(file.relative_to(ROOT)),'source_sha256':hashlib.sha256(data).hexdigest(),
        'parts':len(parts),'vertices':sum(len(p[1]) for p in parts),'indices':sum(len(p[2]) for p in parts)})
(DEST/'LICENSE.txt').write_bytes((SOURCE/'LICENSE-webxr-input-profiles.md').read_bytes())
(DEST/'provenance.json').write_text(json.dumps({'source':'Existing MIT WebXR Input Profiles assets','models':records},indent=2)+'\n')
print(json.dumps(records,indent=2))

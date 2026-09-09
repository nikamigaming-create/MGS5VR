"""Render the accessible control schema as a code-native SVG companion card."""
import html,json,pathlib,textwrap
root=pathlib.Path(__file__).resolve().parents[1]
schema=json.loads((root/'docs/CONTROL_SCHEMA.json').read_text(encoding='utf-8'))
parts=[]
def text(x,y,value,size=20,weight=400,color='#252625'):
    parts.append(f'<text x="{x}" y="{y}" font-size="{size}" font-weight="{weight}" fill="{color}">{html.escape(value)}</text>')
def rect(x,y,w,h,color):parts.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{color}"/>')
rect(0,0,1440,3000,'#f2efe5');rect(40,40,12,84,'#af2624')
text(76,83,schema['title'],44,900);text(77,119,schema['edition'],20)
y=164
for mode in schema['modes']:
    rect(40,y,1360,48,'#272928');text(61,y+33,mode['title'],24,800,'#fffaf0');y+=74
    for control,action in mode['rows']:
        a=textwrap.wrap(action,78) or [''];c=textwrap.wrap(control,34) or ['']
        for n,line in enumerate(c):text(62,y+n*25,line,21,700)
        for n,line in enumerate(a):text(496,y+n*25,line,21)
        y+=max(len(a),len(c))*25+13
    y+=21
rect(40,y,1360,2,'#af2624');y+=36
for line in textwrap.wrap(schema['footer'],112):text(62,y,line,19);y+=27
svg=f'<svg xmlns="http://www.w3.org/2000/svg" width="1440" height="{y+25}" viewBox="0 0 1440 {y+25}" role="img" aria-labelledby="title desc"><title id="title">MGS5VR complete controller modes</title><desc id="desc">{html.escape(schema["edition"])}</desc><g font-family="Arial, Helvetica, sans-serif">'+''.join(parts)+'</g></svg>'
(root/'docs/images/control-modes.svg').write_text(svg,encoding='utf-8')
print('Updated docs/images/control-modes.svg from docs/CONTROL_SCHEMA.json')

"""Convert the e1_*.json sheet specs (mm) into a draw.io file: frame, title block, vertical
designation box and the block diagram. 1 draw.io px = 1/100 inch -> mm * 3.937."""
import sys, json, io
from xml.sax.saxutils import escape
sys.stdout.reconfigure(encoding='utf-8')
K = 100 / 25.4

def mm(v): return round(v * K, 2)

class Page:
    def __init__(self, name):
        self.name = name; self.cells = []; self.n = 1
    def nid(self):
        self.n += 1; return f'c{self.n}'
    def rect(self, x, y, w, h, lw, dashed=False, extra=''):
        st = f'rounded=0;whiteSpace=wrap;html=1;fillColor=none;strokeColor=#000000;strokeWidth={mm(lw):.2f};{"dashed=1;" if dashed else ""}{extra}'
        self.cells.append(f'<mxCell id="{self.nid()}" value="" style="{st}" vertex="1" parent="1"><mxGeometry x="{mm(x)}" y="{mm(y)}" width="{mm(w)}" height="{mm(h)}" as="geometry"/></mxCell>')
    def line(self, x1, y1, x2, y2, lw, dashed=False, pts=None, arrow=False, arrow0=False):
        st = f'html=1;rounded=0;strokeColor=#000000;strokeWidth={mm(lw):.2f};endArrow={"open" if arrow else "none"};startArrow={"open" if arrow0 else "none"};endFill=0;startFill=0;endSize=8;startSize=8;{"dashed=1;dashPattern=8 4;" if dashed else ""}'
        way = ''
        if pts:
            way = '<Array as="points">' + ''.join(f'<mxPoint x="{mm(px)}" y="{mm(py)}"/>' for px, py in pts) + '</Array>'
        self.cells.append(f'<mxCell id="{self.nid()}" value="" style="{st}" edge="1" parent="1"><mxGeometry relative="1" as="geometry">{way}<mxPoint x="{mm(x1)}" y="{mm(y1)}" as="sourcePoint"/><mxPoint x="{mm(x2)}" y="{mm(y2)}" as="targetPoint"/></mxGeometry></mxCell>')
    def text(self, t, x, y, w, h, pt, align='center', italic=False, bold=False, rotation=0, font='Arial'):
        st = (f'text;html=1;strokeColor=none;fillColor=none;align={align};verticalAlign=middle;whiteSpace=wrap;'
              f'fontFamily={font};fontSize={round(pt*100/72)};fontStyle={(1 if bold else 0) + (2 if italic else 0)};rotation={rotation};spacing=0;spacingLeft=2;')
        val = escape(t.replace('\\n', '\n')).replace('\n', '&lt;br&gt;')
        self.cells.append(f'<mxCell id="{self.nid()}" value="{val}" style="{st}" vertex="1" parent="1"><mxGeometry x="{mm(x)}" y="{mm(y)}" width="{mm(w)}" height="{mm(h)}" as="geometry"/></mxCell>')
    def box(self, t, x, y, w, h, lw, pt):
        st = f'rounded=0;whiteSpace=wrap;html=1;fillColor=#ffffff;strokeColor=#000000;strokeWidth={mm(lw):.2f};fontFamily=Arial;fontSize={round(pt*100/72)};align=center;verticalAlign=middle;'
        val = escape(t.replace('\\n', '\n')).replace('\n', '&lt;br&gt;')
        self.cells.append(f'<mxCell id="{self.nid()}" value="{val}" style="{st}" vertex="1" parent="1"><mxGeometry x="{mm(x)}" y="{mm(y)}" width="{mm(w)}" height="{mm(h)}" as="geometry"/></mxCell>')
    def xml(self, SW, SH):
        return (f'<diagram id="{self.name}" name="{self.name}"><mxGraphModel dx="1000" dy="600" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" '
                f'pageWidth="{round(SW * K)}" pageHeight="{round(SH * K)}" math="0" shadow="0"><root><mxCell id="0"/><mxCell id="1" parent="0"/>'
                + ''.join(self.cells) + '</root></mxGraphModel></diagram>')

def build(spec_path, name):
    s = json.load(io.open(spec_path, encoding='utf-8'))
    SW, SH = s['sheet_w'], s['sheet_h']
    lwM, lwT, lwL = s['lw_main'], s['lw_thin'], s['lw_line']
    pg = Page(name)
    fx, fy, fw, fh = 20.0, 5.0, SW - 25.0, SH - 10.0
    pg.rect(fx, fy, fw, fh, lwM)
    tb = s['stamp']; sx, sy = fx + fw - 185.0, fy + fh - 55.0
    pg.rect(sx, sy, 185, 55, lwM)
    pg.line(sx + 65, sy, sx + 65, sy + 55, lwM)
    pg.line(sx + 7, sy, sx + 7, sy + 25, lwM)
    for xx in (17, 40, 55): pg.line(sx + xx, sy, sx + xx, sy + 55, lwM)
    for yy in (5, 10, 15, 20): pg.line(sx, sy + yy, sx + 65, sy + yy, lwT)
    pg.line(sx, sy + 25, sx + 65, sy + 25, lwM)
    for yy in (30, 35, 40, 45, 50): pg.line(sx, sy + yy, sx + 65, sy + yy, lwT)
    for t, x, w in (('Изм.', 0, 7), ('Лист', 7, 10), ('№ докум.', 17, 23), ('Подп.', 40, 15), ('Дата', 55, 10)):
        pg.text(t, sx + x, sy + 20, w, 5, 6.5, italic=True)
    roles = ['Разраб.', 'Пров.', '', '', 'Н.контр.', 'Утв.']
    names = [tb['developer'], tb['checker'], '', '', tb['normcontrol'], tb['approved']]
    for i in range(6):
        if roles[i]: pg.text(roles[i], sx, sy + 25 + 5 * i, 17, 5, 8, align='left', italic=True)
        if names[i]: pg.text(names[i], sx + 17, sy + 25 + 5 * i, 23, 5, 8, align='left', italic=True)
    pg.line(sx + 65, sy + 15, sx + 185, sy + 15, lwM)
    pg.line(sx + 135, sy + 15, sx + 135, sy + 55, lwM)
    pg.line(sx + 135, sy + 20, sx + 185, sy + 20, lwT)
    pg.line(sx + 135, sy + 25, sx + 185, sy + 25, lwM)
    pg.line(sx + 150, sy + 15, sx + 150, sy + 25, lwM)
    pg.line(sx + 165, sy + 15, sx + 165, sy + 25, lwM)
    pg.line(sx + 140, sy + 20, sx + 140, sy + 25, lwT)
    pg.line(sx + 145, sy + 20, sx + 145, sy + 25, lwT)
    pg.text('Лит.', sx + 135, sy + 15, 15, 5, 6.5, italic=True)
    pg.text('Лист', sx + 150, sy + 15, 15, 5, 6.5, italic=True)
    pg.text('Листов', sx + 165, sy + 15, 20, 5, 6.5, italic=True)
    pg.text(tb['sheet'], sx + 150, sy + 20, 15, 5, 8, italic=True)
    pg.text(tb['sheets'], sx + 165, sy + 20, 20, 5, 8, italic=True)
    pg.text(tb['designation'], sx + 65, sy, 120, 15, 20, italic=True)
    pg.text(tb['name'], sx + 66, sy + 15, 68, 40, 11, italic=True)
    pg.text(tb['org'], sx + 135, sy + 25, 50, 30, 12, italic=True)
    # vertical designation box 14 x 70 at the top-right (text rotated -90: box geometry given as unrotated 70x14 centred)
    vx, vy = fx + fw - 14.0, fy
    pg.rect(vx, vy, 14, 70, lwM)
    pg.text(tb['designation'], vx + 7 - 35, vy + 35 - 7, 70, 14, 13.5, italic=True, rotation=-90)
    for it in s['items']:
        if it['t'] == 'box':
            pg.box(it['text'], it['x'], it['y'], it['w'], it['h'], lwL, it.get('size', 12))
        elif it['t'] == 'line':
            pts = it['pts']
            pg.line(pts[0][0], pts[0][1], pts[-1][0], pts[-1][1], lwL, dashed=it.get('dashed', False),
                    pts=pts[1:-1], arrow=it.get('arrow', False), arrow0=it.get('arrow0', False))
        elif it['t'] == 'dash':
            pg.rect(it['x'], it['y'], it['w'], it['h'], lwL, extra='dashed=1;dashPattern=10 3 1.5 3;')
        elif it['t'] == 'text':
            pg.text(it['text'], it['x'], it['y'], it['w'], it['h'], it.get('size', 12), align='left' if it.get('a') == 'l' else 'center')
    return pg.xml(SW, SH)

out = sys.argv[1]
pages = [build(sys.argv[2], 'Э1 А3'), build(sys.argv[3], 'Э1 А4')]
io.open(out, 'w', encoding='utf-8').write('<mxfile host="app.diagrams.net">' + ''.join(pages) + '</mxfile>')
print('saved', out)

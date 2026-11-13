"""Понятная инструкция IRFB4110: схема по блокам, сборка шагами, закупка, проверка.
Запуск: py -3 make_clear_guide.py
Создаёт ../IRFB4110_сборка_поэтапно.html (один файл, работает без интернета) и ../IRFB4110_общая_схема_пайки.svg.
Перед записью проверяет: рисунок схемы даёт те же 19 сетей, что и перечень соединений overall-connections.json."""
import base64, collections, html, io, json, os, sys
from PIL import Image
sys.stdout.reconfigure(encoding='utf-8')
HERE = os.path.dirname(os.path.abspath(__file__))
def rd(n): return json.load(io.open(os.path.join(HERE, n), encoding='utf-8'))
CONN, STEPS, BUY = rd('overall-connections.json'), rd('assembly-steps.json'), rd('purchase-items.json')

# ── цвета сетей ────────────────────────────────────────────────────────────────
COL = {'B+': '#d32f2f', 'BATT': '#d32f2f', 'FUSED': '#d32f2f', 'D': '#8d5524', 'GND': '#1f2a37', '+12V': '#e07000', 'VPRE': '#b08900',
       'AUX1': '#b08900', 'AUX2': '#b08900', 'GPIO': '#7b34b7', 'IN': '#7b34b7', 'EN': '#12805c', 'TH': '#12805c',
       'OUTA': '#185bbd', 'OUTB': '#00849b', 'G1': '#185bbd', 'G2': '#185bbd', 'G3': '#00849b', 'G4': '#00849b'}
NETNAME = {'B+': 'B+ — плюс после выключателя', 'BATT': 'Плюс аккумулятора до F1', 'FUSED': 'Между F1 и S1', 'D': 'D — стоки, минус электролизёра',
           'GND': 'GND / S — минус аккумулятора', '+12V': '+12 В — питание драйвера', 'VPRE': 'VPRE — вход стабилизатора',
           'AUX1': 'Между F2 и D1', 'AUX2': 'Между D1 и R13', 'GPIO': 'Сигнал GPIO32', 'IN': 'IN — входы драйвера',
           'EN': 'EN — разрешение драйвера', 'TH': 'Между T1 и R11', 'OUTA': 'OUTA — выход A', 'OUTB': 'OUTB — выход B',
           'G1': 'Затвор Q1', 'G2': 'Затвор Q2', 'G3': 'Затвор Q3', 'G4': 'Затвор Q4'}
THICK = {'B+', 'BATT', 'FUSED', 'D', 'GND'}

svg, segs, ports, flags = [], [], {}, []
def esc(s): return html.escape(str(s), quote=True)
def wire(net, *pts):
    w = 5 if net in THICK else 2.6
    for a, b in zip(pts, pts[1:]):
        assert a[0] == b[0] or a[1] == b[1], ('только прямые углы', net, a, b)
        segs.append((net, a, b))
    p = ' '.join(f'{x},{y}' for x, y in pts)
    svg.append(f'<polyline class="w" data-net="{net}" points="{p}" fill="none" stroke="{COL[net]}" stroke-width="{w}" stroke-linejoin="round" stroke-linecap="round"/>')
def dot(net, x, y): svg.append(f'<circle class="w" data-net="{net}" cx="{x}" cy="{y}" r="5" fill="{COL[net]}"/>')
def text(x, y, s, cls='t', anchor='start', extra=''):
    svg.append(f'<text x="{x}" y="{y}" class="{cls}" text-anchor="{anchor}" {extra}>{esc(s)}</text>')
def port(pid, net, x, y):
    assert pid in CONN['nodes'], pid
    ports[pid] = {'net': net, 'x': x, 'y': y}
def flag(net, x, y, side, label=None):
    """метка сети: такая же метка в другом месте листа = тот же провод"""
    label = label or net
    wd = 12 + 8.2 * len(label); h = 24; flags.append((net, x, y))
    if side == 'r': pts = f'{x},{y} {x+10},{y-h/2} {x+10+wd},{y-h/2} {x+10+wd},{y+h/2} {x+10},{y+h/2}'; tx = x + 10 + wd / 2; ty = y + 5
    elif side == 'l': pts = f'{x},{y} {x-10},{y-h/2} {x-10-wd},{y-h/2} {x-10-wd},{y+h/2} {x-10},{y+h/2}'; tx = x - 10 - wd / 2; ty = y + 5
    elif side == 'u': pts = f'{x},{y} {x+wd/2},{y-10} {x+wd/2},{y-10-h} {x-wd/2},{y-10-h} {x-wd/2},{y-10}'; tx = x; ty = y - 17
    else: pts = f'{x},{y} {x+wd/2},{y+10} {x+wd/2},{y+10+h} {x-wd/2},{y+10+h} {x-wd/2},{y+10}'; tx = x; ty = y + 27
    svg.append(f'<g class="flag w" data-net="{net}"><polygon points="{pts}" fill="#fff" stroke="{COL[net]}" stroke-width="2"/><text x="{tx}" y="{ty}" class="fl" style="fill:{COL[net]}" text-anchor="middle">{esc(label)}</text></g>')

class part:
    def __init__(self, pid): self.pid = pid
    def __enter__(self): svg.append(f'<g class="part" data-part="{esc(self.pid)}" tabindex="0" role="button" aria-label="{esc(self.pid)}">')
    def __exit__(self, *a): svg.append('</g>')
def lead(a, b): svg.append(f'<line class="s" x1="{a[0]}" y1="{a[1]}" x2="{b[0]}" y2="{b[1]}"/>')
def hit(x, y, w, h): svg.append(f'<rect class="hit" x="{x}" y="{y}" width="{w}" height="{h}"/>')

def res(pid, val, x, y, vert=False, nets=('', ''), swap=False):
    n1, n2 = nets
    with part(pid):
        if vert:
            a, b = (x, y - 50), (x, y + 50)
            hit(x - 16, y - 50, 90, 100)
            lead(a, (x, y - 26)); lead((x, y + 26), b)
            svg.append(f'<rect class="b" x="{x-10}" y="{y-26}" width="20" height="52"/>')
            text(x + 17, y - 4, pid, 'ref'); text(x + 17, y + 13, val, 'val')
        else:
            a, b = (x - 50, y), (x + 50, y)
            hit(x - 50, y - 34, 100, 72)
            lead(a, (x - 28, y)); lead((x + 28, y), b)
            svg.append(f'<rect class="b" x="{x-28}" y="{y-10}" width="56" height="20"/>')
            text(x, y - 16, pid, 'ref', 'middle'); text(x, y + 27, val, 'val', 'middle')
    if swap: port(pid + '.2', n1, *a); port(pid + '.1', n2, *b)
    else: port(pid + '.1', n1, *a); port(pid + '.2', n2, *b)

def cap(pid, val, x, y, nets, polar, left=False):
    with part(pid):
        hit(x - 70 if left else x - 16, y - 50, 86 if left else 74, 100)
        lead((x, y - 50), (x, y - 8)); lead((x, y + 8), (x, y + 50))
        svg.append(f'<line class="s2" x1="{x-16}" y1="{y-8}" x2="{x+16}" y2="{y-8}"/>')
        if polar:
            svg.append(f'<rect class="fillk" x="{x-16}" y="{y+5}" width="32" height="6"/>')
            text(x - 19 if left else x - 28, y - 14, '+', 'plus')
        else: svg.append(f'<line class="s2" x1="{x-16}" y1="{y+8}" x2="{x+16}" y2="{y+8}"/>')
        tx, an = (x - 21, 'end') if left else (x + 21, 'start')
        text(tx, y - 8, pid, 'ref', an)
        for i, v in enumerate(val.split('|')): text(tx, y + 8 + 15 * i, v, 'val', an)
    port(pid + '.+', nets[0], x, y - 50); port(pid + '.-', nets[1], x, y + 50)

def diode_shape(x, y, vert, kind):
    """катод: сверху (вертикально) или справа/слева через ось"""
    if vert:
        svg.append(f'<polygon class="dk" points="{x-13},{y+11} {x+13},{y+11} {x},{y-11}"/>')
        if kind == 'd': svg.append(f'<line class="s2" x1="{x-14}" y1="{y-11}" x2="{x+14}" y2="{y-11}"/>')
        if kind == 'tvs': svg.append(f'<polyline class="s2" fill="none" points="{x-19},{y-5} {x-14},{y-11} {x+14},{y-11} {x+19},{y-17}"/>')
        if kind == 'z': svg.append(f'<polyline class="s2" fill="none" points="{x-19},{y-16} {x-14},{y-11} {x+14},{y-11} {x+19},{y-6}"/>')

def dv(pid, val, x, y, nets, kind, left=False):
    with part(pid):
        hit(x - 84 if left else x - 20, y - 50, 104 if left else 96, 100)
        lead((x, y - 50), (x, y - 11)); lead((x, y + 11), (x, y + 50))
        diode_shape(x, y, True, kind)
        tx, an = (x - 24, 'end') if left else (x + 24, 'start')
        text(tx, y - 4, pid, 'ref', an)
        for i, v in enumerate(val.split('|')): text(tx, y + 12 + 15 * i, v, 'val', an)
    port(pid + '.K', nets[0], x, y - 50); port(pid + '.A', nets[1], x, y + 50)

def dh(pid, val, x, y, nets, k_left=False):
    """горизонтальный диод; nets = (анод, катод)"""
    a, k = ((x + 35, y), (x - 35, y)) if k_left else ((x - 35, y), (x + 35, y))
    s = 1 if not k_left else -1
    with part(pid):
        hit(x - 35, y - 34, 70, 72)
        lead(a, (x - 11 * s, y)); lead((x + 11 * s, y), k)
        svg.append(f'<polygon class="dk" points="{x-11*s},{y-13} {x-11*s},{y+13} {x+11*s},{y}"/>')
        svg.append(f'<line class="s2" x1="{x+11*s}" y1="{y-14}" x2="{x+11*s}" y2="{y+14}"/>')
        text(x, y - 19, pid, 'ref', 'middle'); text(x, y + 30, val, 'val', 'middle')
    port(pid + '.A', nets[0], *a); port(pid + '.K', nets[1], *k)

def fuse(pid, val, x, y, nets):
    with part(pid):
        hit(x - 35, y - 34, 70, 52)
        lead((x - 35, y), (x + 35, y))
        svg.append(f'<rect class="b" x="{x-22}" y="{y-9}" width="44" height="18"/><line class="s" x1="{x-22}" y1="{y}" x2="{x+22}" y2="{y}"/>')
        text(x, y - 15, pid, 'ref', 'middle'); text(x, y + 26, val, 'val', 'middle')
    port(pid + '.1', nets[0], x - 35, y); port(pid + '.2', nets[1], x + 35, y)

def switch(pid, val, x, y, nets, nc=False):
    with part(pid):
        hit(x - 35, y - 42, 70, 60)
        lead((x - 35, y), (x - 16, y)); lead((x + 16, y), (x + 35, y))
        y2 = y + 2 if nc else y - 16
        svg.append(f'<circle class="o" cx="{x-16}" cy="{y}" r="3"/><circle class="o" cx="{x+16}" cy="{y}" r="3"/><line class="s2" x1="{x-14}" y1="{y-1}" x2="{x+16}" y2="{y2-3}"/>')
        text(x, y - 23, f'{pid} · {val}', 'ref', 'middle')
    port(pid + '.1', nets[0], x - 35, y); port(pid + '.2', nets[1], x + 35, y)

def nmos(pid, x, y, g):
    with part(pid):
        hit(x - 45, y - 65, 100, 130)
        lead((x, y - 65), (x, y - 22)); lead((x, y + 22), (x, y + 65))
        svg.append(f'<line class="s" x1="{x}" y1="{y-22}" x2="{x-14}" y2="{y-22}"/><line class="s" x1="{x}" y1="{y+22}" x2="{x-14}" y2="{y+22}"/><line class="s" x1="{x}" y1="{y}" x2="{x-14}" y2="{y}"/><line class="s" x1="{x}" y1="{y}" x2="{x}" y2="{y+22}"/>')
        svg.append(f'<line class="s2" x1="{x-14}" y1="{y-28}" x2="{x-14}" y2="{y+28}"/><line class="s2" x1="{x-21}" y1="{y-22}" x2="{x-21}" y2="{y+22}"/>')
        lead((x - 45, y), (x - 21, y))
        svg.append(f'<polygon class="dk" points="{x-14},{y} {x-6},{y-5} {x-6},{y+5}"/>')
        text(x + 8, y - 40, 'D 2', 'pin'); text(x + 8, y + 52, 'S 3', 'pin'); text(x - 24, y - 8, 'G 1', 'pin', 'end')
        text(x + 8, y - 15, pid, 'ref')
    port(pid + '.1', g, x - 45, y); port(pid + '.2', 'D', x, y - 65); port(pid + '.3', 'GND', x, y + 65)

def dual(pid, x, top, bot):
    y = (top + bot) / 2
    with part(pid):
        hit(x - 34, top, 108, bot - top)
        lead((x, top), (x, y - 26))
        svg.append(f'<line class="s" x1="{x-22}" y1="{y-26}" x2="{x+22}" y2="{y-26}"/>')
        for dx in (-22, 22):
            lead((x + dx, y - 26), (x + dx, y - 11)); lead((x + dx, y + 11), (x + dx, bot))
            svg.append(f'<polygon class="dk" points="{x+dx-11},{y+11} {x+dx+11},{y+11} {x+dx},{y-11}"/><line class="s2" x1="{x+dx-12}" y1="{y-11}" x2="{x+dx+12}" y2="{y-11}"/>')
        text(x + 6, top + 18, '2', 'pin'); text(x - 38, bot - 8, '1', 'pin'); text(x + 27, bot - 8, '3', 'pin')
        text(x + 37, y - 6, pid, 'ref'); text(x + 37, y + 10, 'MBR60100CT', 'val')
    port(pid + '.2', 'B+', x, top); port(pid + '.1', 'D', x - 22, bot); port(pid + '.3', 'D', x + 22, bot)

def box(pid, x, y, w, h, title, sub, pins, top=False):
    """pins: (порт, сторона l/r/b/t, смещение, подпись, сеть)"""
    with part(pid):
        svg.append(f'<rect class="b box" x="{x}" y="{y}" width="{w}" height="{h}" rx="6"/>')
        ty = y + 20 if top else y + h / 2 - (6 if sub else -5)
        text(x + w / 2, ty, title, 'ref', 'middle')
        for i, v in enumerate(sub.split('|') if sub else []): text(x + w / 2, ty + 17 + 15 * i, v, 'val', 'middle')
        for pp, side, off, lab, net in pins:
            if side == 'l': px, py = x, y + off; text(x + 7, py + 5, lab, 'pin')
            elif side == 'r': px, py = x + w, y + off; text(x + w - 7, py + 5, lab, 'pin', 'end')
            elif side == 'b': px, py = x + off, y + h; text(px, py - 7, lab, 'pin', 'middle')
            else: px, py = x + off, y; text(px, py + 15, lab, 'pin', 'middle')
            port(f'{pid}.{pp}', net, px, py)

def card(x, y, w, h, n, title, sub, bid):
    svg.append(f'<g id="{bid}"><rect class="card" x="{x}" y="{y}" width="{w}" height="{h}" rx="12"/><circle cx="{x+24}" cy="{y+22}" r="13" class="num"/><text x="{x+24}" y="{y+27}" class="numt" text-anchor="middle">{n}</text><text x="{x+46}" y="{y+28}" class="h">{esc(title)}</text><text x="{x+46+len(title)*10.4+14}" y="{y+28}" class="sub">{esc(sub)}</text></g>')

# ══ БЛОК 1: СИЛОВАЯ ЦЕПЬ ══════════════════════════════════════════════════════════
card(20, 20, 1560, 420, 1, 'Силовая цепь', 'большой ток: толстые провода и шины', 'b1')
BP, DB, GD = 120, 225, 400           # рельсы B+, D, GND
box('АКБ', 40, 95, 110, 330, 'АКБ', '24 В Li-ion|после BMS', [('P+', 'r', BP - 95, 'P+', 'BATT'), ('P-', 'r', GD - 95, 'P−', 'GND')])
wire('BATT', (150, BP), (195, BP))
fuse('F1', '80 А', 230, BP, ('BATT', 'FUSED'))
wire('FUSED', (265, BP), (305, BP))
switch('S1', 'выкл.', 340, BP, ('FUSED', 'B+'))
wire('B+', (375, BP), (870, BP))
wire('B+', (400, BP), (400, 85)); dot('B+', 400, BP); flag('B+', 400, 85, 'r', 'B+ → к F2')
yc = (BP + GD) / 2
for x, c, v, pol in ((440, 'C1', '1000 мкФ|63 В', True), (540, 'C2', '1000 мкФ|63 В', True), (640, 'C3', '1 мкФ|100 В', False)):
    wire('B+', (x, BP), (x, yc - 50)); wire('GND', (x, yc + 50), (x, GD)); dot('B+', x, BP); dot('GND', x, GD)
    cap(c, v, x, yc, ('B+', 'GND'), pol)
box('НАГРУЗКА', 690, 142, 130, 58, 'Электролизёр', '', [('+', 't', 65, '+', 'B+'), ('-', 'b', 65, '−', 'D')])
wire('B+', (755, BP), (755, 142)); dot('B+', 755, BP)
wire('D', (755, 200), (755, DB), (1545, DB))
dual('D3', 870, BP, DB)
for x in (848, 892): dot('D', x, DB)
QY = (DB + GD) / 2
for i, g in enumerate(('G1', 'G2', 'G3', 'G4')):
    x = 1120 + i * 105
    nmos(f'Q{i+1}', x, QY, g)
    wire('D', (x, DB), (x, QY - 65)); wire('GND', (x, QY + 65), (x, GD))
    wire(g, (x - 45, QY), (x - 52, QY)); flag(g, x - 52, QY, 'l')
    dot('D', x, DB); dot('GND', x, GD)
wire('GND', (150, GD), (1545, GD)); wire('GND', (260, GD), (260, 345)); dot('GND', 260, GD); flag('GND', 260, 345, 'u')
wire('D', (1545, DB), (1545, QY - 50)); wire('GND', (1545, QY + 50), (1545, GD))
with part('TV1'):
    hit(1472, QY - 50, 98, 100); lead((1545, QY - 50), (1545, QY - 11)); lead((1545, QY + 11), (1545, QY + 50)); diode_shape(1545, QY, True, 'tvs')
    text(1522, QY + 24, 'TV1', 'ref', 'end'); text(1522, QY + 40, 'SM8S33A', 'val', 'end')
port('TV1.K', 'D', 1545, QY - 50); port('TV1.A', 'GND', 1545, QY + 50)
text(1100, 215, 'D — общая шина; радиатор под этим потенциалом', 'note')
text(660, 392, 'GND / S — общая шина, минус аккумулятора', 'note')

# ══ БЛОК 2: ПИТАНИЕ ДРАЙВЕРА ══════════════════════════════════════════════════════
card(20, 455, 770, 365, 2, 'Питание драйвера 12 В', 'малый ток', 'b2')
ML, TOP, GB = 590, 520, 785
YC = 690
flag('B+', 62, ML, 'l'); wire('B+', (62, ML), (70, ML))
fuse('F2', '250 мА', 105, ML, ('B+', 'AUX1'))
wire('AUX1', (140, ML), (160, ML))
dh('D1', '1N4007', 195, ML, ('AUX1', 'AUX2'))
wire('AUX2', (230, ML), (250, ML))
res('R13', '100 Ом, 1 Вт', 300, ML, nets=('AUX2', 'VPRE'))
wire('VPRE', (350, ML), (570, ML))
for x in (360, 440, 520):
    wire('VPRE', (x, ML), (x, YC - 50)); dot('VPRE', x, ML); wire('GND', (x, YC + 50), (x, GB)); dot('GND', x, GB)
dv('TV2', '1.5KE24A', 360, YC, ('VPRE', 'GND'), 'tvs', left=True)
cap('C4', '10 мкФ|50 В', 440, YC, ('VPRE', 'GND'), True)
cap('C5', '330 нФ|50 В', 520, YC, ('VPRE', 'GND'), False)
box('U2', 570, ML - 30, 90, 70, 'U2', 'L7812CV', [('1', 'l', 30, '1', 'VPRE'), ('3', 'r', 30, '3', '+12V'), ('2', 'b', 45, '2', 'GND')])
wire('GND', (615, ML + 40), (615, GB)); dot('GND', 615, GB)
wire('VPRE', (545, ML), (545, TOP), (560, TOP)); dot('VPRE', 545, ML)
dh('D2', '1N4007', 595, TOP, ('+12V', 'VPRE'), k_left=True)
wire('+12V', (630, TOP), (680, TOP), (680, ML))
wire('+12V', (660, ML), (770, ML)); dot('+12V', 680, ML)
for x, c, v, pol in ((695, 'C6', '10 мкФ|25 В', True), (770, 'C7', '100 нФ|50 В', False)):
    wire('+12V', (x, ML), (x, YC - 50)); wire('GND', (x, YC + 50), (x, GB))
    cap(c, v, x, YC, ('+12V', 'GND'), pol, left=True)
dot('+12V', 695, ML); dot('GND', 695, GB)
wire('+12V', (770, ML), (770, 555)); flag('+12V', 770, 555, 'u')
wire('GND', (320, GB), (770, GB)); flag('GND', 320, GB, 'l')

# ══ БЛОК 3: ДРАЙВЕР ═══════════════════════════════════════════════════════════════
card(810, 455, 770, 365, 3, 'Управление и драйвер', 'короткие тонкие провода', 'b3')
EN, UT = 540, 565                     # шина EN, верх U1
p1, p2, p3, p4 = UT + 55, UT + 92, UT + 129, UT + 166
flag('+12V', 882, EN, 'l'); wire('+12V', (882, EN), (890, EN))
switch('T1', 'NC 70 °C', 925, EN, ('+12V', 'TH'), nc=True)
wire('TH', (960, EN), (975, EN))
res('R11', '2,2 кОм', 1025, EN, nets=('TH', 'EN'))
wire('EN', (1075, EN), (1405, EN))
box('ESP', 830, p2 - 48, 110, 100, 'ESP32', '', [('32', 'r', 48, 'GPIO32', 'GPIO'), ('GND', 'r', 48 + p3 - p2, 'GND', 'GND')], top=True)
wire('GPIO', (940, p2), (955, p2))
res('R9', '100 Ом', 1005, p2, nets=('GPIO', 'IN'))
wire('IN', (1055, p2), (1200, p2))
res('R10', '10 кОм', 1075, p2 + 85, vert=True, nets=('IN', 'GND'), swap=True)
wire('IN', (1075, p2), (1075, p2 + 35)); dot('IN', 1075, p2)
wire('GND', (1075, p2 + 135), (1075, GB)); dot('GND', 1075, GB)
wire('IN', (1170, p2), (1170, p4), (1200, p4)); dot('IN', 1170, p2)
box('U1', 1200, UT, 130, 195, 'U1', 'UCC27524DR', [
    ('1', 'l', p1 - UT, '1 ENA', 'EN'), ('2', 'l', p2 - UT, '2 INA', 'IN'), ('3', 'l', p3 - UT, '3 GND', 'GND'), ('4', 'l', p4 - UT, '4 INB', 'IN'),
    ('8', 'r', p1 - UT, 'ENB 8', 'EN'), ('7', 'r', p2 - UT, 'OUTA 7', 'OUTA'), ('6', 'r', p3 - UT, 'VDD 6', '+12V'), ('5', 'r', p4 - UT, 'OUTB 5', 'OUTB')], top=True)
wire('EN', (1200, p1), (1185, p1), (1185, EN)); dot('EN', 1185, EN)
wire('EN', (1330, p1), (1350, p1), (1350, EN)); dot('EN', 1350, EN)
wire('GND', (1200, p3), (1150, p3), (1150, GB)); dot('GND', 1150, GB)
wire('GND', (940, p3), (955, p3), (955, GB)); dot('GND', 955, GB)
wire('OUTA', (1330, p2), (1338, p2)); flag('OUTA', 1338, p2, 'r')
wire('+12V', (1330, p3), (1338, p3)); flag('+12V', 1338, p3, 'r')
wire('OUTB', (1330, p4), (1338, p4)); flag('OUTB', 1338, p4, 'r')
wire('EN', (1405, EN), (1405, 640))
res('R12', '1 кОм', 1405, 690, vert=True, nets=('EN', 'GND'))
wire('GND', (1405, 740), (1405, GB)); dot('GND', 1405, GB)
CR, YC = 610, 695
flag('+12V', 1475, CR, 'u')
wire('+12V', (1475, CR), (1555, CR))
for x, c, pol, v in ((1475, 'C8', True, '47 мкФ'), (1515, 'C9', False, '1 мкФ'), (1555, 'C10', False, '100 нФ')):
    wire('+12V', (x, CR), (x, YC - 50)); wire('GND', (x, YC + 50), (x, GB))
    with part(c):
        hit(x - 19, YC - 50, 38, 100); lead((x, YC - 50), (x, YC - 7)); lead((x, YC + 7), (x, YC + 50))
        svg.append(f'<line class="s2" x1="{x-13}" y1="{YC-7}" x2="{x+13}" y2="{YC-7}"/>')
        if pol: svg.append(f'<rect class="fillk" x="{x-13}" y="{YC+4}" width="26" height="5"/>')
        else: svg.append(f'<line class="s2" x1="{x-13}" y1="{YC+7}" x2="{x+13}" y2="{YC+7}"/>')
        text(x, YC - 16, c, 'ref small', 'middle'); text(x, YC + 25, v, 'val tiny', 'middle')
    port(c + '.+', '+12V', x, YC - 50); port(c + '.-', 'GND', x, YC + 50)
    if x != 1475: dot('+12V', x, CR)
    dot('GND', x, GB)
wire('GND', (900, GB), (1555, GB)); flag('GND', 900, GB, 'l')

# ══ БЛОК 4: ЗАТВОРЫ ═══════════════════════════════════════════════════════════════
card(20, 835, 1560, 275, 4, 'Обвязка затворов', 'у ножек транзистора; у каждого своя', 'b4')
for i, (out, g, q, rg, rp, z) in enumerate((('OUTA', 'G1', 'Q1', 'R1', 'R5', 'Z1'), ('OUTA', 'G2', 'Q2', 'R2', 'R6', 'Z2'), ('OUTB', 'G3', 'Q3', 'R3', 'R7', 'Z3'), ('OUTB', 'G4', 'Q4', 'R4', 'R8', 'Z4'))):
    x0 = 45 + i * 387; yl, yb = 905, 1085
    flag(out, x0 + 62, yl, 'l'); wire(out, (x0 + 62, yl), (x0 + 75, yl))
    res(rg, '10 Ом', x0 + 125, yl, nets=(out, g))
    wire(g, (x0 + 175, yl), (x0 + 322, yl)); flag(g, x0 + 322, yl, 'r', g)
    res(rp, '10 кОм', x0 + 190, 995, vert=True, nets=(g, 'GND'))
    wire(g, (x0 + 190, yl), (x0 + 190, 945)); dot(g, x0 + 190, yl)
    wire('GND', (x0 + 190, 1045), (x0 + 190, yb))
    dv(z, 'BZX55C15', x0 + 280, 995, (g, 'GND'), 'z')
    wire(g, (x0 + 280, yl), (x0 + 280, 945)); dot(g, x0 + 280, yl)
    wire('GND', (x0 + 280, 1045), (x0 + 280, yb))
    wire('GND', (x0 + 80, yb), (x0 + 280, yb)); dot('GND', x0 + 190, yb); flag('GND', x0 + 80, yb, 'l')

# ── проверка рисунка ───────────────────────────────────────────────────────────
def on(p, s):
    (x, y), (a, b) = p, (s[1], s[2])
    return min(a[0], b[0]) - .01 <= x <= max(a[0], b[0]) + .01 and min(a[1], b[1]) - .01 <= y <= max(a[1], b[1]) + .01
err = []
for pid, n in CONN['nodes'].items():
    if not n['joint'] and pid not in ports and pid.split('.')[0] in {p['id'] for p in CONN['parts']} and pid in {q for p in CONN['parts'] for q in p['ports']}:
        err.append('нет на схеме: ' + pid)
# 1) сети рисунка == сети перечня соединений
par = {}
def fnd(x):
    par.setdefault(x, x)
    while par[x] != x: par[x] = par[par[x]]; x = par[x]
    return x
for c in CONN['instructions']: par[fnd(c['a'])] = fnd(c['b'])
g1, g2 = collections.defaultdict(set), collections.defaultdict(set)
for pid in ports: g1[fnd(pid)].add(pid); g2[ports[pid]['net']].add(pid)
a1 = sorted(sorted(v) for v in g1.values()); a2 = sorted(sorted(v) for v in g2.values())
if a1 != a2:
    for v in a1:
        if v not in a2: err.append('в перечне иначе: ' + ' '.join(v))
    for v in a2:
        if v not in a1: err.append('на рисунке иначе: ' + ' '.join(v))
# 2) каждый вывод лежит на проводе своей сети; провод не касается концом чужой сети; куски одной сети связаны линиями или метками
for pid, p in ports.items():
    same = [s for s in segs if s[0] == p['net']]
    pts = [(p['x'], p['y'])]
    if not any(on(pts[0], s) for s in same): err.append(f'вывод {pid} не на проводе {p["net"]} {pts[0]}')
    for s in segs:
        if s[0] != p['net'] and on(pts[0], s): err.append(f'вывод {pid} ({p["net"]}) лежит на проводе {s[0]}')
for s in segs:
    for e in (s[1], s[2]):
        for t in segs:
            if t[0] != s[0] and on(e, t): err.append(f'провод {s[0]} {e} упирается в {t[0]}')
for net in {s[0] for s in segs}:
    S = [s for s in segs if s[0] == net]; comp = list(range(len(S)))
    def cf(i):
        while comp[i] != i: i = comp[i]
        return i
    for i, a in enumerate(S):
        for j, b in enumerate(S):
            if i < j and (on(a[1], b) or on(a[2], b) or on(b[1], a) or on(b[2], a)): comp[cf(i)] = cf(j)
    roots = {cf(i) for i in range(len(S))}
    flagged = {cf(i) for i, s in enumerate(S) for f in flags if f[0] == net and on((f[1], f[2]), s)}
    if len(roots) > 1 and roots - flagged: err.append(f'сеть {net}: кусок без связи и без метки')
cross = 0
for i, a in enumerate(segs):
    for b in segs[i + 1:]:
        if a[0] != b[0]:
            h, v = (a, b) if a[1][1] == a[2][1] else (b, a)
            if h[1][1] == h[2][1] and v[1][0] == v[2][0] and min(h[1][0], h[2][0]) < v[1][0] < max(h[1][0], h[2][0]) and min(v[1][1], v[2][1]) < h[1][1] < max(v[1][1], v[2][1]): cross += 1
if err:
    print('СХЕМА НЕ СХОДИТСЯ:'); [print(' ', e) for e in err]; sys.exit(1)
print(f'схема: {len(ports)} выводов, {len(a2)} сетей совпали с перечнем, пересечений проводов без соединения: {cross}')

STYLE = """.w{transition:opacity .12s}.s{stroke:#1f2a37;stroke-width:2;fill:none}.s2{stroke:#1f2a37;stroke-width:3;fill:none}.b{fill:#fff;stroke:#1f2a37;stroke-width:2}.box{fill:#f4f7fa}
.dk{fill:#1f2a37}.fillk{fill:#1f2a37}.o{fill:#fff;stroke:#1f2a37;stroke-width:2}.hit{fill:transparent}.card{fill:#fbfcfd;stroke:#d9e0e7;stroke-width:1.5}
.sc text{font-family:Segoe UI,Arial,sans-serif;fill:#1f2a37;paint-order:stroke;stroke:#fbfcfd;stroke-width:3.5px;stroke-linejoin:round}.sc .fl{stroke:none}.sc .numt{stroke:none;fill:#fff}.ref{font-size:15px;font-weight:700}.ref.small{font-size:13px}.val{font-size:13px;fill:#52606d}.val.tiny{font-size:11.5px}.pin{font-size:12.5px;fill:#52606d}.plus{font-size:18px;font-weight:700}
.h{font-size:19px;font-weight:700}.sub{font-size:14px;fill:#52606d}.note{font-size:13px;fill:#52606d}.fl{font-size:13.5px;font-weight:700}.num{fill:#1f2a37}.numt{font-size:15px;font-weight:700}"""
W, H = 1600, 1130
SVG_BODY = ''.join(svg)
standalone = f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" class="sc"><style>{STYLE}</style><rect width="{W}" height="{H}" fill="#fff"/>{SVG_BODY}</svg>'
io.open(os.path.join(HERE, '..', 'IRFB4110_общая_схема_пайки.svg'), 'w', encoding='utf-8').write(standalone)

# ── фото ───────────────────────────────────────────────────────────────────────
def b64(path, box_=None, maxw=420, q=78):
    im = Image.open(os.path.join(HERE, path)).convert('RGB')
    if box_: im = im.crop(box_)
    if im.width > maxw: im = im.resize((maxw, round(im.height * maxw / im.width)), Image.LANCZOS)
    b = io.BytesIO(); im.save(b, 'JPEG', quality=q, optimize=True)
    return 'data:image/jpeg;base64,' + base64.b64encode(b.getvalue()).decode()
PH = {'mos': b64('irfb4110.jpg'), 'u1': b64('so8b.jpg'), 'reg': b64('l7812-front.jpg'), 'res': b64('resistor.jpg', maxw=300), 'zen': b64('zener-glass.jpg', maxw=300),
      'd1': b64('1n4007.jpg', maxw=300), 'tv2': b64('tvs24.jpg', maxw=300), 'tv1': b64('sm8s33a-back.jpg', maxw=300), 'mbr': b64('mbr60100ct.jpg', maxw=320), 't1': b64('thermostat.jpg', maxw=300),
      'adapter': b64('adapter-orientation.png', maxw=560), 'esp': b64('../H2_board_pinout.png', (1380, 1150, 3080, 4500), 425, 82)}

# ── данные ─────────────────────────────────────────────────────────────────────
labels = {k: v['label'] for k, v in CONN['nodes'].items() if k in ports}
partsinfo = {p['id']: p['name'] for p in CONN['parts']}
steps = [{'id': s['id'], 'section': s['section'], 'title': s['title'], 'note': s['note'], 'kind': s['kind'], 'primary': s['primary'],
          'a': (s['connection'] or {}).get('a'), 'b': (s['connection'] or {}).get('b')} for s in STEPS]
assert len(steps) == 86
BUY_NOW = {'g0-1': 3.50, 'g0-3': None, 'g0-4': 1.16, 'g0-6': 3.05, 'g0-7': 3.62}   # цены 07.11.2026 со страниц БелЧип
STOCK = {'g0-1': 'на складе 1179', 'g0-2': 'осталось 4 шт.', 'g0-4': 'на складе 40', 'g0-6': 'на складе 23', 'g0-7': 'на складе 1163', 'g0-8': 'на складе 66', 'g0-10': 'на складе 47'}
from purchase_final import FINAL, GROUPS
src = {i['id']: i for i in BUY['items']}
items = []
for group, iid, ref, name, qty, why in FINAL:
    i = src.get(iid, {'price': None, 'url': None, 'search': None})
    items.append({'group': group, 'title': GROUPS[group], 'ref': ref, 'name': name, 'qty': qty, 'why': why, 'id': iid, 'optional': False,
                  'price': BUY_NOW.get(iid, i['price']), 'url': i['url'], 'search': i.get('search'), 'stock': STOCK.get(iid, '')})
assert len(items) == 38 and len({i['id'] for i in items}) == 38
subtotal = round(sum(i['price'] * int(i['qty']) for i in items if i['price']), 2)
assert subtotal == 26.73, subtotal
DATA = {'ports': ports, 'labels': labels, 'parts': partsinfo, 'steps': steps, 'colors': COL, 'netnames': NETNAME, 'items': items, 'subtotal': subtotal, 'photos': PH, 'W': W, 'H': H}

PAGE = io.open(os.path.join(HERE, 'clear_guide_template.html'), encoding='utf-8').read()
out = PAGE.replace('/*STYLE*/', STYLE).replace('<!--SCHEMA-->', SVG_BODY).replace('/*DATA*/null', json.dumps(DATA, ensure_ascii=False).replace('</', '<\\/'))
p = os.path.join(HERE, '..', 'IRFB4110_сборка_поэтапно.html')
io.open(p, 'w', encoding='utf-8').write(out)
print('готово:', os.path.normpath(p), round(len(out.encode('utf-8')) / 1e6, 2), 'МБ', '| закупка с ценой:', subtotal)

"""Плата драйвера силового ключа 4 × IRFB4110 (управление на плате, силовой ток — по шинам).
Запуск: py -3 make_pcb.py
Одна разводка на нижнем слое для двух способов:
  • завод — двухсторонняя плата: снизу вся разводка, сверху только перемычка JP1 дорожкой; маска, шелкография;
  • ЛУТ — односторонняя: та же нижняя медь, JP1 — проволочная перемычка сверху.
Перед выводом проверяет: цепи платы = цепи схемы (overall-connections.json), зазоры, связность каждой цепи."""
import io, json, math, os, sys, zipfile, collections
from shapely.geometry import Point, LineString, Polygon, MultiPolygon, box
from shapely.ops import unary_union
from shapely import affinity
sys.stdout.reconfigure(encoding='utf-8')
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, 'вывод'); os.makedirs(OUT, exist_ok=True)

W, H = 104.0, 82.0          # размер платы, мм
CLR = 0.5                   # зазор медь–медь (с запасом для ЛУТ)
EDGE = 0.7                  # зазор меди до края
TRACE_SIG, TRACE_PWR, TRACE_FINE = 0.8, 1.0, 0.4

pads, silk, silk_b, traces, jumpers, holes_npth, bodies = [], [], [], [], [], [], []   # bodies: (ref, контур корпуса сверху)
parts = {}                  # ref -> описание (для сборочного чертежа)

def pad(ref, name, x, y, shape, w, h=None, drill=None, net=None, layer='th'):
    pads.append(dict(ref=ref, name=name, x=x, y=y, shape=shape, w=w, h=h or w, drill=drill, net=net, layer=layer))
def tr(net, width, *pts): traces.append((net, width, pts))
def sline(*pts, w=0.15, bottom=False): (silk_b if bottom else silk).append(('line', w, pts))
def scirc(x, y, r, w=0.15): silk.append(('circle', w, (x, y, r)))
def stext(x, y, s, size=1.2, anchor='c', bottom=False): (silk_b if bottom else silk).append(('text', max(size, 1.0), (x, y, s, anchor)))

# ручные места подписей (x, y, привязка), чтобы не налезали на соседей
LP = {'C4': (23.0, 17.5, 'r'), 'C7': (40.66, 27.4, 'c'), 'C8': (60.7, 42.3, 'l'), 'C9': (55, 51.8, 'c'), 'C10': (51, 51.8, 'c'), 'D2': (33.08, 37.9, 'c'),
      **{f'R{n + 5}': (x + 13.08, 68.6, 'c') for n, x in enumerate([14, 34, 54, 74])}}

# ── посадочные места ───────────────────────────────────────────────────────────
def axial(ref, val, x1, y1, x2, y2, n1, n2, drill, d, body_len, body_d, p1='1', p2='2', band=None):
    pad(ref, p1, x1, y1, 'circle', d, drill=drill, net=n1); pad(ref, p2, x2, y2, 'circle', d, drill=drill, net=n2)
    L = math.hypot(x2 - x1, y2 - y1); ux, uy = (x2 - x1) / L, (y2 - y1) / L; nx, ny = -uy, ux
    cx, cy = (x1 + x2) / 2, (y1 + y2) / 2; a, b = body_len / 2, body_d / 2
    q = [(cx - ux * a + nx * b, cy - uy * a + ny * b), (cx + ux * a + nx * b, cy + uy * a + ny * b), (cx + ux * a - nx * b, cy + uy * a - ny * b), (cx - ux * a - nx * b, cy - uy * a - ny * b)]
    sline(*q, q[0]); bodies.append((ref, Polygon(q)))
    sline((x1 + ux * (d / 2 + .3), y1 + uy * (d / 2 + .3)), (cx - ux * a, cy - uy * a)); sline((cx + ux * a, cy + uy * a), (x2 - ux * (d / 2 + .3), y2 - uy * (d / 2 + .3)))
    if band:   # полоса катода у вывода p2 (band='2') или p1
        s = 1 if band == p2 else -1; bx, by = cx + s * ux * (a - .5), cy + s * uy * (a - .5)
        sline((bx + nx * b, by + ny * b), (bx - nx * b, by - ny * b), w=0.5)
    parts[ref] = val
    return (cx + nx * (b + 1.3), cy + ny * (b + 1.3))

def res(ref, val, x1, y1, x2, y2, n1, n2, big=False):
    o = axial(ref, val, x1, y1, x2, y2, n1, n2, 1.0 if big else 0.8, 2.2 if big else 1.8, 9.5 if big else 6.4, 3.8 if big else 2.4)
    lab(ref, x1, y1, x2, y2)
def lab(ref, x1, y1, x2, y2, off=2.6):
    if ref in LP: stext(*LP[ref][:2], ref, anchor=LP[ref][2]); return
    if abs(x1 - x2) < .01: stext(x1 + off, (y1 + y2) / 2, ref, anchor='l')
    else: stext((x1 + x2) / 2, y1 - off, ref)

def diode(ref, val, xa, ya, xk, yk, na, nk, kind='1n4007'):
    drill, d, L, D = {'1n4007': (1.0, 2.0, 5.2, 2.7), 'do35': (0.8, 1.8, 3.8, 1.9), 'do201': (1.3, 2.6, 9.5, 5.3)}[kind]
    axial(ref, val, xa, ya, xk, yk, na, nk, drill, d, L, D, 'A', 'K', band='K')
    lab(ref, xa, ya, xk, yk, off=2.6 if kind != 'do201' else 3.8)

def cer(ref, val, x1, y1, x2, y2, n1, n2):
    pad(ref, '1', x1, y1, 'circle', 1.6, drill=0.8, net=n1); pad(ref, '2', x2, y2, 'circle', 1.6, drill=0.8, net=n2)
    cx, cy = (x1 + x2) / 2, (y1 + y2) / 2
    if abs(x1 - x2) < .01: sline((cx - 1.4, cy - 3.6), (cx + 1.4, cy - 3.6), (cx + 1.4, cy + 3.6), (cx - 1.4, cy + 3.6), (cx - 1.4, cy - 3.6)); bodies.append((ref, box(cx - 1.4, cy - 3.6, cx + 1.4, cy + 3.6)))
    else: sline((cx - 3.6, cy - 1.4), (cx + 3.6, cy - 1.4), (cx + 3.6, cy + 1.4), (cx - 3.6, cy + 1.4), (cx - 3.6, cy - 1.4)); bodies.append((ref, box(cx - 3.6, cy - 1.4, cx + 3.6, cy + 1.4)))
    lab(ref, x1, y1, x2, y2, off=2.4); parts[ref] = val

def elco(ref, val, xp, yp, xm, ym, np_, nm, dia=6.3):
    pad(ref, '+', xp, yp, 'rect', 1.6, drill=0.8, net=np_); pad(ref, '-', xm, ym, 'circle', 1.6, drill=0.8, net=nm)
    cx, cy = (xp + xm) / 2, (yp + ym) / 2; scirc(cx, cy, dia / 2); bodies.append((ref, Point(cx, cy).buffer(dia / 2, 32)))
    ux, uy = (xp - xm) / 2.5, (yp - ym) / 2.5
    px, py = cx + ux * (dia / 2 + 1.0), cy + uy * (dia / 2 + 1.0)
    sline((px - .6, py), (px + .6, py)); sline((px, py - .6), (px, py + .6))
    parts[ref] = val
    if ref in LP: stext(*LP[ref][:2], ref, anchor=LP[ref][2])
    else: stext(cx + (dia / 2 + 1.8 if abs(uy) > .5 else 0), cy + (0 if abs(uy) > .5 else -(dia / 2 + 1.4)), ref, anchor='l' if abs(uy) > .5 else 'c'); parts[ref] = val

def to220(ref, val, x1, y, nets, labels, body='s', rev=False):
    """TO-220 стоя. Вид сверху: если металл смотрит на юг (к нижнему краю), лицевая сторона — на север,
    и вывод 1 оказывается СПРАВА (rev=True). Если металл на север — вывод 1 слева."""
    xs_ = [x1 + 2.54 * (2 - i if rev else i) for i in range(3)]
    for i, (n, l) in enumerate(zip(nets, labels)):
        pad(ref, str(i + 1), xs_[i], y, 'oval', 1.8, 2.6, drill=1.2, net=n)
    cx = x1 + 2.54
    # body = куда смотрит металл: 's' — на юг, 'n' — на север. Корпус 4,6 мм: от выводов до металла 2,6, до лица 2,0
    if body == 's': y0, y1 = y - 2.0, y + 2.6
    else: y0, y1 = y - 2.6, y + 2.0
    sline((cx - 5.1, y0), (cx + 5.1, y0), (cx + 5.1, y1), (cx - 5.1, y1), (cx - 5.1, y0))
    ty = y1 - .4 if body == 's' else y0 + .4        # толстая линия = металлическая сторона
    sline((cx - 5.1, ty), (cx + 5.1, ty), w=0.6)
    bodies.append((ref, box(cx - 5.1, y0, cx + 5.1, y1)))
    for i, l in enumerate(labels): stext(xs_[i], y0 - 1.3 if body == 's' else y1 + 1.3, l, size=1.0)
    parts[ref] = val

def term(ref, val, x, y, nets, labels, vertical=False):
    for i, (n, l) in enumerate(zip(nets, labels)):
        px, py = (x, y + 5.08 * i) if vertical else (x + 5.08 * i, y)
        pad(ref, str(i + 1), px, py, 'rect' if i == 0 else 'circle', 2.6, drill=1.3, net=n)
    if vertical: sline((x - 3.6, y - 2.6), (x + 3.9, y - 2.6), (x + 3.9, y + 7.7), (x - 3.6, y + 7.7), (x - 3.6, y - 2.6))
    else: sline((x - 2.6, y - 3.2), (x + 7.7, y - 3.2), (x + 7.7, y + 4.0), (x - 2.6, y + 4.0), (x - 2.6, y - 3.2)); bodies.append((ref, box(x - 2.6, y - 3.2, x + 7.7, y + 4.0)))
    parts[ref] = val

def soic8_bottom(ref, val, cx, cy, nets):
    """UCC27524 ставится СНИЗУ (на сторону меди). Вид сверху сквозь плату, корпус повёрнут:
    верхний ряд слева направо 1 2 3 4, нижний ряд 8 7 6 5."""
    xs = [cx - 1.905, cx - 0.635, cx + 0.635, cx + 1.905]
    for i, x in enumerate(xs):
        pad(ref, str(i + 1), x, cy - 2.7, 'rect', 0.6, 1.95, net=nets[i + 1], layer='b')
        pad(ref, str(8 - i), x, cy + 2.7, 'rect', 0.6, 1.95, net=nets[8 - i], layer='b')
    sline((cx - 2.45, cy - 1.95), (cx + 2.45, cy - 1.95), (cx + 2.45, cy + 1.95), (cx - 2.45, cy + 1.95), (cx - 2.45, cy - 1.95), bottom=True)
    silk_b.append(('circle', 0.3, (cx - 1.905, cy - 0.9, 0.35)))
    stext(cx + 5.3, cy - 1.2, 'U1', bottom=True); stext(cx + 5.3, cy + 0.6, 'снизу', size=0.9, bottom=True)
    parts[ref] = val

# ── раскладка ──────────────────────────────────────────────────────────────────
# Q1–Q4 у нижнего края, металлом наружу: тыльная сторона металла выходит к краю платы (≈0,2 мм за край),
# радиатор ставится вдоль края. Вид сверху: S слева (gx), D посередине, G справа (gx+5,08).
Q_Y = 79.6
GX = [14, 34, 54, 74]
OUTN = ['OUTA', 'OUTA', 'OUTB', 'OUTB']
for i, gx in enumerate(GX):
    n = i + 1; g = f'G{n}'
    to220(f'Q{n}', 'IRFB4110PBF', gx, Q_Y, [g, None, 'GND'], ['G', 'D', 'S'], body='s', rev=True)
    res(f'R{n}', '10 Ом', gx + 9.08, 46, gx + 9.08, 56.16, OUTN[i], g)
    res(f'R{n + 4}', '10 кОм', gx + 13.08, 56.16, gx + 13.08, 66.32, g, 'GND')
    diode(f'Z{n}', 'BZX55C15', gx + 17.08, 63.78, gx + 17.08, 56.16, 'GND', g, kind='do35')
    tr(g, TRACE_SIG, (gx + 9.08, 56.16), (gx + 17.08, 56.16))
    tr(g, TRACE_SIG, (gx + 9.08, 56.16), (gx + 9.08, Q_Y), (gx + 5.08, Q_Y))
    stext(gx + 2.54, Q_Y - 5.0, f'Q{n}', size=1.3)
stext(52, 70.5, 'Q1–Q4: металлом к краю платы, к радиатору', size=1.0)

# U1 и его цепи
UC = (58, 32)
soic8_bottom('U1', 'UCC27524DR', *UC, {1: 'EN', 2: 'IN', 3: 'GND', 4: 'IN', 5: 'OUTB', 6: '+12V', 7: 'OUTA', 8: 'EN'})
cx, cy = UC
xs = [cx - 1.905, cx - 0.635, cx + 0.635, cx + 1.905]
top, bot = cy - 2.7 - 0.975, cy + 2.7 + 0.975
tr('OUTA', TRACE_FINE, (xs[1], bot), (xs[1], 40)); tr('OUTA', TRACE_SIG, (xs[1], 40), (23.08, 40), (23.08, 46)); tr('OUTA', TRACE_SIG, (43.08, 40), (43.08, 46))
tr('OUTB', TRACE_FINE, (xs[3], bot), (xs[3], 40)); tr('OUTB', TRACE_SIG, (xs[3], 40), (83.08, 40), (83.08, 46)); tr('OUTB', TRACE_SIG, (63.08, 40), (63.08, 46))
tr('+12V', TRACE_FINE, (xs[2], bot), (xs[2], 44.5)); tr('+12V', TRACE_PWR, (48.5, 44.5), (59.4, 44.5))
tr('GND', TRACE_FINE, (xs[2], top), (xs[2], 32), (63, 32))
tr('IN', TRACE_FINE, (xs[1], top), (xs[1], 26.5), (xs[3], 26.5), (xs[3], top)); tr('IN', TRACE_SIG, (xs[3], 26.5), (68, 26.5), (68, 18))
tr('EN', TRACE_FINE, (xs[0], top), (xs[0], 27.3), (53.4, 27.3)); tr('EN', TRACE_FINE, (xs[0], bot), (xs[0], 37.5), (53.4, 37.5))
tr('EN', TRACE_SIG, (53.4, 22), (53.4, 37.5))
cer('C10', '100 нФ', 51, 44.5, 51, 49.58, '+12V', 'GND')
cer('C9', '1 мкФ', 55, 44.5, 55, 49.58, '+12V', 'GND')
elco('C8', '47 мкФ / 25 В, Ø5', 59.4, 44.5, 59.4, 47.0, '+12V', 'GND', dia=5.0)
res('R12', '1 кОм', 53.4, 32, 43.24, 32, 'EN', 'GND')
res('R11', '2,2 кОм', 43.24, 22, 53.4, 22, 'TH', 'EN')
res('R10', '10 кОм', 68, 36.66, 68, 26.5, 'GND', 'IN')
res('R9', '100 Ом', 78.16, 18, 68, 18, 'GPIO', 'IN')
tr('GPIO', TRACE_SIG, (78.16, 18), (90, 18), (90, 5))
pad('JP1', 'a', 48.5, 37, 'circle', 1.8, drill=0.9, net='+12V'); pad('JP1', 'b', 48.5, 44.5, 'circle', 1.8, drill=0.9, net='+12V')
jumpers.append(('JP1', (48.5, 37), (48.5, 44.5))); parts['JP1'] = 'перемычка'
sline((47.3, 37), (47.3, 44.5), w=0.3); sline((49.7, 37), (49.7, 44.5), w=0.3); stext(45.6, 40.8, 'JP1', anchor='r')

# питание драйвера
term('J1', 'клеммник 5,08 мм, 2 конт.', 5, 5, ['AUX1', 'GND'], ['+', '−'])
term('J3', 'клеммник 5,08 мм, 2 конт.', 38.16, 5, ['+12V', 'TH'], ['', ''])
term('J2', 'клеммник 5,08 мм, 2 конт.', 90, 5, ['GPIO', 'GND'], ['', ''])
diode('D1', '1N4007', 5, 11, 5, 21.16, 'AUX1', 'AUX2')
tr('AUX1', TRACE_PWR, (5, 5), (5, 11))
res('R13', '100 Ом, 1 Вт', 7, 24, 22.24, 24, 'AUX2', 'VPRE', big=True)
tr('AUX2', TRACE_PWR, (5, 21.16), (5, 24), (7, 24))
diode('TV2', '1.5KE24A', 12.76, 11.5, 28, 11.5, 'GND', 'VPRE', kind='do201')
elco('C4', '10 мкФ / 50 В', 28, 17.5, 25.5, 17.5, 'VPRE', 'GND')
cer('C5', '330 нФ', 32, 21, 32, 15.92, 'VPRE', 'GND')
# L7812 металлом на север (к C4/C5), лицом на юг → вывод 1 (IN) слева
to220('U2', 'L7812CV', 28, 30, ['VPRE', 'GND', '+12V'], ['IN', 'G', 'OUT'], body='n')
diode('D2', '1N4007', 38.16, 35.5, 28, 35.5, '+12V', 'VPRE')
tr('VPRE', TRACE_PWR, (28, 11.5), (28, 35.5)); tr('VPRE', TRACE_PWR, (22.24, 24), (28, 24)); tr('VPRE', TRACE_PWR, (28, 21), (32, 21))
cer('C7', '100 нФ', 40.66, 20, 40.66, 25.08, '+12V', 'GND'); tr('+12V', TRACE_PWR, (38.16, 20), (40.66, 20))
elco('C6', '10 мкФ / 25 В', 38.16, 15, 40.66, 15, '+12V', 'GND')
tr('+12V', TRACE_PWR, (38.16, 5), (38.16, 37), (48.5, 37)); tr('+12V', TRACE_PWR, (33.08, 30), (38.16, 30))
tr('TH', TRACE_SIG, (43.24, 5), (43.24, 22))

# подписи клемм
stext(7.54, 10.8 - 1.6, '', size=1)
stext(5, 1.0, '+ после F2', size=1.0); stext(10.08, 1.0, '−', size=1.3)
stext(40.7, 1.0, 'термостат T1', size=1.0)
stext(92.54, 1.0, 'ESP32: 32 / GND', size=1.0)
stext(8, 30.5, 'B+ через F2 250 мА', size=0.9, anchor='l'); stext(8, 32.0, 'GND — к шине S', size=0.9, anchor='l')
stext(62, 60 + 21, '', size=1)
stext(84, 28, 'H2 · драйвер IRFB4110', size=1.4); stext(84, 30.4, 'v1 · 13.11.2026', size=1.0)
for x, y in ((5, 76), (99, 70), (99, 14), (5, 40)):
    holes_npth.append((x, y, 3.2)); scirc(x, y, 3.2)

import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon as MPoly
from matplotlib.textpath import TextPath
from matplotlib.font_manager import FontProperties
FP = FontProperties(family='DejaVu Sans')
def mpoly(ax, g, **kw):
    for q in (g.geoms if hasattr(g, 'geoms') else [g]):
        if q.is_empty: continue
        ax.add_patch(MPoly(list(q.exterior.coords), closed=True, **kw))
        for r in q.interiors: ax.add_patch(MPoly(list(r.coords), closed=True, fc='white', ec='none', zorder=kw.get('zorder', 1) + .1))
def text_polys(x, y, s, size, anchor='c', mirror=False):
    if not s: return []
    tp = TextPath((0, 0), s, size=size * 1.25, prop=FP)
    bb = tp.get_extents(); dx = {'c': -bb.width / 2, 'l': 0, 'r': -bb.width}[anchor] - bb.x0; dy = -bb.height / 2 - bb.y0
    out = []
    for poly in tp.to_polygons():
        pts = [((-1 if mirror else 1) * (px + dx) + x, y - (py + dy)) for px, py in poly]
        if len(pts) >= 3: out.append(Polygon(pts))
    # даже-нечётное правило для букв с дырками
    res_ = Polygon()
    for q in sorted(out, key=lambda q: -abs(q.area)):
        q = q.buffer(0)
        res_ = res_.symmetric_difference(q)
    return [res_]

# ── проверки ───────────────────────────────────────────────────────────────────
def pad_geom(p, grow=0.0):
    x, y, w, h = p['x'], p['y'], p['w'] + 2 * grow, p['h'] + 2 * grow
    if p['shape'] == 'circle': return Point(x, y).buffer(w / 2, 32)
    if p['shape'] == 'rect': return box(x - w / 2, y - h / 2, x + w / 2, y + h / 2)
    r = min(w, h) / 2
    if h >= w: return LineString([(x, y - h / 2 + r), (x, y + h / 2 - r)]).buffer(r, 32)
    return LineString([(x - w / 2 + r, y), (x + w / 2 - r, y)]).buffer(r, 32)
def tr_geom(t, grow=0.0):
    net, w, pts = t
    return LineString(pts).buffer(w / 2 + grow, 16, cap_style='round', join_style='round')

err = []
board = box(0, 0, W, H)
inner = box(EDGE, EDGE, W - EDGE, H - EDGE)
nets = sorted({p['net'] for p in pads if p['net']} | {t[0] for t in traces})
# медь каждой цепи (без заливки)
cu = collections.defaultdict(list)
for p in pads:
    if p['layer'] in ('th', 'b'): cu[p['net']].append(pad_geom(p))
for t in traces: cu[t[0]].append(tr_geom(t))
cu = {n: unary_union(g) for n, g in cu.items()}
# заливка GND
keep = [cu[n].buffer(CLR + 0.05) for n in cu if n != 'GND']
keep += [Point(x, y).buffer(d / 2 + 1.0) for x, y, d in holes_npth]
base = inner.difference(unary_union(keep))
gth = [p for p in pads if p['net'] == 'GND' and p['drill']]
anti = unary_union([pad_geom(p, CLR) for p in gth])
spokes = []
for p in gth:
    r = max(p['w'], p['h']) / 2 + CLR + 0.8
    spokes += [box(p['x'] - r, p['y'] - 0.35, p['x'] + r, p['y'] + 0.35), box(p['x'] - 0.35, p['y'] - r, p['x'] + 0.35, p['y'] + r)]
pour = base.difference(anti).buffer(-0.3).buffer(0.3)     # убрать перемычки тоньше 0,6 мм
pour = unary_union([pour, unary_union(spokes).intersection(base).intersection(anti.buffer(0.01))])
LABEL_CUT = unary_union(text_polys(97.5, 52, 'НИЗ', 2.2, mirror=True) + text_polys(97.5, 56, 'H2', 2.2, mirror=True)
                        + text_polys(UC[0] - 6.4, UC[1] - 2.7, '1', 1.6, mirror=True))     # метка ножки 1 U1 прямо в меди
pour = pour.difference(LABEL_CUT.buffer(0.15))
gnd_all = unary_union([pour, cu['GND']])
gnd_pads = [pad_geom(p) for p in pads if p['net'] == 'GND']
polys = list(gnd_all.geoms) if isinstance(gnd_all, MultiPolygon) else [gnd_all]
polys = [q for q in polys if any(q.intersects(g) for g in gnd_pads)]     # острова без контакта убрать
cu['GND'] = unary_union(polys)
if isinstance(cu['GND'], MultiPolygon): err.append(f"GND распалась на {len(cu['GND'].geoms)} куска: " + '; '.join(f'{[round(v,1) for v in q.bounds]}' for q in cu['GND'].geoms))
# зазоры между цепями
names = list(cu)
for i, a in enumerate(names):
    for b in names[i + 1:]:
        if a is None or b is None: continue
        d = cu[a].distance(cu[b])
        if d < CLR - 1e-6: err.append(f'зазор {a}–{b}: {d:.2f} мм')
for p in pads:
    if p['net'] is None:
        for n in names:
            if n and pad_geom(p).distance(cu[n]) < CLR - 1e-6: err.append(f'зазор {p["ref"]}.{p["name"]} (без цепи)–{n}')
for n in names:
    if n and not inner.contains(cu[n]) and n != 'GND': err.append(f'{n}: медь ближе {EDGE} мм к краю')
# связность каждой цепи (перемычки соединяют свои площадки, у них одна цепь)
for n in names:
    if not n: continue
    g = cu[n]
    if isinstance(g, MultiPolygon):
        if n in {'+12V'} and len(g.geoms) == 2 and jumpers: continue   # две части +12V соединены JP1
        err.append(f'{n}: медь не связана ({len(g.geoms)} куска)')
# каждая площадка лежит в меди своей цепи
for p in pads:
    if p['net'] and not cu[p['net']].contains(Point(p['x'], p['y'])): err.append(f'площадка {p["ref"]}.{p["name"]} не в меди {p["net"]}')
# пересечение отверстий
th = [p for p in pads if p['drill']]
for i, a in enumerate(th):
    for b in th[i + 1:]:
        if math.hypot(a['x'] - b['x'], a['y'] - b['y']) < (a['drill'] + b['drill']) / 2 + 0.5: err.append(f'отверстия близко: {a["ref"]}.{a["name"]} и {b["ref"]}.{b["name"]}')
# корпуса деталей не налезают друг на друга, на чужие площадки и на головки винтов (M3, Ø6)
for i, (ra, ga) in enumerate(bodies):
    for rb, gb in bodies[i + 1:]:
        if ra != rb and ga.intersection(gb).area > 0.05: err.append(f'корпуса налезают: {ra} и {rb}')
    for p in pads:
        if p['ref'] != ra and p['layer'] == 'th' and ga.intersection(pad_geom(p)).area > 0.05: err.append(f'корпус {ra} закрывает площадку {p["ref"]}.{p["name"]}')
    for x, y, d in holes_npth:
        if ga.distance(Point(x, y)) < 3.0: err.append(f'корпус {ra} мешает винту в ({x}, {y})')

# сверка со схемой
CONN = json.load(io.open(os.path.join(HERE, '..', 'IRFB4110_фотомонтаж', 'overall-connections.json'), encoding='utf-8'))
MAP = {'J1.1': 'F2.2', 'J1.2': 'АКБ.P-', 'J3.1': 'T1.1', 'J3.2': 'T1.2', 'J2.1': 'ESP.32', 'J2.2': 'ESP.GND'}
PN = {'C4.+': 'C4.+', 'C4.-': 'C4.-'}
par = {}
def f(x):
    par.setdefault(x, x)
    while par[x] != x: par[x] = par[par[x]]; x = par[x]
    return x
for c in CONN['instructions']: par[f(c['a'])] = f(c['b'])
board_ports = {}
for p in pads:
    if not p['net'] or p['ref'] == 'JP1': continue
    key = f"{p['ref']}.{p['name']}"
    key = MAP.get(key, key)
    if p['ref'][0] == 'C' and p['name'] in '12': key = f"{p['ref']}.{'+' if p['name'] == '1' else '-'}"
    if p['ref'] == 'Q1' or p['ref'][0] == 'Q': key = f"{p['ref']}.{p['name']}"
    board_ports[key] = p['net']
by_board = collections.defaultdict(set); by_sch = collections.defaultdict(set)
for k, n in board_ports.items():
    if k not in par and k not in {q for pp in CONN['parts'] for q in pp['ports']}: err.append(f'нет в схеме: {k}'); continue
    by_board[n].add(k); by_sch[f(k)].add(k)
A = sorted(sorted(v) for v in by_board.values()); B = sorted(sorted(v) for v in by_sch.values())
if A != B:
    for v in A:
        if v not in B: err.append('плата: ' + ' '.join(v))
    for v in B:
        if v not in A: err.append('схема: ' + ' '.join(v))

print(f'деталей: {len(parts)}, площадок: {len(pads)}, дорожек: {len(traces)}, перемычек: {len(jumpers)}')
if err:
    print('ОШИБКИ:'); [print('  ' + e) for e in err]
json.dump({'pads': pads, 'traces': traces, 'jumpers': jumpers, 'holes': holes_npth, 'W': W, 'H': H}, io.open(os.path.join(OUT, 'layout.json'), 'w', encoding='utf-8'), ensure_ascii=False)

# ── картинка для проверки ───────────────────────────────────────────────────────
def silk_geom(items, mirror=False):
    g = []
    for kind, w, d in items:
        if kind == 'line': g.append(LineString(d).buffer(w / 2, 8))
        elif kind == 'circle': g.append(Point(d[0], d[1]).buffer(d[2] + w / 2).difference(Point(d[0], d[1]).buffer(max(d[2] - w / 2, 0))))
        else: g += text_polys(d[0], d[1], d[2], w, d[3], mirror)
    return unary_union(g) if g else Polygon()
SILK = silk_geom(silk); SILK_B = silk_geom(silk_b, mirror=True)

colors = {'GND': '#6b7c8f', '+12V': '#e07000', 'VPRE': '#b08900', 'AUX1': '#c0392b', 'AUX2': '#c0392b', 'EN': '#12805c', 'TH': '#12805c', 'IN': '#7b34b7',
          'GPIO': '#7b34b7', 'OUTA': '#185bbd', 'OUTB': '#00849b', 'G1': '#185bbd', 'G2': '#185bbd', 'G3': '#00849b', 'G4': '#00849b'}
fig, ax = plt.subplots(figsize=(W / 8, H / 8), dpi=110)
ax.add_patch(MPoly([(0, 0), (W, 0), (W, H), (0, H)], fc='#0f3b23', ec='k'))
def mpoly2(ax, g, fc, z):
    for q in (g.geoms if hasattr(g, 'geoms') else [g]):
        if q.is_empty: continue
        ax.add_patch(MPoly(list(q.exterior.coords), closed=True, fc=fc, ec='none', zorder=z))
        for r in q.interiors: ax.add_patch(MPoly(list(r.coords), closed=True, fc='#0f3b23', ec='none', zorder=z + .1))
mpoly2(ax, cu['GND'], colors['GND'], 2)
for n, g in cu.items():
    if n and n != 'GND': mpoly2(ax, g, colors.get(n, '#999'), 3)
for p in pads:
    if p['net'] is None: mpoly(ax, pad_geom(p), fc='#999', ec='none', zorder=2)
for p in pads:
    if p['drill']: ax.add_patch(plt.Circle((p['x'], p['y']), p['drill'] / 2, fc='black', zorder=4))
for x, y, d in holes_npth: ax.add_patch(plt.Circle((x, y), d / 2, fc='black', zorder=4))
from matplotlib.path import Path
from matplotlib.patches import PathPatch
def ppatch(ax, g, fc, z):
    for q in (g.geoms if hasattr(g, 'geoms') else [g]):
        if q.is_empty: continue
        verts, codes = [], []
        for ring in [q.exterior, *q.interiors]:
            c = list(ring.coords); verts += c; codes += [Path.MOVETO] + [Path.LINETO] * (len(c) - 2) + [Path.CLOSEPOLY]
        ax.add_patch(PathPatch(Path(verts, codes), fc=fc, ec='none', zorder=z))
ppatch(ax, SILK, '#ffffffdd', 5)
for _, a, b in jumpers: ax.plot([a[0], b[0]], [a[1], b[1]], color='yellow', lw=2, zorder=6)
for e in err: pass
ax.set_xlim(-1, W + 1); ax.set_ylim(H + 1, -1); ax.set_aspect('equal'); ax.axis('off')
fig.savefig(os.path.join(OUT, 'проверка.png'), bbox_inches='tight', facecolor='white'); plt.close(fig)
json.dump({'err': err}, io.open(os.path.join(OUT, 'drc.json'), 'w', encoding='utf-8'), ensure_ascii=False)
if err: sys.exit(1)
print('проверки пройдены')

exec(io.open(os.path.join(HERE, 'pcb_output.py'), encoding='utf-8').read())

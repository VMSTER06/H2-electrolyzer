# Вывод файлов платы. Подключается в конце make_pcb.py (exec), использует его переменные.
from shapely.geometry.polygon import orient
from matplotlib.backends.backend_pdf import PdfPages

def polys_of(g):
    if g.is_empty: return []
    return [orient(q) for q in (g.geoms if hasattr(g, 'geoms') else [g]) if not q.is_empty and q.area > 1e-4]

def gerber(path, geom, func):
    f = io.open(path, 'w', encoding='ascii', newline='\n')
    f.write(f'G04 H2 IRFB4110 driver v1*\n%TF.FileFunction,{func}*%\n%FSLAX46Y46*%\n%MOMM*%\n%LPD*%\nG01*\n')
    def xy(x, y): return f'X{round(x * 1e6)}Y{round((H - y) * 1e6)}'
    def ring(coords):
        c = list(coords)
        f.write('G36*\n' + xy(*c[0]) + 'D02*\n' + ''.join(xy(*p) + 'D01*\n' for p in c[1:]) + 'G37*\n')
    for q in sorted(polys_of(geom), key=lambda q: -Polygon(q.exterior).area):
        f.write('%LPD*%\n'); ring(q.exterior.coords)
        if q.interiors:
            f.write('%LPC*%\n')
            for r in q.interiors: ring(r.coords)
            f.write('%LPD*%\n')
    f.write('M02*\n'); f.close()

def outline(path):
    f = io.open(path, 'w', encoding='ascii', newline='\n')
    f.write('G04 board outline*\n%TF.FileFunction,Profile,NP*%\n%FSLAX46Y46*%\n%MOMM*%\n%ADD10C,0.150000*%\nD10*\nG01*\n')
    pts = [(0, 0), (W, 0), (W, H), (0, H), (0, 0)]
    f.write(''.join(f'X{round(x * 1e6)}Y{round((H - y) * 1e6)}D0{2 if i == 0 else 1}*\n' for i, (x, y) in enumerate(pts)))
    f.write('M02*\n'); f.close()

def drill(path, holes, plated):
    tools = sorted({round(d, 2) for _, _, d in holes})
    f = io.open(path, 'w', encoding='ascii', newline='\n')
    f.write('M48\n; H2 IRFB4110 driver v1\n' + ('; #@! TF.FileFunction,Plated,1,2,PTH\n' if plated else '; #@! TF.FileFunction,NonPlated,1,2,NPTH\n') + 'METRIC,TZ\n')
    for i, d in enumerate(tools, 1): f.write(f'T{i}C{d:.3f}\n')
    f.write('%\n')
    for i, d in enumerate(tools, 1):
        f.write(f'T{i}\n')
        for x, y, dd in holes:
            if round(dd, 2) == d: f.write(f'X{x:.3f}Y{H - y:.3f}\n')
    f.write('T0\nM30\n'); f.close()

B_CU = unary_union([g for n, g in cu.items() if n] + [pad_geom(p) for p in pads if p['net'] is None])
F_CU = unary_union([pad_geom(p) for p in pads if p['layer'] == 'th'] + [LineString([a, b]).buffer(0.5, 16) for _, a, b in jumpers])
B_MASK = unary_union([pad_geom(p, 0.05) for p in pads])
F_MASK = unary_union([pad_geom(p, 0.05) for p in pads if p['layer'] == 'th'])
clip = box(0.3, 0.3, W - 0.3, H - 0.3)
F_SILK = SILK.intersection(clip).difference(F_MASK.buffer(0.15))
B_SILK = SILK_B.intersection(clip).difference(B_MASK.buffer(0.15))
G = os.path.join(OUT, 'gerber'); os.makedirs(G, exist_ok=True)
for fn in os.listdir(G): os.remove(os.path.join(G, fn))
pre = os.path.join(G, 'h2_irfb4110_driver')
gerber(pre + '-B_Cu.gbl', B_CU, 'Copper,L2,Bot'); gerber(pre + '-F_Cu.gtl', F_CU, 'Copper,L1,Top')
gerber(pre + '-B_Mask.gbs', B_MASK, 'Soldermask,Bot'); gerber(pre + '-F_Mask.gts', F_MASK, 'Soldermask,Top')
gerber(pre + '-B_Silkscreen.gbo', B_SILK, 'Legend,Bot'); gerber(pre + '-F_Silkscreen.gto', F_SILK, 'Legend,Top')
outline(pre + '-Edge_Cuts.gm1')
drill(pre + '-PTH.drl', [(p['x'], p['y'], p['drill']) for p in pads if p['drill']], True)
drill(pre + '-NPTH.drl', holes_npth, False)
zp = os.path.join(OUT, 'h2_driver_gerber.zip')
with zipfile.ZipFile(zp, 'w', zipfile.ZIP_DEFLATED) as z:
    for fn in sorted(os.listdir(G)): z.write(os.path.join(G, fn), fn)

# ── PDF в масштабе 1:1 ─────────────────────────────────────────────────────────
A4 = (210, 297)
def page(pdf, title, draw, note):
    fig = plt.figure(figsize=(A4[0] / 25.4, A4[1] / 25.4))
    x0, y0 = (A4[0] - W) / 2, 60
    ax = fig.add_axes([x0 / A4[0], 1 - (y0 + H) / A4[1], W / A4[0], H / A4[1]])
    ax.set_xlim(0, W); ax.set_ylim(H, 0); ax.axis('off')
    draw(ax)
    fig.text(0.5, 1 - 20 / A4[1], title, ha='center', va='center', fontsize=15, weight='bold', family='DejaVu Sans')
    fig.text(0.5, 1 - 30 / A4[1], note, ha='center', va='top', fontsize=8.5, family='DejaVu Sans', linespacing=1.5)
    rx = fig.add_axes([x0 / A4[0], 1 - (y0 + H + 22) / A4[1], 50 / A4[0], 8 / A4[1]]); rx.set_xlim(0, 50); rx.set_ylim(0, 8); rx.axis('off')
    rx.plot([0, 50], [2, 2], 'k', lw=1.2)
    for i in range(0, 51, 5): rx.plot([i, i], [2, 4 if i % 10 else 5.5], 'k', lw=1)
    rx.text(25, 9.5, 'линейка 50 мм: проверь штангенциркулем, что распечатка в масштабе 1:1', ha='center', va='bottom', fontsize=8, family='DejaVu Sans')
    pdf.savefig(fig); plt.close(fig)

def draw_lut(ax):
    ppatch(ax, B_CU, 'black', 2)
    for p in pads:
        if p['drill']: ax.add_patch(plt.Circle((p['x'], p['y']), 0.3, fc='white', ec='none', zorder=3))
    ax.add_patch(MPoly([(0, 0), (W, 0), (W, H), (0, H)], closed=True, fc='none', ec='black', lw=0.4, zorder=4))
    for x, y, d in holes_npth: ax.add_patch(plt.Circle((x, y), d / 2, fc='none', ec='black', lw=0.4, zorder=4))

def draw_asm(ax):
    ax.add_patch(MPoly([(0, 0), (W, 0), (W, H), (0, H)], closed=True, fc='#eef2f6', ec='none', zorder=0))
    for p in pads:
        if p['layer'] == 'th': ppatch(ax, pad_geom(p), '#c9a227', 2)
    ppatch(ax, F_SILK, 'black', 3)
    for p in pads:
        if p['drill']: ax.add_patch(plt.Circle((p['x'], p['y']), p['drill'] / 2, fc='white', ec='none', zorder=4))
    for _, a, b in jumpers: ax.plot([a[0], b[0]], [a[1], b[1]], color='#d50032', lw=2.5, zorder=5)
    ax.add_patch(MPoly([(0, 0), (W, 0), (W, H), (0, H)], closed=True, fc='none', ec='black', lw=0.6, zorder=6))
    for x, y, d in holes_npth: ax.add_patch(plt.Circle((x, y), d / 2, fc='white', ec='black', lw=0.4, zorder=6))

with PdfPages(os.path.join(OUT, 'ЛУТ_нижний_слой_1-1.pdf')) as pdf:
    page(pdf, 'ЛУТ: нижний слой (медь), масштаб 1:1', draw_lut,
         'Печатать лазерным принтером в масштабе 100 % («реальный размер», без подгонки под страницу).\n'
         'Надпись «НИЗ» на распечатке зеркальная — так и должно быть:\n'
         'после переноса на медь она читается правильно.\n'
         'Белые точки — центры отверстий для сверловки. U1 паяется прямо на медь (снизу).')
with PdfPages(os.path.join(OUT, 'Сборка_расположение_деталей.pdf')) as pdf:
    page(pdf, 'Сборка: вид сверху (со стороны деталей), 1:1', draw_asm,
         'Все детали, кроме U1, ставятся сверху. U1 (UCC27524DR) — снизу, на сторону меди,\n'
         'ножкой 1 к точке на нижней шелкографии (на плате ЛУТ — как на рисунке нижнего слоя).\n'
         'JP1 (красная линия): на заводской плате это уже дорожка, на плате ЛУТ — кусок провода сверху.\n'
         'Q1–Q4 стоят по нижнему краю металлом наружу, к радиатору.\n'
         'Силовые провода 4 мм² паять прямо к ножкам D и S, ток не идёт через плату.')

# ── картинки для просмотра ──────────────────────────────────────────────────────
for name, layer in (('вид_медь_низ.png', 'b'), ('вид_сверху.png', 't')):
    fig, ax = plt.subplots(figsize=(W / 10, H / 10), dpi=150)
    ax.set_xlim(-1, W + 1); ax.set_ylim(H + 1, -1); ax.set_aspect('equal'); ax.axis('off')
    ax.add_patch(MPoly([(0, 0), (W, 0), (W, H), (0, H)], closed=True, fc='#1d5c3a', ec='black', zorder=0))
    if layer == 'b':
        ppatch(ax, B_CU, '#d9a441', 1); ppatch(ax, B_SILK, 'white', 3)
    else:
        ppatch(ax, B_CU, '#2a6e49', 1); ppatch(ax, F_CU, '#d9a441', 2); ppatch(ax, F_SILK, 'white', 3)
    for p in pads:
        if p['drill']: ax.add_patch(plt.Circle((p['x'], p['y']), p['drill'] / 2, fc='#111', zorder=4))
    for x, y, d in holes_npth: ax.add_patch(plt.Circle((x, y), d / 2, fc='#111', zorder=4))
    fig.savefig(os.path.join(OUT, name), bbox_inches='tight', facecolor='white'); plt.close(fig)
print('файлы:', ', '.join(sorted(os.listdir(OUT))))

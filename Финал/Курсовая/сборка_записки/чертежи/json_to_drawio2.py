"""draw.io version of Э1/Э2 in the sample frame (same geometry as draw_e12.py / eskd.py).
Usage: py -3 json_to_drawio2.py e2_a3.json e2 out.drawio"""
import sys, io, json, importlib.util
spec = importlib.util.spec_from_file_location('j1', __file__.replace('json_to_drawio2.py', 'json_to_drawio.py'))
src = io.open(spec.origin, encoding='utf-8').read().split('def build(')[0]
exec(src)                                  # Page, mm, K

ROWS = list('ABCDEFGHJKLMNPQRS')


def build2(path, kind):
    s = json.load(io.open(path, encoding='utf-8'))
    SW, SH = s['sheet_w'], s['sheet_h']
    lwM, lwT, lwL = 0.7, 0.25, 0.35
    pg = Page(kind.upper())
    fx, fy, fw, fh = 20.0, 5.0, SW - 25.0, SH - 10.0
    pg.rect(fx, fy, fw, fh, lwM)
    tb = s['stamp']
    pg.rect(fx, fy, 70, 14, lwM)
    pg.text(tb['designation'], fx, fy, 70, 14, 13, italic=True, rotation=180)
    sx, sy = fx + fw - 185, fy + fh - 55
    pg.rect(sx, sy, 185, 55, lwM)
    pg.line(sx + 65, sy, sx + 65, sy + 55, lwM); pg.line(sx + 7, sy, sx + 7, sy + 25, lwM)
    for xx in (17, 40, 55): pg.line(sx + xx, sy, sx + xx, sy + 55, lwM)
    for yy in (5, 10, 15, 20, 30, 35, 40, 45, 50): pg.line(sx, sy + yy, sx + 65, sy + yy, lwT)
    pg.line(sx, sy + 25, sx + 65, sy + 25, lwM)
    for t, x, w in (('Изм.', 0, 7), ('Лист', 7, 10), ('№ докум.', 17, 23), ('Подп.', 40, 15), ('Дата', 55, 10)):
        pg.text(t, sx + x, sy + 20, w, 5, 6, italic=True)
    for i, (r, n) in enumerate(zip(['Разраб.', 'Пров.', 'Т.контр.', 'Реценз.', 'Н.контр.', 'Утв.'] if kind == 'e2' else ['Разраб.', 'Пров.', '', '', '', ''],
                                   [tb['developer'], tb['checker'], '', '', '', ''])):
        if r: pg.text(r, sx, sy + 25 + 5 * i, 17, 5, 7.5, align='left', italic=True)
        if n: pg.text(n, sx + 17, sy + 25 + 5 * i, 23, 5, 7.5, align='left', italic=True)
    pg.line(sx + 65, sy + 15, sx + 185, sy + 15, lwM); pg.line(sx + 135, sy + 15, sx + 135, sy + 55, lwM)
    pg.text(tb['designation'], sx + 65, sy, 120, 15, 20, italic=True)
    name = tb['name']
    if kind == 'e2':
        pg.line(sx + 65, sy + 40, sx + 135, sy + 40, lwM)
        pg.text(name, sx + 66, sy + 15, 68, 25, 9, italic=True)
        for xx in (150, 167): pg.line(sx + xx, sy + 15, sx + xx, sy + 35, lwM)
        for yy in (20, 35, 40): pg.line(sx + 135, sy + yy, sx + 185, sy + yy, lwM)
        for xx in (140, 145): pg.line(sx + xx, sy + 20, sx + xx, sy + 35, lwT)
        pg.line(sx + 155, sy + 35, sx + 155, sy + 40, lwT)
        pg.text('Лит.', sx + 135, sy + 15, 15, 5, 6.5, italic=True)
        pg.text('Масса', sx + 150, sy + 15, 17, 5, 6.5, italic=True)
        pg.text('Масштаб', sx + 167, sy + 15, 18, 5, 6.5, italic=True)
        pg.text('У', sx + 135, sy + 20, 5, 15, 9, italic=True)
        pg.text('Лист', sx + 135, sy + 35, 20, 5, 6.5, align='left', italic=True)
        pg.text('Листов 1', sx + 155, sy + 35, 30, 5, 6.5, align='left', italic=True)
        pg.text(tb['org'], sx + 135, sy + 40, 50, 15, 11, italic=True)
        # zone grid
        x0, x1, yt, cols = fx + 12, fx + fw - 8, fy + 20, 24
        cw = (x1 - x0) / cols
        pg.line(x0, yt, x1, yt, lwM)
        for i in range(cols + 1): pg.line(x0 + i * cw, yt - 1.8, x0 + i * cw, yt + 1.8, lwL)
        for i in range(cols): pg.text(f'{i + 1:02d}', x0 + i * cw, yt + 1.5, cw, 5, 8.5, italic=True)
        xl, y0, y1 = fx + 10, yt + 9, fy + fh - 6
        rh = (y1 - y0) / len(ROWS)
        pg.line(xl, y0, xl, y1, lwM)
        for i in range(len(ROWS) + 1): pg.line(xl - 1.8, y0 + i * rh, xl + 1.8, y0 + i * rh, lwL)
        for i, r in enumerate(ROWS): pg.text(r, xl - 8, y0 + i * rh, 6, rh, 8.5, italic=True)
        zone = lambda x, y: f"{ROWS[min(len(ROWS) - 1, int((y - y0) // rh))]}{int((x - x0) // cw) + 1:02d}"
        X0, Y0, kx, ky = 36, 34, (228 - 36) / (227 - 25), (288 - 34) / (287 - 10)
        T = lambda x, y: (X0 + (x - 25) * kx, Y0 + (y - 10) * ky)
    else:
        pg.text(name, sx + 66, sy + 15, 68, 40, 9, italic=True)
        pg.line(sx + 135, sy + 20, sx + 185, sy + 20, lwT); pg.line(sx + 135, sy + 25, sx + 185, sy + 25, lwM)
        for xx in (150, 165): pg.line(sx + xx, sy + 15, sx + xx, sy + 25, lwM)
        for xx in (140, 145): pg.line(sx + xx, sy + 20, sx + xx, sy + 25, lwT)
        pg.text('Лит.', sx + 135, sy + 15, 15, 5, 6.5, italic=True); pg.text('Лист', sx + 150, sy + 15, 15, 5, 6.5, italic=True)
        pg.text('Листов', sx + 165, sy + 15, 20, 5, 6.5, italic=True)
        pg.text('1', sx + 150, sy + 20, 15, 5, 7.5, italic=True)
        pg.text('1', sx + 165, sy + 20, 20, 5, 7.5, italic=True)
        pg.text(tb['org'], sx + 135, sy + 25, 50, 30, 11, italic=True)
        kx = ky = 1; T = lambda x, y: (x, y); zone = None
    for it in s['items']:
        t = it['t']
        if t in ('box', 'dash', 'text'):
            x, y = T(it['x'], it['y']); w, h = it['w'] * kx, it['h'] * ky
            if t == 'box':
                pg.box(it.get('text', ''), x, y, w, h, lwL, it.get('size', 12))
                if zone: pg.text(zone(x + w / 2, y + h / 2), x + w - 12, y + h - 4.5, 11.5, 4, 6.5, align='right', italic=True)
            elif t == 'dash':
                pg.rect(x, y, w, h, lwL, extra='dashed=1;dashPattern=10 3 1.5 3;')
            else:
                pg.text(it['text'], x, y, w, h, it.get('size', 12), align='left' if it.get('a') == 'l' else 'center')
        elif t == 'line':
            pts = [T(*p) for p in it['pts']]
            n = it.get('n', 1)
            pg.line(pts[0][0], pts[0][1], pts[-1][0], pts[-1][1], lwM if n > 1 else lwL, dashed=it.get('dashed', False),
                    pts=pts[1:-1], arrow=it.get('arrow', False), arrow0=it.get('arrow0', False))
            if n > 1:   # шина: штрих и число линий (СТП 3.2.5–3.2.7)
                (x1, y1), (x2, y2) = max(zip(pts, pts[1:]), key=lambda pq: abs(pq[1][0] - pq[0][0]) + abs(pq[1][1] - pq[0][1]))
                if abs(y2 - y1) < 0.01:
                    L = abs(x2 - x1); mx, my = x1 + (x2 - x1) * (0.5 if L < 15 else 0.3 if L < 25 else 0.13), y1
                    pg.line(mx - 1.2, my + 1.5, mx + 1.2, my - 1.5, lwL)
                    pg.text(str(n), mx, my + 1.6, 5, 4, 8, align='left', italic=True)
                else:
                    mx, my = x1, (y1 + y2) / 2
                    pg.line(mx - 1.5, my + 1.2, mx + 1.5, my - 1.2, lwL)
                    pg.text(str(n), mx - 6, my, 5, 4, 8, align='right', italic=True)
    return pg.xml(SW, SH)


if __name__ == '__main__':
    x = build2(sys.argv[1], sys.argv[2])
    io.open(sys.argv[3], 'w', encoding='utf-8').write('<mxfile host="app.diagrams.net">' + x + '</mxfile>')
    print('saved', sys.argv[3])

"""Э1 и Э2 (А3) в рамке по образцу кафедры: повернутая графа обозначения слева вверху,
Э1 — упрощенная основная надпись, Э2 — сетка зон, коды зон у блоков, форма 1 с «Масса/Масштаб».
Содержимое схем берется из e1_a3.json / e2_a3.json (координаты в мм)."""
import sys, io, json
from eskd import Sheet, LW_LINE, LW_MAIN

PT = 0.3528 * 0.716          # pt -> capital height, mm
ROWS = list('ABCDEFGHJKLMNPQRS')


def render(spec, kind):
    d = json.load(io.open(spec, encoding='utf-8'))
    st = dict(d['stamp']); st['liter'] = 'У' if kind == 'e2' else ''
    if kind == 'e1': st['keep_sheet'] = True
    st['name'] = st['name'].replace('\\n', '\n')
    s = Sheet(d['sheet_w'], d['sheet_h'])
    s.frame(st['designation'])
    if kind == 'e2':
        s.zone_grid(24, ROWS)
        s.stamp_form1(st, full=True)
        # content of e2_a3.json was laid out for x 25..397, y 10..287 -> fit into the grid area
        X0, Y0, sx, sy = 36, 34, (228 - 36) / (227 - 25), (288 - 34) / (287 - 10)
        T = lambda x, y: (X0 + (x - 25) * sx, Y0 + (y - 10) * sy)
    else:
        s.stamp_form1(st, full=False)
        T = lambda x, y: (x, y)
        sx = sy = 1
    for it in d['items']:
        t = it['t']
        if t in ('box', 'dash'):
            x, y = T(it['x'], it['y']); w, h = it['w'] * sx, it['h'] * sy
            if t == 'box':
                s.rect(x, y, w, h)
                txt = it.get('text', '').replace('\\n', '\n')
                hh = it.get('size', 12) * PT
                if kind == 'e2':
                    # как у Лазовского: наименование вверху блока, код зоны крупно по центру внизу
                    s.text_fit(x + w / 2, y + 2.2, w - 2, txt, hh, 'ma', italic=True)
                    s.text(x + w / 2, y + h - 2.8, s.zone(x + w / 2, y + h / 2), 3.0, 'mm')
                else:
                    s.text_fit(x + w / 2, y + h / 2, w - 2, txt, hh)
            else:
                s.rect(x, y, w, h, dash=(8, 2, 1.5, 2))
        elif t == 'line':
            pts = [T(*p) for p in it['pts']]
            n = it.get('n', 1)
            if n > 1:
                # СТП 3.2.5–3.2.7: связь из нескольких проводов — шина (линия 2b) со штрихом и числом линий
                s.line(pts, LW_MAIN)
                if it.get('arrow'): s.arrow(*pts[-2], *pts[-1], 3.0, lw=LW_MAIN)
                if it.get('arrow0'): s.arrow(*pts[1], *pts[0], 3.0, lw=LW_MAIN)
                # штрих — посередине самого длинного участка, вдали от стрелок; число линий — рядом со штрихом
                (x1, y1), (x2, y2) = max(zip(pts, pts[1:]), key=lambda pq: abs(pq[1][0] - pq[0][0]) + abs(pq[1][1] - pq[0][1]))
                f = 0.25 if abs(x2 - x1) + abs(y2 - y1) > 25 else 0.5
                mx, my = x1 + (x2 - x1) * f, y1 + (y2 - y1) * f
                s.line([(mx - 1.2, my + 1.6), (mx + 1.2, my - 1.6)], LW_LINE)
                if abs(y2 - y1) < 0.01:      # горизонтальный участок: число под линией справа от штриха
                    s.text(mx + 1.0, my + 3.0, str(n), 2.5, 'lm', italic=(kind == 'e2'))
                else:                         # вертикальный участок: число справа от штриха
                    s.text(mx + 1.8, my + 1.2, str(n), 2.5, 'lm', italic=(kind == 'e2'))
                continue
            s.line(pts, dash=(4, 2) if it.get('dashed') else None)
            if it.get('arrow'): s.arrow(*pts[-2], *pts[-1], 3.0)
            if it.get('arrow0'): s.arrow(*pts[1], *pts[0], 3.0)
        elif t == 'text':
            x, y = T(it['x'], it['y']); w, h = it['w'] * sx, it['h'] * sy
            txt = it['text'].replace('\\n', '\n')
            hh = it.get('size', 12) * PT
            if it.get('a') == 'l':
                s.text(x, y + (h / 2 if '\n' not in txt else 0), txt, hh, 'lm' if '\n' not in txt else 'la',
                       italic=(kind == 'e2'))
            else:
                if it.get('top'): s.text_fit(x + w / 2, y + 2.2, max(w - 2, 10), txt, hh, 'ma', italic=(kind == 'e2'))
                else: s.text_fit(x + w / 2, y + h / 2, max(w, 10), txt, hh, italic=(kind == 'e2'))
    return s


if __name__ == '__main__':
    spec, kind, out = sys.argv[1], sys.argv[2], sys.argv[3]
    s = render(spec, kind)
    s.save(out + '.png', out + '.pdf')
    print('ok', out)

"""ESKD sheet renderer (PIL, 300 dpi, all coordinates in mm) modelled on the department samples:
frame 20/5/5/5 mm, designation box rotated 180° in the top-left corner, GOST 2.104 title blocks
(form 1 full / simplified, form 2, form 2a), zone grid, UGO primitives for schemes."""
from PIL import Image, ImageDraw, ImageFont
import math

DPI = 300
K = DPI / 25.4
FONT_I = 'C:/Windows/Fonts/ariali.ttf'
FONT_R = 'C:/Windows/Fonts/arial.ttf'
_fc = {}

def font(h, italic=True):
    """h = capital letter height in mm."""
    key = (round(h, 2), italic)
    if key not in _fc:
        _fc[key] = ImageFont.truetype(FONT_I if italic else FONT_R, max(6, int(round(h / 0.716 * K))))
    return _fc[key]

LW_MAIN, LW_THIN, LW_LINE = 0.7, 0.25, 0.35


class Sheet:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.img = Image.new('RGB', (int(round(w * K)), int(round(h * K))), 'white')
        self.d = ImageDraw.Draw(self.img)
        self.fx, self.fy, self.fw, self.fh = 20.0, 5.0, w - 25.0, h - 10.0

    # ---------- basic ----------
    def P(self, x, y):
        return (x * K, y * K)

    def line(self, pts, lw=LW_LINE, dash=None):
        pts = [self.P(*p) for p in pts]
        width = max(1, int(round(lw * K)))
        if not dash:
            self.d.line(pts, fill='black', width=width, joint='curve')
            for p in pts[1:-1]:
                r = width / 2
                self.d.ellipse([p[0] - r, p[1] - r, p[0] + r, p[1] + r], fill='black')
            return
        on, off = dash[0] * K, dash[1] * K
        for (x1, y1), (x2, y2) in zip(pts, pts[1:]):
            L = math.hypot(x2 - x1, y2 - y1)
            if L == 0: continue
            ux, uy = (x2 - x1) / L, (y2 - y1) / L
            t = 0.0
            pattern = dash if len(dash) > 2 else (dash[0], dash[1])
            i = 0
            while t < L:
                seg = pattern[i % len(pattern)] * K
                if i % 2 == 0:
                    e = min(t + seg, L)
                    self.d.line([(x1 + ux * t, y1 + uy * t), (x1 + ux * e, y1 + uy * e)], fill='black', width=width)
                t += seg; i += 1

    def rect(self, x, y, w, h, lw=LW_LINE, dash=None):
        if dash:
            self.line([(x, y), (x + w, y), (x + w, y + h), (x, y + h), (x, y)], lw, dash)
        else:
            self.d.rectangle([x * K, y * K, (x + w) * K, (y + h) * K], outline='black', width=max(1, int(round(lw * K))))

    def text(self, x, y, s, h=3.5, anchor='mm', italic=True):
        f = font(h, italic)
        lines = str(s).split('\n')
        lh = h * 1.6
        y0 = y - (len(lines) - 1) * lh / 2 if anchor[1] == 'm' else y
        for i, l in enumerate(lines):
            self.d.text(self.P(x, y0 + i * lh), l, fill='black', font=f, anchor=anchor)

    def text_fit(self, x, y, w, s, h=3.5, anchor='mm', italic=True, hmin=2.0):
        """shrink font so that the longest line fits into w mm"""
        while h > hmin:
            f = font(h, italic)
            if max(self.d.textlength(l, font=f) for l in str(s).split('\n')) <= w * K: break
            h -= 0.1
        self.text(x, y, s, h, anchor, italic)

    def dot(self, x, y, r=0.6):
        self.d.ellipse([(x - r) * K, (y - r) * K, (x + r) * K, (y + r) * K], fill='black')

    def arrow(self, x1, y1, x2, y2, L=3.0, filled=False, lw=LW_LINE, half=math.radians(30)):
        """СТП 3.2.11: стрелка направления сигнала — развал 60° (half = 30°); обрыв цепи питания — 30°"""
        a = math.atan2(y2 - y1, x2 - x1)
        p1 = (x2 - L * math.cos(a - half), y2 - L * math.sin(a - half))
        p2 = (x2 - L * math.cos(a + half), y2 - L * math.sin(a + half))
        if filled:
            self.d.polygon([self.P(x2, y2), self.P(*p1), self.P(*p2)], fill='black')
        else:
            self.line([p1, (x2, y2), p2], lw)

    # ---------- frame ----------
    def frame(self, designation, box=True):
        self.d.rectangle([0, 0, self.img.size[0] - 1, self.img.size[1] - 1], outline='black', width=2)
        self.rect(self.fx, self.fy, self.fw, self.fh, LW_MAIN)
        if not box: return
        # designation box rotated 180° (ESKD graph 26) in the top-left corner
        bx, by, bw, bh = self.fx, self.fy, 70, 14
        self.rect(bx, by, bw, bh, LW_MAIN)
        h = 5.0
        while h > 2.5 and ImageDraw.Draw(self.img).textlength(designation, font=font(h)) > (bw - 5) * K:
            h -= 0.1
        pad = int(LW_MAIN * K) + 2
        tw, th = int(bw * K) - 2 * pad, int(bh * K) - 2 * pad
        tmp = Image.new('RGB', (tw, th), 'white')
        ImageDraw.Draw(tmp).text((tw / 2, th / 2), designation, fill='black', font=font(h), anchor='mm')
        self.img.paste(tmp.rotate(180), (int(bx * K) + pad, int(by * K) + pad))

    def zone_grid(self, cols, rows, x0=None, x1=None, y_top=None, y0=None, y1=None, x_left=None):
        """top ruler with column numbers and a left ruler with row letters (as in the sample)"""
        x0 = x0 or self.fx + 12; x1 = x1 or self.fx + self.fw - 8
        y_top = y_top or self.fy + 20
        cw = (x1 - x0) / cols
        self.line([(x0, y_top), (x1, y_top)], LW_MAIN)
        for i in range(cols + 1):
            self.line([(x0 + i * cw, y_top - 1.8), (x0 + i * cw, y_top + 1.8)], LW_LINE)
        for i in range(cols):
            self.text(x0 + (i + 0.5) * cw, y_top + 4.2, f'{i + 1:02d}', 3.0)
        x_left = x_left or self.fx + 10
        y0 = y0 or y_top + 9; y1 = y1 or self.fy + self.fh - 6
        rh = (y1 - y0) / len(rows)
        self.line([(x_left, y0), (x_left, y1)], LW_MAIN)
        for i in range(len(rows) + 1):
            self.line([(x_left - 1.8, y0 + i * rh), (x_left + 1.8, y0 + i * rh)], LW_LINE)
        for i, r in enumerate(rows):
            self.text(x_left - 4, y0 + (i + 0.5) * rh, r, 3.0)
        self.grid = dict(x0=x0, cw=cw, y0=y0, rh=rh, rows=rows)

    def zone(self, x, y):
        g = self.grid
        c = int((x - g['x0']) // g['cw']) + 1
        r = g['rows'][min(len(g['rows']) - 1, max(0, int((y - g['y0']) // g['rh'])))]
        return f'{r}{c:02d}'

    # ---------- title blocks ----------
    def _left_part(self, sx, sy, rows_top, roles, names):
        """left 65 mm of a title block: rows_top rows above the header, header, then role rows"""
        n_rows = rows_top + 1 + len(roles)
        H = 5 * n_rows
        self.line([(sx + 65, sy), (sx + 65, sy + H)], LW_MAIN)
        for xx in (17, 40, 55):
            self.line([(sx + xx, sy), (sx + xx, sy + H)], LW_MAIN)
        self.line([(sx + 7, sy), (sx + 7, sy + 5 * (rows_top + 1))], LW_MAIN)
        for i in range(1, n_rows):
            lw = LW_MAIN if i == rows_top + 1 else LW_THIN
            self.line([(sx, sy + 5 * i), (sx + 65, sy + 5 * i)], lw)
        hy = sy + 5 * rows_top + 2.5
        for x, w, t in ((0, 7, 'Изм.'), (7, 10, 'Лист'), (17, 23, '№ докум.'), (40, 15, 'Подп.'), (55, 10, 'Дата')):
            self.text_fit(sx + x + w / 2, hy, w - 0.6, t, 2.2)
        for i, (r, n) in enumerate(zip(roles, names)):
            yy = sy + 5 * (rows_top + 1 + i) + 2.5
            if r: self.text_fit(sx + 0.7, yy, 16, r, 2.5, 'lm')
            if n: self.text_fit(sx + 17.7, yy, 22, n, 2.5, 'lm')

    def stamp_form1(self, st, full=True):
        """185×55; full=True: GOST 2.104 form 1 with Лит./Масса/Масштаб (Э2, Э3 in the sample),
        full=False: simplified variant Лит./Лист/Листов (Э1 in the sample)"""
        sx, sy = self.fx + self.fw - 185, self.fy + self.fh - 55
        if st.get('sheets') == '1' and not st.get('keep_sheet'): st = dict(st, sheet='')
        self.rect(sx, sy, 185, 55, LW_MAIN)
        roles = ['Разраб.', 'Пров.', 'Т.контр.', 'Реценз.', 'Н.контр.', 'Утв.'] if full else ['Разраб.', 'Пров.', '', '', '', '']
        self._left_part(sx, sy, 4, roles,
                        [st.get('developer', ''), st.get('checker', ''), '', '', st.get('normcontrol', ''), st.get('approved', '')])
        self.line([(sx + 65, sy + 15), (sx + 185, sy + 15)], LW_MAIN)
        self.text_fit(sx + 125, sy + 7.5, 116, st['designation'], 7.0)
        self.line([(sx + 135, sy + 15), (sx + 135, sy + 55)], LW_MAIN)
        if full:
            self.line([(sx + 65, sy + 40), (sx + 135, sy + 40)], LW_MAIN)
            self.text_fit(sx + 100, sy + 27.5, 68, st['name'], 3.0)
            for xx in (150, 167):
                self.line([(sx + xx, sy + 15), (sx + xx, sy + 35)], LW_MAIN)
            self.line([(sx + 135, sy + 20), (sx + 185, sy + 20)], LW_MAIN)
            self.line([(sx + 135, sy + 35), (sx + 185, sy + 35)], LW_MAIN)
            self.line([(sx + 135, sy + 40), (sx + 185, sy + 40)], LW_MAIN)
            for xx in (140, 145):
                self.line([(sx + xx, sy + 20), (sx + xx, sy + 35)], LW_THIN)
            self.line([(sx + 155, sy + 35), (sx + 155, sy + 40)], LW_THIN)
            self.text(sx + 142.5, sy + 17.5, 'Лит.', 2.5); self.text(sx + 158.5, sy + 17.5, 'Масса', 2.5)
            self.text(sx + 176, sy + 17.5, 'Масштаб', 2.5)
            self.text(sx + 137.5, sy + 27.5, st.get('liter', ''), 3.5)
            self.text(sx + 136, sy + 37.5, 'Лист ' + st.get('sheet', ''), 2.5, 'lm')
            self.text(sx + 156, sy + 37.5, 'Листов ' + st.get('sheets', ''), 2.5, 'lm')
            self.text_fit(sx + 160, sy + 47.5, 48, st['org'], 3.5)
        else:
            self.text_fit(sx + 100, sy + 35, 68, st['name'], 3.0)
            for xx, y2 in ((150, 25), (165, 25)):
                self.line([(sx + xx, sy + 15), (sx + xx, sy + y2)], LW_MAIN)
            self.line([(sx + 135, sy + 20), (sx + 185, sy + 20)], LW_THIN)
            self.line([(sx + 135, sy + 25), (sx + 185, sy + 25)], LW_MAIN)
            for xx in (140, 145):
                self.line([(sx + xx, sy + 20), (sx + xx, sy + 25)], LW_THIN)
            self.text(sx + 142.5, sy + 17.5, 'Лит.', 2.5); self.text(sx + 157.5, sy + 17.5, 'Лист', 2.5)
            self.text(sx + 175, sy + 17.5, 'Листов', 2.5)
            self.text(sx + 137.5, sy + 22.5, st.get('liter', ''), 2.5)
            self.text(sx + 157.5, sy + 22.5, st.get('sheet', ''), 2.5)
            self.text(sx + 175, sy + 22.5, st.get('sheets', ''), 2.5)
            self.text_fit(sx + 160, sy + 40, 48, st['org'], 3.5)

    def stamp_form2(self, st):
        """185×40 first sheet of a text document (перечень элементов, ведомость)"""
        sx, sy = self.fx + self.fw - 185, self.fy + self.fh - 40
        if st.get('sheets') == '1': st = dict(st, sheet='')
        self.rect(sx, sy, 185, 40, LW_MAIN)
        self._left_part(sx, sy, 2, ['Разраб.', 'Пров.', '', 'Н.контр.', 'Утв.'],
                        [st.get('developer', ''), st.get('checker', ''), '', st.get('normcontrol', ''), st.get('approved', '')])
        self.line([(sx + 65, sy + 15), (sx + 185, sy + 15)], LW_MAIN)
        self.text_fit(sx + 125, sy + 7.5, 116, st['designation'], 7.0)
        self.line([(sx + 135, sy + 15), (sx + 135, sy + 40)], LW_MAIN)
        self.text_fit(sx + 100, sy + 27.5, 68, st['name'], 3.0)
        for xx in (150, 165):
            self.line([(sx + xx, sy + 15), (sx + xx, sy + 25)], LW_MAIN)
        self.line([(sx + 135, sy + 20), (sx + 185, sy + 20)], LW_THIN)
        self.line([(sx + 135, sy + 25), (sx + 185, sy + 25)], LW_MAIN)
        for xx in (140, 145):
            self.line([(sx + xx, sy + 20), (sx + xx, sy + 25)], LW_THIN)
        self.text(sx + 142.5, sy + 17.5, 'Лит.', 2.5); self.text(sx + 157.5, sy + 17.5, 'Лист', 2.5)
        self.text(sx + 175, sy + 17.5, 'Листов', 2.5)
        self.text(sx + 137.5, sy + 22.5, st.get('liter', ''), 2.5)
        self.text(sx + 157.5, sy + 22.5, st.get('sheet', ''), 2.5)
        self.text(sx + 175, sy + 22.5, st.get('sheets', ''), 2.5)
        self.text_fit(sx + 160, sy + 32.5, 48, st['org'], 3.5)
        return sy

    def stamp_form2a(self, st):
        """185×15 subsequent sheets"""
        sx, sy = self.fx + self.fw - 185, self.fy + self.fh - 15
        self.rect(sx, sy, 185, 15, LW_MAIN)
        self._left_part(sx, sy, 2, [], [])
        self.line([(sx + 175, sy), (sx + 175, sy + 15)], LW_MAIN)
        self.line([(sx + 175, sy + 7), (sx + 185, sy + 7)], LW_THIN)
        self.text(sx + 180, sy + 3.5, 'Лист', 2.2)
        self.text(sx + 180, sy + 11, st.get('sheet', ''), 3.0)
        self.text_fit(sx + 120, sy + 7.5, 106, st['designation'], 6.0)
        return sy

    # ---------- UGO ----------
    def module(self, x, y, w, left, right, title, desig, pitch=6.0, top=6.0, fw=13.0, zone=True, h=None, pin_h=2.6, pinnum=True):
        """ESKD module: left pin field, central field (function + designator + zone), right pin field.
        Returns dict pin name -> (x, y) of the outer pin end."""
        n = max(len(left), len(right), 1)
        H = h or (top + n * pitch + 2)
        self.rect(x, y, w, H)
        if left: self.line([(x + fw, y), (x + fw, y + H)])
        if right: self.line([(x + w - fw, y), (x + w - fw, y + H)])
        cx = x + (fw if left else 0); cw = w - (fw if left else 0) - (fw if right else 0)
        self.text_fit(cx + cw / 2, y + 4.5, cw - 1, title, 3.0)
        if zone and hasattr(self, 'grid'):
            self.text(cx + cw / 2, y + H - 6.5, self.zone(x + w / 2, y + H / 2) + '\n' + desig, 2.8)
        else:
            self.text(cx + cw / 2, y + H - 4, desig, 3.0)
        pins = {}
        k = 0                                   # номера контактов модуля (01, 02, …) у выводов, как в образце кафедры
        for i, p in enumerate(left):
            yy = y + top + (i + 0.5) * pitch
            if p:
                self.text_fit(x + 0.8, yy, fw - 1.2, p, pin_h, 'lm')
                k += 1
                if pinnum: self.text(x - 0.8, yy - 1.5, f'{k:02d}', 2.3, 'rm')
            pins[p] = (x, yy)
        for i, p in enumerate(right):
            yy = y + top + (i + 0.5) * pitch
            if p:
                self.text_fit(x + w - 0.8, yy, fw - 1.2, p, pin_h, 'rm')
                k += 1
                if pinnum: self.text(x + w + 0.8, yy - 1.5, f'{k:02d}', 2.3, 'lm')
            pins[p] = (x + w, yy)
        return pins, H

    def resistor(self, x, y, vertical=False, label='', value=''):
        """ГОСТ 2.728: 10×4 mm, (x, y) = first terminal; returns other terminal"""
        if vertical:
            self.line([(x, y), (x, y + 3)]); self.rect(x - 2, y + 3, 4, 10); self.line([(x, y + 13), (x, y + 16)])
            if label: self.text(x + 3.0, y + 6.5, label, 2.6, 'lm')
            if value: self.text(x + 3.0, y + 10.5, value, 2.4, 'lm')
            return (x, y + 16)
        self.line([(x, y), (x + 3, y)]); self.rect(x + 3, y - 2, 10, 4); self.line([(x + 13, y), (x + 16, y)])
        if label: self.text(x + 8, y - 4.2, label, 2.6)
        if value: self.text(x + 8, y + 4.4, value, 2.4)
        return (x + 16, y)

    def capacitor(self, x, y, label='', value='', polar=False):
        """vertical capacitor, (x, y) top terminal; returns bottom terminal"""
        self.line([(x, y), (x, y + 5)]); self.line([(x - 4, y + 5), (x + 4, y + 5)], 0.45)
        self.line([(x - 4, y + 6.5), (x + 4, y + 6.5)], 0.45); self.line([(x, y + 6.5), (x, y + 11.5)])
        if polar: self.text(x - 3.5, y + 3.2, '+', 2.6)
        if label: self.text(x + 5, y + 4.5, label, 2.6, 'lm')
        if value: self.text(x + 5, y + 8.0, value, 2.4, 'lm')
        return (x, y + 11.5)

    def diode(self, x, y, vertical=True, label='', led=False, up=False):
        """vertical diode anode at top (current downward), (x, y) top terminal; returns bottom terminal.
        up=True: anode at bottom."""
        self.line([(x, y), (x, y + 4)])
        a, b = (y + 4, y + 9) if not up else (y + 9, y + 4)
        self.d.polygon([self.P(x - 2.5, a), self.P(x + 2.5, a), self.P(x, b)], outline='black', width=max(1, int(LW_LINE * K)))
        self.line([(x - 2.5, b), (x + 2.5, b)])
        self.line([(x, y + 9), (x, y + 13)])
        if led:
            for dy in (5.0, 7.5):
                self.arrow(x + 2.0, y + dy, x + 5.0, y + dy - 2.5, 1.4, True, half=0.26)
                self.line([(x + 2.0, y + dy), (x + 5.0, y + dy - 2.5)])
        if label: self.text(x + (6 if led else 4), y + 7, label, 2.6, 'lm')
        return (x, y + 13)

    def ground(self, x, y):
        self.line([(x, y), (x, y + 2.5)]); self.line([(x - 3, y + 2.5), (x + 3, y + 2.5)], 0.45)

    def supply(self, x, y, label, up=True):
        """power net label: short line with an arrow and the voltage"""
        if up:
            self.line([(x, y), (x, y - 4)]); self.arrow(x, y, x, y - 4, 1.8, half=math.radians(15))
            self.text(x, y - 6, label, 2.6)
        else:
            self.line([(x, y), (x, y + 4)]); self.arrow(x, y, x, y + 4, 1.8, half=math.radians(15))
            self.text(x, y + 6.5, label, 2.6)

    def button(self, x, y, label=''):
        """normally open push button, vertical, (x, y) top terminal; returns bottom terminal"""
        self.line([(x, y), (x, y + 4)]); self.line([(x, y + 10), (x, y + 14)])
        self.line([(x, y + 10), (x - 3.0, y + 4.8)])
        self.line([(x - 4.5, y + 7.2), (x - 1.6, y + 7.2)]); self.line([(x - 4.5, y + 6.0), (x - 4.5, y + 8.4)])
        if label: self.text(x + 2.0, y + 7, label, 2.6, 'lm')
        return (x, y + 14)

    def npn(self, x, y, label=''):
        """NPN transistor, base at (x, y) (left), returns (collector_top, emitter_bottom)"""
        cx = x + 7
        self.d.ellipse([(cx - 5) * K, (y - 5) * K, (cx + 5) * K, (y + 5) * K], outline='black', width=max(1, int(LW_LINE * K)))
        self.line([(x, y), (cx - 1.5, y)]); self.line([(cx - 1.5, y - 3), (cx - 1.5, y + 3)], 0.5)
        self.line([(cx - 1.5, y - 1.2), (cx + 2.2, y - 3.6), (cx + 2.2, y - 7)])
        self.line([(cx - 1.5, y + 1.2), (cx + 2.2, y + 3.6), (cx + 2.2, y + 7)])
        self.arrow(cx - 1.5, y + 1.2, cx + 1.6, y + 3.2, 1.6, True, half=0.35)
        if label: self.text(cx + 6, y, label, 2.6, 'lm')
        return (cx + 2.2, y - 7), (cx + 2.2, y + 7)

    def buzzer(self, x, y, label=''):
        """sound emitter HA (ГОСТ 2.741, simplified): rectangle with a horn, (x, y) top terminal; returns bottom"""
        self.line([(x, y), (x, y + 3)]); self.rect(x - 2.5, y + 3, 5, 6)
        self.d.polygon([self.P(x + 2.5, y + 4), self.P(x + 6, y + 1.5), self.P(x + 6, y + 10.5), self.P(x + 2.5, y + 8)],
                       outline='black', width=max(1, int(LW_LINE * K)))
        self.line([(x, y + 9), (x, y + 12)])
        if label: self.text(x + 7.5, y + 6, label, 2.6, 'lm')
        return (x, y + 12)

    def motor(self, x, y, label=''):
        """motor M, vertical, (x, y) top terminal; returns bottom terminal"""
        self.line([(x, y), (x, y + 2)])
        self.d.ellipse([(x - 4) * K, (y + 2) * K, (x + 4) * K, (y + 10) * K], outline='black', width=max(1, int(LW_LINE * K)))
        self.text(x, y + 6, 'M', 3.0); self.line([(x, y + 10), (x, y + 12)])
        if label: self.text(x + 5.5, y + 6, label, 2.6, 'lm')
        return (x, y + 12)

    def relay_contact(self, x, y, label=''):
        """normally open relay contact, vertical, (x, y) top; returns bottom"""
        self.line([(x, y), (x, y + 4)]); self.line([(x, y + 10), (x, y + 14)])
        self.line([(x, y + 10), (x - 3.0, y + 4.8)])
        if label: self.text(x + 2.0, y + 7, label, 2.6, 'lm')
        return (x, y + 14)

    def conn_table(self, x, y, desig, rows, title_w=12, w1=12, w2=22):
        """connector drawn as a table 'Конт. | Цепь' (as XP1/XP2 in the sample); returns pin -> (x_right, y)"""
        self.text(x + (w1 + w2) / 2, y - 3, desig, 3.0)
        self.rect(x, y, w1 + w2, 6 * (len(rows) + 1))
        self.line([(x + w1, y), (x + w1, y + 6 * (len(rows) + 1))])
        self.text(x + w1 / 2, y + 3, 'Конт.', 2.4); self.text(x + w1 + w2 / 2, y + 3, 'Цепь', 2.4)
        pins = {}
        for i, (c, n) in enumerate(rows):
            yy = y + 6 * (i + 1)
            self.line([(x, yy), (x + w1 + w2, yy)])
            self.text(x + w1 / 2, yy + 3, c, 2.6); self.text_fit(x + w1 + w2 / 2, yy + 3, w2 - 1, n, 2.6)
            pins[c] = (x + w1 + w2, yy + 3)
        return pins

    def wire(self, pts, num=None, at=0, side='above', lw=LW_LINE):
        """wire with its number written over the segment `at` (wire numbers are independent of pin numbers)"""
        self.line(pts, lw)
        if num is not None:
            (x1, y1), (x2, y2) = pts[at], pts[at + 1]
            mx = x1 + (x2 - x1) * 0.5
            if y1 == y2:
                self.text(mx, y1 - 2.0 if side == 'above' else y1 + 2.4, num, 2.6)
            else:
                self.text(x1 + 2.6, (y1 + y2) / 2, num, 2.6, 'lm')

    def save(self, png, pdf=None):
        self.img.save(png, dpi=(DPI, DPI))
        if pdf:
            self.img.save(pdf, 'PDF', resolution=DPI)

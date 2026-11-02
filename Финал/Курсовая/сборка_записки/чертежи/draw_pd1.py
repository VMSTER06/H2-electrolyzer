"""ПД1 — схема программы блока управления (ГОСТ 19.701–90), А3, рамка и основная надпись по образцу кафедры.
Колонки обходят основную надпись; переходы между колонками — через соединители А, Б, В.
Usage: py -3 draw_pd1.py <out>"""
import sys
sys.path.insert(0, __file__.rsplit('\\', 1)[0] if '\\' in __file__ else '.')
from eskd import Sheet, LW_LINE

STAMP = dict(designation='ГУИР.6-05-0611-05.114 ПД1', developer='Мурачев', checker='Селезнев', liter='У',
             name='Микропроцессорное устройство\nдистанционного управления\nобъектом\nСхема программы',
             org='БГУИР, гр. 434701', sheet='1', sheets='1')
W, H = 56.0, 14.0          # процесс: b = 4a (a = 14)
DW, DH = 56.0, 18.0        # решение
R = 4.0                    # соединитель
TH = 3.0                   # высота шрифта в символах


class Flow:
    def __init__(self, s):
        self.s = s

    def num(self, x, y, n):
        """номер блока — в верхней части символа в разрыве контура (СТП, табл. 3.23); (x, y) — центр разрыва"""
        s, k = self.s, self.s.P(1, 0)[0]
        w = 1.6 + 1.5 * len(str(n))
        s.d.rectangle([(x - w / 2) * k, (y - 1.0) * k, (x + w / 2) * k, (y + 1.0) * k], fill='white')
        s.text(x, y, str(n), 2.5)

    def proc(self, x, y, n, t, pre=False):
        s = self.s
        s.rect(x - W / 2, y - H / 2, W, H)
        if pre:
            for dx in (-W / 2 + 3, W / 2 - 3):
                s.line([(x + dx, y - H / 2), (x + dx, y + H / 2)])
        s.text_fit(x, y, W - (9 if pre else 4), t, TH)
        self.num(x - W / 2 + 7, y - H / 2, n)

    def term(self, x, y, n, t):
        s, w, h = self.s, 40.0, 12.0
        s.d.rounded_rectangle([(x - w / 2) * s.P(1, 0)[0], (y - h / 2) * s.P(1, 0)[0],
                               (x + w / 2) * s.P(1, 0)[0], (y + h / 2) * s.P(1, 0)[0]],
                              radius=h / 2 * s.P(1, 0)[0], outline='black', width=4)
        s.text(x, y, t, TH)
        self.num(x - w / 2 + 9, y - h / 2, n)

    def dec(self, x, y, n, t):
        s = self.s
        s.line([(x, y - DH / 2), (x + DW / 2, y), (x, y + DH / 2), (x - DW / 2, y), (x, y - DH / 2)])
        s.text_fit(x, y, DW * 0.62, t, 2.8)
        self.num(x - 3 * DW / 8, y - DH / 8, n)

    def conn(self, x, y, letter):
        s, k = self.s, self.s.P(1, 0)[0]
        s.d.ellipse([(x - R) * k, (y - R) * k, (x + R) * k, (y + R) * k], outline='black', width=4)
        s.text(x, y, letter, 3.5)

    def v(self, x, y1, y2, arrow=False):
        self.s.line([(x, y1), (x, y2)])
        if arrow: self.s.arrow(x, y1, x, y2, 2.5)

    def label(self, x, y, t, a='lm'):
        self.s.text(x, y, t, 2.5, a)


def draw(out):
    s = Sheet(420, 297)
    s.frame(STAMP['designation'])
    s.stamp_form1(STAMP, full=True)
    f = Flow(s)

    # ── колонка 1: инициализация (setup) ───────────────────────────────────
    x = 55
    f.term(x, 30, 1, 'Начало')
    steps = [(2, 'Настройка выводов\nмикроконтроллера', False),
             (3, 'Загрузка настроек,\nпроверка CRC-16', False),
             (4, 'Инициализация I2C,\nвыбор индикатора', False),
             (5, 'Калибровка датчика\nнаклона MPU-6050', True),
             (6, 'Инициализация датчиков\nдавления и температуры', False),
             (7, 'Инициализация\nрадиомодуля nRF24L01+', False),
             (8, 'Запуск сторожевого\nтаймера и журнала', False)]
    y, prev = 52, 36
    for n, t, pre in steps:
        f.v(x, prev, y - H / 2)
        f.proc(x, y, n, t, pre)
        prev, y = y + H / 2, y + 22
    f.v(x, prev, y - R)
    f.conn(x, y, 'А')

    # ── колонка 2: опрос датчиков, кнопок, прием команд (loop) ─────────────
    x, bx = 128, 128 + 34
    f.conn(x, 28, 'А')
    f.v(x, 32, 50 - DH / 2)
    def opt(yd, n, cond, blocks, merge):
        """решение «Да» вниз через blocks, «Нет» — обход справа до точки слияния"""
        f.dec(x, yd, n, cond)
        f.label(x + 1.5, yd + DH / 2 + 2.5, 'Да')
        f.label(x + DW / 2 + 1, yd - 2.5, 'Нет')
        s.line([(x + DW / 2, yd), (bx, yd), (bx, merge), (x, merge)])
        s.arrow(bx, merge, x, merge, 2.5)
        prev = yd + DH / 2
        for yb, nb, tb, pre in blocks:
            f.v(x, prev, yb - H / 2)
            f.proc(x, yb, nb, tb, pre)
            prev = yb + H / 2
        f.v(x, prev, merge)
        s.dot(x, merge)
    opt(50, 9, 'Прошло\n100 мс?', [(74, 10, 'Измерение давления,\nпроверка порога 6,8 бар', False),
                                   (96, 11, 'Измерение угла наклона,\nпроверка порога 30°', True)], 108)
    f.v(x, 108, 124 - DH / 2)
    opt(124, 12, 'Прошла\n1 с?', [(148, 13, 'Измерение\nтемпературы', False)], 160)
    f.v(x, 160, 174 - H / 2)
    f.proc(x, 174, 14, 'Управление выводным\nвентилятором', False)
    f.v(x, 174 + H / 2, 196 - H / 2)
    f.proc(x, 196, 15, 'Опрос кнопок\nSB1–SB3', False)
    f.v(x, 196 + H / 2, 220 - DH / 2)
    opt(220, 16, 'Принята\nкоманда?', [(244, 17, 'Исполнение\nкоманды', True)], 256)
    f.v(x, 256, 270 - H / 2)
    f.proc(x, 270, 18, 'Контроль радиоканала,\nсмена частотного канала', False)
    f.v(x, 270 + H / 2, 284 - R)
    f.conn(x, 284, 'Б')

    # ── колонка 3: отправка состояния, режимы, регулирование ────────────────
    x3, x4 = 200, 282
    f.conn(x3, 28, 'Б')
    f.v(x3, 32, 50 - H / 2)
    f.proc(x3, 50, 19, 'Передача состояния\nпо радиоканалу', False)
    f.v(x3, 50 + H / 2, 76 - DH / 2)
    f.dec(x3, 76, 20, 'Идет\nпродувка?')
    f.label(x3 + DW / 2 + 1, 76 - 2.5, 'Да')
    f.label(x3 + 1.5, 76 + DH / 2 + 2.5, 'Нет')
    s.line([(x3 + DW / 2, 76), (x4 - DW / 2, 76)])
    f.v(x3, 76 + DH / 2, 130 - DH / 2)
    f.dec(x3, 130, 23, 'Выключено или\nавария?')
    f.label(x3 + DW / 2 + 1, 130 - 2.5, 'Да')
    f.label(x3 + 1.5, 130 + DH / 2 + 2.5, 'Нет')
    s.line([(x3 + DW / 2, 130), (x4 - W / 2, 130)])
    f.v(x3, 130 + DH / 2, 158 - H / 2)
    f.proc(x3, 158, 25, 'Регулирование\nдавления', True)
    f.v(x3, 158 + H / 2, 182 - H / 2)
    f.proc(x3, 182, 26, 'Контроль роста давления\nпри включенном ключе', False)
    f.v(x3, 182 + H / 2, 204 - R)
    f.conn(x3, 204, 'В')

    # ── колонка 4: продувка и отключение ────────────────────────────────────
    bx4 = x4 + 34
    f.dec(x4, 76, 21, 'P ≤ 0,05 бар или\nистекло время?')
    f.label(x4 + 1.5, 76 + DH / 2 + 2.5, 'Да')
    f.label(x4 + DW / 2 + 1, 76 - 2.5, 'Нет')
    f.v(x4, 76 + DH / 2, 102 - H / 2)
    f.proc(x4, 102, 22, 'Закрыть клапан,\nвыключить систему', False)
    s.line([(x4 + DW / 2, 76), (bx4, 76), (bx4, 154), (x4, 154)])
    s.arrow(bx4, 154, x4, 154, 2.5)
    s.line([(x4, 102 + H / 2), (x4, 114), (bx4, 114)])
    s.dot(bx4, 114)
    f.proc(x4, 130, 24, 'Отключить ключ\nи все реле', False)
    f.v(x4, 130 + H / 2, 172 - R)
    s.dot(x4, 154)
    f.conn(x4, 172, 'В')

    # ── колонка 5: индикация, переход к началу цикла ────────────────────────
    x5 = 372
    f.conn(x5, 28, 'В')
    f.v(x5, 32, 52 - H / 2)
    f.proc(x5, 52, 27, 'Обновление светодиодов\nи индикатора', False)
    f.v(x5, 52 + H / 2, 76 - R)
    f.conn(x5, 76, 'А')

    s.save(out + '.png', out + '.pdf')


if __name__ == '__main__':
    draw(sys.argv[1])
    print('ok')

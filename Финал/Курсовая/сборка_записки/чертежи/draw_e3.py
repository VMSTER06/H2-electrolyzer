"""Э3 — схема электрическая принципиальная, 2 листа А3 (блок управления, пульт), по образцу кафедры:
модули — прямоугольники с полями выводов, у платы — логические номера контактов (101…119, 201…219),
провода пронумерованы отдельно и сходятся в линии групповой связи."""
import sys, math
sys.path.insert(0, __file__.rsplit('\\', 1)[0] if '\\' in __file__ else '.')
from eskd import Sheet, LW_MAIN, LW_LINE

ROWS = list('ABCDEFGHJKLMNPQRS')
STAMP = dict(designation='ГУИР.6-05-0611-05.114 Э3', developer='Мурачев', checker='Селезнев', liter='У',
             name='Микропроцессорное устройство\nдистанционного управления\nобъектом\nСхема электрическая\nпринципиальная',
             org='БГУИР, гр. 434701')
# ESP32 DevKit v1 (38 выводов): логический номер контакта -> обозначение вывода
LEFT = {101: '3V3', 102: 'EN', 103: 'IO36', 104: 'IO39', 105: 'IO34', 106: 'IO35', 107: 'IO32', 108: 'IO33',
        109: 'IO25', 110: 'IO26', 111: 'IO27', 112: 'IO14', 113: 'IO12', 114: 'GND', 115: 'IO13', 119: '5V'}
RIGHT = {201: 'GND ', 202: 'IO23', 203: 'IO22', 206: 'IO21', 208: 'IO19', 209: 'IO18', 210: 'IO5', 211: 'IO17',
         212: 'IO16', 213: 'IO4', 215: 'IO2', 216: 'IO15'}


def board(s, x, y, left, right, desig, title='ESP32\nDevKit v1', pitch=7.0):
    """board UGO with logical contact numbers outside the pin fields; returns pin -> (x, y)"""
    pins, H = s.module(x, y, 62, [LEFT[c] for c in left], [RIGHT[c] for c in right], title, desig,
                       pitch=pitch, top=8, fw=14, pin_h=2.8, pinnum=False)
    s.text(x + 31, y + H / 2 + 2, '', 3)
    out = {}
    for c in left:
        px, py = pins[LEFT[c]]
        s.text(px - 2.0, py - 1.9, str(c), 2.4, 'rm')
        out[c] = (px, py)
    for c in right:
        px, py = pins[RIGHT[c]]
        s.text(px + 2.0, py - 1.9, str(c), 2.4, 'lm')
        out[c] = (px, py)
    return out


def to_bus(s, p, bus_x, num, label_at='bus'):
    """horizontal wire from point p to the group line at bus_x with the wire number"""
    x, y = p
    s.line([(x, y), (bus_x, y)])
    lx = bus_x + (-4 if bus_x > x else 4)
    s.text(lx, y - 1.9, str(num), 2.6)


def bus(s, x, y1, y2):
    s.line([(x, y1), (x, y2)], LW_MAIN)


def bus_pair(s, lb, rb, y1, y2):
    """одна линия групповой связи вокруг платы: левая и правая ветви замкнуты сверху (как в образце)"""
    s.line([(lb, y2), (lb, y1), (rb, y1), (rb, y2)], LW_MAIN)


def sheet_brain():
    s = Sheet(420, 297)
    s.frame(STAMP['designation'])
    s.zone_grid(24, ROWS)
    st = dict(STAMP, sheet='1', sheets='2'); s.stamp_form1(st, full=True)
    s.text(150, 36, 'Устройство контроля', 3.5)

    # ---------------- board DD1 ----------------
    bx, by = 178, 52
    L = [101, 103, 107, 108, 109, 110, 111, 112, 113, 114, 115, 119]
    R = [201, 202, 203, 206, 208, 209, 210, 211, 212, 213, 215, 216]
    P = board(s, bx, by, L, R, 'DD1')
    LB, RB = 160, 258                       # group lines
    bus_pair(s, LB, RB, 44, 226)
    # board power pins
    s.line([P[101], (P[101][0] - 8, P[101][1])]); s.supply(P[101][0] - 8, P[101][1], '+3,3В')
    s.line([P[114], (P[114][0] - 12, P[114][1])]); s.ground(P[114][0] - 12, P[114][1])
    s.line([P[119], (P[119][0] - 8, P[119][1])]); s.supply(P[119][0] - 8, P[119][1], '+5В', up=False)
    s.line([P[201], (P[201][0] + 12, P[201][1])]); s.ground(P[201][0] + 12, P[201][1])
    wn = {103: 1, 107: 2, 108: 3, 109: 4, 110: 5, 111: 6, 112: 7, 113: 8, 115: 9,
          202: 10, 203: 11, 206: 12, 208: 13, 209: 14, 210: 15, 211: 16, 212: 17, 213: 18, 215: 19, 216: 20}
    for c, n in wn.items():
        to_bus(s, P[c], LB if c < 200 else RB, n)

    # ---------------- left side ----------------
    # pressure sensor BP1 + divider R1/R2
    bp, h = s.module(38, 40, 30, [], ['+U', 'OUT', 'GND'], 'P', 'BP1', pitch=5, top=6, fw=11)
    s.line([bp['+U'], (bp['+U'][0] + 6, bp['+U'][1])]); s.supply(bp['+U'][0] + 6, bp['+U'][1], '+5В')
    s.line([bp['GND'], (bp['GND'][0] + 4, bp['GND'][1])]); s.ground(bp['GND'][0] + 4, bp['GND'][1])
    e = s.resistor(bp['OUT'][0] + 10, bp['OUT'][1], label='R1', value='10 кОм')
    s.line([bp['OUT'], (bp['OUT'][0] + 10, bp['OUT'][1])])
    nx, ny = e[0] + 6, e[1]
    s.line([e, (nx, ny)]); s.dot(nx, ny)
    r2 = s.resistor(nx, ny, vertical=True, label='R2', value='20 кОм'); s.ground(*r2)
    to_bus(s, (nx, ny), LB, 1)

    # MOSFET key A5 + R7 (gate) + R8 (gate–source); электролизер — к клеммам модуля, питание — шина 12 В
    a5, h = s.module(70, 76, 40, ['OUT+', 'OUT−', 'V+', 'V−'], ['IN', 'GND'], 'MOSFET', 'A5', pitch=5.5, top=6, fw=11)
    s.line([a5['GND'], (a5['GND'][0] + 4, a5['GND'][1])]); s.ground(a5['GND'][0] + 4, a5['GND'][1])
    gy = a5['IN'][1]
    r8s = (a5['IN'][0] + 20, gy)
    s.line([a5['IN'], (a5['IN'][0] + 12, gy)]); s.dot(a5['IN'][0] + 12, gy)
    r9 = s.resistor(a5['IN'][0] + 12, gy, vertical=True, label='R8', value='10 кОм'); s.ground(*r9)
    s.line([(a5['IN'][0] + 12, gy), (a5['IN'][0] + 20, gy)])
    e = s.resistor(a5['IN'][0] + 20, gy, label='R7', value='100 Ом')
    to_bus(s, e, LB, 2)
    for pn, t in (('OUT+', 'Электролизер+'), ('OUT−', 'Электролизер−')):
        s.line([a5[pn], (62, a5[pn][1])]); s.text(61, a5[pn][1], t, 2.6, 'rm')
    vy = a5['V+'][1]
    s.line([a5['V+'], (56, vy)]); s.arrow(a5['V+'][0], vy, 56, vy, 1.8, half=math.radians(15)); s.text(55, vy, '+12В', 2.6, 'rm')
    s.line([a5['V−'], (64, a5['V−'][1])]); s.ground(64, a5['V−'][1])

    # relay module K1 (3 channels) + XP2 + exhaust fan M1
    k1, h = s.module(84, 116, 44, ['COM1', 'NO1', 'COM2', 'NO2', 'COM3', 'NO3'], ['VCC', 'GND', 'IN1', 'IN2', 'IN3'],
                     'K', 'K1', pitch=5.0, top=6, fw=12)
    s.line([k1['VCC'], (k1['VCC'][0] + 5, k1['VCC'][1])]); s.supply(k1['VCC'][0] + 5, k1['VCC'][1], '+5В')
    s.line([k1['GND'], (k1['GND'][0] + 9, k1['GND'][1])]); s.ground(k1['GND'][0] + 9, k1['GND'][1])
    to_bus(s, k1['IN1'], LB, 5); to_bus(s, k1['IN2'], LB, 6); to_bus(s, k1['IN3'], LB, 4)
    cx = k1['COM1'][0] - 6
    for c in ('COM1', 'COM2', 'COM3'):
        s.line([k1[c], (cx, k1[c][1])])
    s.line([(cx, k1['COM1'][1]), (cx, k1['COM3'][1])]); s.dot(cx, k1['COM2'][1])
    s.line([(cx, k1['COM1'][1]), (cx, k1['COM1'][1] - 4)]); s.supply(cx, k1['COM1'][1] - 4, '+12В')
    xp3 = s.conn_table(35, 118, 'XP2', [('1', 'Клапан'), ('2', 'Поджиг'), ('3', 'Общий')], w1=8, w2=18)
    s.line([k1['NO1'], (cx - 3, k1['NO1'][1]), (cx - 3, xp3['1'][1]), xp3['1']])
    s.line([k1['NO2'], (cx - 5, k1['NO2'][1]), (cx - 5, xp3['2'][1]), xp3['2']])
    s.line([xp3['3'], (xp3['3'][0] + 4, xp3['3'][1])]); s.ground(xp3['3'][0] + 4, xp3['3'][1])
    mx = 60
    s.line([k1['NO3'], (mx, k1['NO3'][1])])
    m = s.motor(mx, k1['NO3'][1], 'M1'); s.ground(*m)

    # buttons SB1..SB3
    for i, (num, lab) in enumerate([(9, 'SB1'), (7, 'SB2'), (3, 'SB3')]):
        x = 92 + i * 22; y = 168 + i * 5
        s.line([(x, y), (x, y)]); s.dot(x, y) if False else None
        to_bus(s, (x, y), LB, num)
        b = s.button(x, y, lab); s.ground(*b)
    # LED HL2 (LOCK) via R5
    e = s.resistor(110, 206, label='R5', value='220 Ом')
    to_bus(s, e, LB, 8)
    s.line([(110, 206), (94, 206)])
    d = s.diode(94, 206, led=True, label='HL2'); s.ground(*d)

    # ---------------- right side ----------------
    a1, h = s.module(280, 40, 50, ['CE', 'CSN', 'SCK', 'MOSI', 'MISO', 'IRQ'], ['VCC', 'GND'], 'RF', 'A1',
                     pitch=5.5, top=6, fw=12)
    for pin, n in (('CE', 18), ('CSN', 15), ('SCK', 14), ('MOSI', 10), ('MISO', 13)):
        to_bus(s, a1[pin], RB, n)
    vx = a1['VCC'][0] + 13
    s.line([a1['VCC'], (vx, a1['VCC'][1])]); s.dot(vx, a1['VCC'][1]); s.supply(vx, a1['VCC'][1], '+3,3В')
    s.line([a1['GND'], (a1['GND'][0] + 3, a1['GND'][1])]); s.ground(a1['GND'][0] + 3, a1['GND'][1])
    c = s.capacitor(vx, a1['VCC'][1], 'C1', '470 мкФ', polar=True); s.ground(*c)

    a3, h = s.module(280, 88, 50, ['SCL', 'SDA'], ['VCC', 'GND'], 'G', 'A3', pitch=5.5, top=6, fw=12)
    to_bus(s, a3['SCL'], RB, 11); to_bus(s, a3['SDA'], RB, 12)
    s.line([a3['VCC'], (a3['VCC'][0] + 5, a3['VCC'][1])]); s.supply(a3['VCC'][0] + 5, a3['VCC'][1], '+3,3В')
    s.line([a3['GND'], (a3['GND'][0] + 3, a3['GND'][1])]); s.ground(a3['GND'][0] + 3, a3['GND'][1])

    a4, h = s.module(282, 116, 42, ['LV', 'LV1', 'LV2', 'GND'], ['HV', 'HV1', 'HV2', 'GND '], 'U', 'A4',
                     pitch=5.5, top=6, fw=11)
    to_bus(s, a4['LV1'], RB, 12); to_bus(s, a4['LV2'], RB, 11)
    s.line([a4['LV'], (a4['LV'][0] - 9, a4['LV'][1])]); s.supply(a4['LV'][0] - 9, a4['LV'][1], '+3,3В')
    s.line([a4['GND'], (a4['GND'][0] - 4, a4['GND'][1])]); s.ground(a4['GND'][0] - 4, a4['GND'][1])
    hg, h = s.module(356, 116, 42, ['VCC', 'SDA', 'SCL', 'GND'], [], 'HG', 'HG1', pitch=5.5, top=6, fw=11)
    for a, b, n in (('HV1', 'SDA', 21), ('HV2', 'SCL', 22)):
        s.line([a4[a], hg[b]])
    vx = a4['HV'][0] + 9
    s.line([a4['HV'], hg['VCC']]); s.dot(vx, a4['HV'][1]); s.supply(vx, a4['HV'][1], '+5В')
    s.line([a4['GND '], hg['GND']]); gx = a4['HV'][0] + 14; s.dot(gx, hg['GND'][1]); s.ground(gx, hg['GND'][1])

    bk, h = s.module(316, 166, 34, ['DQ'], ['VDD', 'GND'], 'BK', 'BK1', pitch=5.5, top=6, fw=11)
    qx = bk['DQ'][0] - 16
    s.line([bk['DQ'], (qx, bk['DQ'][1])]); s.dot(qx, bk['DQ'][1])
    to_bus(s, (qx, bk['DQ'][1]), RB, 20)
    r3top = (qx, bk['DQ'][1] - 18)
    s.resistor(qx, r3top[1] + 2, vertical=True, label='R3', value='4,7 кОм')
    s.supply(qx, r3top[1] + 2, '+3,3В')
    s.line([bk['VDD'], (bk['VDD'][0] + 8, bk['VDD'][1])]); s.supply(bk['VDD'][0] + 8, bk['VDD'][1], '+3,3В')
    s.line([bk['GND'], (bk['GND'][0] + 3, bk['GND'][1])]); s.ground(bk['GND'][0] + 3, bk['GND'][1])

    # LEDs HL1 (POWER, IO17), HL3 (MODE, IO2)
    for (y, n, rl, hl, x) in ((184, 16, 'R4', 'HL1', 290), (204, 19, 'R6', 'HL3', 330)):
        s.line([(RB, y), (x - 16, y)]); s.text(RB + 4, y - 1.9, str(n), 2.6)
        e = s.resistor(x - 16, y, label=rl, value='220 Ом')
        s.line([e, (e[0] + 5, e[1])])
        d = s.diode(e[0] + 5, e[1], led=True, label=hl); s.ground(*d)
    # buzzer HA1 directly on IO16
    y = 219
    s.line([(RB, y), (300, y)]); s.text(RB + 4, y - 1.9, '17', 2.6)
    s.ground(*s.buzzer(300, y, "HA1"))

    # ---------------- power (bottom-left) ----------------
    xp1 = s.conn_table(38, 244, 'XP1', [('1', '+12В'), ('2', 'GND')], w1=8, w2=16)
    s.line([xp1['2'], (xp1['2'][0] + 4, xp1['2'][1])]); s.ground(xp1['2'][0] + 4, xp1['2'][1])
    a7, h = s.module(96, 240, 46, ['IN+', 'IN−'], ['OUT+', 'OUT−'], 'U', 'A7', pitch=6, top=8, fw=11)
    sa = s.switch_h(xp1['1'][0] + 2, xp1['1'][1], 'SA1'); s.line([xp1['1'], (xp1['1'][0] + 2, xp1['1'][1])])
    s.line([sa, (80, xp1['1'][1]), (80, a7['IN+'][1]), a7['IN+']]); s.dot(80, a7['IN+'][1])
    s.line([(80, xp1['1'][1]), (80, xp1['1'][1] - 6)]); s.supply(80, xp1['1'][1] - 6, '+12В')
    s.line([a7['IN−'], (90, a7['IN−'][1])]); s.ground(90, a7['IN−'][1])
    s.line([a7['OUT+'], (a7['OUT+'][0] + 10, a7['OUT+'][1])]); s.supply(a7['OUT+'][0] + 10, a7['OUT+'][1], '+5В')
    s.line([a7['OUT−'], (a7['OUT−'][0] + 5, a7['OUT−'][1])]); s.ground(a7['OUT−'][0] + 5, a7['OUT−'][1])
    m2 = s.motor(186, 250, 'M2'); s.supply(186, 250, '+12В'); s.ground(*m2)
    s.text(205, 282, '', 2.5)
    return s


def sheet_remote():
    s = Sheet(420, 297)
    s.frame(STAMP['designation'])
    s.zone_grid(24, ROWS)
    s.stamp_form2a(dict(STAMP, sheet='2'))
    s.text(150, 36, 'Устройство удаленного управления', 3.5)
    global RIGHT
    bx, by = 178, 52
    L = [101, 105, 107, 108, 109, 110, 111, 112, 113, 114, 115, 119]
    R = [201, 202, 203, 206, 208, 209, 210, 211, 212, 213, 215]
    P = board(s, bx, by, L, R, 'DD2')
    LB, RB = 160, 258
    bus_pair(s, LB, RB, 44, 226)
    s.line([P[101], (P[101][0] - 8, P[101][1])]); s.supply(P[101][0] - 8, P[101][1], '+3,3В')
    s.line([P[114], (P[114][0] - 12, P[114][1])]); s.ground(P[114][0] - 12, P[114][1])
    s.line([P[119], (P[119][0] - 8, P[119][1])]); s.supply(P[119][0] - 8, P[119][1], '+5В', up=False)
    s.line([P[201], (P[201][0] + 12, P[201][1])]); s.ground(P[201][0] + 12, P[201][1])
    wn = {105: 1, 107: 2, 108: 3, 109: 4, 110: 5, 111: 6, 112: 7, 113: 8, 115: 9,
          202: 10, 203: 11, 206: 12, 208: 13, 209: 14, 210: 15, 211: 16, 212: 17, 213: 18, 215: 19}
    for c, n in wn.items():
        to_bus(s, P[c], LB if c < 200 else RB, n)
    # left: buttons SB6..SB11 (IO34 melody with external pull-up R9, IO25, IO26, IO27, IO32, IO33)
    btns = [(1, 'SB6'), (4, 'SB7'), (5, 'SB8'), (6, 'SB9'), (2, 'SB10'), (3, 'SB11')]
    for i, (num, lab) in enumerate(btns):
        x = 40 + i * 19; y = 66 + i * 7
        to_bus(s, (x, y), LB, num)
        b = s.button(x, y, lab); s.ground(*b)
        if num == 1:
            s.dot(x, y); s.resistor(x, y - 16, vertical=True, label='R9', value='10 кОм')
            s.supply(x, y - 16, '+3,3В')
    # LEDs HL4 (TX, IO14), HL5 (POWER, IO13), HL6 (STATUS, IO12)
    for i, (n, rl, hl) in enumerate(((7, 'R10', 'HL4'), (9, 'R11', 'HL5'), (8, 'R12', 'HL6'))):
        y = 146 + i * 24; x = 100
        e = s.resistor(x + 12, y, label=rl, value='220 Ом')
        s.line([e, (LB, y)]); s.text(LB - 4, y - 1.9, str(n), 2.6)
        s.line([(x, y), (x + 12, y)])
        d = s.diode(x, y, led=True, label=hl); s.ground(*d)
    # right: radio A2 + C2, OLED HG2, buttons SB4 (IO16), SB5 (IO17), buzzer
    a2, h = s.module(280, 40, 50, ['CE', 'CSN', 'SCK', 'MOSI', 'MISO', 'IRQ'], ['VCC', 'GND'], 'RF', 'A2',
                     pitch=5.5, top=6, fw=12)
    for pin, n in (('CE', 18), ('CSN', 15), ('SCK', 14), ('MOSI', 10), ('MISO', 13)):
        to_bus(s, a2[pin], RB, n)
    vx = a2['VCC'][0] + 13
    s.line([a2['VCC'], (vx, a2['VCC'][1])]); s.dot(vx, a2['VCC'][1]); s.supply(vx, a2['VCC'][1], '+3,3В')
    s.line([a2['GND'], (a2['GND'][0] + 3, a2['GND'][1])]); s.ground(a2['GND'][0] + 3, a2['GND'][1])
    c = s.capacitor(vx, a2['VCC'][1], 'C2', '470 мкФ', polar=True); s.ground(*c)
    hg, h = s.module(280, 92, 46, ['SCL', 'SDA'], ['VCC', 'GND'], 'HG', 'HG2', pitch=5.5, top=6, fw=12)
    to_bus(s, hg['SCL'], RB, 11); to_bus(s, hg['SDA'], RB, 12)
    s.line([hg['VCC'], (hg['VCC'][0] + 5, hg['VCC'][1])]); s.supply(hg['VCC'][0] + 5, hg['VCC'][1], '+3,3В')
    s.line([hg['GND'], (hg['GND'][0] + 3, hg['GND'][1])]); s.ground(hg['GND'][0] + 3, hg['GND'][1])
    for i, (num, lab) in enumerate(((17, 'SB4'), (16, 'SB5'))):
        x = 312 - i * 22; y = 130 + i * 6
        to_bus(s, (x, y), RB, num)
        b = s.button(x, y, lab); s.ground(*b)
    y = 172
    s.line([(RB, y), (300, y)]); s.text(RB + 4, y - 1.9, '19', 2.6)
    s.ground(*s.buzzer(300, y, 'HA2'))
    # power: battery GB1, charger A6 (TP4056), boost A8
    s.text(60, 254, 'GB1', 2.6, 'rm')
    gbx, gby = 66, 246
    s.line([(gbx, gby), (gbx, gby + 4)]); s.line([(gbx - 4, gby + 4), (gbx + 4, gby + 4)], 0.45)
    s.line([(gbx - 2, gby + 6), (gbx + 2, gby + 6)], 0.9); s.line([(gbx, gby + 6), (gbx, gby + 10)])
    a6, h = s.module(80, 236, 40, ['BAT+', 'BAT−'], ['OUT+', 'OUT−'], 'TP4056', 'A6', pitch=6, top=8, fw=11)
    s.line([(gbx, gby), (gbx, a6['BAT+'][1]), a6['BAT+']]); s.line([(gbx, gby + 10), (gbx, a6['BAT−'][1] + 6), (a6['BAT−'][0] - 4, a6['BAT−'][1] + 6), (a6['BAT−'][0] - 4, a6['BAT−'][1]), a6['BAT−']])
    a8, h = s.module(146, 236, 40, ['IN+', 'IN−'], ['OUT+', 'OUT−'], 'U', 'A8', pitch=6, top=8, fw=11)
    sb = s.switch_h(a6['OUT+'][0] + 6, a6['OUT+'][1], 'SA2'); s.line([a6['OUT+'], (a6['OUT+'][0] + 6, a6['OUT+'][1])])
    s.line([sb, a8['IN+']]); s.line([a6['OUT−'], a8['IN−']])
    s.line([a8['OUT+'], (a8['OUT+'][0] + 8, a8['OUT+'][1])]); s.supply(a8['OUT+'][0] + 8, a8['OUT+'][1], '+5В')
    s.line([a8['OUT−'], (a8['OUT−'][0] + 4, a8['OUT−'][1])]); s.ground(a8['OUT−'][0] + 4, a8['OUT−'][1])
    return s


if __name__ == '__main__':
    out = sys.argv[1]
    a = sheet_brain(); a.save(out + '_1.png')
    b = sheet_remote(); b.save(out + '_2.png')
    a.img.save(out + '.pdf', 'PDF', resolution=300, save_all=True, append_images=[b.img])
    print('ok')

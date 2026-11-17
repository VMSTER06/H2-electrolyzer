"""Генератор поэтапной схемы подключения «Устройство контроля ГСМ».

Каждый этап — один модуль: какой вывод модуля к какому выводу Arduino Nano
вести (без координат на макетной плате). На этапе — схема «модуль ↔ Nano»
линиями цвета провода, таблица соединений, шаги и проверка. Оформление —
тёмная тема H2_Wiring_Full.html. Всё строится из одних данных ниже.

Запуск:  python3 make_gsm_wiring.py   →   ГСМ_схема_подключения.html
"""
from dataclasses import dataclass
from pathlib import Path

OUT = Path(__file__).with_name("ГСМ_схема_подключения.html")

# ─────────────────────────── Данные ───────────────────────────

NANO_TOP = ["D13", "3V3", "REF", "A0", "A1", "A2", "A3", "A4", "A5", "A6",
            "A7", "5V", "RST", "GND", "VIN"]
NANO_BOT = ["D12", "D11", "D10", "D9", "D8", "D7", "D6", "D5", "D4", "D3",
            "D2", "GND", "RST", "RX0", "TX1"]

# Цвета проводов: ключ → (цвет на рисунке, название).
COLORS = {
    "red":       ("#d04444", "красный"),
    "darkred":   ("#8b1e1e", "тёмно-красный"),
    "black":     ("#1a1a1a", "чёрный"),
    "yellow":    ("#d4c044", "жёлтый"),
    "orange":    ("#ff9933", "оранжевый"),
    "blue":      ("#3a6fd8", "синий"),
    "lightblue": ("#7fb8ff", "голубой"),
    "cyan":      ("#22c3d6", "бирюзовый"),
    "green":     ("#44aa44", "зелёный"),
    "pink":      ("#ee99cc", "розовый"),
    "purple":    ("#aa44aa", "фиолетовый"),
    "brown":     ("#885522", "коричневый"),
}
POWER = {"5V": "#ff6666", "GND": "#a0a0a0"}   # цвет подписи общих линий
SIGNAL_PIN = "#e8c84a"


@dataclass(frozen=True)
class Conn:
    """Одно соединение: вывод модуля → вывод Nano."""
    pin: str          # вывод модуля
    nano: str         # вывод Arduino Nano ("5V", "GND", "D8", …)
    color: str        # цвет провода (ключ COLORS)
    via: str = ""     # деталь в разрыве (резистор)
    note: str = ""


@dataclass(frozen=True)
class Stage:
    key: str
    icon: str
    title: str        # модуль с обозначением
    label: str        # короткое имя для распиновки и шагов («LCD»); "" — имя уже в выводе
    conns: tuple[Conn, ...]
    unused: tuple[tuple[str, str], ...] = ()
    tips: tuple[str, ...] = ()   # дополнительные шаги
    check: str = ""
    note: str = ""


STAGES = [
    Stage("usb", "🔋", "Питание: разъём USB Type-C (XP1)", "USB-C", (
        Conn("VBUS (+5 В)", "5V", "red", note="линия +5 В"),
        Conn("GND", "GND", "black", note="общий провод"),
    ), tips=(
        "Все провода «+5 В» и «GND» следующих модулей соединяются с этими же линиями: удобно сделать "
        "две общие шины (лужёный провод) — +5 В и GND — и паять модули к ним.",
    ), check="между 5V и GND Nano НЕТ короткого замыкания (не 0 Ом).",
       note="Используются только VBUS и GND, остальные выводы переходника свободны."),

    Stage("leds", "💡", "Светодиоды VD1–VD4 (панель)", "", (
        Conn("VD1 «OK» зелёный, анод +", "D8", "green", via="R1 220 Ом"),
        Conn("VD2 «LOW» жёлтый, анод +", "D10", "yellow", via="R2 220 Ом"),
        Conn("VD3 «GAS» красный, анод +", "D9", "red", via="R3 220 Ом"),
        Conn("VD4 «TEMP» красный, анод +", "D12", "darkred", via="R4 220 Ом"),
        Conn("катоды − всех VD1–VD4", "GND", "black"),
    ), tips=(
        "Анод — длинная ножка (+), катод — короткая (−).",
        "Резистор 220 Ом ставится между выводом Nano и анодом каждого светодиода.",
        "Провода от Nano к панели — МГТФ по 20 см, скрутить в жгут.",
    ), check="мультиметр «диод»: красный щуп на вывод Nano, чёрный на GND — светодиод слабо светится."),

    Stage("buttons", "🔘", "Кнопки SA1–SA3 (панель)", "", (
        Conn("SA1, вывод 1", "D7", "blue"),
        Conn("SA2, вывод 1", "D5", "lightblue"),
        Conn("SA3, вывод 1", "D6", "cyan"),
        Conn("вывод 2 всех SA1–SA3", "GND", "black"),
    ), tips=(
        "Кнопка замыкает вывод Nano на GND; подтяжка внутренняя (INPUT_PULLUP), резисторы не нужны.",
        "У тактовой кнопки 4 ножки: берите две по диагонали — так работает при любом повороте.",
    ), check="D7–GND звонится только при нажатой SA1 (D5 — SA2, D6 — SA3)."),

    Stage("lcd", "🖥", "Дисплей LCD-2004B-IIC (DA1)", "LCD", (
        Conn("GND", "GND", "black"),
        Conn("VCC", "5V", "red"),
        Conn("SDA", "A4", "pink", note="шина I²C, данные"),
        Conn("SCL", "A5", "purple", note="шина I²C, такт"),
    ), tips=("Провода 20 см паять прямо на штырьки платы I²C, на каждую пайку — термоусадка.",),
       check="SDA звонится с A4 Nano, SCL — с A5, VCC — с 5V, GND — с GND.",
       note="Разъём платы I²C — сзади дисплея. Порядок выводов проверьте по надписям на ней."),

    Stage("mq2", "📊", "Датчик газа MQ-2 (DD1)", "MQ-2", (
        Conn("VCC", "5V", "red"),
        Conn("GND", "GND", "black"),
        Conn("AO", "A0", "cyan", note="аналоговый выход"),
    ), unused=(("DO", "оставить свободным"),),
       check="AO звонится с A0 Nano.",
       note="DO не подключается: нужен только аналоговый выход AO. Провода 12 см."),

    Stage("dht", "🌡", "Датчик температуры DHT11 (DD2)", "DHT11", (
        Conn("DATA (S)", "D2", "brown"),
        Conn("VCC (+)", "5V", "red"),
        Conn("GND (−)", "GND", "black"),
    ), check="DATA звонится с D2 Nano и НЕ звонится с GND.",
       note="У разных модулей порядок выводов разный — смотрите надписи S / + / − на плате модуля. "
            "Провода 12 см."),

    Stage("sr04", "📏", "Датчик расстояния HC-SR04 (DD3)", "HC-SR04", (
        Conn("VCC", "5V", "red"),
        Conn("Trig", "D3", "orange", note="запуск измерения"),
        Conn("Echo", "D4", "yellow", note="эхо-импульс"),
        Conn("GND", "GND", "black"),
    ), check="Trig звонится с D3, Echo — с D4, Trig и Echo не замкнуты между собой.",
       note="Провода 10 см."),

    Stage("buzz", "🔊", "Зуммер SFM-27 (BQ1)", "зуммер", (
        Conn("+ (красный)", "D11", "red"),
        Conn("− (чёрный)", "GND", "black"),
    ), check="красный провод звонится с D11 Nano.",
       note="Зуммер полярный: красный вывод — плюс. Свои провода зуммера можно нарастить, спайку изолировать."),
]

RULES = [
    "Сначала впаяйте Arduino Nano в плату: две крайние точки по диагонали, выровнять, затем остальные выводы.",
    "Каждый этап — один модуль. Слева вывод модуля, справа — вывод Arduino Nano, к которому его вести.",
    "Цвет линии на схеме = цвет провода. Номер провода в кружке совпадает с таблицей этапа.",
    "«5V» и «GND» — общие линии: к ним сходятся провода питания всех модулей.",
    "Внешний провод: зачистить и залудить 3–4 мм, пропаять, на пайку — термоусадка.",
    "Паяльник 25–40 Вт, 1–2 секунды на точку, припой должен лечь блестящим конусом.",
    "После каждого этапа — проверка из последнего пункта. Ошибка, пропущенная рано, дорого стоит потом.",
]

FINAL_CHECK = [
    "Осмотр с лупой: нет капель припоя между соседними выводами Nano.",
    "<b>5V–GND:</b> нет короткого замыкания (не 0 Ом).",
    "Прозвонить каждый провод по общей таблице ниже: вывод модуля ↔ вывод Nano.",
    "Первое включение — от USB-C (XP1), без прошивки. Между 5V и GND должно быть <b>4,7–5,2 В</b>. "
    "Что-то греется или пахнет — сразу отключить.",
    "MQ-2 тёплый — это норма (внутри нагреватель), показания стабилизируются через 1–2 мин.",
    "Прошить gsm_control_lcd2004.ino. Пустой экран при горящей подсветке — покрутить подстроечник "
    "контраста на плате I²C; ничего — сменить адрес LCD_ADDR на 0x3F.",
]

# Сквозная нумерация проводов по всем этапам.
NUMBERED = [(st, c) for st in STAGES for c in st.conns]
WIRE_NO = {(st.key, c.pin): n for n, (st, c) in enumerate(NUMBERED, 1)}

# ─────────────────────────── Утилиты ───────────────────────────


def text_on(hex_color: str) -> str:
    """Цвет текста (чёрный/белый), читаемый на заданной заливке."""
    r, g, b = (int(hex_color[i:i + 2], 16) for i in (1, 3, 5))
    return "#111" if 0.299 * r + 0.587 * g + 0.114 * b > 150 else "#fff"


def swatch(color: str) -> str:
    return f'<span class="color-swatch" style="background:{COLORS[color][0]}"></span>{COLORS[color][1]}'


def nano_color(pin: str) -> str:
    return POWER.get(pin, SIGNAL_PIN)


def pin_name(st: Stage, c: Conn) -> str:
    """Вывод вместе с модулем: «SDA LCD», «SA1» (имя уже в выводе)."""
    short = c.pin.split(",")[0].split(" (")[0]
    return f"{short} {st.label}".strip() if st.label else short


def signal_dst(pin: str) -> str:
    """Что подключено к сигнальному выводу Nano (у каждого сигнального — ровно одно)."""
    found = [pin_name(st, c) for st, c in NUMBERED if c.nano == pin]
    assert len(found) == 1, f"к {pin} подключено {len(found)} проводов"
    return found[0]


def plural(n: int) -> str:
    if n % 10 == 1 and n % 100 != 11:
        return ""
    return "а" if 2 <= n % 10 <= 4 and not 12 <= n % 100 <= 14 else "ов"


# ─────────────────────────── SVG ───────────────────────────

ROW_H, HDR, GAP = 30, 28, 16
X_MOD0, X_MOD1, X_NANO0, X_NANO1, WIDTH = 20, 300, 600, 720, 740


def diagram(stages: list[Stage]) -> str:
    """Модули слева, Arduino Nano справа, линии цвета провода с номерами."""
    out, nano_rows = [], []
    y = 20
    for st in stages:
        rows = [(c, True) for c in st.conns] + [(Conn(p, "", "black", note=t), False) for p, t in st.unused]
        h = HDR + ROW_H * len(rows) + 8
        out.append(f'<rect x="{X_MOD0}" y="{y}" width="{X_MOD1 - X_MOD0}" height="{h}" rx="7" '
                   f'fill="#161b22" stroke="#30363d"/>')
        out.append(f'<text x="{X_MOD0 + 12}" y="{y + 18}" fill="#58a6ff" font-size="12" font-weight="bold" '
                   f'font-family="Arial">{st.icon} {st.title}</text>')
        yy = y + HDR + ROW_H / 2
        for c, used in rows:
            out.append(f'<text x="{X_MOD1 - 10}" y="{yy + 4}" text-anchor="end" fill="#e0e0e0" font-size="12" '
                       f'font-family="monospace">{c.pin}</text>')
            out.append(f'<circle cx="{X_MOD1}" cy="{yy}" r="4" fill="#d4c044"/>')
            if not used:
                out.append(f'<text x="{X_MOD1 + 14}" y="{yy + 4}" fill="#6e7681" font-size="11" '
                           f'font-family="Arial">не подключать</text>')
                yy += ROW_H
                continue
            color = COLORS[c.color][0]
            stroke = "#6e7681" if c.color == "black" else color   # чёрный не виден на тёмном фоне
            out.append(f'<line x1="{X_MOD1}" y1="{yy}" x2="{X_NANO0}" y2="{yy}" stroke="{stroke}" stroke-width="3"/>')
            cx = X_MOD1 + 34
            out.append(f'<circle cx="{cx}" cy="{yy}" r="10" fill="{color}" stroke="#e0e0e0" stroke-width="1.5"/>'
                       f'<text x="{cx}" y="{yy + 4}" text-anchor="middle" fill="{text_on(color)}" font-size="10" '
                       f'font-weight="bold" font-family="Arial">{WIRE_NO[(st.key, c.pin)]}</text>')
            mid = (X_MOD1 + X_NANO0) / 2 + 20
            if c.via:
                out.append(f'<rect x="{mid - 26}" y="{yy - 8}" width="52" height="16" rx="3" fill="#c8b48a" '
                           f'stroke="#0d1117"/><text x="{mid}" y="{yy - 12}" text-anchor="middle" fill="#c9d1d9" '
                           f'font-size="10" font-family="monospace">{c.via}</text>')
            elif c.note:
                out.append(f'<text x="{mid}" y="{yy - 6}" text-anchor="middle" fill="#8b949e" font-size="10" '
                           f'font-family="Arial">{c.note}</text>')
            nano_rows.append((yy, c.nano))
            yy += ROW_H
        y += h + GAP
    height = y + 4

    cx = (X_NANO0 + X_NANO1) / 2
    out.append(f'<rect x="{X_NANO0}" y="8" width="{X_NANO1 - X_NANO0}" height="{height - 16}" rx="8" '
               f'fill="#0c1a10" stroke="#2ea043" stroke-width="2"/>')
    out.append(f'<text x="{cx + 30}" y="{height / 2}" text-anchor="middle" fill="#2ea043" font-size="12" '
               f'font-weight="bold" font-family="monospace" opacity="0.6" '
               f'transform="rotate(-90 {cx + 30} {height / 2})">ARDUINO NANO</text>')
    for yy, pin in nano_rows:
        out.append(f'<circle cx="{X_NANO0}" cy="{yy}" r="4" fill="#d4c044"/>'
                   f'<text x="{X_NANO0 + 10}" y="{yy + 5}" fill="{nano_color(pin)}" font-size="14" '
                   f'font-weight="bold" font-family="monospace">{pin}</text>')
    return (f'<svg viewBox="0 0 {WIDTH} {height}" class="scheme" xmlns="http://www.w3.org/2000/svg" '
            f'role="img" aria-label="Схема подключения к Arduino Nano">\n' + "\n".join(out) + "\n</svg>")


def pinout_svg() -> str:
    """Распиновка Nano: подключённые выводы подсвечены и подписаны."""
    used = {c.nano for _, c in NUMBERED}
    step, x0 = 46, 70
    width = x0 + 15 * step + 10
    out = [f'<rect x="{x0 - 28}" y="70" width="{15 * step + 10}" height="120" rx="8" fill="#0c1a10" '
           f'stroke="#2ea043" stroke-width="2"/>',
           f'<rect x="{x0 - 62}" y="112" width="34" height="36" rx="3" fill="#30363d" stroke="#6e7681"/>',
           f'<text x="{x0 - 45}" y="134" text-anchor="middle" fill="#c9d1d9" font-size="9" font-family="Arial">USB</text>',
           f'<text x="{x0 + 7 * step}" y="136" text-anchor="middle" fill="#3fb950" font-size="15" '
           f'font-weight="bold" font-family="monospace">ARDUINO NANO</text>']
    for pins, y_pin, y_lab, y_dst, angle in ((NANO_TOP, 88, 108, 58, -45), (NANO_BOT, 172, 160, 204, 45)):
        for i, pin in enumerate(pins):
            x = x0 + i * step
            on = pin in used
            out.append(f'<circle cx="{x}" cy="{y_pin}" r="7" fill="{"#d4c044" if on else "#4a4630"}" stroke="#0d1117"/>')
            out.append(f'<text x="{x}" y="{y_lab}" text-anchor="middle" fill="{"#fff" if on else "#6e7681"}" '
                       f'font-size="10" font-weight="{"bold" if on else "normal"}" font-family="monospace">{pin}</text>')
            if not on:
                continue
            if pin in POWER:
                out.append(f'<text x="{x}" y="{y_dst}" text-anchor="middle" fill="{POWER[pin]}" font-size="10" '
                           f'font-weight="bold" font-family="Arial">общая</text>')
            else:
                out.append(f'<text x="{x}" y="{y_dst}" fill="{SIGNAL_PIN}" font-size="11" font-family="Arial" '
                           f'transform="rotate({angle} {x} {y_dst})">{signal_dst(pin)}</text>')
    return (f'<svg viewBox="0 -50 {width} 340" class="pinout" xmlns="http://www.w3.org/2000/svg" role="img" '
            f'aria-label="Распиновка Arduino Nano с подключёнными выводами">\n' + "\n".join(out) + "\n</svg>")


# ─────────────────────────── HTML ───────────────────────────


def conn_step(st: Stage, c: Conn) -> str:
    color = COLORS[c.color][0]
    via = f" через <b>{c.via}</b>" if c.via else ""
    return (f'<span class="wn" style="background:{color};color:{text_on(color)}">{WIRE_NO[(st.key, c.pin)]}</span> '
            f'<b>{c.pin}</b> → <b class="pin" style="color:{nano_color(c.nano)}">{c.nano}</b> Nano{via} '
            f'<span class="muted">({COLORS[c.color][1]} провод)</span>')


def steps_box(items: list[str], check: str = "", head: str = "Порядок действий") -> str:
    rows = [f'  <div class="note-item"><span class="note-icon">{i}</span>'
            f'<span class="note-text">{t}</span></div>' for i, t in enumerate(items, 1)]
    if check:
        rows.append(f'  <div class="note-item check"><span class="note-icon">✔</span>'
                    f'<span class="note-text"><b>Проверка:</b> {check}</span></div>')
    return f'<div class="notes-box">\n  <div class="section-title">{head}</div>\n' + "\n".join(rows) + "\n</div>"


def conn_rows(st: Stage, with_via: bool) -> list[str]:
    rows = []
    for c in st.conns:
        extra = f'<td class="via">{c.via or "—"}</td>' if with_via else ""
        note = c.note if with_via else (c.via or c.note)
        rows.append(f'    <tr><td class="num">{WIRE_NO[(st.key, c.pin)]}</td><td class="from">{c.pin}</td>'
                    f'<td class="arrow">→</td><td class="to" style="color:{nano_color(c.nano)}">{c.nano}</td>'
                    f'{extra}<td>{swatch(c.color)}</td><td class="note">{note}</td></tr>')
    return rows


def conn_table(st: Stage) -> str:
    rows = conn_rows(st, with_via=True)
    rows += [f'    <tr><td class="num">—</td><td class="from">{pin}</td><td class="arrow"></td>'
             f'<td class="to muted">—</td><td class="via">—</td><td></td><td class="note">{text}</td></tr>'
             for pin, text in st.unused]
    return ('<div class="wirelist">\n  <table>\n    <tr><th>№</th><th>Вывод модуля</th><th></th><th>Вывод Nano</th>'
            '<th>Через</th><th>Цвет провода</th><th>Заметка</th></tr>\n' + "\n".join(rows) + "\n  </table>\n</div>")


def section(n: int, title: str, where: str, left: str, side: str, prev: str, nxt: str) -> str:
    total = len(STAGES) + 1
    return f"""<section class="stage" id="stage-{n}">
<div class="stage-head">
  <div><div class="stage-num">Этап {n} из {total}</div><h2>{title}</h2></div>
  <div class="stage-where">{where}</div>
</div>
<div class="stage-body">
<div class="stage-left">
{left}
</div>
<div class="stage-side">
{side}
</div>
</div>
<div class="stage-nav">{prev}<a href="#top">↑ Все этапы</a>{nxt}</div>
</section>"""


def stage_block(n: int, st: Stage) -> str:
    steps = [conn_step(st, c) for c in st.conns]
    steps += [f"<b>{p}</b> — {t}." for p, t in st.unused]
    steps += list(st.tips)
    side = steps_box(steps, st.check) + (f'\n<div class="stage-note">{st.note}</div>' if st.note else "")
    count = len(st.conns)
    return section(n, f"{st.icon} {st.title}", f"{count} провод{plural(count)}",
                   f"{diagram([st])}\n{conn_table(st)}", side,
                   f'<a href="#stage-{n - 1}">← Этап {n - 1}</a>', f'<a href="#stage-{n + 1}">Этап {n + 1} →</a>')


def overview_table() -> str:
    rows = []
    for st in STAGES:
        rows.append(f'    <tr class="grp"><td colspan="6">{st.icon} {st.title}</td></tr>')
        rows += conn_rows(st, with_via=False)
    return (f'<div class="wirelist">\n  <h3>🔌 ВСЕ СОЕДИНЕНИЯ — {len(NUMBERED)} ПРОВОДОВ</h3>\n  <table>\n'
            '    <tr><th>№</th><th>Вывод модуля</th><th></th><th>Вывод Nano</th><th>Цвет</th><th>Заметка</th></tr>\n'
            + "\n".join(rows) + "\n  </table>\n</div>")


def nano_table() -> str:
    """Обратная таблица: вывод Nano → что к нему подключено."""
    order = [p for p in NANO_TOP + NANO_BOT if p not in POWER] + list(POWER)
    rows = []
    for pin in dict.fromkeys(order):
        items = [f"№{WIRE_NO[(st.key, c.pin)]} {st.title.split(' (')[0]} · {c.pin}"
                 for st, c in NUMBERED if c.nano == pin]
        if items:
            rows.append(f'    <tr><td class="to" style="color:{nano_color(pin)}">{pin}</td>'
                        f'<td class="note2">{"; ".join(items)}</td></tr>')
    return ('<div class="wirelist">\n  <h3>📍 ПО ВЫВОДАМ NANO — ЧТО К ЧЕМУ ПОДКЛЮЧЕНО</h3>\n  <table>\n'
            '    <tr><th>Вывод Nano</th><th>Подключено</th></tr>\n' + "\n".join(rows) + "\n  </table>\n</div>")


def contents() -> str:
    last = len(STAGES) + 1
    items = ['  <a href="#stage-0"><span>0</span>Как читать · распиновка Nano</a>']
    items += [f'  <a href="#stage-{n}"><span>{n}</span>{st.title}</a>' for n, st in enumerate(STAGES, 1)]
    items.append(f'  <a href="#stage-{last}"><span>{last}</span>Проверка перед включением</a>')
    items.append('  <a href="#spravka"><span>📋</span>Все соединения одной таблицей</a>')
    return '<nav class="contents" aria-label="Этапы">\n' + "\n".join(items) + "\n</nav>"


CSS = """
  * { box-sizing: border-box; margin: 0; padding: 0; }
  html { scroll-behavior: smooth; }
  body { background: #0d1117; color: #e0e0e0; font-family: Arial, sans-serif; padding: 20px; font-size: 13px; }
  h1 { text-align: center; color: #58a6ff; font-size: 1.5em; margin-bottom: 4px; }
  .subtitle { text-align: center; color: #666; margin-bottom: 18px; font-size: 0.85em; }
  a { color: #58a6ff; text-decoration: none; }
  a:hover { text-decoration: underline; }
  a:focus-visible { outline: 2px solid #58a6ff; outline-offset: 2px; border-radius: 4px; }
  .muted { color: #8b949e; }

  .contents { display: grid; grid-template-columns: repeat(auto-fill, minmax(260px, 1fr)); gap: 6px;
              max-width: 1300px; margin: 0 auto 20px; }
  .contents a { display: flex; gap: 10px; align-items: center; padding: 7px 12px; color: #c9d1d9;
                background: #161b22; border: 1px solid #30363d; border-radius: 8px; font-size: 12px; }
  .contents a:hover { border-color: #58a6ff; text-decoration: none; }
  .contents span { min-width: 26px; height: 26px; border-radius: 50%; background: #1f6feb; color: #fff;
                   font-weight: bold; display: flex; align-items: center; justify-content: center; }

  .stage { max-width: 1300px; margin: 0 auto 26px; border: 1px solid #30363d; border-radius: 10px;
           overflow: hidden; scroll-margin-top: 10px; }
  .stage-head { display: flex; justify-content: space-between; align-items: flex-end; gap: 16px;
                padding: 12px 18px; background: #0c2033; border-bottom: 1px solid #1c4a7a; }
  .stage-num { color: #f0a000; font-weight: bold; font-size: 12px; letter-spacing: 1px; }
  .stage-head h2 { color: #58a6ff; font-size: 1.3em; margin-top: 2px; }
  .stage-where { color: #8b949e; font-size: 12px; }
  .stage-body { display: grid; grid-template-columns: minmax(0, 1.3fr) minmax(0, 1fr); gap: 18px; padding: 16px; }
  .stage-left, .stage-side { display: grid; gap: 14px; align-content: start; }
  .stage-note { color: #f0a000; font-size: 12px; padding: 8px 12px; border: 1px dashed #4a3a00;
                border-radius: 8px; background: #1a1500; }
  .stage-nav { display: flex; justify-content: space-between; padding: 8px 18px; border-top: 1px solid #21262d;
               background: #0f1620; font-size: 12px; }

  .scheme { width: 100%; display: block; background: #0d1117; border-radius: 10px; border: 1px solid #21262d; }
  .pinout { width: 100%; display: block; }

  .section-title { padding: 6px 12px; font-weight: bold; font-size: 0.75em; letter-spacing: 1px;
                   text-transform: uppercase; background: #002030; color: #58a6ff; }
  .notes-box { border: 1px solid #30363d; border-radius: 10px; overflow: hidden; }
  .note-item { padding: 8px 14px; border-bottom: 1px solid #21262d; font-size: 13px; line-height: 1.5;
               display: flex; gap: 10px; align-items: flex-start; }
  .note-item:last-child { border-bottom: none; }
  .note-icon { min-width: 22px; height: 22px; border-radius: 50%; background: #f0a000; color: #111;
               font-weight: bold; font-size: 11px; display: flex; align-items: center; justify-content: center; }
  .note-text { color: #c9d1d9; }
  .note-text b { color: #fff; }
  .note-text b.pin { font-family: monospace; font-size: 14px; }
  .wn { display: inline-block; min-width: 20px; padding: 0 5px; border-radius: 10px; font-size: 11px;
        font-weight: bold; text-align: center; border: 1px solid #e0e0e0; }
  .note-item.check { background: #07251a; }
  .note-item.check .note-icon { background: #2ea043; color: #fff; }
  .note-item.check .note-text b { color: #56d364; }

  .wirelist { border: 1px solid #30363d; border-radius: 10px; overflow: hidden; }
  .wirelist h3 { padding: 10px 16px; background: #002030; color: #58a6ff; font-size: 0.9em;
                 letter-spacing: 2px; font-weight: bold; }
  table { width: 100%; border-collapse: collapse; }
  th { background: #161b22; color: #8b949e; padding: 6px 10px; font-size: 11px; text-align: left;
       border-bottom: 1px solid #30363d; }
  td { padding: 6px 10px; border-bottom: 1px solid #21262d; font-size: 12px; color: #c9d1d9; }
  tr:last-child td { border-bottom: none; }
  td.num { width: 34px; color: #58a6ff; font-weight: bold; font-family: monospace; }
  td.from { font-family: monospace; color: #e0e0e0; }
  td.to { font-family: monospace; font-weight: bold; font-size: 14px; }
  td.arrow { color: #6e7681; width: 20px; }
  td.via { font-family: monospace; color: #c8b48a; }
  td.note { color: #8b949e; font-size: 11px; }
  td.note2 { color: #c9d1d9; }
  tr.grp td { background: #0f1620; color: #79b8ff; font-size: 11px; font-weight: bold;
              letter-spacing: 1px; text-transform: uppercase; }
  .color-swatch { display: inline-block; width: 14px; height: 14px; border-radius: 3px;
                  vertical-align: middle; margin-right: 6px; border: 1px solid #666; }

  .spravka { max-width: 1300px; margin: 30px auto 0; display: grid; gap: 18px; }
  .footer { text-align: center; color: #444; font-size: 11px; margin: 14px 0; }

  @media (max-width: 900px) {
    body { padding: 12px; }
    .stage-body { grid-template-columns: 1fr; padding: 10px; }
    .stage-head { flex-direction: column; align-items: flex-start; }
    .wirelist { overflow-x: auto; }
  }
  @media print {
    .contents, .stage-nav { display: none; }
    .stage, .spravka { page-break-before: always; }
  }
"""


def build() -> str:
    assert len(WIRE_NO) == len(NUMBERED), "повтор вывода в одном модуле"
    last = len(STAGES) + 1
    intro = section(0, "Как читать · распиновка Arduino Nano", "сначала прочитайте этот этап",
                    f'<div class="notes-box"><div class="section-title">Какие выводы Nano заняты</div>\n'
                    f'{pinout_svg()}\n</div>',
                    steps_box(RULES, head="Правила"), "<span></span>", '<a href="#stage-1">Этап 1 →</a>')
    stages = "\n\n".join(stage_block(n, st) for n, st in enumerate(STAGES, 1))
    final = section(last, "✅ Проверка перед первым включением", "всё устройство", diagram(STAGES),
                    steps_box(FINAL_CHECK, head="Чек-лист"),
                    f'<a href="#stage-{last - 1}">← Этап {last - 1}</a>', '<a href="#spravka">Все соединения →</a>')
    return f"""<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Устройство контроля ГСМ — Подключение по этапам</title>
<style>{CSS}</style>
</head>
<body>
<h1 id="top">⚡ Устройство контроля ГСМ — какой вывод куда вести</h1>
<div class="subtitle">Arduino Nano · LCD-2004 I²C · MQ-2 · DHT11 · HC-SR04 · 4 светодиода · 3 кнопки ·
зуммер SFM-27 · питание USB Type-C 5 В · {last} этапов · {len(NUMBERED)} проводов</div>
{contents()}

{intro}

{stages}

{final}

<section class="spravka" id="spravka">
<h1>📋 Все соединения</h1>
{overview_table()}
{nano_table()}
<div class="footer"><a href="#top">↑ К списку этапов</a></div>
</section>

</body>
</html>
"""


if __name__ == "__main__":
    OUT.write_text(build(), encoding="utf-8")
    print(f"Готово: {OUT} ({OUT.stat().st_size // 1024} КБ)")

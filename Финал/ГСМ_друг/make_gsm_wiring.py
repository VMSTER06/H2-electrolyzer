"""Генератор «Устройство контроля ГСМ — схема подключения и монтажная ведомость».

Оформление повторяет H2_Wiring_Full.html (тёмная тема, таблицы по модулям,
монтажная ведомость, графические схемы). Все таблицы и рисунки строятся
из одних данных ниже, поэтому расхождений между ними быть не может.

Запуск:  python3 make_gsm_wiring.py   →   ГСМ_схема_подключения.html
"""
from dataclasses import dataclass
from pathlib import Path

OUT = Path(__file__).with_name("ГСМ_схема_подключения.html")

# ─────────────────────────── Данные ───────────────────────────

# Arduino Nano на основной плате: верхний ряд B2–B16, нижний ряд H2–H16.
NANO_TOP = ["D13", "3V3", "REF", "A0", "A1", "A2", "A3", "A4", "A5", "A6",
            "A7", "5V", "RST", "GND", "VIN"]
NANO_BOT = ["D12", "D11", "D10", "D9", "D8", "D7", "D6", "D5", "D4", "D3",
            "D2", "GND", "RST", "RX0", "TX1"]
NANO_HOLE = {**{p: f"B{i + 2}" for i, p in enumerate(NANO_TOP)},
             **{p: f"H{i + 2}" for i, p in enumerate(NANO_BOT)}}
NANO_HOLE["5V"], NANO_HOLE["GND"] = "B13", "H13"

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

# Цвета типов сигналов (легенда графической схемы, как в H2_Wiring_Full).
SIG = {
    "i2c": ("#8a7bff", "I²C (дисплей)"),
    "adc": ("#56d364", "Аналоговый (MQ-2)"),
    "dig": ("#e8c84a", "Цифровые датчики"),
    "led": ("#d85cff", "Светодиоды"),
    "btn": ("#ffa500", "Кнопки"),
    "snd": ("#ff5555", "Зуммер"),
    "p5":  ("#ff6666", "+5 В"),
    "gnd": ("#888888", "GND"),
}


@dataclass(frozen=True)
class Group:
    key: str
    icon: str
    module: str     # название модуля с обозначением
    detail: str     # уточнение для заголовка секции
    css: str        # класс цвета секции (из H2_Wiring_Full)
    board: str      # "main" — основная плата, "panel" — панельная
    length: str     # длина проводов

    @property
    def title(self) -> str:
        return f"{self.icon} {self.module}" + (f" — {self.detail}" if self.detail else "")


@dataclass(frozen=True)
class Wire:
    n: int
    group: str
    pin: str            # вывод модуля / откуда идёт провод
    hole: str           # отверстие на основной плате
    color: str
    sig: str            # тип сигнала (ключ SIG)
    nano: str = ""      # вывод Nano, к которому снизу отгибается конец
    bus: str = ""       # "+5 В" или "GND", если провод идёт на шину
    panel: str = ""     # отверстие на панельной плате (жгут)
    panel_to: str = ""  # куда отгибается конец на панели
    note: str = ""


GROUPS = [
    Group("harness", "🔗", "Жгут", "основная плата ↔ панельная (8 проводов)", "s-wires", "panel",
          "20 см, МГТФ, скрутить в жгут"),
    Group("lcd", "🖥", "Дисплей LCD-2004B-IIC (DA1)", "I²C", "s-i2c", "main", "20 см"),
    Group("mq2", "📊", "Датчик газа MQ-2 (DD1)", "", "s-adc", "main", "12 см"),
    Group("dht", "🌡", "Датчик температуры DHT11 (DD2)", "", "s-adc", "main", "12 см"),
    Group("sr04", "📏", "Датчик расстояния HC-SR04 (DD3)", "", "s-nrf", "main", "10 см"),
    Group("usb", "🔋", "Разъём питания USB Type-C (XP1)", "", "s-pwr", "main", "12 см"),
    Group("buzz", "🔊", "Зуммер SFM-27 (BQ1)", "", "s-relay", "main", "12 см"),
]
GROUP = {g.key: g for g in GROUPS}

WIRES = [
    Wire(1, "harness", "D8", "I6", "green", "led", nano="D8", panel="J3", panel_to="I3",
         note="R1 → VD1 «OK» (зелёный)"),
    Wire(2, "harness", "D10", "I4", "yellow", "led", nano="D10", panel="J7", panel_to="I7",
         note="R2 → VD2 «LOW» (жёлтый)"),
    Wire(3, "harness", "D9", "I5", "red", "led", nano="D9", panel="J11", panel_to="I11",
         note="R3 → VD3 «GAS» (красный)"),
    Wire(4, "harness", "D12", "I2", "darkred", "led", nano="D12", panel="J15", panel_to="I15",
         note="R4 → VD4 «TEMP» (красный)"),
    Wire(5, "harness", "D7", "I7", "blue", "btn", nano="D7", panel="K6", panel_to="L6",
         note="Кнопка SA1"),
    Wire(6, "harness", "D5", "I9", "lightblue", "btn", nano="D5", panel="K12", panel_to="L12",
         note="Кнопка SA2"),
    Wire(7, "harness", "D6", "I8", "cyan", "btn", nano="D6", panel="K18", panel_to="L18",
         note="Кнопка SA3"),
    Wire(8, "harness", "GND", "Q21", "black", "gnd", bus="GND", panel="P9", panel_to="O9",
         note="Общий провод панели"),

    Wire(9, "lcd", "GND", "M21", "black", "gnd", bus="GND"),
    Wire(10, "lcd", "VCC", "L24", "red", "p5", bus="+5 В"),
    Wire(11, "lcd", "SDA", "A9", "pink", "i2c", nano="A4", note="Шина I²C, данные"),
    Wire(12, "lcd", "SCL", "A10", "purple", "i2c", nano="A5", note="Шина I²C, тактирование"),

    Wire(13, "mq2", "VCC", "E24", "red", "p5", bus="+5 В", note="Нагреватель, модуль тёплый — норма"),
    Wire(14, "mq2", "GND", "F21", "black", "gnd", bus="GND"),
    Wire(15, "mq2", "AO", "A5", "cyan", "adc", nano="A0", note="Аналоговый выход"),

    Wire(16, "dht", "DATA (S)", "I12", "brown", "dig", nano="D2",
         note="Рядом I13 — там GND, не замкнуть!"),
    Wire(17, "dht", "VCC (+)", "G24", "red", "p5", bus="+5 В"),
    Wire(18, "dht", "GND (−)", "H21", "black", "gnd", bus="GND"),

    Wire(19, "sr04", "VCC", "J24", "red", "p5", bus="+5 В"),
    Wire(20, "sr04", "Trig", "I11", "orange", "dig", nano="D3", note="Запуск измерения"),
    Wire(21, "sr04", "Echo", "I10", "yellow", "dig", nano="D4", note="Эхо-импульс"),
    Wire(22, "sr04", "GND", "K21", "black", "gnd", bus="GND"),

    Wire(23, "usb", "VBUS", "C24", "red", "p5", bus="+5 В", note="Питание всего устройства"),
    Wire(24, "usb", "GND", "D21", "black", "gnd", bus="GND"),

    Wire(25, "buzz", "+ (красный)", "I3", "red", "snd", nano="D11", note="Зуммер полярный"),
    Wire(26, "buzz", "− (чёрный)", "O21", "black", "gnd", bus="GND"),
]

# Выводы модулей, которые не подключаются.
UNUSED = {"mq2": [("DO", "Цифровой выход не нужен")]}

# Шины и перемычки основной платы (лужёный провод, снизу).
BUSES = [
    ("Шина +5 В", "столбец 24: A24 → R24", "пайка к A24, E24, J24, N24, R24", "p5"),
    ("Перемычка 5V", "B13 (5V Nano) → A13 → по ряду A → A24", "A5, A9, A10 оставить свободными", "p5"),
    ("Шина GND", "столбец 21: C21 → R21", "НЕ до A21 — по ряду A идёт +5 В", "gnd"),
    ("Перемычка GND", "H13 (GND Nano) → I13 → по ряду I → I21", "I12 рядом — туда идёт DHT11", "gnd"),
]

# Панельная плата: детали и шина GND.
PANEL_LEDS = [  # (обозн., цвет, анод, катод, резистор, верх R, низ R, подпись)
    ("VD1", "green", "C3", "C4", "R1", "D3", "I3", "OK"),
    ("VD2", "yellow", "C7", "C8", "R2", "D7", "I7", "LOW"),
    ("VD3", "red", "C11", "C12", "R3", "D11", "I11", "GAS"),
    ("VD4", "red", "C15", "C16", "R4", "D15", "I15", "TEMP"),
]
PANEL_BUTTONS = [  # (обозн., четыре ножки, сигнал, GND)
    ("SA1", ("L1", "L6", "N1", "N6"), "L6", "N1"),
    ("SA2", ("L7", "L12", "N7", "N12"), "L12", "N7"),
    ("SA3", ("L13", "L18", "N13", "N18"), "L18", "N13"),
]
PANEL_GND = [  # (название, от, до, что к ней припаять)
    ("ряд B", "B4", "B16", "катоды C4, C8, C12, C16"),
    ("столбец 9", "B9", "O9", "соединяет ряды B и O"),
    ("ряд O", "O1", "O18", "ножки N1, N7, N13"),
]

STAGES = [
    ("Arduino Nano на основную плату",
     "Отметить угол A1 с двух сторон. Nano штырьками со стороны деталей: верхний ряд B2–B16, "
     "нижний H2–H16, USB к столбцу 1. Сначала две точки по диагонали (B2, H16), выровнять, затем остальные 28.",
     "блестящие конусы, нет перемычек припоя между выводами"),
    ("Шина +5 В и перемычка 5V",
     "Лужёный провод ≈ 5 см снизу вдоль столбца 24 (A24–R24). Перемычка от 5V Nano (B13) через A13 по ряду A до A24.",
     "B13 звонится с R24"),
    ("Шина GND и перемычка GND",
     "Провод ≈ 4,5 см вдоль столбца 21 (C21–R21). Перемычка от GND Nano (H13) через I13 по ряду I до I21.",
     "H13 звонится с R21; между R24 и R21 НЕТ замыкания"),
    ("Светодиоды VD1–VD4 (панель)",
     "Отметить A1 на панели. Анод (длинная ножка) / катод: VD1 C3/C4, VD2 C7/C8, VD3 C11/C12, VD4 C15/C16. "
     "Вплотную к плате — верх входит в отверстия крышки.",
     "мультиметр «диод»: красный щуп на анод — светодиод слабо светится"),
    ("Резисторы R1–R4 220 Ом (панель)",
     "Выводы под шаг 5 отверстий. R1 D3–I3, R2 D7–I7, R3 D11–I11, R4 D15–I15, плашмя. "
     "Анод отогнуть к верхнему выводу: C3–D3, C7–D7, C11–D11, C15–D15.",
     "от I3 через R1 к аноду VD1 — 220 Ом (так же R2–R4)"),
    ("Кнопки SA1–SA3 (панель)",
     "SA1 L1, L6, N1, N6; SA2 L7, L12, N7, N12; SA3 L13, L18, N13, N18. До щелчка, ровно. "
     "Сигнал — ножка L, GND — ножка N по диагонали.",
     "кнопка стоит ровно, 12 ножек пропаяны"),
    ("Общий провод (GND) панели",
     "Лужёный провод по ряду B (B4–B16), к нему катоды C4, C8, C12, C16. Столбец 9 (B9–O9). "
     "Ряд O (O1–O18), к нему N1, N7, N13. Ножки N6, N12, N18 НЕ отгибать.",
     "L6–O1 звонится только при нажатой SA1; L12–O7 — SA2; L18–O13 — SA3"),
    ("Жгут из 8 проводов",
     "Провода 1–8 (МГТФ, 20 см). На основной плате конец снизу отогнуть к выводу Nano в ряду H; "
     "на панели J → I, K6 → L6, K12 → L12, K18 → L18, P9 → O9. Подписать концы скотчем.",
     "каждый провод звонится от вывода Nano до точки на панели"),
    ("Дисплей LCD-2004B-IIC (DA1)",
     "Провода 9–12 на штырьки платы I²C, на каждую пайку термоусадка.",
     "A9 ↔ A4 Nano, A10 ↔ A5, VCC ↔ R24, GND ↔ R21"),
    ("Датчик газа MQ-2 (DD1)", "Провода 13–15. DO оставить свободным.", "AO звонится с A0 Nano"),
    ("Датчик DHT11 (DD2)",
     "Провода 16–18. Порядок выводов у модулей разный — смотреть надписи S / + / −.",
     "DATA звонится с D2 и НЕ звонится с GND"),
    ("Датчик HC-SR04 (DD3)", "Провода 19–22, выводы датчика смотрят к основной плате.",
     "Trig ↔ D3, Echo ↔ D4, между собой не замкнуты"),
    ("Разъём USB Type-C (XP1)", "Провода 23–24: только VBUS и GND, остальные выводы свободны.",
     "VBUS ↔ R24, GND ↔ R21, замыкания нет"),
    ("Зуммер SFM-27 (BQ1)", "Провода 25–26. Красный — плюс. Свои провода зуммера можно нарастить, спайку изолировать.",
     "красный провод звонится с D11"),
    ("Проверка перед первым включением", "Пункты — в блоке «Перед первым включением» ниже.", "всё по чек-листу"),
]

NOTES = [
    ("Отверстие = буква ряда + номер столбца.", "I6 — ряд I, столбец 6. Основная плата 5×7 см: 24 столбца × ряды A–R; "
     "панельная: 18 столбцов × ряды A–X."),
    ("Угол A1 отметить маркером на обеих платах с двух сторон.", "Ориентироваться только по этой метке."),
    ("Вид со стороны пайки — зеркальный.", "Плата перевёрнута, столбец 1 оказывается справа. "
     "Все координаты в этой ведомости — со стороны деталей."),
    ("Перемычка", "— конец провода или вывода детали, отогнутый снизу к соседнему отверстию и пропаянный вместе с ним одной каплей."),
    ("Шина", "— лужёный провод снизу вдоль ряда отверстий, припаянный к пятачкам через каждые 3–4 отверстия."),
    ("Внешний провод:", "зачистить и залудить 3–4 мм, вставить СВЕРХУ, снизу отогнуть к указанному выводу Nano и пропаять вместе."),
    ("Паяльник 25–40 Вт, 1–2 с на точку.", "Припой должен лечь блестящим конусом."),
    ("После каждого этапа — проверка из таблицы этапов.", "Ошибка, пропущенная на раннем этапе, дорого стоит потом."),
    ("Все провода питания модулей — только на шины.", "+5 В — столбец 24, GND — столбец 21 основной платы."),
]

CHECKLIST = [
    ("Осмотр с лупой:", "нет капель припоя между соседними выводами Nano и в рядах A, I."),
    ("R24–R21:", "нет короткого замыкания (не 0 Ом)."),
    ("Прозвонить каждый провод из ведомости:", "вывод модуля ↔ нужный вывод Nano или шина."),
    ("Светодиоды:", "режим «диод» — красный щуп на J3 (J7, J11, J15), чёрный на P9 — светодиод слабо светится."),
    ("Кнопки:", "K6–P9 звонится только при нажатой SA1 (K12 — SA2, K18 — SA3)."),
    ("Первое включение — от USB-C (XP1), без прошивки.", "На R24 должно быть 4,7–5,2 В. Что-то греется или пахнет — сразу отключить."),
    ("MQ-2 тёплый — это норма", "(внутри нагреватель), показания стабилизируются через 1–2 мин."),
    ("Прошить gsm_control_lcd2004.ino.", "Пустой экран при горящей подсветке — покрутить подстроечник контраста на плате I²C; "
     "ничего — сменить адрес LCD_ADDR на 0x3F."),
]

# ─────────────────────────── Утилиты ───────────────────────────


def hole_rc(hole: str) -> tuple[int, int]:
    """'I6' → (8, 6): индекс ряда с нуля и номер столбца."""
    return ord(hole[0]) - ord("A"), int(hole[1:])


def text_on(hex_color: str) -> str:
    """Цвет текста (чёрный/белый), читаемый на заданной заливке."""
    r, g, b = (int(hex_color[i:i + 2], 16) for i in (1, 3, 5))
    return "#111" if 0.299 * r + 0.587 * g + 0.114 * b > 150 else "#fff"


def swatch(color: str) -> str:
    return f'<span class="color-swatch" style="background:{COLORS[color][0]}"></span>{COLORS[color][1]}'


def target(w: Wire) -> str:
    """Куда приходит провод на основной плате — для таблиц."""
    if w.nano:
        return f"{w.nano} · {w.hole}"
    return f"{w.bus} · {w.hole}"


def wires_of(group: str) -> list[Wire]:
    return [w for w in WIRES if w.group == group]


# ─────────────────────────── HTML: таблицы ───────────────────────────


def section(title: str, css: str, rows: list[tuple[str, str, str, str]]) -> str:
    """Секция карточки устройства: строки (вывод, точка, класс точки, заметка)."""
    body = "\n".join(f'        <tr><td>{a}</td><td class="{cls}">{b}</td><td>{c}</td></tr>'
                     for a, b, cls, c in rows)
    return (f'    <div class="section">\n      <div class="section-title {css}">{title}</div>\n'
            f'      <table>\n{body}\n      </table>\n    </div>')


def pin_class(w: Wire) -> str:
    return {"+5 В": "pin-p5", "GND": "pin-gnd"}.get(w.bus, "")


def main_board_card() -> str:
    parts = [section("🔌 Шины основной платы (лужёный провод снизу)", "s-pwr",
                     [(name, where, "pin-p5" if sig == "p5" else "pin-gnd", note)
                      for name, where, note, sig in BUSES])]
    for g in GROUPS:
        if g.board != "main":
            continue
        rows = [(f"{w.pin} <span class='wn'>№{w.n}</span>", target(w), pin_class(w), w.note)
                for w in wires_of(g.key)]
        rows += [(pin, "—", "", note) for pin, note in UNUSED.get(g.key, [])]
        parts.append(section(f"{g.title} · {g.length}", g.css, rows))
    return ('  <div class="device">\n    <div class="device-header brain-hdr">'
            '🧠 ОСНОВНАЯ ПЛАТА 5×7 см · Arduino Nano · датчики, дисплей, питание</div>\n'
            + "\n".join(parts) + "\n  </div>")


def panel_board_card() -> str:
    by_end = {w.panel_to: w for w in wires_of("harness")}
    leds = []
    for vd, color, anode, cathode, r, r_top, r_bot, label in PANEL_LEDS:
        w = by_end[r_bot]
        leds.append((f"{vd} «{label}» ({COLORS[color][1]})", f"{w.nano} · {w.panel}",
                     "", f"{r} 220 Ом {r_top}–{r_bot}; анод {anode}, катод {cathode} → GND"))
    buttons = []
    for sa, legs, sig, gnd in PANEL_BUTTONS:
        w = by_end[sig]
        buttons.append((f"{sa}", f"{w.nano} · {w.panel}", "",
                        f"ножки {', '.join(legs)}; сигнал {sig}, GND {gnd}"))
    gnd_rows = [(name, f"{a} → {b}", "pin-gnd", note) for name, a, b, note in PANEL_GND]
    harness = [(f"№{w.n} · {'GND' if w.bus else w.nano} ({w.hole})", f"{w.panel} → {w.panel_to}",
                pin_class(w), f"{swatch(w.color)} · {w.note}") for w in wires_of("harness")]
    parts = [
        section("💡 Светодиоды × 4 (Nano → 220 Ом → LED → GND)", "s-led", leds),
        section("🔘 Кнопки × 3 (INPUT_PULLUP → GND)", "s-btn", buttons),
        section("⏚ Общий провод панели (лужёный провод снизу)", "s-pwr", gnd_rows),
        section(f"{GROUP['harness'].title} · {GROUP['harness'].length}", "s-wires", harness),
    ]
    return ('  <div class="device">\n    <div class="device-header remote-hdr">'
            '🎛 ПАНЕЛЬНАЯ ПЛАТА · 4 светодиода, 3 кнопки (на крышке)</div>\n'
            + "\n".join(parts) + "\n  </div>")


def notes_box(title: str, items: list[tuple[str, str]], css: str = "s-note", kind: str = "") -> str:
    rows = "\n".join(f'  <div class="note-item"><span class="note-icon">{i}</span>'
                     f'<span class="note-text"><strong>{head}</strong> {text}</span></div>'
                     for i, (head, text) in enumerate(items, 1))
    return f'<div class="notes-box {kind}">\n  <div class="section-title {css}">{title}</div>\n{rows}\n</div>'


def stages_box() -> str:
    rows = "\n".join(
        f'    <tr><td class="num">{i}</td><td class="stage">{name}</td><td class="act">{act}</td>'
        f'<td class="chk">✔ {chk}</td></tr>'
        for i, (name, act, chk) in enumerate(STAGES, 1))
    return ('<div class="wirelist">\n  <h3>🛠 ПОРЯДОК СБОРКИ — 15 ЭТАПОВ</h3>\n  <table class="stages">\n'
            '    <tr><th>№</th><th>Этап</th><th>Что делать</th><th>Проверка</th></tr>\n'
            f'{rows}\n  </table>\n</div>')


def wirelist() -> str:
    rows = []
    for g in GROUPS:
        rows.append(f'    <tr class="grp"><td colspan="6">{g.title} · {g.length}</td></tr>')
        for w in wires_of(g.key):
            if g.key == "harness":
                src = f"Основная {w.hole}" + (f" ({w.nano})" if w.nano else " (шина GND)")
                dst = f"Панель {w.panel} → {w.panel_to}"
            else:
                src = f"{g.module.split(' (')[0]} · {w.pin}"
                dst = (f"Основная {w.hole} → {NANO_HOLE[w.nano]} ({w.nano})" if w.nano
                       else f"Основная {w.hole} (шина {w.bus})")
            rows.append(f'    <tr><td class="num">{w.n}</td><td class="from">{src}</td><td class="to">{dst}</td>'
                        f'<td class="color">{swatch(w.color)}</td><td class="len">{g.length.split(",")[0]}</td>'
                        f'<td class="note">{w.note}</td></tr>')
    return ('<div class="wirelist">\n  <h3>🔌 МОНТАЖНАЯ ВЕДОМОСТЬ — 26 ПРОВОДОВ</h3>\n  <table>\n'
            '    <tr><th>№</th><th>ОТ</th><th>К</th><th>Цвет</th><th>Длина</th><th>Заметка</th></tr>\n'
            + "\n".join(rows) + "\n  </table>\n</div>")


# ─────────────────────────── SVG: функциональная схема ───────────────────────────

ROW_H, BOX_HDR, BOX_GAP, TOP = 22, 24, 18, 90


def functional_svg() -> str:
    """ Nano в центре, слева модули основной платы, справа панель. """
    left = [g for g in GROUPS if g.board == "main"]
    lx0, lx1, nx0, nx1, rx0, rx1 = 40, 250, 440, 600, 790, 1020
    out, nano_left, nano_right = [], [], []
    y = TOP

    def pin_row(x_text: float, yy: float, text: str, anchor: str) -> str:
        return (f'<text x="{x_text}" y="{yy + 4}" text-anchor="{anchor}" fill="#c9d1d9" '
                f'font-size="11" font-family="monospace">{text}</text>')

    for g in left:
        ws = wires_of(g.key)
        unused = UNUSED.get(g.key, [])
        h = BOX_HDR + ROW_H * (len(ws) + len(unused)) + 6
        out.append(f'<rect x="{lx0}" y="{y}" width="{lx1 - lx0}" height="{h}" rx="6" fill="#161b22" stroke="#30363d"/>')
        out.append(f'<text x="{lx0 + 10}" y="{y + 16}" fill="#58a6ff" font-size="11" font-weight="bold" '
                   f'font-family="Arial">{g.module}</text>')
        yy = y + BOX_HDR + ROW_H / 2
        for w in ws:
            out.append(pin_row(lx1 - 8, yy, w.pin, "end"))
            color = SIG[w.sig][0]
            if w.nano:
                out.append(f'<line x1="{lx1}" y1="{yy}" x2="{nx0}" y2="{yy}" stroke="{color}" stroke-width="2"/>')
                out.append(f'<text x="{(lx1 + nx0) / 2}" y="{yy - 4}" text-anchor="middle" fill="#8b949e" '
                           f'font-size="9" font-family="monospace">№{w.n} · {w.hole}</text>')
                nano_left.append((yy, w.nano, color))
            elif g.key == "usb":  # разъём питания — к выводам 5V/GND Nano через шины
                nano_pin = "5V" if w.bus == "+5 В" else "GND"
                out.append(f'<line x1="{lx1}" y1="{yy}" x2="{nx0}" y2="{yy}" stroke="{color}" '
                           f'stroke-width="2" stroke-dasharray="5,3"/>')
                out.append(f'<text x="{(lx1 + nx0) / 2}" y="{yy - 4}" text-anchor="middle" fill="#8b949e" '
                           f'font-size="9" font-family="monospace">№{w.n} · шина {w.bus}</text>')
                nano_left.append((yy, nano_pin, color))
            else:
                out.append(f'<line x1="{lx1}" y1="{yy}" x2="{lx1 + 22}" y2="{yy}" stroke="{color}" '
                           f'stroke-width="2" stroke-dasharray="4,2"/>')
                out.append(f'<text x="{lx1 + 26}" y="{yy + 4}" fill="{color}" font-size="10" '
                           f'font-family="monospace">{w.bus.replace(" ", "")} · {w.hole}</text>')
            yy += ROW_H
        for pin, _ in unused:
            out.append(pin_row(lx1 - 8, yy, pin, "end"))
            out.append(f'<text x="{lx1 + 8}" y="{yy + 4}" fill="#6e7681" font-size="10" '
                       f'font-family="Arial">не подключать</text>')
            yy += ROW_H
        y += h + BOX_GAP
    height = y + 10

    # Панель справа: светодиоды и кнопки.
    harness = {w.panel_to: w for w in wires_of("harness")}
    blocks = [
        ("Светодиоды (панель)", [(f"{r} 220 Ом → {vd} «{lab}»", harness[r_bot])
                                  for vd, _, _, _, r, _, r_bot, lab in PANEL_LEDS]),
        ("Кнопки (панель)", [(f"{sa} → GND", harness[sig]) for sa, _, sig, _ in PANEL_BUTTONS]),
        ("Общий провод панели", [("GND панели (P9)", harness["O9"])]),
    ]
    y = TOP
    for title, rows in blocks:
        h = BOX_HDR + ROW_H * len(rows) + 6
        out.append(f'<rect x="{rx0}" y="{y}" width="{rx1 - rx0}" height="{h}" rx="6" fill="#161b22" stroke="#30363d"/>')
        out.append(f'<text x="{rx0 + 10}" y="{y + 16}" fill="#bc8cff" font-size="11" font-weight="bold" '
                   f'font-family="Arial">{title}</text>')
        yy = y + BOX_HDR + ROW_H / 2
        for text, w in rows:
            out.append(pin_row(rx0 + 8, yy, text, "start"))
            color = SIG[w.sig][0]
            dash = ' stroke-dasharray="5,3"' if w.bus else ""
            out.append(f'<line x1="{nx1}" y1="{yy}" x2="{rx0}" y2="{yy}" stroke="{color}" stroke-width="2"{dash}/>')
            out.append(f'<text x="{(nx1 + rx0) / 2}" y="{yy - 4}" text-anchor="middle" fill="#8b949e" '
                       f'font-size="9" font-family="monospace">№{w.n} · {w.hole} → {w.panel}</text>')
            nano_right.append((yy, w.nano or "GND", color))
            yy += ROW_H
        y += h + BOX_GAP
    height = max(height, y + 10)

    # Arduino Nano.
    out.append(f'<rect x="{nx0}" y="20" width="{nx1 - nx0}" height="{height - 40}" rx="8" fill="#0c1a10" '
               f'stroke="#2ea043" stroke-width="2"/>')
    mid = (nx0 + nx1) / 2
    for i, t in enumerate(("ARDUINO NANO", "ATmega328P", "основная плата")):
        out.append(f'<text x="{mid}" y="{42 + i * 15}" text-anchor="middle" fill="#3fb950" '
                   f'font-size="{12 if i == 0 else 9}" font-weight="{"bold" if i == 0 else "normal"}" '
                   f'font-family="monospace">{t}</text>')
    for yy, pin, color in nano_left:
        out.append(f'<text x="{nx0 + 8}" y="{yy + 4}" fill="{color}" font-size="10" font-family="monospace">'
                   f'{pin} ({NANO_HOLE[pin]})</text>')
    for yy, pin, color in nano_right:
        out.append(f'<text x="{nx1 - 8}" y="{yy + 4}" text-anchor="end" fill="{color}" font-size="10" '
                   f'font-family="monospace">{pin} ({NANO_HOLE[pin]})</text>')

    return (f'<svg viewBox="0 0 1060 {height}" class="scheme" xmlns="http://www.w3.org/2000/svg" '
            f'role="img" aria-label="Функциональная схема подключения к Arduino Nano">\n'
            + "\n".join(out) + "\n</svg>")


# ─────────────────────────── SVG: карты плат ───────────────────────────

STEP, PAD = 20, 34


class BoardMap:
    """Карта макетной платы со стороны деталей."""

    def __init__(self, cols: int, rows: int):
        self.cols, self.rows = cols, rows
        self.items: list[str] = []
        self.top: list[str] = []  # поверх всего (маркеры концов проводов)

    def xy(self, hole: str) -> tuple[float, float]:
        r, c = hole_rc(hole)
        return PAD + (c - 1) * STEP, PAD + r * STEP

    def size(self) -> tuple[int, int]:
        return PAD * 2 + (self.cols - 1) * STEP, PAD * 2 + (self.rows - 1) * STEP

    def grid(self) -> list[str]:
        w, h = self.size()
        out = [f'<rect x="{PAD - 14}" y="{PAD - 14}" width="{w - 2 * PAD + 28}" height="{h - 2 * PAD + 28}" '
               f'rx="6" fill="#161b22" stroke="#30363d"/>']
        for c in range(1, self.cols + 1):
            x = PAD + (c - 1) * STEP
            out.append(f'<text x="{x}" y="{PAD - 18}" text-anchor="middle" fill="#6e7681" font-size="9" '
                       f'font-family="monospace">{c}</text>')
        for r in range(self.rows):
            y = PAD + r * STEP
            out.append(f'<text x="{PAD - 22}" y="{y + 3}" text-anchor="middle" fill="#6e7681" font-size="9" '
                       f'font-family="monospace">{chr(65 + r)}</text>')
            for c in range(1, self.cols + 1):
                out.append(f'<circle cx="{PAD + (c - 1) * STEP}" cy="{y}" r="2.2" fill="#30363d"/>')
        x, y = self.xy("A1")
        out.append(f'<circle cx="{x}" cy="{y}" r="7" fill="none" stroke="#ff5577" stroke-width="2"/>')
        return out

    def line(self, a: str, b: str, color: str, width: float = 3, dash: str = "") -> None:
        (x1, y1), (x2, y2) = self.xy(a), self.xy(b)
        d = f' stroke-dasharray="{dash}"' if dash else ""
        self.items.append(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{color}" '
                          f'stroke-width="{width}" stroke-linecap="round"{d}/>')

    def path(self, holes: list[str], color: str, width: float = 3, dash: str = "") -> None:
        for a, b in zip(holes, holes[1:]):
            self.line(a, b, color, width, dash)

    def pad(self, hole: str, color: str = "#d4c044", r: float = 5) -> None:
        x, y = self.xy(hole)
        self.items.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{color}" stroke="#0d1117"/>')

    def label(self, hole: str, text: str, dx: float = 0, dy: float = 0, color: str = "#c9d1d9",
              size: int = 8, anchor: str = "middle") -> None:
        x, y = self.xy(hole)
        self.items.append(f'<text x="{x + dx}" y="{y + dy}" text-anchor="{anchor}" fill="{color}" '
                          f'font-size="{size}" font-family="monospace">{text}</text>')

    def wire_end(self, hole: str, w: Wire) -> None:
        x, y = self.xy(hole)
        fill = COLORS[w.color][0]
        self.top.append(f'<circle cx="{x}" cy="{y}" r="8" fill="{fill}" stroke="#e0e0e0" stroke-width="1.5"/>'
                        f'<text x="{x}" y="{y + 3}" text-anchor="middle" fill="{text_on(fill)}" font-size="8" '
                        f'font-weight="bold" font-family="Arial">{w.n}</text>')

    def svg(self, label: str) -> str:
        w, h = self.size()
        return (f'<svg viewBox="0 0 {w} {h}" class="board" xmlns="http://www.w3.org/2000/svg" role="img" '
                f'aria-label="{label}">\n' + "\n".join(self.grid() + self.items + self.top) + "\n</svg>")


def main_board_svg() -> str:
    b = BoardMap(24, 18)
    p5, gnd = SIG["p5"][0], SIG["gnd"][0]
    # Шины и перемычки (снизу — пунктир).
    b.line("A24", "R24", p5, 4, "6,3")
    b.path(["B13", "A13", "A24"], p5, 3, "6,3")
    b.line("C21", "R21", gnd, 4, "6,3")
    b.path(["H13", "I13", "I21"], gnd, 3, "6,3")
    # Arduino Nano.
    x0, y0 = b.xy("B2")
    x1, y1 = b.xy("H16")
    b.items.append(f'<rect x="{x0 - 10}" y="{y0 - 10}" width="{x1 - x0 + 20}" height="{y1 - y0 + 20}" rx="5" '
                   f'fill="#0c1a10" stroke="#2ea043" stroke-width="1.5" opacity="0.92"/>')
    ux, uy = b.xy("E1")
    b.items.append(f'<rect x="{ux - 12}" y="{uy - 9}" width="22" height="18" rx="2" fill="#30363d" stroke="#6e7681"/>'
                   f'<text x="{ux - 1}" y="{uy + 3}" text-anchor="middle" fill="#c9d1d9" font-size="7" '
                   f'font-family="Arial">USB</text>')
    b.label("E9", "Arduino Nano", dy=4, color="#3fb950", size=13)
    used = {w.nano for w in WIRES if w.nano} | {"5V", "GND"}
    for pins, row, dy in ((NANO_TOP, "B", 14), (NANO_BOT, "H", -9)):
        for i, pin in enumerate(pins):
            hole = f"{row}{i + 2}"
            b.pad(hole, "#d4c044" if pin in used else "#5a5330", 5)
            b.label(hole, pin, dy=dy, color="#e0e0e0" if pin in used else "#6e7681", size=7)
    # Концы проводов и отгибы к выводам Nano.
    for w in WIRES:
        if w.nano:
            b.line(w.hole, NANO_HOLE[w.nano], COLORS[w.color][0], 2.5, "2,2")
        b.wire_end(w.hole, w)
    b.label("R24", "+5 В", dy=26, color=p5, size=10)
    b.label("R21", "GND", dy=26, color=gnd, size=10)
    return b.svg("Карта основной платы со стороны деталей")


def panel_board_svg() -> str:
    b = BoardMap(18, 24)
    gnd = SIG["gnd"][0]
    for _, a, z, _ in PANEL_GND:
        b.line(a, z, gnd, 4, "6,3")
    for vd, color, anode, cathode, r, r_top, r_bot, label in PANEL_LEDS:
        b.line(anode, r_top, "#c9d1d9", 2, "2,2")          # анод отогнут к резистору
        b.line(cathode, f"B{hole_rc(cathode)[1]}", gnd, 2, "2,2")  # катод — к шине ряда B
        (xa, ya), (xc, _) = b.xy(anode), b.xy(cathode)
        b.items.append(f'<ellipse cx="{(xa + xc) / 2}" cy="{ya}" rx="15" ry="9" fill="{COLORS[color][0]}" '
                       f'stroke="#e0e0e0" stroke-width="1"/>')
        b.label(anode, f"{vd} {label}", dx=STEP / 2, dy=-2 * STEP + 3, size=8)
        (xt, yt), (_, yb) = b.xy(r_top), b.xy(r_bot)
        b.items.append(f'<line x1="{xt}" y1="{yt}" x2="{xt}" y2="{yb}" stroke="#c9d1d9" stroke-width="1.5"/>'
                       f'<rect x="{xt - 6}" y="{yt + 22}" width="12" height="{yb - yt - 44}" rx="3" '
                       f'fill="#c8b48a" stroke="#0d1117"/>')
        b.label(r_top, r, dx=10, dy=(yb - yt) / 2 + 3, size=8, anchor="start")
        b.pad(r_top, "#d4c044", 3.5)
        b.pad(r_bot, "#d4c044", 3.5)
    for sa, legs, sig, gnd_leg in PANEL_BUTTONS:
        (x0, y0), (x1, y1) = b.xy(legs[0]), b.xy(legs[3])
        b.items.append(f'<rect x="{x0 - 8}" y="{y0 - 8}" width="{x1 - x0 + 16}" height="{y1 - y0 + 16}" rx="4" '
                       f'fill="#30363d" stroke="#6e7681"/>'
                       f'<circle cx="{(x0 + x1) / 2}" cy="{(y0 + y1) / 2}" r="9" fill="#21262d" stroke="#8b949e"/>'
                       f'<text x="{(x0 + x1) / 2}" y="{y0 - 12}" text-anchor="middle" fill="#c9d1d9" font-size="9" '
                       f'font-family="Arial">{sa}</text>')
        for leg in legs:
            b.pad(leg, "#d4c044", 3.5)
        b.line(gnd_leg, f"O{hole_rc(gnd_leg)[1]}", gnd, 2, "2,2")
    for w in wires_of("harness"):
        b.line(w.panel, w.panel_to, COLORS[w.color][0], 2.5, "2,2")
        b.wire_end(w.panel, w)
    return b.svg("Карта панельной платы со стороны деталей")


def legend() -> str:
    items = []
    for key, (color, name) in SIG.items():
        dash = ' stroke-dasharray="4,2"' if key in ("p5", "gnd") else ""
        items.append(f'  <span><svg width="28" height="10"><line x1="0" y1="5" x2="28" y2="5" stroke="{color}" '
                     f'stroke-width="3"{dash}/></svg>{name}</span>')
    return '<div class="legend">\n' + "\n".join(items) + "\n</div>"


# ─────────────────────────── Страница ───────────────────────────

CSS = """
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { background: #0d1117; color: #e0e0e0; font-family: Arial, sans-serif;
         padding: 20px; font-size: 13px; }
  h1 { text-align: center; color: #58a6ff; font-size: 1.5em; margin-bottom: 4px; }
  .subtitle { text-align: center; color: #666; margin-bottom: 20px; font-size: 0.85em; }

  .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; align-items: start; }

  .device { border: 1px solid #30363d; border-radius: 10px; overflow: hidden; }
  .device-header { padding: 12px 16px; font-weight: bold; font-size: 1.1em;
                   text-align: center; letter-spacing: 1px; }
  .brain-hdr  { background: #0c2033; color: #58a6ff; border-bottom: 1px solid #1c4a7a; }
  .remote-hdr { background: #1a0c2e; color: #bc8cff; border-bottom: 1px solid #4a1c7a; }

  .section-title { padding: 5px 12px; font-weight: bold; font-size: 0.75em;
                   letter-spacing: 1px; text-transform: uppercase; }
  .s-nrf    { background: #1a1a00; color: #e8c84a; }
  .s-relay  { background: #200a0a; color: #ff6b6b; }
  .s-i2c    { background: #0a0a20; color: #79b8ff; }
  .s-adc    { background: #0a1a0a; color: #56d364; }
  .s-btn    { background: #1a0e00; color: #f0a000; }
  .s-led    { background: #1a001a; color: #d85cff; }
  .s-pwr    { background: #0a1a10; color: #3fb950; }
  .s-note   { background: #200000; color: #ff8888; }
  .s-ok     { background: #07251a; color: #56d364; }
  .s-wires  { background: #002030; color: #58a6ff; }

  table { width: 100%; border-collapse: collapse; }
  td { padding: 4px 12px; border-bottom: 1px solid #21262d; vertical-align: middle; }
  td:first-child { color: #8b949e; width: 34%; }
  td:nth-child(2){ font-weight: bold; color: #e0e0e0; width: 26%; font-family: monospace; font-size: 12px; }
  td:last-child  { color: #8b949e; font-size: 11px; }
  tr:last-child td { border-bottom: none; }
  .wn { color: #6e7681; font-size: 10px; font-family: monospace; }

  .pin-p5  { color: #ff6666 !important; }
  .pin-gnd { color: #a0a0a0 !important; }

  .notes-box { margin-top: 20px; border: 1px solid #30363d; border-radius: 10px; overflow: hidden; }
  .notes-box .section-title { font-size: 0.8em; }
  .note-item { padding: 6px 16px; border-bottom: 1px solid #21262d;
               font-size: 12px; display: flex; gap: 8px; }
  .note-item:last-child { border-bottom: none; }
  .note-icon { min-width: 20px; color: #f0a000; font-weight: bold; }
  .note-text { color: #c9d1d9; }
  .note-text strong { color: #ff8888; }
  .notes-box.ok .note-text strong { color: #56d364; }

  .bus-link { margin-top: 16px; text-align: center; padding: 10px;
              border: 1px dashed #1c4a2a; border-radius: 8px; color: #3fb950; font-size: 12px; }
  .bus-link span { font-weight: bold; }
  .bus-link .p5 { color: #ff6666; }
  .bus-link .gnd { color: #a0a0a0; }

  .wirelist { margin-top: 20px; border: 1px solid #30363d; border-radius: 10px; overflow: hidden; }
  .wirelist h3 { padding: 10px 16px; background: #002030; color: #58a6ff; font-size: 0.9em;
                 letter-spacing: 2px; font-weight: bold; }
  .wirelist th { background: #161b22; color: #8b949e; padding: 7px 10px;
                 font-size: 11px; text-align: left; border-bottom: 1px solid #30363d; }
  .wirelist td { padding: 5px 10px; font-size: 12px; border-bottom: 1px solid #21262d;
                 width: auto; font-weight: normal; }
  .wirelist td.num { width: 34px; color: #58a6ff; font-weight: bold; font-family: monospace; }
  .wirelist td.from, .wirelist td.to { font-family: monospace; color: #e0e0e0; }
  .wirelist td.color { width: 15%; color: #c9d1d9; font-family: Arial, sans-serif; }
  .wirelist td.len { width: 60px; color: #8b949e; font-family: Arial, sans-serif; }
  .wirelist td.note { color: #8b949e; font-size: 11px; }
  .wirelist tr.grp td { background: #0f1620; color: #79b8ff; font-size: 11px; font-weight: bold;
                        letter-spacing: 1px; text-transform: uppercase; }
  .stages td.stage { color: #e0e0e0; font-weight: bold; width: 21%; font-family: Arial, sans-serif; }
  .stages td.act { color: #c9d1d9; font-size: 12px; font-family: Arial, sans-serif; }
  .stages td.chk { color: #56d364; font-size: 11px; width: 26%; }

  .color-swatch { display: inline-block; width: 14px; height: 14px;
                  border-radius: 3px; vertical-align: middle; margin-right: 6px;
                  border: 1px solid #666; }

  .page { page-break-before: always; padding-top: 20px; }
  .page h1 { font-size: 1.4em; }
  .legend { display: flex; gap: 14px; flex-wrap: wrap; justify-content: center; margin-bottom: 18px; }
  .legend span { display: flex; align-items: center; gap: 5px; font-size: 11px; }
  .scheme { width: 100%; max-width: 1060px; display: block; margin: 0 auto; background: #0d1117;
            border-radius: 10px; border: 1px solid #21262d; }
  .boards { display: grid; grid-template-columns: 1.35fr 1fr; gap: 20px; margin-top: 22px; align-items: start; }
  .board-card { border: 1px solid #30363d; border-radius: 10px; overflow: hidden; }
  .board-card .device-header { font-size: 0.95em; }
  .board { width: 100%; display: block; background: #0d1117; }
  .board-note { padding: 8px 14px; color: #8b949e; font-size: 11px; border-top: 1px solid #21262d; }
  .footer { text-align: center; color: #444; font-size: 11px; margin-top: 14px; }

  @media (max-width: 900px) {
    .grid, .boards { grid-template-columns: 1fr; }
    .wirelist { overflow-x: auto; }
  }
  @media print {
    body { background: white; color: #111; padding: 10px; font-size: 11px; }
    .device, .notes-box, .wirelist, .board-card { border: 1px solid #888; }
    .device-header, .section-title, .wirelist h3, .wirelist th, .wirelist tr.grp td {
      color: #000 !important; background: #ddd !important; }
    td:first-child, td:last-child { color: #555; }
    td:nth-child(2), .wirelist td.from, .wirelist td.to, .stages td.stage, .note-text { color: #000 !important; }
    .note-text strong { color: #c00 !important; }
  }
"""


def build() -> str:
    assert len(WIRES) == 26 and [w.n for w in WIRES] == list(range(1, 27))
    holes = [w.hole for w in WIRES]
    assert len(holes) == len(set(holes)), "два провода в одном отверстии основной платы"

    return f"""<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Устройство контроля ГСМ — Схема подключения и монтаж</title>
<style>{CSS}</style>
</head>
<body>

<h1>⚡ Устройство контроля ГСМ — Схема подключения и монтажная ведомость</h1>
<div class="subtitle">Arduino Nano · LCD-2004 I²C · MQ-2 · DHT11 · HC-SR04 · 4 светодиода · 3 кнопки ·
зуммер SFM-27 · питание USB Type-C 5 В · 26 проводов</div>

<div class="grid">
{main_board_card()}
{panel_board_card()}
</div><!-- /grid -->

<div class="bus-link">
  ⏚ Шины основной платы: <span class="p5">+5 В — столбец 24</span> · <span class="gnd">GND — столбец 21</span> ·
  угол <span>A1</span> отмечен маркером · все координаты — <span>со стороны деталей</span>
</div>

{notes_box("⚠ Важные замечания перед сборкой", NOTES)}

<!-- ═══════════════ СТРАНИЦА 2 — ПОРЯДОК СБОРКИ И ВЕДОМОСТЬ ═══════════════ -->
<div class="page">
{stages_box()}

{wirelist()}

<div class="footer">Соедините всё по этой ведомости — и схема заработает с первого раза.
Сверяйте каждый провод после пайки тестером (звонок).</div>
</div>

<!-- ═══════════════ СТРАНИЦА 3 — ГРАФИЧЕСКИЕ СХЕМЫ ═══════════════ -->
<div class="page">
<h1>Графические схемы подключения</h1>
<div class="subtitle">Устройство контроля ГСМ · Arduino Nano · номера на линиях — номера проводов из ведомости</div>
{legend()}
<div style="overflow-x:auto;">
{functional_svg()}
</div>

<div class="boards">
  <div class="board-card">
    <div class="device-header brain-hdr">🧠 Основная плата 5×7 см — вид со стороны деталей</div>
{main_board_svg()}
    <div class="board-note">Кружок с номером — конец провода из ведомости. Точечная линия — конец отогнут снизу
    к выводу Nano. Пунктир — шины +5 В и GND (лужёный провод снизу). Красное кольцо — угол A1.</div>
  </div>
  <div class="board-card">
    <div class="device-header remote-hdr">🎛 Панельная плата — вид со стороны деталей</div>
{panel_board_svg()}
    <div class="board-note">Провода жгута 1–8 входят сверху и снизу отгибаются: J → I (к резистору), K → L (к кнопке),
    P9 → O9 (к шине). Пунктир — общий провод GND.</div>
  </div>
</div>

{notes_box("✅ Перед первым включением", CHECKLIST, "s-ok", "ok")}
</div>

</body>
</html>
"""


if __name__ == "__main__":
    OUT.write_text(build(), encoding="utf-8")
    print(f"Готово: {OUT}")

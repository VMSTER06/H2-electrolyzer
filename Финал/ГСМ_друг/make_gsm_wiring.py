"""Генератор поэтапной паяльной схемы «Устройство контроля ГСМ».

Структура как в исходном PDF: этап 0 — как читать схему, этапы 1–15 — по
одному на блок, с картой платы (ярко — что делаем сейчас, бледно — сделано
раньше) и нумерованными шагами с проверкой. Оформление — тёмная тема
H2_Wiring_Full.html. В конце справочник: монтажная ведомость и
функциональная схема. Всё строится из одних данных ниже.

Запуск:  python3 make_gsm_wiring.py   →   ГСМ_схема_подключения.html
"""
from dataclasses import dataclass, field
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

# Цвета типов сигналов (легенда функциональной схемы, как в H2_Wiring_Full).
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
P5, GND = SIG["p5"][0], SIG["gnd"][0]
PAD_COLOR = "#d4c044"


@dataclass(frozen=True)
class Group:
    key: str
    stage: int      # этап, на котором паяются провода группы
    icon: str
    module: str     # название модуля с обозначением
    length: str     # длина проводов
    where: str      # где модуль стоит в корпусе
    note: str = ""  # пояснение под картой (как внизу страницы PDF)

    @property
    def title(self) -> str:
        return f"{self.icon} {self.module}"


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
    Group("harness", 8, "🔗", "Жгут: основная плата ↔ панельная", "20 см, МГТФ",
          "скрутить в жгут"),
    Group("lcd", 9, "🖥", "Дисплей LCD-2004B-IIC (DA1)", "20 см", "дисплей стоит на крышке",
          "Разъём платы I²C — сзади дисплея. Порядок выводов проверьте по надписям на ней."),
    Group("mq2", 10, "📊", "Датчик газа MQ-2 (DD1)", "12 см", "стоит у задней стенки",
          "DO не подключается: нужен только аналоговый выход AO."),
    Group("dht", 11, "🌡", "Датчик температуры DHT11 (DD2)", "12 см", "у задней стенки",
          "У разных модулей порядок выводов разный — смотрите надписи S / + / − на плате модуля."),
    Group("sr04", 12, "📏", "Датчик расстояния HC-SR04 (DD3)", "10 см", "стоит на дне",
          "Выводы HC-SR04 смотрят к основной плате, провода короткие."),
    Group("usb", 13, "🔋", "Разъём питания USB Type-C (XP1)", "12 см", "правая стенка",
          "Используются только VBUS (+5 В) и GND, остальные выводы переходника свободны."),
    Group("buzz", 14, "🔊", "Зуммер SFM-27 (BQ1)", "12 см", "передняя стенка",
          "Зуммер полярный: красный вывод — плюс. Свои провода зуммера можно нарастить, спайку изолировать."),
]
GROUP = {g.key: g for g in GROUPS}

WIRES = [
    Wire(1, "harness", "D8", "I6", "green", "led", nano="D8", panel="J3", panel_to="I3",
         note="к R1, светодиод VD1 «OK»"),
    Wire(2, "harness", "D10", "I4", "yellow", "led", nano="D10", panel="J7", panel_to="I7",
         note="R2, VD2 «LOW»"),
    Wire(3, "harness", "D9", "I5", "red", "led", nano="D9", panel="J11", panel_to="I11",
         note="R3, VD3 «GAS»"),
    Wire(4, "harness", "D12", "I2", "darkred", "led", nano="D12", panel="J15", panel_to="I15",
         note="R4, VD4 «TEMP»"),
    Wire(5, "harness", "D7", "I7", "blue", "btn", nano="D7", panel="K6", panel_to="L6",
         note="кнопка SA1"),
    Wire(6, "harness", "D5", "I9", "lightblue", "btn", nano="D5", panel="K12", panel_to="L12",
         note="кнопка SA2"),
    Wire(7, "harness", "D6", "I8", "cyan", "btn", nano="D6", panel="K18", panel_to="L18",
         note="кнопка SA3"),
    Wire(8, "harness", "GND", "Q21", "black", "gnd", bus="GND", panel="P9", panel_to="O9",
         note="GND панели"),

    Wire(9, "lcd", "GND", "M21", "black", "gnd", bus="GND"),
    Wire(10, "lcd", "VCC", "L24", "red", "p5", bus="+5 В"),
    Wire(11, "lcd", "SDA", "A9", "pink", "i2c", nano="A4"),
    Wire(12, "lcd", "SCL", "A10", "purple", "i2c", nano="A5"),

    Wire(13, "mq2", "VCC", "E24", "red", "p5", bus="+5 В"),
    Wire(14, "mq2", "GND", "F21", "black", "gnd", bus="GND"),
    Wire(15, "mq2", "AO", "A5", "cyan", "adc", nano="A0"),

    Wire(16, "dht", "DATA (S)", "I12", "brown", "dig", nano="D2",
         note="Не замкните с I13 (там GND)!"),
    Wire(17, "dht", "VCC (+)", "G24", "red", "p5", bus="+5 В"),
    Wire(18, "dht", "GND (−)", "H21", "black", "gnd", bus="GND"),

    Wire(19, "sr04", "VCC", "J24", "red", "p5", bus="+5 В"),
    Wire(20, "sr04", "Trig", "I11", "orange", "dig", nano="D3"),
    Wire(21, "sr04", "Echo", "I10", "yellow", "dig", nano="D4"),
    Wire(22, "sr04", "GND", "K21", "black", "gnd", bus="GND"),

    Wire(23, "usb", "VBUS (+5 В)", "C24", "red", "p5", bus="+5 В", note="Красный провод."),
    Wire(24, "usb", "GND", "D21", "black", "gnd", bus="GND", note="Чёрный провод."),

    Wire(25, "buzz", "+ (красный)", "I3", "red", "snd", nano="D11"),
    Wire(26, "buzz", "− (чёрный)", "O21", "black", "gnd", bus="GND"),
]

# Выводы модулей, которые не подключаются.
UNUSED = {"mq2": [("DO", "оставить свободным")]}

# Панельная плата.
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
PANEL_GND = [("B4", "B16"), ("B9", "O9"), ("O1", "O18")]


@dataclass(frozen=True)
class Stage:
    n: int
    title: str
    where: str                  # подзаголовок справа в шапке
    views: tuple[str, ...]      # какие карты показать
    steps: tuple[str, ...] = ()
    check: str = ""
    group: str = ""             # для этапов с проводами: группа из GROUPS
    note: str = ""


def wire_step(w: Wire) -> str:
    """Шаг «вывод → отверстие» для провода модуля."""
    if w.nano:
        text = (f"<b>{w.pin}</b> → основная <b>{w.hole}</b>; снизу отогнуть к выводу "
                f"{w.nano} Nano ({NANO_HOLE[w.nano]}) и пропаять вместе.")
    else:
        text = f"<b>{w.pin}</b> → основная <b>{w.hole}</b> (шина {w.bus})."
    return f"{text} {w.note}".strip()


def harness_step(w: Wire) -> str:
    src = f"{w.hole} (вывод {w.nano})" if w.nano else f"{w.hole} (шина GND)"
    return f"Основная <b>{src}</b> → панель <b>{w.panel}</b> ({w.note})."


def wires_of(group: str) -> list[Wire]:
    return [w for w in WIRES if w.group == group]


def module_stage(g: Group, extra: tuple[str, ...], check: str) -> Stage:
    steps = tuple(wire_step(w) for w in wires_of(g.key))
    steps += tuple(f"<b>{pin}</b> — {text}." for pin, text in UNUSED.get(g.key, []))
    count = len(wires_of(g.key))
    return Stage(g.stage, g.module, f"{count} провода по {g.length} · {g.where}", ("main",),
                 steps + extra, check, g.key, g.note)


STAGES = [
    Stage(0, "Как читать эту схему", "сначала прочитайте этот этап", ("main", "panel"), (
        "Отверстие называется буквой ряда и номером столбца: <b>I6</b> — ряд I, столбец 6.",
        "Перед началом отметьте маркером <b>угол A1</b> на обеих платах С ДВУХ сторон. "
        "Ориентируйтесь только по этой метке.",
        "«Вид со стороны деталей» — плата лежит деталями вверх. «Вид со стороны пайки» — перевёрнута: "
        "столбцы идут ЗЕРКАЛЬНО, столбец 1 оказывается справа.",
        "<b>Перемычка</b> — конец провода или вывода детали, отогнутый с нижней стороны к соседнему "
        "отверстию и пропаянный вместе с ним одной каплей припоя.",
        "<b>Шина</b> — лужёный провод, уложенный снизу вдоль ряда отверстий и припаянный к пятачкам "
        "через каждые 3–4 отверстия.",
        "<b>Внешний провод:</b> зачистить и залудить 3–4 мм, вставить СВЕРХУ в отверстие, снизу отогнуть "
        "к указанному выводу и пропаять вместе.",
        "<b>Ярко</b> на каждом этапе выделено то, что делается на этом этапе. <b>Бледно</b> — уже "
        "сделанное раньше.",
        "После каждого этапа выполняйте проверку из последнего пункта. Пропущенная ошибка на раннем "
        "этапе дорого стоит потом.",
        "Паяльник 25–40 Вт, 1–2 секунды на точку, припой должен лечь блестящим конусом.",
    ), note="Основная плата 5×7 см — 24 столбца, ряды A–R. Панельная плата — 18 столбцов, ряды A–X."),

    Stage(1, "Arduino Nano на основную плату", "основная плата · детали сверху, пайка снизу",
          ("main", "main_solder"), (
              "Отметьте маркером угол A1 с обеих сторон основной платы.",
              "Вставьте Nano штырьками в плату со стороны деталей: верхний ряд в <b>B2–B16</b> "
              "(D13 → B2 … 5V → B13, GND → B15, VIN → B16), нижний в <b>H2–H16</b> (D12 → H2 … D2 → H12, "
              "GND → H13, TX1 → H16). Разъём USB — к краю со столбцом 1. Сверьте надписи на самой Nano!",
              "Прижмите Nano, переверните плату и припаяйте два крайних вывода по диагонали: "
              "<b>B2 и H16</b> (на карте пайки обведены).",
              "Проверьте, что Nano лежит ровно и плотно. Если перекосилась — прогрейте одну из двух "
              "точек и поправьте.",
              "Припаяйте остальные 28 выводов рядов B и H.",
          ), "на каждом выводе блестящий конус, между соседними выводами нет перемычек из припоя."),

    Stage(2, "Шина +5 В и перемычка от вывода 5V", "основная плата · вид со стороны пайки",
          ("main_solder",), (
              "Отрежьте лужёный провод ≈ 5 см, выпрямите его (потяните плоскогубцами).",
              "С нижней стороны уложите его вдоль столбца 24 от <b>A24 до R24</b>. Концы вставьте в A24 "
              "и R24 и загните. Припаяйте к пятачкам A24, E24, J24, N24, R24.",
              "Перемычка 5V: отрезок лужёного провода ≈ 3 см. Один конец пропаяйте вместе с выводом "
              "5V Nano (<b>B13</b>) через соседнее отверстие A13.",
              "Уложите этот провод вдоль ряда A от <b>A13 до A24</b> и припаяйте к шине в A24. "
              "Отверстия A5, A9, A10 оставьте свободными — туда пойдут провода датчиков.",
          ), "вывод 5V Nano (B13) звонится с R24."),

    Stage(3, "Шина GND и перемычка от вывода GND", "основная плата · вид со стороны пайки",
          ("main_solder",), (
              "Лужёный провод ≈ 4,5 см уложите снизу вдоль столбца 21 от <b>C21 до R21</b> "
              "(НЕ до A21 — по ряду A уже идёт +5 В). Припаяйте к C21, H21, M21, R21.",
              "Перемычка GND: отрезок ≈ 3 см. Один конец пропаяйте вместе с выводом GND Nano "
              "(<b>H13</b>) через соседнее отверстие I13.",
              "Уложите его вдоль ряда I от <b>I13 до I21</b> и припаяйте к шине в I21. Осторожно: "
              "I12 рядом — туда позже пойдёт сигнал D2, не залейте его припоем.",
              "Проверка: вывод GND Nano (H13) звонится с R21.",
          ), "между R24 (+5 В) и R21 (GND) НЕТ короткого замыкания — мультиметр показывает сотни Ом "
             "и больше, но не 0."),

    Stage(4, "Светодиоды VD1–VD4", "панельная плата", ("panel", "panel_solder"), (
        "Отметьте угол A1 на панельной плате с двух сторон.",
        "VD1 (зелёный): длинная ножка (анод, +) в <b>C3</b>, короткая (катод, −) в <b>C4</b>.",
        "VD2 (жёлтый): анод в <b>C7</b>, катод в <b>C8</b>.",
        "VD3 (красный): анод в <b>C11</b>, катод в <b>C12</b>. VD4 (красный): анод в <b>C15</b>, "
        "катод в <b>C16</b>.",
        "Прижмите все светодиоды ВПЛОТНУЮ к плате — их верх должен войти в отверстия крышки. "
        "Припаяйте 8 ножек, лишнее пока не откусывайте.",
    ), "мультиметр в режиме «диод», красный щуп на анод, чёрный на катод — светодиод слабо светится."),

    Stage(5, "Резисторы R1–R4", "панельная плата", ("panel", "panel_solder"), (
        "Согните выводы резисторов 220 Ом под шаг 5 отверстий (12,7 мм).",
        "R1 — в <b>D3 и I3</b>, R2 — в <b>D7 и I7</b>, R3 — в <b>D11 и I11</b>, R4 — в <b>D15 и I15</b>. "
        "Резисторы лежат плашмя, вплотную к плате.",
        "Снизу отогните длинную ножку светодиода (анод) к верхнему выводу резистора и пропаяйте "
        "вместе: C3–D3, C7–D7, C11–D11, C15–D15.",
        "Нижние выводы резисторов (I3, I7, I11, I15) просто припаяйте, не обрезая, — к ним на этапе 8 "
        "придут провода.",
    ), "от I3 через R1 к аноду VD1 — 220 Ом (то же для R2–R4)."),

    Stage(6, "Кнопки SA1–SA3", "панельная плата", ("panel", "panel_solder"), (
        "SA1: ножки в <b>L1, L6, N1, N6</b>.",
        "SA2: ножки в <b>L7, L12, N7, N12</b>.",
        "SA3: ножки в <b>L13, L18, N13, N18</b>.",
        "Вставляйте до упора (до щелчка), кнопка должна стоять ровно. Припаяйте все 12 ножек.",
        "Неважно, какой стороной повернуть кнопку: сигнал берётся с ножки L, GND — с ножки N "
        "по диагонали, это работает при любом повороте.",
    ), "все кнопки стоят ровно, 12 ножек пропаяны."),

    Stage(7, "Общий провод (GND) панели", "панельная плата", ("panel", "panel_solder"), (
        "Лужёный провод снизу вдоль ряда B от <b>B4 до B16</b>.",
        "Короткие ножки светодиодов (катоды C4, C8, C12, C16) отогните к ряду B и припаяйте к этому проводу.",
        "Лужёный провод вдоль столбца 9 от <b>B9 до O9</b> (соединяет верх и низ платы).",
        "Лужёный провод вдоль ряда O от <b>O1 до O18</b>.",
        "Ножки кнопок N1, N7, N13 отогните к ряду O и припаяйте. Ножки N6, N12, N18 к шине НЕ отгибайте.",
        "Откусите лишние концы выводов (≈ 1,5 мм над пайкой).",
    ), "L6–O1 звонится только при нажатой SA1; L12–O7 — при SA2; L18–O13 — при SA3."),

    Stage(8, "8 проводов: основная плата ↔ панельная", "провода МГТФ по 20 см, скрутить в жгут",
          ("main", "panel"),
          tuple(harness_step(w) for w in wires_of("harness")) + (
              "Как паять: на основной плате конец провода снизу отогнуть к выводу Nano в ряду H "
              "(I6 → H6 и т. д.); Q21 — прямо к шине. На панели: J → I (к выводу резистора), "
              "K6 → L6, K12 → L12, K18 → L18, P9 → O9.",
              "Подпишите концы проводов малярным скотчем.",
          ), "прозвоните каждый провод от вывода Nano до точки на панели.", "harness",
          "Номера в кружках на картах — номера проводов из таблицы выше."),

    module_stage(GROUP["lcd"], ("Провода паять прямо на штырьки платы I²C, на каждую пайку — термоусадка.",),
                 "A9 звонится с A4 Nano, A10 — с A5, VCC — с R24, GND — с R21."),
    module_stage(GROUP["mq2"], (), "AO звонится с A0 Nano."),
    module_stage(GROUP["dht"], (), "DATA звонится с D2 Nano и НЕ звонится с GND."),
    module_stage(GROUP["sr04"], (), "Trig звонится с D3, Echo — с D4, Trig и Echo не замкнуты между собой."),
    module_stage(GROUP["usb"], (), "VBUS звонится с R24, GND — с R21, между ними НЕТ замыкания."),
    module_stage(GROUP["buzz"], (), "красный провод звонится с D11 Nano."),

    Stage(15, "Проверка перед первым включением", "основная и панельная платы", ("main", "panel"), (
        "Осмотр с лупой: нет капель припоя между соседними выводами Nano и в рядах A, I.",
        "<b>R24–R21:</b> нет короткого замыкания (не 0 Ом).",
        "Прозвонить каждый провод из ведомости (в конце документа): вывод модуля ↔ нужный вывод Nano или шина.",
        "Светодиоды: «диод» — красный щуп на J3 (J7, J11, J15), чёрный на P9 — светодиод слабо светится.",
        "Кнопки: K6–P9 звонится только при нажатой SA1 (K12 — SA2, K18 — SA3).",
        "Первое включение — от USB-C (XP1), без прошивки. На R24 должно быть <b>4,7–5,2 В</b>. "
        "Что-то греется или пахнет — сразу отключить.",
        "MQ-2 тёплый — это норма (внутри нагреватель), показания стабилизируются через 1–2 мин.",
    ), "прошить gsm_control_lcd2004.ino. Пустой экран при горящей подсветке — покрутить подстроечник "
       "контраста на плате I²C; ничего — сменить адрес LCD_ADDR на 0x3F."),
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


# ─────────────────────────── Карты плат ───────────────────────────

STEP, PAD = 20, 34


@dataclass
class Item:
    stage: int
    svg: str
    dim: bool = True        # бледнеет на следующих этапах (подписи — нет)
    only_cur: bool = False  # видно только на своём этапе (подсказки)


@dataclass
class BoardMap:
    """Карта макетной платы. mirror=True — вид со стороны пайки."""
    cols: int
    rows: int
    mirror: bool = False
    items: list[Item] = field(default_factory=list)

    def col_x(self, c: int) -> float:
        return PAD + ((self.cols - c) if self.mirror else (c - 1)) * STEP

    def xy(self, hole: str) -> tuple[float, float]:
        r, c = hole_rc(hole)
        return self.col_x(c), PAD + r * STEP

    def size(self) -> tuple[int, int]:
        return PAD * 2 + (self.cols - 1) * STEP, PAD * 2 + (self.rows - 1) * STEP

    def add(self, stage: int, svg: str, dim: bool = True, only_cur: bool = False) -> None:
        self.items.append(Item(stage, svg, dim, only_cur))

    # Примитивы ------------------------------------------------------------

    def line(self, stage: int, a: str, b: str, color: str, width: float = 3, dash: str = "") -> None:
        (x1, y1), (x2, y2) = self.xy(a), self.xy(b)
        d = f' stroke-dasharray="{dash}"' if dash else ""
        self.add(stage, f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{color}" '
                        f'stroke-width="{width}" stroke-linecap="round"{d}/>')

    def path(self, stage: int, holes: list[str], color: str, width: float = 3, dash: str = "") -> None:
        for a, b in zip(holes, holes[1:]):
            self.line(stage, a, b, color, width, dash)

    def pad(self, stage: int, hole: str, color: str = PAD_COLOR, r: float = 5) -> None:
        x, y = self.xy(hole)
        self.add(stage, f'<circle cx="{x}" cy="{y}" r="{r}" fill="{color}" stroke="#0d1117"/>')

    def ring(self, stage: int, hole: str) -> None:
        x, y = self.xy(hole)
        self.add(stage, f'<circle cx="{x}" cy="{y}" r="10" fill="none" stroke="#ff9f1c" stroke-width="2.5"/>',
                 only_cur=True)

    def label(self, stage: int, hole: str, text: str, dx: float = 0, dy: float = 0,
              color: str = "#c9d1d9", size: int = 8) -> None:
        x, y = self.xy(hole)
        self.add(stage, f'<text x="{x + dx}" y="{y + dy}" text-anchor="middle" fill="{color}" '
                        f'font-size="{size}" font-family="monospace">{text}</text>', dim=False)

    def rect(self, stage: int, a: str, b: str, margin: float, attrs: str) -> str:
        (x1, y1), (x2, y2) = self.xy(a), self.xy(b)
        x, y = min(x1, x2) - margin, min(y1, y2) - margin
        svg = (f'<rect x="{x}" y="{y}" width="{abs(x2 - x1) + 2 * margin}" '
               f'height="{abs(y2 - y1) + 2 * margin}" {attrs}/>')
        self.add(stage, svg)
        return svg

    def wire_end(self, stage: int, hole: str, w: Wire) -> None:
        x, y = self.xy(hole)
        fill = COLORS[w.color][0]
        self.add(stage, f'<circle cx="{x}" cy="{y}" r="8" fill="{fill}" stroke="#e0e0e0" stroke-width="1.5"/>'
                        f'<text x="{x}" y="{y + 3}" text-anchor="middle" fill="{text_on(fill)}" font-size="8" '
                        f'font-weight="bold" font-family="Arial">{w.n}</text>')

    # Отрисовка ------------------------------------------------------------

    def grid(self) -> str:
        w, h = self.size()
        out = [f'<rect x="{PAD - 14}" y="{PAD - 14}" width="{w - 2 * PAD + 28}" height="{h - 2 * PAD + 28}" '
               f'rx="6" fill="#161b22" stroke="#30363d"/>',
               f'<rect x="{PAD - STEP / 2}" y="{PAD - STEP / 2}" width="{self.cols * STEP}" '
               f'height="{self.rows * STEP}" fill="url(#holes)"/>']
        for c in range(1, self.cols + 1):
            out.append(f'<text x="{self.col_x(c)}" y="{PAD - 18}" text-anchor="middle" fill="#6e7681" '
                       f'font-size="9" font-family="monospace">{c}</text>')
        for r in range(self.rows):
            y = PAD + r * STEP + 3
            for x in (PAD - 24, w - PAD + 24):
                out.append(f'<text x="{x}" y="{y}" text-anchor="middle" fill="#6e7681" font-size="9" '
                           f'font-family="monospace">{chr(65 + r)}</text>')
        x, y = self.xy("A1")
        out.append(f'<circle cx="{x}" cy="{y}" r="7" fill="none" stroke="#ff5577" stroke-width="2"/>')
        return "\n".join(out)

    def svg(self, current: int, label: str) -> str:
        layers = {"done": [], "keep": [], "cur": []}
        for it in self.items:
            if it.stage > current or (it.only_cur and it.stage != current):
                continue
            layer = "cur" if it.stage == current else ("done" if it.dim else "keep")
            layers[layer].append(it.svg)
        w, h = self.size()
        body = [self.grid()]
        if layers["done"]:
            body.append('<g class="done">' + "".join(layers["done"]) + "</g>")
        body += layers["keep"]
        if layers["cur"]:
            body.append('<g class="cur">' + "".join(layers["cur"]) + "</g>")
        return (f'<svg viewBox="0 0 {w} {h}" class="board" xmlns="http://www.w3.org/2000/svg" role="img" '
                f'aria-label="{label}">\n' + "\n".join(body) + "\n</svg>")


def build_main(mirror: bool) -> BoardMap:
    b = BoardMap(24, 18, mirror)
    under = "" if mirror else "6,3"   # шины снизу: на виде сверху — пунктир

    # Этап 1 — Arduino Nano.
    if mirror:
        b.rect(1, "B2", "H16", 10, 'rx="5" fill="none" stroke="#2ea043" stroke-width="1.2" stroke-dasharray="4,3"')
    else:
        b.rect(1, "B2", "H16", 10, 'rx="5" fill="#0c1a10" stroke="#2ea043" stroke-width="1.5"')
        ux, uy = b.xy("E1")
        b.add(1, f'<rect x="{ux - 11}" y="{uy - 9}" width="22" height="18" rx="2" fill="#30363d" stroke="#6e7681"/>'
                 f'<text x="{ux}" y="{uy + 3}" text-anchor="middle" fill="#c9d1d9" font-size="7" '
                 f'font-family="Arial">USB</text>', dim=False)
        b.label(1, "E9", "Arduino Nano", dy=4, color="#3fb950", size=13)
    for pins, row, dy in ((NANO_TOP, "B", 14), (NANO_BOT, "H", -9)):
        for i, pin in enumerate(pins):
            hole = f"{row}{i + 2}"
            b.pad(1, hole)
            b.label(1, hole, pin, dy=dy, size=7)
    if mirror:
        b.ring(1, "B2")
        b.ring(1, "H16")

    # Этап 2 — шина +5 В, этап 3 — шина GND.
    b.line(2, "A24", "R24", P5, 4, under)
    b.path(2, ["B13", "A13", "A24"], P5, 3, under)
    b.line(3, "C21", "R21", GND, 4, under)
    b.path(3, ["H13", "I13", "I21"], GND, 3, under)
    if mirror:
        for hole in ("A24", "E24", "J24", "N24", "R24"):
            b.pad(2, hole, P5, 4)
        for hole in ("C21", "H21", "M21", "R21"):
            b.pad(3, hole, "#bbbbbb", 4)
    b.label(2, "R24", "+5 В", dy=26, color=P5, size=10)
    b.label(3, "R21", "GND", dy=26, color=GND, size=10)

    # Этапы 8–14 — внешние провода.
    for w in WIRES:
        stage = GROUP[w.group].stage
        if w.nano:
            b.line(stage, w.hole, NANO_HOLE[w.nano], COLORS[w.color][0], 2.5, "2,2")
        b.wire_end(stage, w.hole, w)
    return b


def build_panel(mirror: bool) -> BoardMap:
    b = BoardMap(18, 24, mirror)
    under = "" if mirror else "6,3"

    for vd, color, anode, cathode, r, r_top, r_bot, label in PANEL_LEDS:
        # Этап 4 — светодиод.
        (xa, ya), (xc, _) = b.xy(anode), b.xy(cathode)
        if mirror:
            b.pad(4, anode, COLORS[color][0], 5)
            b.pad(4, cathode, COLORS[color][0], 5)
        else:
            b.add(4, f'<ellipse cx="{(xa + xc) / 2}" cy="{ya}" rx="15" ry="9" fill="{COLORS[color][0]}" '
                     f'stroke="#e0e0e0" stroke-width="1"/>')
            b.label(4, anode, f"{vd} {label}", dx=(xc - xa) / 2, dy=-2 * STEP + 3)
        # Этап 5 — резистор и отгиб анода.
        (xt, yt), (_, yb) = b.xy(r_top), b.xy(r_bot)
        if mirror:
            b.pad(5, r_top, PAD_COLOR, 5)
            b.pad(5, r_bot, PAD_COLOR, 5)
            b.line(5, anode, r_top, "#ff9f1c", 3)
        else:
            b.add(5, f'<line x1="{xt}" y1="{yt}" x2="{xt}" y2="{yb}" stroke="#c9d1d9" stroke-width="1.5"/>'
                     f'<rect x="{xt - 6}" y="{yt + 22}" width="12" height="{yb - yt - 44}" rx="3" '
                     f'fill="#c8b48a" stroke="#0d1117"/>')
            b.pad(5, r_top, PAD_COLOR, 3.5)
            b.pad(5, r_bot, PAD_COLOR, 3.5)
            b.label(5, r_top, r, dx=14, dy=(yb - yt) / 2 + 3)
        # Этап 7 — катод к шине ряда B.
        b.line(7, cathode, f"B{hole_rc(cathode)[1]}", GND, 3 if mirror else 2, "" if mirror else "2,2")

    for sa, legs, sig, gnd_leg in PANEL_BUTTONS:
        # Этап 6 — кнопка.
        if mirror:
            for leg in legs:
                b.pad(6, leg, PAD_COLOR, 5)
        else:
            b.rect(6, legs[0], legs[3], 8, 'rx="4" fill="#30363d" stroke="#6e7681"')
            (x0, y0), (x1, y1) = b.xy(legs[0]), b.xy(legs[3])
            b.add(6, f'<circle cx="{(x0 + x1) / 2}" cy="{(y0 + y1) / 2}" r="9" fill="#21262d" stroke="#8b949e"/>')
            for leg in legs:
                b.pad(6, leg, PAD_COLOR, 3.5)
        b.label(6, legs[0], sa, dx=(b.xy(legs[1])[0] - b.xy(legs[0])[0]) / 2, dy=-12, size=9)
        # Этап 7 — ножка N к шине ряда O.
        b.line(7, gnd_leg, f"O{hole_rc(gnd_leg)[1]}", GND, 3 if mirror else 2, "" if mirror else "2,2")

    # Этап 7 — шины GND панели.
    for a, z in PANEL_GND:
        b.line(7, a, z, GND, 4, under)

    # Этап 8 — концы жгута.
    for w in wires_of("harness"):
        b.line(8, w.panel, w.panel_to, COLORS[w.color][0], 2.5, "2,2")
        b.wire_end(8, w.panel, w)
    return b


VIEWS = {
    "main": ("🧠 Основная плата 5×7 см — вид со стороны деталей", build_main(False), "brain-hdr"),
    "main_solder": ("🧠 Основная плата — вид со стороны пайки (зеркально, столбец 1 справа!)",
                    build_main(True), "solder-hdr"),
    "panel": ("🎛 Панельная плата — со стороны деталей", build_panel(False), "remote-hdr"),
    "panel_solder": ("🎛 Панельная плата — со стороны пайки (зеркально)", build_panel(True), "solder-hdr"),
}

# ─────────────────────────── HTML: этапы ───────────────────────────


def board_card(view: str, stage: int) -> str:
    title, board, css = VIEWS[view]
    return (f'<div class="board-card {view}">\n  <div class="device-header {css}">{title}</div>\n'
            f'{board.svg(stage, title)}\n</div>')


def steps_list(stage: Stage) -> str:
    items = [f'  <div class="note-item"><span class="note-icon">{i}</span>'
             f'<span class="note-text">{text}</span></div>' for i, text in enumerate(stage.steps, 1)]
    if stage.check:
        items.append(f'  <div class="note-item check"><span class="note-icon">✔</span>'
                     f'<span class="note-text"><b>Проверка:</b> {stage.check}</span></div>')
    head = "Правила" if stage.n == 0 else ("Чек-лист" if stage.n == len(STAGES) - 1 else "Порядок действий")
    return (f'<div class="notes-box">\n  <div class="section-title s-wires">{head}</div>\n'
            + "\n".join(items) + "\n</div>")


def wires_table(group: str) -> str:
    g = GROUP[group]
    rows = []
    for w in wires_of(group):
        if group == "harness":
            dst = f"основная {w.hole} → панель {w.panel}"
        else:
            dst = f"{w.hole} → {w.nano} Nano" if w.nano else f"{w.hole} (шина {w.bus})"
        rows.append(f'    <tr><td class="num">{w.n}</td><td class="pin">{w.pin}</td>'
                    f'<td class="dst">{dst}</td><td>{swatch(w.color)}</td></tr>')
    for pin, text in UNUSED.get(group, []):
        rows.append(f'    <tr><td class="num">—</td><td class="pin">{pin}</td><td class="dst">не подключать</td>'
                    f'<td></td></tr>')
    first = "Вывод Nano" if group == "harness" else "Вывод"
    return (f'<div class="module-card">\n  <div class="section-title s-i2c">{g.title} · провода {g.length}</div>\n'
            f'  <table>\n    <tr><th>№</th><th>{first}</th><th>Куда</th><th>Цвет</th></tr>\n'
            + "\n".join(rows) + "\n  </table>\n</div>")


def stage_nav(n: int) -> str:
    last = len(STAGES) - 1
    prev = f'<a href="#stage-{n - 1}">← Этап {n - 1}</a>' if n > 0 else "<span></span>"
    nxt = f'<a href="#stage-{n + 1}">Этап {n + 1} →</a>' if n < last else '<a href="#spravka">Справочник →</a>'
    return f'<div class="stage-nav">{prev}<a href="#top">↑ Все этапы</a>{nxt}</div>'


def stage_block(s: Stage) -> str:
    last = len(STAGES) - 1
    side = [steps_list(s)]
    if s.group:
        side.insert(0, wires_table(s.group))
    if s.note:
        side.append(f'<div class="stage-note">{s.note}</div>')
    boards = "\n".join(board_card(v, s.n) for v in s.views)
    pair = " pair" if all(v.startswith("panel") for v in s.views) and len(s.views) == 2 else ""
    return f"""<section class="stage" id="stage-{s.n}">
<div class="stage-head">
  <div><div class="stage-num">Этап {s.n} из {last}</div><h2>{s.title}</h2></div>
  <div class="stage-where">{s.where}</div>
</div>
<div class="stage-body">
<div class="stage-boards{pair}">
{boards}
</div>
<div class="stage-side">
{chr(10).join(side)}
</div>
</div>
{stage_nav(s.n)}
</section>"""


def contents() -> str:
    links = "\n".join(f'  <a href="#stage-{s.n}"><span>{s.n}</span>{s.title}</a>' for s in STAGES)
    return f'<nav class="contents" aria-label="Этапы">\n{links}\n  <a href="#spravka"><span>📋</span>Справочник: ведомость и схема</a>\n</nav>'


def legend_stage() -> str:
    return ('<div class="legend">'
            '<span><i class="lg cur"></i>ярко — делаем на этом этапе</span>'
            '<span><i class="lg done"></i>бледно — сделано раньше</span>'
            '<span><i class="lg a1"></i>угол A1 (отметьте маркером)</span>'
            '<span><svg width="28" height="10"><line x1="0" y1="5" x2="28" y2="5" stroke="#888" stroke-width="3" '
            'stroke-dasharray="6,3"/></svg>шина снизу платы (на виде сверху)</span>'
            '<span><svg width="28" height="10"><line x1="0" y1="5" x2="28" y2="5" stroke="#d4c044" stroke-width="2.5" '
            'stroke-dasharray="2,2"/></svg>конец провода отогнут снизу</span></div>')


# ─────────────────────────── Справочник ───────────────────────────


def wirelist() -> str:
    rows = []
    for g in GROUPS:
        rows.append(f'    <tr class="grp"><td colspan="6">{g.title} · этап {g.stage} · {g.length}</td></tr>')
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


ROW_H, BOX_HDR, BOX_GAP, TOP = 22, 24, 18, 90


def functional_svg() -> str:
    """Nano в центре, слева модули основной платы, справа панель."""
    lx0, lx1, nx0, nx1, rx0, rx1 = 40, 250, 440, 600, 790, 1020
    out, nano_left, nano_right = [], [], []

    def pin_row(x_text: float, yy: float, text: str, anchor: str) -> str:
        return (f'<text x="{x_text}" y="{yy + 4}" text-anchor="{anchor}" fill="#c9d1d9" '
                f'font-size="11" font-family="monospace">{text}</text>')

    def mid_label(x0: float, x1: float, yy: float, text: str) -> str:
        return (f'<text x="{(x0 + x1) / 2}" y="{yy - 4}" text-anchor="middle" fill="#8b949e" '
                f'font-size="9" font-family="monospace">{text}</text>')

    def box(x0: float, x1: float, y: float, h: float, title: str, color: str) -> None:
        out.append(f'<rect x="{x0}" y="{y}" width="{x1 - x0}" height="{h}" rx="6" fill="#161b22" stroke="#30363d"/>')
        out.append(f'<text x="{x0 + 10}" y="{y + 16}" fill="{color}" font-size="11" font-weight="bold" '
                   f'font-family="Arial">{title}</text>')

    y = TOP
    for g in (g for g in GROUPS if g.key != "harness"):
        ws, unused = wires_of(g.key), UNUSED.get(g.key, [])
        h = BOX_HDR + ROW_H * (len(ws) + len(unused)) + 6
        box(lx0, lx1, y, h, g.module, "#58a6ff")
        yy = y + BOX_HDR + ROW_H / 2
        for w in ws:
            out.append(pin_row(lx1 - 8, yy, w.pin, "end"))
            color = SIG[w.sig][0]
            if w.nano or g.key == "usb":
                nano_pin = w.nano or ("5V" if w.bus == "+5 В" else "GND")
                dash = "" if w.nano else ' stroke-dasharray="5,3"'
                out.append(f'<line x1="{lx1}" y1="{yy}" x2="{nx0}" y2="{yy}" stroke="{color}" stroke-width="2"{dash}/>')
                out.append(mid_label(lx1, nx0, yy, f"№{w.n} · {w.hole}" if w.nano else f"№{w.n} · шина {w.bus}"))
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
        box(rx0, rx1, y, h, title, "#bc8cff")
        yy = y + BOX_HDR + ROW_H / 2
        for text, w in rows:
            out.append(pin_row(rx0 + 8, yy, text, "start"))
            color = SIG[w.sig][0]
            dash = ' stroke-dasharray="5,3"' if w.bus else ""
            out.append(f'<line x1="{nx1}" y1="{yy}" x2="{rx0}" y2="{yy}" stroke="{color}" stroke-width="2"{dash}/>')
            out.append(mid_label(nx1, rx0, yy, f"№{w.n} · {w.hole} → {w.panel}"))
            nano_right.append((yy, w.nano or "GND", color))
            yy += ROW_H
        y += h + BOX_GAP
    height = max(height, y + 10)

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


def legend_sig() -> str:
    items = []
    for key, (color, name) in SIG.items():
        dash = ' stroke-dasharray="4,2"' if key in ("p5", "gnd") else ""
        items.append(f'<span><svg width="28" height="10"><line x1="0" y1="5" x2="28" y2="5" stroke="{color}" '
                     f'stroke-width="3"{dash}/></svg>{name}</span>')
    return '<div class="legend">' + "".join(items) + "</div>"


# ─────────────────────────── Страница ───────────────────────────

DEFS = f"""<svg width="0" height="0" style="position:absolute" aria-hidden="true">
  <defs>
    <pattern id="holes" x="{PAD - STEP / 2}" y="{PAD - STEP / 2}" width="{STEP}" height="{STEP}"
             patternUnits="userSpaceOnUse"><circle cx="{STEP / 2}" cy="{STEP / 2}" r="2.2" fill="#30363d"/></pattern>
    <filter id="glow" x="-50%" y="-50%" width="200%" height="200%">
      <feDropShadow dx="0" dy="0" stdDeviation="2.2" flood-color="#ffb347" flood-opacity="0.95"/>
    </filter>
  </defs>
</svg>"""

CSS = """
  * { box-sizing: border-box; margin: 0; padding: 0; }
  html { scroll-behavior: smooth; }
  body { background: #0d1117; color: #e0e0e0; font-family: Arial, sans-serif;
         padding: 20px; font-size: 13px; }
  h1 { text-align: center; color: #58a6ff; font-size: 1.5em; margin-bottom: 4px; }
  .subtitle { text-align: center; color: #666; margin-bottom: 18px; font-size: 0.85em; }
  a { color: #58a6ff; text-decoration: none; }
  a:hover { text-decoration: underline; }
  a:focus-visible { outline: 2px solid #58a6ff; outline-offset: 2px; border-radius: 4px; }

  .contents { display: grid; grid-template-columns: repeat(auto-fill, minmax(250px, 1fr)); gap: 6px;
              max-width: 1300px; margin: 0 auto 14px; }
  .contents a { display: flex; gap: 10px; align-items: center; padding: 7px 12px; color: #c9d1d9;
                background: #161b22; border: 1px solid #30363d; border-radius: 8px; font-size: 12px; }
  .contents a:hover { border-color: #58a6ff; text-decoration: none; }
  .contents span { min-width: 26px; height: 26px; border-radius: 50%; background: #1f6feb; color: #fff;
                   font-weight: bold; display: flex; align-items: center; justify-content: center; }

  .legend { display: flex; gap: 16px; flex-wrap: wrap; justify-content: center; margin: 4px 0 18px;
            color: #8b949e; font-size: 11px; }
  .legend span { display: flex; align-items: center; gap: 6px; }
  .lg { display: inline-block; width: 14px; height: 14px; border-radius: 50%; }
  .lg.cur { background: #d4c044; box-shadow: 0 0 6px 2px #ffb347; }
  .lg.done { background: #d4c044; opacity: .3; }
  .lg.a1 { border: 2px solid #ff5577; }

  .stage { max-width: 1300px; margin: 0 auto 26px; border: 1px solid #30363d; border-radius: 10px;
           overflow: hidden; scroll-margin-top: 10px; }
  .stage-head { display: flex; justify-content: space-between; align-items: flex-end; gap: 16px;
                padding: 12px 18px; background: #0c2033; border-bottom: 1px solid #1c4a7a; }
  .stage-num { color: #f0a000; font-weight: bold; font-size: 12px; letter-spacing: 1px; }
  .stage-head h2 { color: #58a6ff; font-size: 1.3em; margin-top: 2px; }
  .stage-where { color: #8b949e; font-size: 12px; text-align: right; }
  .stage-body { display: grid; grid-template-columns: minmax(0, 1.25fr) minmax(0, 1fr); gap: 18px; padding: 16px; }
  .stage-boards { display: grid; gap: 16px; align-content: start; }
  .stage-boards.pair { grid-template-columns: 1fr 1fr; }
  .stage-side { display: grid; gap: 14px; align-content: start; }
  .stage-note { color: #f0a000; font-size: 12px; padding: 8px 12px; border: 1px dashed #4a3a00;
                border-radius: 8px; background: #1a1500; }
  .stage-nav { display: flex; justify-content: space-between; padding: 8px 18px; border-top: 1px solid #21262d;
               background: #0f1620; font-size: 12px; }

  .board-card { border: 1px solid #30363d; border-radius: 10px; overflow: hidden; }
  .stage-boards:not(.pair) .board-card.panel { max-width: 470px; }
  .device-header { padding: 8px 14px; font-weight: bold; font-size: 0.9em; text-align: center; letter-spacing: .5px; }
  .brain-hdr  { background: #0c2033; color: #58a6ff; border-bottom: 1px solid #1c4a7a; }
  .remote-hdr { background: #1a0c2e; color: #bc8cff; border-bottom: 1px solid #4a1c7a; }
  .solder-hdr { background: #1a1500; color: #e8c84a; border-bottom: 1px solid #4a3a00; }
  .board { width: 100%; display: block; background: #0d1117; }
  .board .done { opacity: .3; }
  .board .cur { filter: url(#glow); }

  .section-title { padding: 6px 12px; font-weight: bold; font-size: 0.75em;
                   letter-spacing: 1px; text-transform: uppercase; }
  .s-i2c   { background: #0a0a20; color: #79b8ff; }
  .s-wires { background: #002030; color: #58a6ff; }

  .notes-box, .module-card { border: 1px solid #30363d; border-radius: 10px; overflow: hidden; }
  .note-item { padding: 7px 14px; border-bottom: 1px solid #21262d; font-size: 12.5px; line-height: 1.45;
               display: flex; gap: 10px; }
  .note-item:last-child { border-bottom: none; }
  .note-icon { min-width: 22px; height: 22px; border-radius: 50%; background: #f0a000; color: #111;
               font-weight: bold; font-size: 11px; display: flex; align-items: center; justify-content: center; }
  .note-text { color: #c9d1d9; }
  .note-text b { color: #fff; }
  .note-item.check { background: #07251a; }
  .note-item.check .note-icon { background: #2ea043; color: #fff; }
  .note-item.check .note-text b { color: #56d364; }

  table { width: 100%; border-collapse: collapse; }
  th { background: #161b22; color: #8b949e; padding: 6px 10px; font-size: 11px; text-align: left;
       border-bottom: 1px solid #30363d; }
  td { padding: 5px 10px; border-bottom: 1px solid #21262d; font-size: 12px; color: #c9d1d9; }
  tr:last-child td { border-bottom: none; }
  td.num { width: 34px; color: #58a6ff; font-weight: bold; font-family: monospace; }
  td.pin, td.dst { font-family: monospace; color: #e0e0e0; }
  .color-swatch { display: inline-block; width: 14px; height: 14px; border-radius: 3px;
                  vertical-align: middle; margin-right: 6px; border: 1px solid #666; }

  .spravka { max-width: 1300px; margin: 30px auto 0; }
  .spravka > h1 { margin-top: 10px; }
  .wirelist { margin-top: 10px; border: 1px solid #30363d; border-radius: 10px; overflow: hidden; }
  .wirelist h3 { padding: 10px 16px; background: #002030; color: #58a6ff; font-size: 0.9em;
                 letter-spacing: 2px; font-weight: bold; }
  .wirelist td.from, .wirelist td.to { font-family: monospace; color: #e0e0e0; }
  .wirelist td.color { width: 15%; }
  .wirelist td.len { width: 60px; color: #8b949e; }
  .wirelist td.note { color: #8b949e; font-size: 11px; }
  .wirelist tr.grp td { background: #0f1620; color: #79b8ff; font-size: 11px; font-weight: bold;
                        letter-spacing: 1px; text-transform: uppercase; }
  .scheme { width: 100%; max-width: 1060px; display: block; margin: 0 auto; background: #0d1117;
            border-radius: 10px; border: 1px solid #21262d; }
  .footer { text-align: center; color: #444; font-size: 11px; margin: 14px 0; }

  @media (max-width: 900px) {
    body { padding: 12px; }
    .stage-body, .stage-boards.pair { grid-template-columns: 1fr; padding: 10px; }
    .stage-head { flex-direction: column; align-items: flex-start; }
    .stage-where { text-align: left; }
    .wirelist { overflow-x: auto; }
  }
  @media print {
    body { padding: 0; }
    .contents, .stage-nav { display: none; }
    .stage { page-break-before: always; border: none; margin: 0; }
    .spravka { page-break-before: always; }
  }
"""


def build() -> str:
    assert [w.n for w in WIRES] == list(range(1, 27)), "нумерация проводов 1–26"
    holes = [w.hole for w in WIRES]
    assert len(holes) == len(set(holes)), "два провода в одном отверстии основной платы"
    assert [s.n for s in STAGES] == list(range(len(STAGES))), "нумерация этапов"

    stages = "\n\n".join(stage_block(s) for s in STAGES)
    return f"""<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Устройство контроля ГСМ — Пайка по этапам</title>
<style>{CSS}</style>
</head>
<body>
{DEFS}
<h1 id="top">⚡ Устройство контроля ГСМ — паяльная схема по этапам</h1>
<div class="subtitle">Arduino Nano · LCD-2004 I²C · MQ-2 · DHT11 · HC-SR04 · 4 светодиода · 3 кнопки ·
зуммер SFM-27 · питание USB Type-C 5 В · {len(STAGES) - 1} этапов · 26 проводов</div>
{contents()}
{legend_stage()}

{stages}

<section class="spravka" id="spravka">
<h1>📋 Справочник</h1>
<div class="subtitle">Все провода одной таблицей и общая функциональная схема — для прозвонки на этапе 15</div>
{wirelist()}
<div class="footer">Сверяйте каждый провод после пайки тестером (звонок).</div>
{legend_sig()}
<div style="overflow-x:auto;">
{functional_svg()}
</div>
<div class="footer"><a href="#top">↑ К списку этапов</a></div>
</section>

</body>
</html>
"""


if __name__ == "__main__":
    OUT.write_text(build(), encoding="utf-8")
    print(f"Готово: {OUT} ({OUT.stat().st_size // 1024} КБ)")

import sys, copy, re
from docx import Document
from docx.shared import Mm, Pt
from docx.enum.section import WD_SECTION, WD_ORIENT
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_LINE_SPACING
from docx.oxml.ns import qn
from docx.oxml import OxmlElement
sys.stdout.reconfigure(encoding='utf-8')

SRC, DST, SHEET = sys.argv[1], sys.argv[2], sys.argv[3]
d = Document(SRC)
P = d.paragraphs

def set_text(p, text):
    """Replace paragraph text keeping the first run's formatting."""
    runs = p.runs
    if not runs:
        p.add_run(text); return
    runs[0].text = text
    for r in runs[1:]:
        r._r.getparent().remove(r._r)

def find(prefix):
    for p in d.paragraphs:
        if p.text.startswith(prefix):
            return p
    raise KeyError(prefix)

def sub(prefix, old, new, count=1):
    p = find(prefix)
    assert old in p.text, (prefix, old)
    set_text(p, p.text.replace(old, new, count))

def delete(p):
    p._p.getparent().remove(p._p)

def insert_after(p, text, style=None):
    new = copy.deepcopy(p._p)
    p._p.addnext(new)
    from docx.text.paragraph import Paragraph
    np_ = Paragraph(new, p._parent)
    set_text(np_, text)
    return np_

# ---------- Введение ----------
sub('Микропроцессорные устройства дистанционного управления', '[2, 3]', '[1, 2]')

# ---------- 1.2 ----------
sub('Микроконтроллер применяется в составе отладочной платы',
    'ширина 27,9 мм).', 'ширина 27,9 мм) [2, 5, 6].')
sub('Плата ESP32 DevKit v1 содержит модуль', '[16]', '[7]')
sub('Микроконтроллер ATmega328P выпускается', '[2].', '[3].')
sub('Микроконтроллер STM32F103C8T6 выпускается', '[3].', '[4].')
sub('Для получения более подробной информации о рассмотренных микроконтроллерах',
    '[1], [2], [3], [16].', '[1–7].')
# ---------- 1.3 ----------
sub('Модуль nRF24L01+ выпускается', '[4]', '[8]')
sub('Модуль HC-12 выпускается', '[5]', '[9]')
sub('Модуль RFM69 выпускается', '[6]', '[10]')
# ---------- 1.4 ----------
sub('В проекте рассматривается аналоговый преобразователь давления', '[7]', '[11]')
sub('Достоинствами датчиков данного типа', '[7]', '[11]')
# ---------- 1.5 ----------
sub('Датчик DS18B20 выпускается', '[8]', '[12]')
sub('NTC-термистор с коэффициентом', '[9]', '[13]')
sub('Для получения более подробной информации о рассмотренных датчиках температуры', '[8], [9].', '[12, 13].')
# ---------- 1.6 ----------
sub('Микросхема MPU-6050 выпускается', '[10]', '[14, 15]')
# ---------- 1.7 ----------
sub('OLED-дисплей на контроллере SSD1306', '[11]', '[16]')
sub('Дисплеи LCD1602 и LCD2004 представляют', 'к двум линиям I2C.',
    'к двум линиям I2C [17, 18]. Линии I2C переходника работают при напряжении 5 В, поэтому для подключения '
    'к микроконтроллеру с логическими уровнями 3,3 В применяется двунаправленный преобразователь '
    'уровней на транзисторах BSS138 [19, 20].')
sub('Дисплеи LCD1602 и LCD2004 представляют', ' [14], [15].', '.')
sub('Таким образом, для стационарного блока управления', '[11], [14], [15].', '[16–18].')
# ---------- 1.8 ----------
sub('В проекте используется модуль на электромагнитных реле', '[12]', '[21]')
sub('Альтернативой электромагнитному реле', '[12]', '[21]')
sub('Второй вариант – четырехпроводный вентилятор', 'два импульса на оборот.', 'два импульса на оборот [22].')
delete(find('При подготовке обзора также использованы'))
# ---------- 2.1 ----------
for p in d.paragraphs:
    if p.style.name == 'Heading 2' and p.text.startswith('2.1'):
        set_text(p, '2.1\tТребования к проектируемому устройству')
# ---------- 2.2 ----------
p = find('– дисплеи – символьный жидкокристаллический')
insert_after(p, '– источник питания – стабилизированный источник 5 В блока управления и литий-ионный '
                'аккумулятор 18650 пульта с контроллером заряда TP4056 [23, 24];')
# ---------- 2.3 ----------
sub('Пульт при нажатии кнопок формирует', 'флаги режимов и защит.',
    'флаги режимов и защит. Обмен пакетами выполняется средствами библиотеки RF24 [25].')
p = find('Структурная схема устройства приведена в приложении А')
set_text(p, 'Схема электрическая структурная устройства приведена в приложении А; она выполнена по '
            'ГОСТ 2.701–2008 [26] на листе формата А3. Устройство состоит из семи блоков: блока питания, '
            'микропроцессорного блока на плате ESP32, блока датчиков температуры и давления, датчика '
            'положения MPU-6050, блока отображения информации на дисплее LCD2004, исполнительного блока '
            '(реле и вентилятор) и пульта управления, содержащего плату ESP32, OLED-дисплей и радиомодуль '
            'nRF24L01+. Все информационные связи сходятся на микропроцессорном блоке; блоки между собой '
            'напрямую не обмениваются, а пульт связан с блоком управления двусторонним радиоканалом.')

# ---------- stray blank paragraphs: collapse runs of >1 blank paragraphs to 1 ----------
paras = d.paragraphs
i = 0
removed = 0
while i < len(paras):
    if not paras[i].text.strip() and paras[i].style.name == 'Normal' and 'TOC' not in paras[i]._p.xml:
        j = i
        while j + 1 < len(paras) and not paras[j + 1].text.strip() and paras[j + 1].style.name == 'Normal':
            j += 1
        for k in range(i + 1, j + 1):
            delete(paras[k]); removed += 1
        i = j + 1
    else:
        i += 1
print('blank paragraphs removed:', removed)

# ---------- sources ----------
D24, D26 = '22.09.2024', '22.09.2026'
srcs = [
 'ESP32-WROOM-32 : Datasheet [Электронный ресурс] / Espressif Systems. – Режим доступа : https://www.espressif.com/sites/default/files/documentation/esp32-wroom-32_datasheet_en.pdf. – Дата доступа : 22.09.2026.',
 'ESP32-DevKitC V4 : Getting Started Guide [Электронный ресурс] / Espressif Systems. – Режим доступа : https://docs.espressif.com/projects/esp-idf/en/latest/esp32/hw-reference/esp32/get-started-devkitc.html. – Дата доступа : 22.09.2026.',
 'ATmega328P : Datasheet [Электронный ресурс] / Microchip Technology. – Режим доступа : https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-7810-Automotive-Microcontrollers-ATmega328P_Datasheet.pdf. – Дата доступа : 22.09.2026.',
 'STM32F103C8T6 : Datasheet [Электронный ресурс] / STMicroelectronics. – Режим доступа : https://www.st.com/resource/en/datasheet/stm32f103c8.pdf. – Дата доступа : 22.09.2026.',
 'CP2102N : Single-Chip USB to UART Bridge [Электронный ресурс] / Silicon Laboratories. – Режим доступа : https://www.silabs.com/documents/public/data-sheets/cp2102n-datasheet.pdf. – Дата доступа : 22.09.2026.',
 'AMS1117 : 1 A Low Dropout Voltage Regulator [Электронный ресурс] / Advanced Monolithic Systems. – Режим доступа : http://www.advanced-monolithic.com/pdf/ds1117.pdf. – Дата доступа : 22.09.2026.',
 'ESP32 Series : Datasheet [Электронный ресурс] / Espressif Systems. – Режим доступа : https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf. – Дата доступа : 19.09.2026.',
 'nRF24L01+ : Single Chip 2.4GHz Transceiver. Product Specification v1.0 [Электронный ресурс] / Nordic Semiconductor. – Режим доступа : https://infocenter.nordicsemi.com/pdf/nRF24L01P_PS_v1.0.pdf. – Дата доступа : 22.09.2026.',
 'Радиомодуль HC-12 [Электронный ресурс]. – Режим доступа : https://3d-diy.ru/blog/radiomodul-hc-12/. – Дата доступа : 22.09.2026.',
 'RFM69HCW : Datasheet [Электронный ресурс] / HopeRF. – Режим доступа : https://cdn.sparkfun.com/datasheets/Wireless/General/RFM69HCW-V1.1.pdf. – Дата доступа : 22.09.2026.',
 'Аналоговый датчик давления 0–10 бар [Электронный ресурс] / DFRobot. – Режим доступа : https://wiki.dfrobot.com/Gravity__Water_Pressure_Sensor_SKU__SEN0257. – Дата доступа : 22.09.2026.',
 'DS18B20 : Datasheet [Электронный ресурс] / Analog Devices. – Режим доступа : https://www.analog.com/media/en/technical-documentation/data-sheets/DS18B20.pdf. – Дата доступа : 22.09.2026.',
 'NTC-термистор 3950 [Электронный ресурс] / TDK. – Режим доступа : https://product.tdk.com/en/search/sensor/ntc/chip-ntc-thermistor. – Дата доступа : 22.09.2026.',
 'MPU-6000/MPU-6050 : Product Specification [Электронный ресурс] / InvenSense. – Режим доступа : https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Datasheet1.pdf. – Дата доступа : 22.09.2026.',
 'MPU-6000 and MPU-6050 Register Map and Descriptions [Электронный ресурс] / InvenSense. – Режим доступа : https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map1.pdf. – Дата доступа : 22.09.2026.',
 'SSD1306 : Datasheet [Электронный ресурс] / Solomon Systech. – Режим доступа : https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf. – Дата доступа : 22.09.2026.',
 'HD44780U (LCD-II) : Dot Matrix Liquid Crystal Display Controller/Driver [Электронный ресурс] / Hitachi. – Режим доступа : https://www.sparkfun.com/datasheets/LCD/HD44780.pdf. – Дата доступа : 19.09.2026.',
 'PCF8574; PCF8574A : Remote 8-bit I/O expander for I2C-bus with interrupt [Электронный ресурс] / NXP Semiconductors. – Режим доступа : https://www.nxp.com/docs/en/data-sheet/PCF8574_PCF8574A.pdf. – Дата доступа : 19.09.2026.',
 'BSS138 : N-Channel Logic Level Enhancement Mode Field Effect Transistor [Электронный ресурс] / onsemi. – Режим доступа : https://www.onsemi.com/pdf/datasheet/bss138-d.pdf. – Дата доступа : 22.09.2026.',
 'Bi-Directional Logic Level Converter Hookup Guide [Электронный ресурс] / SparkFun Electronics. – Режим доступа : https://learn.sparkfun.com/tutorials/bi-directional-logic-level-converter-hookup-guide. – Дата доступа : 19.09.2026.',
 'Релейный модуль SRD-05VDC-SL-C [Электронный ресурс]. – Режим доступа : https://components101.com/switches/5v-relay-pinout-working-datasheet. – Дата доступа : 22.09.2026.',
 '4-Wire Pulse Width Modulation (PWM) Controlled Fans : Specification [Электронный ресурс] / Intel Corporation. – Режим доступа : https://www.intel.com/content/dam/support/us/en/documents/motherboards/desktop/sb/4_wire_pwm_spec.pdf. – Дата доступа : 22.09.2026.',
 'TP4056 : 1 A Standalone Linear Li-Ion Battery Charger [Электронный ресурс] / NanJing Top Power ASIC Corp. – Режим доступа : https://www.lcsc.com/datasheet/lcsc_datasheet_1810010410_TPOWER-TP4056_C382139.pdf. – Дата доступа : 22.09.2026.',
 'NCR18650B : Lithium Ion Battery [Электронный ресурс] / Panasonic Corporation. – Режим доступа : https://industrial.panasonic.com/cdbs/www-data/pdf2/ACA4000/ACA4000CE240.pdf. – Дата доступа : 22.09.2026.',
 'RF24 Library Documentation [Электронный ресурс]. – Режим доступа : https://nrf24.github.io/RF24/. – Дата доступа : 22.09.2026.',
 'ГОСТ 2.701–2008. Единая система конструкторской документации. Схемы. Виды и типы. Общие требования к выполнению. – Введ. 01.07.2009. – М. : Стандартинформ, 2009. – 14 с.',
]
src_paras = [p for p in d.paragraphs if re.match(r'^\[\d+\] ', p.text)]
print('old sources:', len(src_paras), 'new:', len(srcs))
for i, p in enumerate(src_paras):
    if i < len(srcs):
        set_text(p, f'[{i + 1}] {srcs[i]}')
    else:
        delete(p)

# ---------- tables: keep every table on one page ----------
for t in d.tables:
    rows = t.rows
    for r in rows[:-1]:
        for c in r.cells:
            for p in c.paragraphs:
                p.paragraph_format.keep_with_next = True

# ---------- verify citation order ----------
body_text = '\n'.join(p.text for p in d.paragraphs if not re.match(r'^\[\d+\] ', p.text))
seen = []
for m in re.finditer(r'\[([\d,\s–-]+)\]', body_text):
    for part in m.group(1).split(','):
        part = part.strip()
        if '–' in part or '-' in part:
            a, b = re.split('[–-]', part); rng = range(int(a), int(b) + 1)
        else:
            rng = [int(part)]
        for n in rng:
            if n not in seen: seen.append(n)
print('citation order:', seen)
missing = [n for n in range(1, len(srcs) + 1) if n not in seen]
print('uncited:', missing, ' out-of-order:', [n for i, n in enumerate(seen) if n != i + 1][:5])

# ---------- appendix A ----------
last_sect = d.sections[-1]
# title page (A4, same as before)
s1 = d.add_section(WD_SECTION.NEW_PAGE)
def centered(text, bold=False):
    p = d.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.first_line_indent = Mm(0)
    r = p.add_run(text); r.bold = bold
    return p
p = centered('ПРИЛОЖЕНИЕ А', bold=True)
# TC field for the table of contents
def add_field(p, instr):
    r = p.add_run(); fc = OxmlElement('w:fldChar'); fc.set(qn('w:fldCharType'), 'begin'); r._r.append(fc)
    r = p.add_run(); it = OxmlElement('w:instrText'); it.set(qn('xml:space'), 'preserve'); it.text = instr; r._r.append(it)
    r = p.add_run(); fc = OxmlElement('w:fldChar'); fc.set(qn('w:fldCharType'), 'end'); r._r.append(fc)
add_field(p, ' TC "Приложение А (обязательное) Схема электрическая структурная" \\l 1 ')
centered('(обязательное)')
centered('Схема электрическая структурная', bold=True)
# A3 landscape sheet
s2 = d.add_section(WD_SECTION.NEW_PAGE)
s2.orientation = WD_ORIENT.LANDSCAPE
s2.page_width = Mm(420); s2.page_height = Mm(297)
for a in ('left_margin', 'right_margin', 'top_margin', 'bottom_margin', 'header_distance', 'footer_distance'):
    setattr(s2, a, Mm(0))
s2.footer.is_linked_to_previous = False
for p in s2.footer.paragraphs:
    for r in p.runs: r._r.getparent().remove(r._r)
s2.header.is_linked_to_previous = False
p = d.add_paragraph()
pf = p.paragraph_format
pf.line_spacing_rule = WD_LINE_SPACING.SINGLE; pf.first_line_indent = Mm(0); pf.left_indent = Mm(0)
pf.space_before = Pt(0); pf.space_after = Pt(0); p.alignment = WD_ALIGN_PARAGRAPH.LEFT
r = p.add_run(); r.font.size = Pt(1)
r.add_picture(SHEET, width=Mm(419), height=Mm(296.3))
for sec in d.sections[1:]:
    pg = sec._sectPr.find(qn('w:pgNumType'))
    if pg is not None: sec._sectPr.remove(pg)
print('sections:', [(round(x.page_width.mm), x._sectPr.find(qn('w:pgNumType')) is not None) for x in d.sections])
d.save(DST)
print('saved', DST)

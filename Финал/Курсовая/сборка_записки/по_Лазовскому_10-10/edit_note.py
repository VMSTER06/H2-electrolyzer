from pathlib import Path
from copy import deepcopy
import re, json, hashlib
from docx import Document
from docx.shared import Pt, Mm
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_LINE_SPACING, WD_TAB_ALIGNMENT, WD_TAB_LEADER
from docx.enum.table import WD_TABLE_ALIGNMENT, WD_CELL_VERTICAL_ALIGNMENT
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.text.paragraph import Paragraph

B = Path(r'S:\Projects\1Electrolizer\Финал\Курсовая')
Q = B/'сборка_записки/по_Лазовскому_10-10'
SRC = B/'434701_Мурачев_В_К_КП2_проверено_09-10/Пояснительная записка.docx'
OUT = B/'Пояснительная записка H2 (КП2 по Лазовскому).docx'
d = Document(SRC)
orig = list(d.element.body)

def para(i): return Paragraph(orig[i], d)
def replace(i, text):
    p=para(i); p.text=text
    return p

def font(run, bold=None):
    run.font.name='Times New Roman'; run.font.size=Pt(14)
    if bold is not None: run.bold=bold
    rp=run._r.get_or_add_rPr()
    rf=rp.find(qn('w:rFonts'))
    if rf is not None:
        for a in list(rf.attrib):
            if 'theme' in a.lower(): del rf.attrib[a]
        for a in ['ascii','hAnsi','eastAsia','cs']: rf.set(qn('w:'+a),'Times New Roman')

def add_before(i, text='', style='PZ Body'):
    e=OxmlElement('w:p'); orig[i].addprevious(e)
    p=Paragraph(e,d);p.style=d.styles[style]
    if text:font(p.add_run(text))
    return p

def block(i, number, title, rows, intro, conclusion):
    add_before(i,intro)
    add_before(i,'','Normal')
    p=add_before(i,f'Таблица {number} – {title}','Normal')
    p.paragraph_format.first_line_indent=Mm(0)
    p.paragraph_format.keep_with_next=True
    t=d.add_table(rows=1,cols=len(rows[0]));orig[i].addprevious(t._tbl)
    for c,v in zip(t.rows[0].cells,rows[0]):c.text=v
    for vals in rows[1:]:
        for c,v in zip(t.add_row().cells,vals):c.text=v
    add_before(i,'','Normal');add_before(i,conclusion)
    return t

# Existing comparisons retain their locations; new comparisons fill omitted topics.
# Renumber references before inserting new table numbers.
for p in d.paragraphs:
    if p.style.name.lower().startswith(('toc','heading')) or 'таблиц' not in p.text.lower():continue
    for r in p.runs:
        r.text=re.sub(r'\b1\.([34])\b',lambda m: '1.'+{'3':'4','4':'6'}[m.group(1)],r.text)

block(88,'1.3','Сравнение вариантов измерения давления',[
 ['Критерий','Датчик с выходом напряжения','Датчик с токовым выходом','Цифровой датчик'],
 ['Выходной сигнал','Аналоговое напряжение','Токовая петля','Цифровой код'],
 ['Вход платы','АЦП через делитель','АЦП через шунт','I2C или SPI'],
 ['Согласование','Делитель до уровня АЦП','Шунт и входная защита','Согласование уровней'],
 ['Обработка','Масштабирование и калибровка','Пересчёт тока в давление','Разбор данных датчика'],
 ['Применение в H2 PRO','Принят для проекта','Альтернативный вариант','Альтернативный вариант'],
], 'Для измерения давления рассматриваются три варианта подключения преобразователя к плате. В таблице 1.3 сопоставлены способ передачи результата, необходимая входная цепь и обработка данных. Сравнение относится к типам преобразователей; конкретные диапазон и допустимая измеряемая среда задаются при выборе исполнения датчика.',
 'Для H2 PRO принят аналоговый выход напряжения: он использует один вход АЦП и не занимает адрес на общей шине I2C. Токовый выход требует дополнительного шунта, а цифровой датчик — поддержки его протокола. При коротком соединении внутри корпуса аналоговый вариант обеспечивает простую связь с существующей платой.')

block(106,'1.5','Сравнение вариантов определения наклона',[
 ['Критерий','Контактный датчик','Трёхосевой акселерометр','MPU-6050'],
 ['Результат','Состояние контакта','Проекции ускорения','Ускорение и угловая скорость'],
 ['Расчёт угла','Не выполняется','Программный','Программный'],
 ['Настройка порога','Зависит от механики','В программе','В программе'],
 ['Подавление вибрации','Устранение дребезга','Фильтрация выборок','Фильтрация выборок'],
 ['Связь с платой','Цифровой вход','Зависит от исполнения','I2C'],
 ['Выбор для проекта','Не выбран','Возможная замена','Выбран модуль GY-521'],
], 'Для защиты установки важно не только обнаружить наклон, но и вывести его величину оператору. В таблице 1.5 приведено сравнение контактного датчика и двух вариантов измерения ускорения. Характеристики MPU-6050 определяются документацией изготовителя [14,15].',
 'Контактный датчик пригоден для фиксированного аварийного положения, но не даёт численного значения угла. Акселерометр позволяет вычислять наклон относительно вертикали. В проекте используется уже предусмотренный модуль GY-521: его цифровой интерфейс совместим с платой, а порог защиты и фильтрация задаются программно. Гироскоп дополняет измерение движения, но статический наклон определяется по ускорению.')

block(123,'1.7','Сравнение вариантов коммутации нагрузки',[
 ['Критерий','Электромагнитное реле','Ключ на MOSFET'],
 ['Управляемая цепь','Контактная группа','Полупроводниковый канал'],
 ['Разделение цепей','Катушка и контакты разделены','Нужен отдельный изолятор'],
 ['Износ при переключении','Есть износ контактов','Нет механических контактов'],
 ['Частые переключения','Ограничены ресурсом','Возможны при расчёте драйвера'],
 ['Управление','Драйвер катушки','Драйвер затвора'],
 ['Применение в проекте','Основной вариант','Вариант для электролизера'],
], 'Для дискретного управления нагрузками рассматриваются электромагнитное реле и транзисторный ключ. Их сравнение по принципу коммутации и требованиям к управлению приведено в таблице 1.7. Номиналы силовых элементов выбираются по току и характеру конкретной нагрузки.',
 'Реле удобно для редко переключаемых цепей клапана, поджига и внешнего охладителя. Для частого регулирования электролизера возможен транзисторный ключ, однако он требует отдельного расчёта потерь и управления затвором. Поэтому в основном исполнении используется релейный модуль, а возможность применения MOSFET рассматривается как альтернативное исполнение.')

# State precisely what is compared: MCU resources, not board supply/pin counts.
replace(61,'В таблице 1.1 сопоставлены вычислительные ресурсы микроконтроллеров, которые могут использоваться на готовых отладочных платах. Плата с установленным микроконтроллером рассматривается как один элемент устройства; характеристики кристалла и контакты разъёмов платы не отождествляются.')
replace(66,'В модуле ESP32-WROOM-32 компании Espressif Systems установлен микроконтроллер ESP32 с двумя ядрами Xtensa LX6 и частотой до 240 МГц. Он содержит 520 КБ оперативной памяти, а модуль — внешнюю Flash-память объёмом 4 МБ. Встроенный Wi-Fi позволяет реализовать веб-интерфейс без дополнительного радиомодуля. Для датчиков и исполнительных элементов доступны аппаратные SPI, I2C, UART и 12-разрядный АЦП; число пригодных контактов определяется разводкой конкретной платы и резервированием линий Flash-памяти [1].')
replace(82,'Радиомодуль HC-12 работает в диапазоне 433 МГц и предоставляет последовательный интерфейс UART. Скорость UART настраивается до 115 200 бит/с; её следует отличать от скорости радиоканала и фактической скорости доставки полезных данных. Для обмена командами через HC-12 проверка получения пакета и повторная передача реализуются в прикладной программе [9].')
replace(83,'Модуль RFM69HCW компании HopeRF выпускается для нескольких частотных диапазонов; для сравнения рассматривается исполнение на 433 МГц. Модуль подключается по SPI и поддерживает скорость передачи до 300 кбит/с. Подтверждение доставки можно реализовать программно. Дальность всех сравниваемых модулей зависит от антенны, мощности, скорости и препятствий, поэтому условные значения дальности не используются как гарантированные характеристики [10].')
replace(81,'Радиомодуль nRF24L01+ компании Nordic Semiconductor работает в диапазоне 2,4 ГГц и подключается к микроконтроллеру по SPI. Скорости радиоканала составляют 250 кбит/с, 1 и 2 Мбит/с. Протокол Enhanced ShockBurst поддерживает аппаратное подтверждение доставки и автоматические повторы; контрольная сумма CRC позволяет обнаруживать повреждённые пакеты. Для H2 PRO эти возможности сокращают задержку подтверждения команды. Требуемая дальность должна подтверждаться испытанием собранных блоков в условиях эксплуатации [8].')
replace(88,'В проекте принят преобразователь с диапазоном от 0 до 10 бар, выходом от 0,5 до 4,5 В и питанием 5 В. Это исходные параметры интерфейса датчика для H2 PRO. Документация [11] иллюстрирует принцип аналогового измерения давления, но относится к другому исполнению и не подтверждает паспортные характеристики установленного датчика. Совместимость с измеряемой средой, присоединение и допустимую температуру определяют по паспорту конкретного преобразователя.')
replace(89,'Линейная характеристика позволяет пересчитывать выходное напряжение в давление и корректировать смещение и масштаб при калибровке. Сигнал до 4,5 В нельзя подавать непосредственно на вход ESP32: используется делитель напряжения. При номиналах 10 и 20 кОм верхнему выходному напряжению соответствует 3,0 В на входе АЦП. Эти параметры согласования относятся к выбранной схеме подключения, а не к данным другого датчика [11].')
replace(187,'Количество доступных выводов определяется разводкой платы. Линии GPIO 6–11 заняты Flash-памятью, GPIO 1 и 3 используются для последовательной диагностики, а GPIO 34, 35, 36 и 39 работают только как входы. После исключения Flash и UART остаются 24 линии: 20 двунаправленных и четыре входных. Распределение выводов выполняется с учётом SPI, I2C, датчиков, кнопок и исполнительных цепей; загрузочные выводы требуют соблюдения допустимых уровней при включении.')
replace(189,'Энергопотребление платы включает микроконтроллер, стабилизатор, преобразователь USB–UART и индикацию. Поэтому ток глубокого сна самого ESP32 нельзя выдавать за ток всей платы. Для расчёта питания ниже приняты отдельные значения тока платы в блоке управления и пульте. В пульте Wi-Fi отключён, а после периода бездействия используется глубокий сон; фактическое потребление собранного пульта зависит также от периферии и преобразователя питания.')
replace(200,'Радиомодуль функционирует на частоте 2,4 ГГц; скорость до 2 Мбит/с достаточна для коротких команд и пакетов состояния. CRC16 используется для контроля целостности, а резервные частотные каналы — для восстановления связи при помехах. Мощность встроенного передатчика программируется от минус 18 до 0 дБм; усиленный модуль требует отдельного учёта внешнего усилителя. В блоке управления установлена максимальная настройка мощности, в пульте — минимальная. Выполнение требования по дальности проверяется на собранном устройстве.')
replace(210,'Для измерения давления выбран аналоговый преобразователь с проектным диапазоном от 0 до 10 бар (см. таблицу 1.3). Верхняя граница диапазона превышает рабочую уставку до 6,5 бар и аварийный порог 6,8 бар. При выборе исполнения необходимо согласовать измеряемую среду и присоединение с баком установки. В таблице 3.3 приведены параметры, принятые для его подключения и обработки сигнала в H2 PRO.')
replace(216,'Принятые параметры позволяют согласовать преобразователь с измерительным входом ESP32 и задать формулу пересчёта. Точность, быстродействие и температурный диапазон конкретного датчика должны подтверждаться его паспортом; проектные параметры интерфейса не заменяют эти характеристики.')
replace(220,'После сравнения датчиков температуры (см. таблицу 1.4) выбран DS18B20 с интерфейсом 1-Wire. Нормированная погрешность ±0,5 °C относится к диапазону от минус 10 до +85 °C, а не ко всему диапазону измерений. Герметичная гильза удобна для установки на корпус электролизера. Цифровая передача исключает измерение сопротивления длинного провода, однако работоспособность линии всё равно зависит от её ёмкости, подтягивающего резистора и прокладки.')

# Add references/interpretation around tables, matching the reference's narrative pattern.
para(230).text=para(230).text.replace('выбран модуль','после сравнения вариантов (см. таблицу 1.5) выбран модуль')
para(251).text=para(251).text.replace('(см. подраздел 1.8)','(см. таблицу 1.7)')
add_before(236,'Таким образом, модуль даёт численные данные для вывода угла и для программной защиты. Разрешение АЦП не следует отождествлять с точностью определения угла: она зависит от установки датчика, калибровки и фильтрации ускорений.')
add_before(249,'Следовательно, два дисплея решают разные задачи: LCD2004 показывает несколько параметров стационарной установки одновременно, а SSD1306 обеспечивает компактную графическую индикацию на пульте. Оба устройства подключаются по I2C к своей плате.')
add_before(257,'Выбранные четыре релейных канала обеспечивают независимое управление нагрузками. Значения тока контактов относятся к указанным изготовителем условиям; для индуктивной нагрузки дополнительно необходимы цепи подавления выбросов и проверка допустимой коммутации.')

def table_at(i):
    from docx.table import Table
    return Table(orig[i],d)
def set_cell(t,r,c,text):t.cell(r,c).text=text
t=table_at(64)
set_cell(t,0,1,'ESP32 в модуле WROOM-32')
set_cell(t,8,0,'Линии GPIO кристалла')
set_cell(t,8,1,'34; на плате доступны не все')
t=table_at(79)
set_cell(t,0,3,'RFM69HCW')
set_cell(t,2,0,'Подтверждение доставки')
for c,v in enumerate(['Аппаратное','Программное','Программное'],1):set_cell(t,2,c,v)
set_cell(t,4,0,'Скорость передачи')
set_cell(t,4,2,'UART до 115,2 кбит/с')
set_cell(t,6,0,'Применение в проекте')
for c,v in enumerate(['Выбран','Альтернатива','Альтернатива'],1):set_cell(t,6,c,v)
t=table_at(193)
set_cell(t,1,1,'ESP32-WROOM-32; плата на 38 контактов')
set_cell(t,11,0,'Расчётный ток платы, мА')
set_cell(t,11,1,'300 — блок управления; 80 — пульт')
t=table_at(204)
set_cell(t,3,0,'Требуемая дальность в помещении, м');set_cell(t,3,1,'Не менее 30; проверяется испытанием')
t=table_at(214)
replace(213,'Таблица 3.3 – Проектные параметры подключения датчика давления')
rows=[['Параметр','Значение'],['Диапазон по заданию, бар','От 0 до 10'],['Выходное напряжение, В','От 0,5 до 4,5'],['Напряжение питания, В','5'],['Ток для расчёта питания, мА','10'],['Вход платы','GPIO 36, ADC1'],['Делитель напряжения, кОм','10 и 20'],['Напряжение на АЦП при верхней границе, В','3,0'],['Рабочая уставка давления, бар','От 0,5 до 6,5'],['Аварийный порог давления, бар','6,8']]
for row,vals in zip(t.rows,rows):
    for c,v in zip(row.cells,vals):c.text=v

# Consistent power accounting: table values are per item; power includes quantity.
replace(259,'Для выбора источника питания принимаются расчётные токи элементов, приведённые в таблице 3.9. Это исходные оценки для проектирования, а не результаты измерения собранного устройства. В графе тока указано значение для одного элемента; мощность строки рассчитана с учётом количества. Потребление платы дано без внешних нагрузок.')
replace(264,'Суммарный расчётный ток шины 5 В составляет 0,7665 А. Токи элементов на 3,3 В также протекают через вход линейного стабилизатора, поэтому входная мощность шины 5 В учитывает его потери:')
replace(266,'\tP5 = U5 · I5 = 5 · 0,7665 = 3,8325 Вт,\t(3.1)')
replace(268,'где P5 – входная мощность шины 5 В, Вт; U5 – напряжение этой шины, В; I5 – суммарный ток её потребителей с учётом линейного стабилизатора, А.')
replace(272,'\tPист = 1,2 · P5 = 1,2 · 3,8325 = 4,599 Вт.\t(3.2)')
replace(274,'Здесь Pист – требуемая мощность источника 5 В, Вт; коэффициент 1,2 задаёт запас 20 %. Требуемый ток Iист, А, определяется с тем же запасом:')
replace(276,'\tIист = 1,2 · I5 = 1,2 · 0,7665 = 0,9198 А.\t(3.3)')
replace(278,'Полученные значения определяют минимальные расчётные параметры источника для низковольтной электроники, а не мощность силовой цепи электролизера.')
replace(283,'Для пульта расчёт ведётся отдельно, как показано в таблице 3.10. В графе тока также приведено потребление одного элемента, а мощность учитывает их количество. В расчёт активного состояния включены плата без Wi-Fi, радиомодуль, OLED-дисплей и три светодиода; после перехода в сон их состояния изменяются.')
replace(288,'При питании внешних элементов от линейного стабилизатора платы суммарный входной ток шины 5 В составляет 0,130 А. Для определения входной мощности учитываются и потери стабилизатора. В следующем выражении Pп – входная мощность пульта, Вт; Uп – напряжение питания, В; Iп – суммарный входной ток, А:')
replace(290,'\tPп = Uп · Iп = 5 · 0,130 = 0,650 Вт.\t(3.4)')
replace(292,'С запасом 20 % определяются Pп.ист – требуемая мощность источника пульта, Вт, и Iп.ист – его требуемый выходной ток, А:')
replace(294,'\tPп.ист = 1,2 · 0,650 = 0,780 Вт;\t(3.5)')
for i in [262,286]:
    t=table_at(i)
    for c,v in enumerate(['Наименование элемента','Количество','Uпит, В','Iодн, А','Pсум, Вт']):set_cell(t,0,c,v)

# Update source metadata only for sources consulted now.
replace(309,'[8] nRF24L01+ : Single Chip 2.4GHz Transceiver. Product Specification v1.0 [Электронный ресурс] / Nordic Semiconductor. – Режим доступа : https://infocenter.nordicsemi.com/pdf/nRF24L01P_PS_v1.0.pdf. – Дата доступа : 22.09.2026.')
replace(310,'[9] HC-12 Wireless RF UART Communication Module V2.6 : User Manual [Электронный ресурс] / HC. – Режим доступа : https://www.hc01.com/downloads/HC-12%20english%20datasheets.pdf. – Дата доступа : 10.10.2026.')
replace(311,'[10] RFM69HCW : Datasheet [Электронный ресурс] / HopeRF. – Режим доступа : https://www.hoperf.com/uploads/RFM69HCW-V1.1datasheet_1695352208.pdf. – Дата доступа : 10.10.2026.')
replace(312,'[11] Gravity: Analog Water Pressure Sensor SEN0257 [Электронный ресурс] / DFRobot. – Режим доступа : https://wiki.dfrobot.com/sen0257. – Дата доступа : 10.10.2026.')

# Normalize styles and inherited paragraph-mark formatting, including TOC page fields.
for name in ['Normal','PZ Body','Heading 1','Heading 2','toc 1','toc 2']:
    s=d.styles[name];s.font.name='Times New Roman';s.font.size=Pt(14)
    s.paragraph_format.line_spacing=Pt(18)
    s.paragraph_format.space_before=Pt(0);s.paragraph_format.space_after=Pt(0)
    for ar in s.element.findall(qn('w:autoRedefine')):s.element.remove(ar)
    for rf in s.element.iter(qn('w:rFonts')):
        for a in list(rf.attrib):
            if 'theme' in a.lower():del rf.attrib[a]
        for a in ['ascii','hAnsi','eastAsia','cs']:rf.set(qn('w:'+a),'Times New Roman')
for n in [1,2]:
    s=d.styles[f'toc {n}'];s.font.bold=False
    s.paragraph_format.alignment=WD_ALIGN_PARAGRAPH.LEFT
    s.paragraph_format.keep_with_next=False
    s.paragraph_format.keep_together=True
    s.paragraph_format.right_indent=Mm(8)
    s.paragraph_format.left_indent=Mm(7 if n==1 else 20)
    s.paragraph_format.first_line_indent=Mm(-7 if n==1 else -13)
    s.paragraph_format.tab_stops.clear_all()
    s.paragraph_format.tab_stops.add_tab_stop(Mm(7 if n==1 else 20))
    s.paragraph_format.tab_stops.add_tab_stop(Mm(165),WD_TAB_ALIGNMENT.RIGHT,WD_TAB_LEADER.DOTS)

for p in d.paragraphs:
    if p.style.name=='PZ Body' and not p.text.startswith('['):
        # Minus signs in prose, not in tables/formulae/reference ranges.
        for r in p.runs:r.text=re.sub(r'(?<=от )–(?=\d)','минус ',r.text)
    if p.text.startswith('Таблица '):
        f=p.paragraph_format;f.alignment=WD_ALIGN_PARAGRAPH.LEFT
        f.first_line_indent=Mm(0);f.left_indent=Mm(0);f.right_indent=Mm(0)
        f.keep_with_next=True;f.keep_together=True
        pp=p._p.get_or_add_pPr();e=OxmlElement('w:suppressAutoHyphens');pp.append(e)
    if p.style.name.lower().startswith('toc'):
        # Remove direct TOC properties; Word regenerates them and then native script normalizes.
        pp=p._p.get_or_add_pPr()
        for e in list(pp):
            if e.tag!=qn('w:pStyle'):pp.remove(e)
    for r in p.runs:
        font(r)
    if re.match(r'^\t.*\(3\.\d+\)$',p.text):
        # Preserve mathematical symbols and make Latin variables italic.
        tx=p.text;p.clear()
        for part in re.split(r'([A-Za-z][A-Za-zА-Яа-я0-9.]*)',tx):
            r=p.add_run(part);font(r)
            if re.match(r'^[A-Za-z]',part):r.italic=True

# All tables use the reference's simple black grid, but STP font/spacing/units.
table_inventory=[]
for idx,t in enumerate(d.tables):
    cols=len(t.columns)
    widths=([55,110] if cols==2 else [59,53,53] if cols==3 else [51,38,38,38] if cols==4 else [66,25,22,26,26])
    t.alignment=WD_TABLE_ALIGNMENT.CENTER;t.autofit=False
    pr=t._tbl.tblPr
    for key in ['tblInd','tblBorders','tblCellMar','tblW']:
        for e in pr.findall(qn('w:'+key)):pr.remove(e)
    w=OxmlElement('w:tblW');w.set(qn('w:type'),'dxa');w.set(qn('w:w'),str(int(Mm(165).twips)));pr.append(w)
    borders=OxmlElement('w:tblBorders')
    for side in ['top','left','bottom','right','insideH','insideV']:
        e=OxmlElement('w:'+side);e.set(qn('w:val'),'single');e.set(qn('w:sz'),'4');e.set(qn('w:color'),'000000');borders.append(e)
    pr.append(borders)
    mar=OxmlElement('w:tblCellMar')
    for side,val in [('top',20),('bottom',20),('left',65),('right',65)]:
        e=OxmlElement('w:'+side);e.set(qn('w:w'),str(val));e.set(qn('w:type'),'dxa');mar.append(e)
    pr.append(mar)
    for col,width in zip(t.columns,widths):col.width=Mm(width)
    for ri,row in enumerate(t.rows):
        trp=row._tr.get_or_add_trPr()
        for key in ['trHeight','cantSplit','tblHeader']:
            for e in trp.findall(qn('w:'+key)):trp.remove(e)
        trp.append(OxmlElement('w:cantSplit'))
        if ri==0:trp.append(OxmlElement('w:tblHeader'))
        for ci,c in enumerate(row.cells):
            c.width=Mm(widths[ci]);c.vertical_alignment=WD_CELL_VERTICAL_ALIGNMENT.CENTER
            for p in c.paragraphs:
                p.style=d.styles['Normal'];f=p.paragraph_format
                f.line_spacing=Pt(18);f.space_before=Pt(0);f.space_after=Pt(0)
                f.first_line_indent=Mm(0);f.left_indent=Mm(0);f.right_indent=Mm(0)
                f.alignment=WD_ALIGN_PARAGRAPH.CENTER if ri==0 or ci>0 else WD_ALIGN_PARAGRAPH.LEFT
                f.keep_with_next=False;f.keep_together=True
                if ri==0:
                    e=OxmlElement('w:suppressAutoHyphens');p._p.get_or_add_pPr().append(e)
                for r in p.runs:font(r,False)
    table_inventory.append({'index':idx+1,'rows':len(t.rows),'columns':cols,'headers':[c.text for c in t.rows[0].cells]})

# Preserve media and diagram anchors while writing a separate deliverable.
d.save(OUT)
(Q/'tables.json').write_text(json.dumps(table_inventory,ensure_ascii=False,indent=2),encoding='utf-8')
print(OUT)
print('logical tables:',len(d.tables))

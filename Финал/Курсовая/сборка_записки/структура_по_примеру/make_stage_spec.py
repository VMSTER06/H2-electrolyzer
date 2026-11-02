from pathlib import Path

ROOT = Path(r"S:\1Electrolizer\Финал\Курсовая\сборка_записки")
source = ROOT / "pz_spec.txt"
target = ROOT / "структура_по_примеру" / "pz_h2_stage12.txt"

lines = source.read_text(encoding="utf-8").splitlines()

start_section_3 = next(i for i, x in enumerate(lines) if x.startswith("#H1 3<TAB>"))
start_refs = next(i for i, x in enumerate(lines) if x == "#H1C Список использованных источников")
start_apps = next(i for i, x in enumerate(lines) if x.startswith("#APP "))

# The example contains eight subsections in section 1. Combine the three
# actuator/indicator subsections of H2 PRO into one final subsection.
body = lines[:start_section_3]

# The introduction follows the supervisor's three-part structure and is sized
# to about three quarters of one A4 page.
intro_start = next(i for i, x in enumerate(body) if x == "#H1C Введение")
section_1 = next(i for i, x in enumerate(body) if x.startswith("#H1 1<TAB>"))
intro = [
    "#H1C Введение",
    "#E",
    "#P Микропроцессорные устройства дистанционного управления применяются при автоматизации технологических установок. Платы микроконтроллеров позволяют контролировать физические параметры и управлять исполнительными элементами [2, 3]. При получении водорода необходимо непрерывно измерять давление и температуру, своевременно отключать электролизер и обеспечивать управление с безопасного расстояния. Основными задачами являются сбор и обработка данных, передача команд и предотвращение аварийных режимов. Задача проекта состоит в разработке устройства, которое объединяет управление, индикацию и автономную защиту установки.",
    "#P Проектируемое устройство должно выполнять следующие функции:",
    "#L – автоматически поддерживать заданное давление;",
    "#L – измерять давление, температуру и угол наклона;",
    "#L – принимать команды с беспроводного пульта;",
    "#L – управлять электролизером, клапаном, поджигом и охлаждением;",
    "#L – отображать параметры на дисплеях и в веб-интерфейсе;",
    "#L – передавать на пульт текущее состояние установки;",
    "#L – отключать исполнительные элементы при аварии.",
    "#P Для решения поставленной задачи устройство должно содержать две платы микроконтроллера ESP32 DevKit v1, два радиомодуля nRF24L01+, датчики давления, температуры и наклона, дисплей LCD2004 блока управления, OLED-дисплей пульта, реле, вентилятор, кнопки и светодиоды. Плата с установленным микроконтроллером рассматривается как один элемент. Стационарный блок управления выполняет измерение, регулирование и защиту, а пульт обеспечивает дистанционное управление. Команды и данные передаются между ними по двустороннему радиоканалу. Блок управления также образует точку доступа Wi-Fi для настройки и наблюдения за параметрами. Каждый из двух блоков имеет собственное устройство вывода информации.",
]
body = body[:intro_start] + intro + body[section_1:]
for i, line in enumerate(body):
    if line.startswith("#H2 1.8<TAB>"):
        body[i] = "#H2 1.8<TAB>Исполнительные устройства, органы управления и индикация"
    elif line.startswith("#H2 1.9<TAB>") or line.startswith("#H2 1.10<TAB>"):
        body[i] = "#E"
    elif line.startswith("#FIG fig_2_1.png<TAB>"):
        body[i] = "#E"

# Terminology and composition required by the supervisor.
for i, line in enumerate(body):
    line = line.replace("обоснования выбора узлов и элементов", "обоснования выбора элементов")
    line = line.replace("выбор узлов", "выбор элементов")
    line = line.replace("функциональным узлам", "функциональным элементам")
    line = line.replace("между узлами системы", "между частями системы")
    if line.startswith("#H2 1.2<TAB>"):
        line = "#H2 1.2<TAB>Плата микроконтроллера"
    elif line.startswith("#P Микроконтроллер – микросхема"):
        line = "#P Плата микроконтроллера представляет собой единый элемент устройства, включающий микроконтроллер, память, стабилизатор питания, преобразователь USB–UART и цепи программирования. Характеристики установленного микроконтроллера определяют вычислительные возможности и набор интерфейсов платы."
    elif line.startswith("#L – микроконтроллер – главный компонент каждого блока"):
        line = "#L – плата микроконтроллера – единый элемент каждого блока, выполняющий обработку данных, исполнение команд и управление остальными компонентами;"
    elif line.startswith("#P Структурная схема устройства приведена на рисунке 2.1."):
        line = "#P Устройство делится на стационарный блок управления, размещаемый рядом с установкой, и носимый пульт, связанные радиоканалом диапазона 2,4 ГГц."
    body[i] = line

# The drawing is a separate A3 graphic document. Put its reference at the very
# end of section 2, as requested, instead of embedding it as Figure 2.1.
refs_heading = next(i for i, x in enumerate(body) if x == "#H1C Список использованных источников") if any(x == "#H1C Список использованных источников" for x in body) else len(body)
body.insert(refs_heading, "#P Структурная схема устройства приведена в приложении А и выполнена на листе формата А3.")

refs = lines[start_refs:start_apps]
extra_refs = [
    "#PL [20] ESP32-DevKitC V4 : Getting Started Guide [Электронный ресурс] / Espressif Systems. – Режим доступа : https://docs.espressif.com/projects/esp-idf/en/latest/esp32/hw-reference/esp32/get-started-devkitc.html. – Дата доступа : 22.09.2026.",
    "#PL [21] CP2102N : Single-Chip USB to UART Bridge [Электронный ресурс] / Silicon Laboratories. – Режим доступа : https://www.silabs.com/documents/public/data-sheets/cp2102n-datasheet.pdf. – Дата доступа : 22.09.2026.",
    "#PL [22] AMS1117 : 1 A Low Dropout Voltage Regulator [Электронный ресурс] / Advanced Monolithic Systems. – Режим доступа : http://www.advanced-monolithic.com/pdf/ds1117.pdf. – Дата доступа : 22.09.2026.",
    "#PL [23] BSS138 : N-Channel Logic Level Enhancement Mode Field Effect Transistor [Электронный ресурс] / onsemi. – Режим доступа : https://www.onsemi.com/pdf/datasheet/bss138-d.pdf. – Дата доступа : 22.09.2026.",
    "#PL [24] MPU-6000 and MPU-6050 Register Map and Descriptions [Электронный ресурс] / InvenSense. – Режим доступа : https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map1.pdf. – Дата доступа : 22.09.2026.",
    "#PL [25] nRF24L01+ Preliminary Product Specification [Электронный ресурс] / Nordic Semiconductor. – Режим доступа : https://infocenter.nordicsemi.com/pdf/nRF24L01P_PS_v1.0.pdf. – Дата доступа : 22.09.2026.",
    "#PL [26] 4-Wire Pulse Width Modulation (PWM) Controlled Fans : Specification [Электронный ресурс] / Intel Corporation. – Режим доступа : https://www.intel.com/content/dam/support/us/en/documents/motherboards/desktop/sb/4_wire_pwm_spec.pdf. – Дата доступа : 22.09.2026.",
    "#PL [27] TP4056 : 1 A Standalone Linear Li-Ion Battery Charger [Электронный ресурс] / NanJing Top Power ASIC Corp. – Режим доступа : https://www.lcsc.com/datasheet/lcsc_datasheet_1810010410_TPOWER-TP4056_C382139.pdf. – Дата доступа : 22.09.2026.",
    "#PL [28] NCR18650B : Lithium Ion Battery [Электронный ресурс] / Panasonic Corporation. – Режим доступа : https://industrial.panasonic.com/cdbs/www-data/pdf2/ACA4000/ACA4000CE240.pdf. – Дата доступа : 22.09.2026.",
]
refs.extend(extra_refs)

# Add one concise citation bridge so every added source belongs to the subject
# matter of the retained chapters.
insert_at = next(i for i, x in enumerate(body) if x.startswith("#H1 2<TAB>"))
body.insert(
    insert_at,
    "#P При подготовке обзора также использованы официальные руководства по плате ESP32 DevKit, преобразователю CP2102N, стабилизатору AMS1117, расширителю PCF8574 и регистрам MPU-6050, а также стандарты выполнения схем [20]–[28].",
)
body.insert(insert_at + 1, "#E")

target.write_text("\n".join(body + refs) + "\n", encoding="utf-8")
print(target)

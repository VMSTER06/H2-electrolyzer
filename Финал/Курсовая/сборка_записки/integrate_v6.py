"""Replace Введение + разделы 1–3 in pz_spec.txt with sections_1_3_v6.txt, rebuild the source list
by order of first citation (citation format "[1,2]" as in the lecturer's sample), remap citations of
the remaining sections (4, 5, conclusion) from the old numbering."""
import io, re, sys
sys.stdout.reconfigure(encoding='utf-8')
SPEC = 'pz_spec.txt'
L = io.open(SPEC, encoding='utf-8-sig').read().split('\n')
new13 = io.open('sections_1_3_v6.txt', encoding='utf-8').read().rstrip('\n').split('\n')

D = 'Дата доступа : 10.10.2026.'
SRC = {
 'esp32wroom': 'ESP32-WROOM-32 : Datasheet [Электронный ресурс] / Espressif Systems. – Режим доступа : https://www.espressif.com/sites/default/files/documentation/esp32-wroom-32_datasheet_en.pdf. – ' + D,
 'devkitc': 'ESP32-DevKitC V4 : Getting Started Guide [Электронный ресурс] / Espressif Systems. – Режим доступа : https://docs.espressif.com/projects/esp-idf/en/latest/esp32/hw-reference/esp32/get-started-devkitc.html. – ' + D,
 'atmega': 'ATmega328P : Datasheet [Электронный ресурс] / Microchip Technology. – Режим доступа : https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-7810-Automotive-Microcontrollers-ATmega328P_Datasheet.pdf. – ' + D,
 'stm32': 'STM32F103C8T6 : Datasheet [Электронный ресурс] / STMicroelectronics. – Режим доступа : https://www.st.com/resource/en/datasheet/stm32f103c8.pdf. – ' + D,
 'cp2102': 'CP2102N : Single-Chip USB to UART Bridge [Электронный ресурс] / Silicon Laboratories. – Режим доступа : https://www.silabs.com/documents/public/data-sheets/cp2102n-datasheet.pdf. – ' + D,
 'ams1117': 'AMS1117 : 1 A Low Dropout Voltage Regulator [Электронный ресурс] / Advanced Monolithic Systems. – Режим доступа : http://www.advanced-monolithic.com/pdf/ds1117.pdf. – ' + D,
 'esp32series': 'ESP32 Series : Datasheet [Электронный ресурс] / Espressif Systems. – Режим доступа : https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf. – ' + D,
 'nrf24': 'nRF24L01+ : Single Chip 2.4GHz Transceiver. Product Specification v1.0 [Электронный ресурс] / Nordic Semiconductor. – Режим доступа : https://infocenter.nordicsemi.com/pdf/nRF24L01P_PS_v1.0.pdf. – ' + D,
 'hc12': 'HC-12 Wireless RF UART Communication Module V2.6 : User Manual [Электронный ресурс] / HC Information Technology. – Режим доступа : https://www.hc01.com/downloads/HC-12%20english%20datasheets.pdf. – ' + D,
 'rfm69': 'RFM69HCW : Datasheet [Электронный ресурс] / HopeRF. – Режим доступа : https://cdn.sparkfun.com/datasheets/Wireless/General/RFM69HCW-V1.1.pdf. – ' + D,
 'pressure': 'Gravity: Analog Water Pressure Sensor SEN0257 [Электронный ресурс] / DFRobot. – Режим доступа : https://wiki.dfrobot.com/Gravity__Water_Pressure_Sensor_SKU__SEN0257. – ' + D,
 'ds18b20': 'DS18B20 : Programmable Resolution 1-Wire Digital Thermometer [Электронный ресурс] / Analog Devices. – Режим доступа : https://www.analog.com/media/en/technical-documentation/data-sheets/DS18B20.pdf. – ' + D,
 'ntc': 'NTC-термистор 3950 [Электронный ресурс] / TDK. – Режим доступа : https://product.tdk.com/en/search/sensor/ntc/chip-ntc-thermistor. – ' + D,
 'mpu_ps': 'MPU-6000/MPU-6050 : Product Specification [Электронный ресурс] / InvenSense. – Режим доступа : https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Datasheet1.pdf. – ' + D,
 'mpu_rm': 'MPU-6000 and MPU-6050 Register Map and Descriptions [Электронный ресурс] / InvenSense. – Режим доступа : https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map1.pdf. – ' + D,
 'ssd1306': 'SSD1306 : Datasheet [Электронный ресурс] / Solomon Systech. – Режим доступа : https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf. – ' + D,
 'hd44780': 'HD44780U (LCD-II) : Dot Matrix Liquid Crystal Display Controller/Driver [Электронный ресурс] / Hitachi. – Режим доступа : https://www.sparkfun.com/datasheets/LCD/HD44780.pdf. – ' + D,
 'pcf8574': 'PCF8574; PCF8574A : Remote 8-bit I/O expander for I2C-bus with interrupt [Электронный ресурс] / NXP Semiconductors. – Режим доступа : https://www.nxp.com/docs/en/data-sheet/PCF8574_PCF8574A.pdf. – ' + D,
 'bss138': 'BSS138 : N-Channel Logic Level Enhancement Mode Field Effect Transistor [Электронный ресурс] / onsemi. – Режим доступа : https://www.onsemi.com/pdf/datasheet/bss138-d.pdf. – ' + D,
 'sparkfun': 'Bi-Directional Logic Level Converter Hookup Guide [Электронный ресурс] / SparkFun Electronics. – Режим доступа : https://learn.sparkfun.com/tutorials/bi-directional-logic-level-converter-hookup-guide. – ' + D,
 'aod4184': 'AOD4184A : 40V N-Channel MOSFET [Электронный ресурс] / Alpha and Omega Semiconductor. – Режим доступа : https://www.aosmd.com/res/datasheets/AOD4184A.pdf. – ' + D,
 'relay': 'Релейный модуль SRD-05VDC-SL-C [Электронный ресурс]. – Режим доступа : https://components101.com/switches/5v-relay-pinout-working-datasheet. – ' + D,
 'ss8050': 'SS8050 : NPN Epitaxial Silicon Transistor [Электронный ресурс] / onsemi. – Режим доступа : https://www.onsemi.com/pdf/datasheet/ss8050-d.pdf. – ' + D,
 'tp4056': 'TP4056 : 1 A Standalone Linear Li-Ion Battery Charger [Электронный ресурс] / NanJing Top Power ASIC Corp. – Режим доступа : https://www.lcsc.com/datasheet/lcsc_datasheet_1810010410_TPOWER-TP4056_C382139.pdf. – ' + D,
 'ncr18650': 'NCR18650B : Lithium Ion Battery [Электронный ресурс] / Panasonic Corporation. – Режим доступа : https://industrial.panasonic.com/cdbs/www-data/pdf2/ACA4000/ACA4000CE240.pdf. – ' + D,
 'rf24': 'RF24 Library Documentation [Электронный ресурс]. – Режим доступа : https://nrf24.github.io/RF24/. – ' + D,
 'gost2701': 'ГОСТ 2.701–2008. Единая система конструкторской документации. Схемы. Виды и типы. Общие требования к выполнению. – Введ. 01.07.2009. – М. : Стандартинформ, 2009. – 14 с.',
 'gost19701': 'ГОСТ 19.701–90. Единая система программной документации. Схемы алгоритмов, программ, данных и систем. Обозначения условные и правила выполнения. – Введ. 01.01.1992. – М. : Стандартинформ, 2010. – 23 с.',
 'intelfan': '4-Wire Pulse Width Modulation (PWM) Controlled Fans : Specification [Электронный ресурс] / Intel Corporation. – Режим доступа : https://www.intel.com/content/dam/support/us/en/documents/motherboards/desktop/sb/4_wire_pwm_spec.pdf. – ' + D,
}
NEWKEYS = ['esp32wroom','devkitc','atmega','stm32','cp2102','ams1117','esp32series','nrf24','hc12','rfm69','pressure',
           'ds18b20','ntc','mpu_ps','mpu_rm','ssd1306','hd44780','pcf8574','bss138','sparkfun','aod4184','relay','ss8050',
           'tp4056','ncr18650','rf24','gost2701']
OLDKEYS = ['esp32wroom','devkitc','atmega','stm32','cp2102','ams1117','esp32series','nrf24','hc12','rfm69','pressure',
           'ds18b20','ntc','mpu_ps','mpu_rm','ssd1306','hd44780','pcf8574','bss138','sparkfun','relay','intelfan',
           'tp4056','ncr18650','rf24','gost2701','gost19701']
cite_re = re.compile(r'\[(\d+(?:\s*[–-]\s*\d+)?(?:\s*,\s*\d+(?:\s*[–-]\s*\d+)?)*)\]')
def nums(g):
    out = []
    for part in g.split(','):
        part = part.strip()
        if re.search('[–-]', part):
            a, b = map(int, re.split('[–-]', part)); out += list(range(a, b + 1))
        else: out.append(int(part))
    return out
def to_keys(lines, keys):
    return [cite_re.sub(lambda m: '[' + ','.join('@' + keys[n - 1] for n in nums(m.group(1))) + ']', l) for l in lines]

i_intro = next(i for i, l in enumerate(L) if l.startswith('#H1C Введение'))
i4 = next(i for i, l in enumerate(L) if l.startswith('#H1 4<TAB>'))
isrc = next(i for i, l in enumerate(L) if l.startswith('#H1C Список'))
iapp = next(i for i, l in enumerate(L) if l.startswith('#APP'))
head = L[:i_intro]
body = to_keys(new13, NEWKEYS) + to_keys(L[i4:isrc], OLDKEYS)
apps = L[iapp:]
# order of first citation
order = []
key_re = re.compile(r'@(\w+)')
for l in body:
    for k in key_re.findall(l):
        if k not in order: order.append(k)
num = {k: i + 1 for i, k in enumerate(order)}
def fmt(ns):
    ns = sorted(set(ns)); out = []; i = 0
    while i < len(ns):
        j = i
        while j + 1 < len(ns) and ns[j + 1] == ns[j] + 1: j += 1
        out.append(f'{ns[i]}–{ns[j]}' if j - i >= 2 else ','.join(map(str, ns[i:j + 1])))
        i = j + 1
    return '[' + ','.join(out) + ']'
body = [re.sub(r'\[(@\w+(?:,@\w+)*)\]', lambda m: fmt([num[k] for k in key_re.findall(m.group(1))]), l) for l in body]
# negative numbers in running text: "от –55" -> "от минус 55" (ГОСТ 2.105), tables untouched
body = [re.sub(r'(?<=\s)–(?=\d)', 'минус ', l) if l.startswith(('#P ', '#L ', '#N ')) else l for l in body]
sources = ['#PL [' + str(i + 1) + '] ' + SRC[k] for i, k in enumerate(order)]
res = head + body + ['#H1C Список использованных источников', '#E'] + sources + apps
io.open(SPEC, 'w', encoding='utf-8-sig').write('\n'.join(res) + '\n')
print('sources:', len(order)); print('unused:', [k for k in SRC if k not in order])

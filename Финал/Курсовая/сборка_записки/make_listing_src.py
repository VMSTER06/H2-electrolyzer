"""Копия прошивок для приложения Е без веб-интерфейса и Wi-Fi (требование 31.10.2026).
Настоящие прошивки (Финал/H2_Brain_v5, H2_Remote_v5) не меняются. Копия компилируется.
Usage: py -3 make_listing_src.py   → сборка_записки/листинг/H2_Brain_v5, H2_Remote_v5"""
import io, os, re, shutil, sys
sys.stdout.reconfigure(encoding='utf-8')

SRC = r'S:\Projects\1Electrolizer\Финал'
DST = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'листинг')
BRAIN = ['H2_Brain_v5.ino', 'h2_protocol.h', 'lcd2004_i2c.h', 'h2_oled.h', 'h2_sound.h', 'h2_events.h', 'build_opt.h']
REMOTE = ['H2_Remote_v5.ino', 'h2_protocol.h', 'h2_oled.h', 'remote_game.h']

WEB = re.compile(r'веб|Веб|ВЕБ|web|Web|WEB|Wi-?Fi|WI-FI|WIFI|браузер|телефон|смартфон|(?<![A-Za-z])OTA(?![A-Za-z])|captive|CAPTIVE|'
                 r'https?:|WebSocket|websocket|эндпоинт|endpoint|JSON|/status|/snd|/log|/settime|/calibrate|'
                 r'/setfans|/reset|/setpressure|/settimer|/setgas|/maint|/cmd|/update|async_tcp|AsyncTCP|'
                 r'ядр[оеау] 0|HTTP|HTML|точк[аиу] доступа|DNS|localStorage|дашборд')


def rd(p): return io.open(p, encoding='utf-8-sig').read().split('\n')
def wr(p, L): io.open(p, 'w', encoding='utf-8', newline='\n').write('\n'.join(L))


def idx(L, s, start=0, exact=False):
    for i in range(start, len(L)):
        if (L[i] == s) if exact else (s in L[i]):
            return i
    raise SystemExit('НЕ НАЙДЕНО: ' + s)


def cut(L, a, b, a_off=0, b_off=0, b_exact=False):
    """удалить строки от строки с a (+a_off) до строки с b (+b_off) включительно"""
    i = idx(L, a) + a_off
    j = idx(L, b, i, b_exact) + b_off
    del L[i:j + 1]


def drop(L, s):
    i = idx(L, s); del L[i]


def section_head(L, title):
    """индекс строки '// ====' перед заголовком раздела title"""
    i = idx(L, title)
    assert L[i - 1].startswith('// ====='), title
    return i - 1


def strip_comments(L, fn):
    """комментарии с упоминанием веба/Wi-Fi: строку-комментарий убрать, хвост кода – отрезать"""
    out, gone = [], 0
    def ind(s): return len(s) - len(s.lstrip())
    i, n = 0, len(L)
    while i < n:
        s = L[i]; t = s.strip()
        if t.startswith('//'):
            # блок подряд идущих комментариев одного отступа убирается целиком, чтобы не
            # оставались оборванные фразы
            j = i
            while j < n and L[j].strip().startswith('//') and ind(L[j]) == ind(s): j += 1
            if any(WEB.search(x) for x in L[i:j]):
                gone += j - i
            else:
                out.extend(L[i:j])
            i = j; continue
        if WEB.search(s) and t.startswith('*'):
            gone += 1; i += 1; continue
        m = re.match(r'^(.*?\S)(\s*//.*)$', s)
        if m and '"' not in m.group(2) and WEB.search(m.group(2)):
            s = m.group(1); gone += 1
            # продолжение хвостового комментария на следующих строках
            while i + 1 < n and L[i + 1].strip().startswith('//') and ind(L[i + 1]) > ind(s):
                i += 1; gone += 1
        out.append(s); i += 1
    # подряд идущие пустые строки комментариев/кода не плодим
    res = []
    for s in out:
        if s.strip() == '' and res and res[-1].strip() == '' and len(res) > 1 and res[-2].strip() == '':
            continue
        res.append(s)
    print(f'{fn}: убрано/обрезано комментариев – {gone}')
    return res


HEADER = r'''/*
 * =====================================================================================
 *  ПРОШИВКА УСТРОЙСТВА КОНТРОЛЯ (ESP32 DevKit v1, ESP32-WROOM-32)
 * =====================================================================================
 *
 *  НАЗНАЧЕНИЕ
 *  ----------
 *  Управление установкой получения водорода: регулирование давления в баке
 *  включением электролизера через транзисторный ключ, управление продувочным
 *  клапаном, пьезоэлектрическим поджигом и выводным вентилятором через модуль
 *  реле, аварийные защиты по давлению, наклону корпуса и температуре, вывод
 *  параметров на индикатор, звуковая сигнализация и двусторонний обмен с
 *  устройством удаленного управления по радиоканалу nRF24L01+.
 *
 *  ПОДКЛЮЧЕНИЕ (см. схему электрическую принципиальную)
 *  ----------------------------------------------------
 *    Радиомодуль nRF24L01+ (SPI): CE – GPIO4, CSN – GPIO5, SCK – GPIO18,
 *        MISO – GPIO19, MOSI – GPIO23, питание 3,3 В, конденсатор 470 мкФ.
 *    Шина I2C: SDA – GPIO21, SCL – GPIO22; датчик наклона MPU-6050 (0x68),
 *        индикатор LCD2004 с PCF8574 (0x27/0x3F) через преобразователь
 *        уровней BSS138 или индикатор SSD1306 (0x3C).
 *    Транзисторный ключ электролизера – GPIO32 (активный высокий уровень,
 *        резистор 10 кОм затвор – общий провод).
 *    Модуль реле: выводной вентилятор – GPIO25, клапан – GPIO26,
 *        поджиг – GPIO27.
 *    Звуковой излучатель – GPIO16.
 *    Кнопки (INPUT_PULLUP): SB1 «Питание» – GPIO13, SB2 «Блокировка» – GPIO14,
 *        SB3 «Продувка» – GPIO33.
 *    Светодиоды: HL1 «Питание» – GPIO17, HL2 «Блокировка» – GPIO12,
 *        HL3 «Режим» – GPIO2.
 *    Датчик давления 0–10 бар (0,5–4,5 В) через делитель 10/20 кОм – GPIO36.
 *    Датчик температуры DS18B20 (1-Wire, 4,7 кОм к 3,3 В) – GPIO15.
 *
 *  РАДИОКАНАЛ
 *  ----------
 *    2 Мбит/с, CRC 16 бит, каналы 76, 100, 120 (смена канала при потере связи).
 *    Пакет состояния передается каждые 100 мс, команды принимаются по приходу.
 *
 *  ТАБЛИЦА КОМАНД
 *  --------------
 *    1 – вкл./выкл. системы;   2 – блокировка, сброс защит;
 *    3 – импульс подачи газа с поджигом;   4 – поджиг (500 мс);
 *    5 – короткая продувка (200 мс);   7 – ручная подача газа;
 *    8 / 9 – целевое давление +0,5 / –0,5 бар;   12 – отключить звук тревоги;
 *    99 – продувка до 0,05 бар (не более 90 с) и выключение.
 *
 *  ЗАЩИТЫ
 *  ------
 *    Давление выше 6,8 бар, наклон корпуса более 30°, неисправность датчика
 *    давления или утечка – отключение ключа и реле, блокировка до сброса
 *    кнопкой «Блокировка», звуковой сигнал с повтором каждые 15 с.
 *    Перегрев (75 °C и выше) – предупреждение и включение выводного вентилятора.
 *    Настройки хранятся во Flash-памяти (NVS) с контрольной суммой CRC-16.
 * =====================================================================================
 */'''


REMOTE_HEADER = r'''/*
 * =====================================================================================
 *  ПРОШИВКА УСТРОЙСТВА УДАЛЕННОГО УПРАВЛЕНИЯ (ESP32 DevKit v1, ESP32-WROOM-32)
 * =====================================================================================
 *
 *  НАЗНАЧЕНИЕ
 *  ----------
 *  Передача команд устройству контроля по радиоканалу nRF24L01+, прием пакетов
 *  состояния, отображение давления, температуры и режимов на индикаторе
 *  SSD1306, светодиодная и звуковая индикация, переход в режим глубокого сна
 *  при отсутствии нажатий кнопок в течение 2 мин.
 *
 *  ПОДКЛЮЧЕНИЕ (см. схему электрическую принципиальную)
 *  ----------------------------------------------------
 *    Радиомодуль nRF24L01+ (SPI): CE – GPIO4, CSN – GPIO5, SCK – GPIO18,
 *        MISO – GPIO19, MOSI – GPIO23, питание 3,3 В, конденсатор 470 мкФ.
 *    Индикатор SSD1306 (I2C): SDA – GPIO21, SCL – GPIO22.
 *    Кнопки (INPUT_PULLUP): SB4 «Питание» – GPIO16, SB5 «Блокировка» – GPIO17,
 *        SB6 «Мелодия» – GPIO34 (резистор 10 кОм к 3,3 В), SB7 «Импульс газа» –
 *        GPIO25, SB8 «Ручная подача газа» – GPIO26, SB9 «Поджиг» – GPIO27,
 *        SB10 «Давление +0,5 бар» – GPIO32, SB11 «Давление –0,5 бар» – GPIO33.
 *    Светодиоды: HL4 «Передача» – GPIO14, HL5 «Питание» – GPIO13,
 *        HL6 «Связь» – GPIO12.
 *    Звуковой излучатель – GPIO2.
 *
 *  РАБОТА
 *  ------
 *    Кнопка SB4: одно нажатие – вкл./выкл. установки, двойное – продувка до нуля.
 *    Кнопка SB5: короткое нажатие – блокировка, удержание 1 с – отключение звука
 *    тревоги. Светодиод HL6 после включения мигает 4 с с частотой 2 Гц, затем горит
 *    при наличии связи и мигает 1 Гц при ее отсутствии. При потере связи на 5 с
 *    выполняется переход на следующий частотный канал (76, 100, 120). Выход из
 *    режима глубокого сна – кнопками SB6 и SB11.
 * =====================================================================================
 */'''

FIX = [   # комментарии, расходившиеся со схемой (излучатель подключен к выводу напрямую)
    ('// ⚡ v6.0: зуммер мозга (через NPN-ключ). true — пассивный', '// ⚡ v6.0: звуковой излучатель мозга. true — пассивный'),
    ('//  ЗУММЕР МОЗГА (PIN_BUZZER, через NPN-ключ)', '//  ЗУММЕР МОЗГА (PIN_BUZZER)'),
    ('// база NPN-ключа через 1 кОм (на GPIO2 — синий LED платы)', '// звуковой излучатель (на GPIO2 — синий LED платы)'),
]


def fix(L):
    out = []
    for s in L:
        for a, b in FIX: s = s.replace(a, b)
        out.append(s)
    return out


def brain():
    d = os.path.join(DST, 'H2_Brain_v5'); os.makedirs(d, exist_ok=True)
    for f in BRAIN: shutil.copy(os.path.join(SRC, 'H2_Brain_v5', f), d)
    p = os.path.join(d, 'H2_Brain_v5.ino'); L = rd(p)

    end = idx(L, ' */', exact=True)
    L[0:end + 1] = HEADER.split('\n')
    for s in ('#include <WiFi.h>', '#include <DNSServer.h>', '#include <ESPAsyncWebServer.h>',
              '#include <AsyncTCP.h>', '#include <Update.h>', 'T_WS_HEARTBEAT  = 1000'):
        drop(L, s)
    i = section_head(L, '//  WI-FI'); j = idx(L, 'AP_PASSWORD', i); del L[i:j + 1]
    i = section_head(L, '//  ВЕБ + WEBSOCKET'); j = idx(L, 'DNSServer dnsServer;', i); del L[i:j + 1]
    cut(L, '// ⚡ v4.4: OTA. ⚡ v6.3:', 'OTA_STALL_MS   = 20000UL')
    cut(L, '// ⚡ v5.0: WebSocket', '(⚡ v6.0: ставится и из веба')
    i = section_head(L, '//  HTML'); j = idx(L, ')rawliteral";', i); del L[i:j + 1]
    for s in ('void  wsBroadcastState();', 'void  wsTick(unsigned long now);', 'void  onWsEvent('):
        drop(L, s)
    i = idx(L, '// ⚡ v6.1: РАЗНОС ПО ЯДРАМ'); j = idx(L, 'void executeCommand(uint8_t cmd) {', i); del L[i:j]
    i = section_head(L, 'ФОРМИРОВАНИЕ JSON СОСТОЯНИЯ'); j = section_head(L, 'ЗВУКОВЫЕ СОБЫТИЯ'); del L[i:j]
    i = section_head(L, 'ВЕБ — строгий разбор'); j = section_head(L, '//  SETUP'); del L[i:j]
    cut(L, '// ⚡ v6.1: очередь команд веба', 'if (!webCmdQueue) Serial.println')
    L[idx(L, 'работа только через веб-интерфейс')] = \
        '  } else { radioOk = false; Serial.println("NRF24: FAIL — радиоканал недоступен"); }'
    cut(L, '  // Wi-Fi AP', 'Serial.println("WebServer + WebSocket: OK')
    drop(L, 'lastWsHeartbeat = millis();')
    i = idx(L, 'static bool wasOta = false;'); j = idx(L, '// ⚡ v6.0: звуковые события', i); L[i:j] = ['']
    cut(L, '// ⚡ v6.3: captive portal DNS.', 'dnsServer.processNextRequest();')
    cut(L, '// ⚡ v6.1: команды веба (поставлены в очередь', 'drainWebCommands();')
    L = [s for s in L if s.strip() != 'wsTick(now);' and 'v5.0: WebSocket — push' not in s]
    L = [re.sub(r'\s*wsStateChanged\s*=\s*true;(\s*//.*)?', '', s) for s in L]
    L = [s for s in L if s.strip() != '']  if False else L
    wr(p, strip_comments(fix(L), 'H2_Brain_v5.ino'))

    p = os.path.join(d, 'h2_sound.h'); L = rd(p)
    i = section_head(L, '//  HTTP /snd/*'); del L[i:]
    L = [s.replace(' || otaInProgress', '') for s in L]
    L = [re.sub(r'\s*wsStateChanged\s*=\s*true;(\s*//.*)?', '', s) for s in L]
    wr(p, strip_comments(fix(L), 'h2_sound.h'))

    p = os.path.join(d, 'h2_events.h'); L = rd(p)
    i = idx(L, 'void evtRegisterHttp() {'); del L[i:]
    L = [re.sub(r'\s*wsStateChanged\s*=\s*true;(\s*//.*)?', '', s) for s in L]
    wr(p, strip_comments(L, 'h2_events.h'))

    for f in ('h2_protocol.h', 'lcd2004_i2c.h', 'h2_oled.h'):
        p = os.path.join(d, f); wr(p, strip_comments(rd(p), f))


def remote():
    d = os.path.join(DST, 'H2_Remote_v5'); os.makedirs(d, exist_ok=True)
    for f in REMOTE: shutil.copy(os.path.join(SRC, 'H2_Remote_v5', f), d)
    p = os.path.join(d, 'H2_Remote_v5.ino'); L = rd(p)
    end = idx(L, ' */', exact=True)
    L[0:end + 1] = REMOTE_HEADER.split('\n')
    wr(p, strip_comments(fix(L), 'H2_Remote_v5.ino'))
    for f in REMOTE[1:]:
        p = os.path.join(d, f); wr(p, strip_comments(rd(p), f))
    shutil.copy(os.path.join(DST, 'H2_Brain_v5', 'h2_protocol.h'), d)   # копии протокола одинаковые
    shutil.copy(os.path.join(DST, 'H2_Brain_v5', 'h2_oled.h'), d)


brain(); remote()
for root, _, fs in os.walk(DST):
    for f in fs:
        if f.endswith(('.ino', '.h')):
            for n, s in enumerate(rd(os.path.join(root, f)), 1):
                if WEB.search(s): print('ОСТАЛОСЬ', f, n, s.strip()[:110])
print('ok')

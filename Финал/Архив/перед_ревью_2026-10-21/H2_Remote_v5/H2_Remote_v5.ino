/*
 * =====================================================================================
 *  H2 CONTROL SYSTEM — ПУЛЬТ v6.0  (ESP32 DevKit v1)
 * =====================================================================================
 *
 *  Адаптирован под мозг v6.0. Мозг и пульт прошивать ОДНОВРЕМЕННО: пакет статуса
 *  стал 16 байт и добавлен пакет нот TonePacket (общий h2_protocol.h v6.0).
 *
 *  ⚡ НОВОЕ В v6.0:
 *    • LED «СТАТУС/СВЯЗЬ» на GPIO12:
 *        – при загрузке быстро мигает (5 Гц) от аппаратного таймера esp_timer —
 *          мигание идёт даже во время инициализации OLED/NRF24 — до тех пор, пока
 *          не придёт первый статус от мозга, но не меньше 1.5 с от старта;
 *        – дальше: связь с мозгом есть → горит постоянно;
 *                  связи нет        → медленно мигает 1 Гц (500/500 мс);
 *        – перед глубоким сном гаснет.
 *    • ЗУММЕР на GPIO2 (пассивный, ШИМ LEDC, громкость 1..10 скважностью):
 *        – мелодии (загрузка, включение, тревоги, пользовательские) ведёт МОЗГ —
 *          он присылает каждую ноту пакетом TonePacket в момент её начала, пульт
 *          играет её сразу (прерывая свой звук) и САМ глушит через durMs —
 *          страховка от «залипшего» писка при потере следующего пакета;
 *        – свои звуки пульта (неблокирующий мини-плеер по millis(), без delay()):
 *            щелчок 4 кГц / 15 мс при отправке команды кнопкой (если щелчки вкл.);
 *            «связь потеряна» (800 Гц → пауза → 600 Гц) при пропадании мозга;
 *          звучат, только если в веб-интерфейсе мозга (вкладка «Звук») выбран
 *          вывод «Пульт» или «Оба»;
 *        – настройки звука (вывод, щелчки, громкость) приходят в байте sound
 *          статуса; последний известный байт хранится в RTC-памяти и переживает
 *          глубокий сон (до первой связи по умолчанию: «Оба», щелчки, громкость 10).
 *        – восьмая кнопка запускает локальную телефонную тему «Мобильник»
 *          из «Бумера» по кругу и останавливает повторным нажатием. Остальные
 *          звуки и команды продолжают работать через мозг.
 *    • StatusPacket 16 байт: bit1 сообщает состояние реле выводного вентилятора;
 *      корпусный получает постоянное питание и не контролируется ESP32.
 *      (на OLED «25°F2»: 2 — выводной вкл, «-» — выкл) и байт sound.
 *    • Выбор драйвера MOSFET/РЕЛЕ убран: электролизёр на мозге управляется только
 *      MOSFET'ом с отдельного пина (GPIO32). В режиме АВТО на OLED: «AUTO T=…».
 *    • Мигание LED TX при ошибке отправки — без delay() (не сбивает тайминг звука).
 *    • Перед глубоким сном модуль NRF24 переводится в power-down.
 *
 *  ⚡ ИЗ v5.0:
 *    • Температура мозга на OLED (temp в StatusPacket).
 *    • Резервный канал NRF24 (channel hopping): пульт синхронно с мозгом
 *      перебирает список каналов RF_CHANNELS при пропадании связи, пока не
 *      найдёт активный. Защита от помех на одной частоте.
 *    • ГЛУБОКИЙ СОН (deep sleep) при простое: если ни одна кнопка не нажата
 *      в течение IDLE_SLEEP_MS (по умолч. 2 минуты), пульт уходит в сон для
 *      экономии батареи. Пробуждение — TARGET− (GPIO33, ext0) либо
 *      МЕЛОДИЯ (GPIO34, ext1). После пробуждения пульт сразу
 *      восстанавливает связь. На внешнем питании (USB) сон можно отключить
 *      через DEEP_SLEEP_ENABLE = false.
 *    • Бит6/бит7 в flags2 — watchdog/overheat: пульт показывает
 *      "БАК НЕ НАБИРАЕТ" и "ПЕРЕГРЕВ" на OLED.
 *
 *  ⚡ ИЗ v4.4:
 *    • Кнопка РЕПУЛЬСОР (cmd=3) — одноразовый выстрел газ+поджиг.
 *    • OLED дублирует экран мозга, показывает гистерезис.
 *
 *  ОБЩИЕ ВОЗМОЖНОСТИ:
 *    • NRF24 на 2 Мбит/с, двусторонняя связь
 *    • Приём статуса каждые 100 мс (10 Гц) и нот зуммера от мозга
 *    • Структура StatusPacket (16 байт): pressure, target, flags, flags2,
 *      tiltAngle, ext, temp, fans, sound
 *    • Индикация TILT-сработки и текущего угла наклона на OLED
 *    • Кнопка LOCK сбрасывает все защёлки (LOCK, TILT, overpressure)
 *    • Регулировка целевого давления двумя кнопками ±0.5 бар
 *    • POWER: одиночный = вкл/выкл, двойной (≤400 мс) = продувка до нуля
 *
 *  ПРОБУЖДЕНИЕ ИЗ СНА:
 *    Будит ТОЛЬКО кнопка TGT− (GPIO 33) — ext0 на одном RTC-пине (ext1 с
 *    ALL_LOW потребовал бы зажать обе кнопки 32+33 одновременно).
 *    Будить по POWER (GPIO 16) нельзя — он не RTC-пин.
 *    Нажатие для пробуждения не отправляет команду, только включает пульт.
 *
 *  СХЕМА ПОДКЛЮЧЕНИЯ
 *  -----------------
 *    NRF24L01:
 *        CE=GPIO4   CSN=GPIO5   SCK=GPIO18   MISO=GPIO19   MOSI=GPIO23
 *        VCC=3.3V + конденсатор 470 мкФ × 10 В между VCC и GND модуля!
 *
 *    OLED SSD1306 (I2C, 128×64):
 *        SDA=GPIO21   SCL=GPIO22   VCC=3.3V   адрес 0x3C
 *
 *    КНОПКИ (INPUT_PULLUP, замыкаются на GND):
 *        GPIO16 → POWER             1× = cmd 1 (вкл/выкл)
 *                                   2× или удержание ≥ 1 с = cmd 99 (продувка до нуля)
 *        GPIO17 → LOCK              по отпусканию: cmd 2 (блокировка + сброс защёлок);
 *                                   ⚡ v6.2: удержание ≥ 1 с = cmd 12 (заглушить тревогу)
 *        GPIO25 → РЕПУЛЬСОР         cmd 3 (выстрел 800мс газ + 100мс поджиг)
 *        GPIO26 → РУЧНАЯ ПОДАЧА     cmd 7
 *        GPIO27 → ПОДЖИГ            cmd 4 (500 мс)
 *        GPIO32 → TARGET +0.5 бар   cmd 8
 *        GPIO33 → TARGET -0.5 бар   cmd 9
 *        GPIO34 → МЕЛОДИЯ           локальная тема «Бумера», повторное нажатие — стоп.
 *                 GPIO34 не имеет внутренней подтяжки: нужен резистор 10 кОм
 *                 между GPIO34 и 3.3 В; кнопка замыкает GPIO34 на GND.
 *
 *    СВЕТОДИОДЫ (GPIO → резистор 220 Ом → анод LED, катод → GND):
 *        GPIO14 → LED зелёный  (мигает при отправке: 1 вспышка = OK, 2 = ошибка)
 *        GPIO13 → LED синий    (горит — пульт включён)
 *        GPIO12 → ⚡ v6.0: LED «СТАТУС/СВЯЗЬ» (поведение — см. «НОВОЕ В v6.0»).
 *                 GPIO12 — strapping-пин (MTDI, выбор напряжения flash): при
 *                 старте он ОБЯЗАН быть в LOW. LED с резистором на GND как раз
 *                 удерживает его в LOW. НЕ подключать этот LED к +3.3 В и не
 *                 ставить на GPIO12 подтяжку вверх — пульт не загрузится!
 *
 *    ⚡ v6.0: ЗУММЕР (пассивный, через NPN-ключ — ток зуммера не идёт через GPIO):
 *        GPIO2 → 1 кОм → база S8050 / 2N2222
 *        эмиттер → GND
 *        зуммер: «+» → +5 В (VIN/USB), «−» → коллектор
 *        диод 1N4148 параллельно зуммеру (катод к +5 В) — гасит выбросы катушки
 *        На GPIO2 сидит встроенный синий LED платы — он подмигивает в такт
 *        звуку, это нормально.
 *        Активный зуммер (со встроенным генератором): BUZZER_PASSIVE = false —
 *        тогда нота = просто «включить на длительность», частота и громкость
 *        игнорируются.
 *
 *  ПИТАНИЕ: PowerBank → micro-USB / 3×AA батарейки
 *
 *  ВАЖНО: скорость NRF24 на пульте и мозге ДОЛЖНА совпадать (2 Мбит/с)!
 *         Структуры пакетов байт-в-байт идентичны с мозгом (h2_protocol.h).
 * =====================================================================================
 */

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <esp_sleep.h>     // ⚡ v5.0: глубокий сон
#include <esp_timer.h>     // ⚡ v6.0: периодический таймер мигания LED статуса при загрузке
#include <esp_task_wdt.h>  // ⚡ v6.2: сторожевой таймер loop
#include "h2_protocol.h"   // ⚡ общий протокол: CmdPacket/StatusPacket/TonePacket, типы
                           //    пакетов PKT_*, вывод звука SND_OUT_*, трубы, каналы,
                           //    MIN/MAX_TARGET_PRESSURE (единый для мозга и пульта)


// ======================== ПИНЫ NRF24 ========================
#define PIN_NRF_CE   4
#define PIN_NRF_CSN  5


// ======================== КНОПКИ ========================
// POWER обрабатывается отдельно (двойной клик), остальные — простой клик.
const uint8_t NUM_SIMPLE_BTNS = 7;
const int     SIMPLE_BTN_PINS[NUM_SIMPLE_BTNS]   = { 17,  25,  26,  27,  32,  33,  34 };
const uint8_t SIMPLE_BTN_CMDS[NUM_SIMPLE_BTNS]   = {  2,   3,   7,   4,   8,   9,  11 };
const char*   SIMPLE_BTN_LABELS[NUM_SIMPLE_BTNS] = {
  "LOCK/RESET", "РЕПУЛЬСОР", "MANUAL", "PIEZO", "TGT +0.5", "TGT -0.5", "MELODY"
};

#define PIN_BTN_POWER   16


// ======================== СВЕТОДИОДЫ ========================
#define PIN_LED_TX     14
#define PIN_LED_POWER  13
#define PIN_LED_STATUS 12   // ⚡ v6.0: LED «СТАТУС/СВЯЗЬ» (на GND — держит strapping-пин в LOW)


// ======================== ⚡ v6.0: ЗУММЕР ========================
#define PIN_BUZZER      2      // база NPN-ключа через 1 кОм (на GPIO2 — синий LED платы)
#define BUZZER_PASSIVE  true   // true  = пассивный зуммер (ШИМ-тон, мелодии);
                               // false = активный (нота = «включить на длительность»)
#define BUZZER_LEDC_CH  0      // канал LEDC — только для ядра 2.x (в 3.x выбирается сам)
const uint32_t BUZZER_ATTACH_HZ = 2000;   // частота при подключении LEDC (дальше — у каждой ноты своя)
const uint8_t  BUZZER_BITS      = 10;     // разрешение ШИМ для тона и громкости
// Громкость 0..10 → скважность (10 бит). 512 = 50 % — максимум громкости пассивного зуммера.
const uint16_t BUZZER_VOL_DUTY[11] = {0, 6, 12, 24, 40, 64, 96, 144, 220, 340, 512};
const uint16_t TONE_MIN_HZ = 50;          // допустимые частоты ноты (как в формате мелодий H2M1)
const uint16_t TONE_MAX_HZ = 10000;
const uint16_t TONE_MAX_MS = 10000;       // самая длинная нота H2M1 — предел автостопа

// Нота локального звука — тот же формат, что у плеера мозга: f = 0 — пауза.
struct Note { uint16_t f; uint16_t d; };
const Note    MEL_CLICK[]       = { {4000, 15} };                          // щелчок кнопки
const uint8_t MEL_CLICK_LEN     = sizeof(MEL_CLICK) / sizeof(MEL_CLICK[0]);
const Note    MEL_LINK_LOST[]   = { {800, 150}, {0, 100}, {600, 250} };   // «связь потеряна»
const uint8_t MEL_LINK_LOST_LEN = sizeof(MEL_LINK_LOST) / sizeof(MEL_LINK_LOST[0]);
// Длинная одноголосная версия «Мобильника» для пассивного зуммера (~27 с):
// основной рингтон по RTTTL https://pushkinnn.narod.ru/mobila/11.htm,
// затем перебор Em–C–Am–G по гитарной аранжировке
// https://gtptabs.com/tabs/54/bumer/saundtrek-2.html и возвращение темы.
// Это адаптация, а не запись оригинального трека. Основной рингтон поднят
// на 4 октавы от RTTTL; проигрывание остаётся неблокирующим.
const Note MEL_BOOMER[] = {
  // Тема, первый проход.
  {1319, 240}, {1568, 480}, {0, 960}, {1568, 240}, {1319, 480}, {0, 960},
  {1760, 240}, {1568, 240}, {1760, 240}, {1568, 240}, {1760, 240}, {1568, 240},
  {1760, 240}, {1568, 240}, {1760, 240}, {1976, 480}, {0, 480},
  // Второй проход, как в инструментальной аранжировке.
  {1319, 240}, {1568, 480}, {0, 960}, {1568, 240}, {1319, 480}, {0, 960},
  {1760, 240}, {1568, 240}, {1760, 240}, {1568, 240}, {1760, 240}, {1568, 240},
  {1760, 240}, {1568, 240}, {1760, 240}, {1976, 480}, {0, 480},
  // Одноголосный перебор аккордов Em–C–Am–G (каждый такт 1920 мс).
  {1319, 240}, {1568, 240}, {1976, 480}, {1568, 480}, {1319, 480},
  {1047, 240}, {1319, 240}, {1568, 480}, {1319, 480}, {1047, 480},
  {880, 240}, {1047, 240}, {1319, 480}, {1047, 480}, {880, 480},
  {784, 240}, {988, 240}, {1175, 480}, {988, 480}, {784, 480}, {0, 240},
  // Возвращение темы и пауза перед повтором всей последовательности.
  {1319, 240}, {1568, 480}, {0, 960}, {1568, 240}, {1319, 480}, {0, 960},
  {1760, 240}, {1568, 240}, {1760, 240}, {1568, 240}, {1760, 240}, {1568, 240},
  {1760, 240}, {1568, 240}, {1760, 240}, {1976, 480}, {0, 480}
};
static_assert(sizeof(MEL_BOOMER) / sizeof(MEL_BOOMER[0]) <= 255,
              "MEL_BOOMER: индексы плеера (sndSeqIdx/sndSeqLen) — uint8_t, не больше 255 нот");
const uint8_t MEL_BOOMER_LEN = sizeof(MEL_BOOMER) / sizeof(MEL_BOOMER[0]);
const uint8_t BOOMER_MAX_VOL    = 8;    // ограничиваем резкость звука на маленьком зуммере
const uint8_t BOOMER_ATTACK_MS  = 8;    // плавное начало убирает щелчок
const uint8_t BOOMER_RELEASE_MS = 10;   // плавный конец ноты
const uint8_t BOOMER_GATE_MS    = 20;   // короткая тишина разделяет повторяющиеся ноты

// Приоритеты (как у плеера мозга): новый локальный звук вытесняет текущий, только если
// его приоритет ≥ приоритета звучащего. Нота от мозга прерывает любой локальный звук.
const uint8_t SPRIO_CLICK = 0;   // щелчок кнопки
const uint8_t SPRIO_TONE  = 1;   // нота мелодии от мозга
const uint8_t SPRIO_ALARM = 2;   // «связь потеряна»
const uint8_t SPRIO_BOOMER = 1;


// ======================== OLED ========================
Adafruit_SSD1306 oled(128, 64, &Wire, -1);
bool oledOk = false;


// ======================== NRF24 ========================
RF24 radio(PIN_NRF_CE, PIN_NRF_CSN);
// Трубы PIPE_R2B/PIPE_B2R и структуры пакетов вынесены в общий h2_protocol.h
// (включён выше) — протокол байт-в-байт идентичен с прошивкой мозга.


// ======================== ТАЙМИНГИ ========================
const unsigned long T_DEBOUNCE     = 50;
const unsigned long T_DOUBLE_CLICK = 400;
const unsigned long T_RF_TIMEOUT   = 3000;
const unsigned long T_DISPLAY_UPD  = 200;
const unsigned long T_LED_BLINK    = 60;     // вспышка LED TX при успешной отправке
const unsigned long T_LED_FAIL_ON  = 80;     // ⚡ v6.0: ошибка отправки — 2 вспышки 80/60 мс
const unsigned long T_LED_FAIL_OFF = 60;
const unsigned long T_SHOW_TARGET  = 2500;   // Спецэкран установки давления, как на мозге

// ⚡ v6.0: LED статуса
// Требование задания на КП: при включении отдельный светодиод мигает 3–5 с с частотой не более 2–3 Гц.
const uint64_t      BOOT_BLINK_PERIOD_US = 250000;   // стартовое мигание: переключение каждые 250 мс (2 Гц)
const unsigned long T_BOOT_BLINK_MIN     = 4000;     // длительность стартового мигания, мс от старта
const unsigned long T_LINK_BLINK         = 500;      // без связи: 500 мс горит / 500 мс нет (1 Гц)

// Пороги MIN/MAX_TARGET_PRESSURE и каналы RF_CHANNELS/RF_CHANNEL_CNT — в общем
// h2_protocol.h (единый источник для мозга и пульта).

// ⚡ Период перебора НАМЕРЕННО не равен мозговому (4000 мс): если обе стороны
// ищут друг друга с ОДИНАКОВЫМ периодом, они могут прыгать «в противофазе»
// (постоянное смещение на 1 канал) и не встретиться никогда. Разные периоды
// сдвигают фазу на каждом цикле и гарантируют пересечение на общем канале.
const unsigned long RF_HOP_SILENCE = 5000;   // мс тишины до прыжка на след. канал
const unsigned long RF_SCAN_FAST   = 1200;   // быстрый скан после старта/пробуждения

// ⚡ v5.0: Глубокий сон при простое
const bool          DEEP_SLEEP_ENABLE = true;     // false если пульт на USB-питании
const unsigned long IDLE_SLEEP_MS     = 120000;   // 2 мин без нажатий → сон
// Пробуждение из сна: ext0 — кнопка TGT− (GPIO 33, LOW); ⚡ v6.0: ext1 ALL_LOW с маской
// из одного пина — кнопка МЕЛОДИЯ (GPIO 34, LOW, внешняя подтяжка 10 кОм к 3,3 В).
// Каждый источник будит по нажатию одной кнопки.


// ======================== СОСТОЯНИЕ (от мозга) ========================
float   remPressure  = 0.0f;
float   remTarget    = 1.0f;
float   prevRemTarget = -1.0f;       // Для детектирования изменения target
uint8_t remFlags     = 0;
uint8_t remFlags2    = 0;
uint8_t remTiltAngle = 0;
uint8_t remExt       = 0;            // ⚡ v6.2: доп. состояния EXT_* (бывший гистерезис)
int8_t  remTemp      = 25;           // ⚡ v5.0: температура
uint8_t remFans      = 0;            // bit0 зарезервирован, bit1 = выводной вентилятор
bool    brainOnline  = false;
unsigned long lastRxTime = 0;
unsigned long lastTargetChange = 0;  // Когда мозг последний раз изменил target

// ⚡ v6.0: последний известный байт настроек звука (StatusPacket.sound). RTC_DATA_ATTR —
// переживает глубокий сон: сразу после пробуждения (ещё до связи с мозгом) щелчки и
// громкость соответствуют настройкам из веба. Значение при первом включении 0xA6:
// вывод «Оба» (SND_OUT_BOTH = 2) | щелчки вкл (bit2) | громкость 10 (bit4..7).
RTC_DATA_ATTR uint8_t remSound = 0xA6;

// ⚡ v5.0: RF channel hopping
uint8_t       rfChannelIdx   = 0;
unsigned long rfLastActivity = 0;

// ⚡ v5.0: deep sleep
unsigned long lastActivityMs = 0;    // последнее нажатие кнопки

bool r_sysOn()    { return remFlags  & (1 << 0); }
bool r_locked()   { return remFlags  & (1 << 1); }
bool r_purging()  { return remFlags  & (1 << 2); }
bool r_repulsor() { return remFlags  & (1 << 3); }  // ⚡ v4.3 (бывший r_burner)
bool r_manual()   { return remFlags  & (1 << 4); }
bool r_electro()  { return remFlags  & (1 << 5); }  // ⚡ v6.0: MOSFET электролизёра открыт
bool r_valve()    { return remFlags  & (1 << 6); }
bool r_piezo()    { return remFlags  & (1 << 7); }
bool r_ovWarn()   { return remFlags2 & (1 << 0); }
bool r_ovLock()   { return remFlags2 & (1 << 1); }
bool r_purgeShort(){return remFlags2 & (1 << 2); }
bool r_tiltLock() { return remFlags2 & (1 << 3); }
bool r_tiltCal()  { return remFlags2 & (1 << 4); }
// flags2 bit5 — резерв (в v5 был выбор драйвера MOSFET/РЕЛЕ, в v6.0 убран)
bool r_watchdog() { return remFlags2 & (1 << 6); }  // ⚡ v5.0: бак не набирает
bool r_overheat() { return remFlags2 & (1 << 7); }  // ⚡ v5.0: перегрев
bool r_psFault()  { return remExt & EXT_PSENS_FAULT; }  // ⚡ v6.2: датчик давления неисправен
bool r_leakLock() { return remExt & EXT_LEAK_LOCK; }
bool r_leakWarn() { return remExt & EXT_LEAK_WARN; }
bool r_maint()    { return remExt & EXT_MAINT_DUE; }
bool r_fanExh()   { return remFans   & (1 << 1); }  // ⚡ v6.0: выводной вентилятор вкл

// ⚡ v6.0: разбор байта sound (громкость 0 = «не задана» → 10, как в протоколе)
uint8_t normVolume(uint8_t v) { return (v == 0 || v > 10) ? 10 : v; }
uint8_t s_out()     { return remSound & 0x03; }              // SND_OUT_*
bool    s_clicks()  { return remSound & (1 << 2); }          // щелчки кнопок разрешены
uint8_t s_volume()  { return normVolume(remSound >> 4); }    // 1..10


// ======================== АНТИДРЕБЕЗГ ========================
bool          simpleLastRaw[NUM_SIMPLE_BTNS];
bool          simpleStable[NUM_SIMPLE_BTNS];
unsigned long simpleDebTime[NUM_SIMPLE_BTNS];

bool          pwrLastRaw   = HIGH;
bool          pwrStable    = HIGH;
unsigned long pwrDebTime   = 0;
unsigned long pwrLastClick = 0;


// ======================== ⚡ v6.0: LED СТАТУСА ========================
esp_timer_handle_t bootBlinkTimer = nullptr;   // таймер мигания загрузки (nullptr — остановлен)


// ======================== ⚡ v6.0: МИНИ-ПЛЕЕР ========================
const Note*   sndSeq       = nullptr;  // локальная последовательность (nullptr — нота от мозга)
uint8_t       sndSeqLen    = 0;
uint8_t       sndSeqIdx    = 0;        // индекс звучащей ноты последовательности
uint8_t       sndVol       = 10;       // громкость текущего звука 1..10
uint8_t       sndPrio      = 0;        // приоритет текущего звука (SPRIO_*)
bool          sndActive    = false;    // идёт нота/пауза: отсчёт sndNoteDur от sndNoteStart
unsigned long sndNoteStart = 0;
uint16_t      sndNoteDur   = 0;
bool          boomerPlaying = false;   // восьмая кнопка: локальная мелодия по кругу
unsigned long lastBrainToneMs = 0;     // последняя нота от мозга (пауза перед возобновлением «Бумера»)
const unsigned long T_BOOMER_RESUME = 1500;   // столько мс тишины от мозга — и «Бумер» продолжается
uint16_t buzzerCurrentDuty = 0;              // не обновлять LEDC без изменения громкости


// ======================== ПРОЧЕЕ ========================
char          lastCmdLabel[16] = "---";
// ⚡ v6.0: неблокирующее мигание LED TX (раньше ошибка отправки мигала через delay()
// ≈ 280 мс — это «растягивало» щелчок и ноты зуммера)
uint8_t       ledTxPulses      = 0;     // сколько вспышек осталось (включая текущую)
bool          ledTxLit         = false;
unsigned long ledTxNext        = 0;     // момент следующего переключения LED TX
unsigned long ledTxOnMs        = 0;
unsigned long ledTxOffMs       = 0;
// ⚡ ТЗ 7.3: перерисовка OLED только при изменении отображаемых данных
unsigned long lastDispCheck    = 0;   // последняя проверка (троттлинг T_DISPLAY_UPD)
unsigned long lastDispDraw     = 0;   // время последней реальной перерисовки
char          lastDispSig[110] = "";  // сигнатура отображаемых полей


// =====================================================================================
//  ⚡ v6.0: LED «СТАТУС/СВЯЗЬ»
//  При включении LED мигает от esp_timer (250 мс, 2 Гц) — независимо от того, чем
//  занят setup() (инициализация OLED/NRF24 с задержками). Через T_BOOT_BLINK_MIN
//  (4 с) таймер останавливается в loop, и LED показывает состояние связи.
// =====================================================================================
static void bootBlinkCb(void* arg) {
  (void)arg;
  static bool lit = true;          // setup() включает LED перед запуском таймера
  lit = !lit;
  digitalWrite(PIN_LED_STATUS, lit ? HIGH : LOW);
}

void startBootBlink() {
  esp_timer_create_args_t args = {};
  args.callback = &bootBlinkCb;    // dispatch по умолчанию — задача esp_timer (не ISR)
  args.name     = "bootLed";
  if (esp_timer_create(&args, &bootBlinkTimer) != ESP_OK) {
    bootBlinkTimer = nullptr;      // без таймера LED сразу перейдёт к логике связи в loop
    return;
  }
  if (esp_timer_start_periodic(bootBlinkTimer, BOOT_BLINK_PERIOD_US) != ESP_OK) {
    esp_timer_delete(bootBlinkTimer);
    bootBlinkTimer = nullptr;
  }
}

void stopBootBlink() {
  if (!bootBlinkTimer) return;
  esp_timer_stop(bootBlinkTimer);
  esp_timer_delete(bootBlinkTimer);
  bootBlinkTimer = nullptr;
}

void updateStatusLed(unsigned long now) {
  if (bootBlinkTimer) {
    if (now < T_BOOT_BLINK_MIN) return;   // стартовое мигание ещё идёт
    stopBootBlink();
    Serial.printf("[LED] Стартовое мигание завершено (%lu мс), мозг %s\n", now, brainOnline ? "найден" : "не найден");
  }
  // Пишем уровень каждый проход (а не только при смене): если колбэк таймера
  // успел переключить LED в момент остановки, следующий же проход это исправит.
  bool lit = brainOnline || ((now / T_LINK_BLINK) % 2 == 0);
  digitalWrite(PIN_LED_STATUS, lit ? HIGH : LOW);
}


// =====================================================================================
//  ⚡ v6.0: ЗУММЕР — низкий уровень
//  Ядро ESP32 3.x сменило LEDC API: ledcSetup+ledcAttachPin → ledcAttach, а
//  ledcWrite принимает ПИН вместо канала. Поддерживаем обе версии. Перед сменой
//  частоты глушим выход: ledcWriteTone сам включает 50 % и давал резкий щелчок.
// =====================================================================================
uint16_t dutyForVolume(uint8_t vol) {
  return BUZZER_VOL_DUTY[vol > 10 ? 10 : vol];
}

void buzzerSetDuty(uint16_t duty) {
#if BUZZER_PASSIVE
  if (duty == buzzerCurrentDuty) return;
 #if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PIN_BUZZER, duty);
 #else
  ledcWrite(BUZZER_LEDC_CH, duty);
 #endif
  buzzerCurrentDuty = duty;
#else
  (void)duty;
#endif
}

void buzzerInit() {
#if BUZZER_PASSIVE
 #if ESP_ARDUINO_VERSION_MAJOR >= 3
  if (!ledcAttach(PIN_BUZZER, BUZZER_ATTACH_HZ, BUZZER_BITS))   // core 3.x
    Serial.println("LEDC: FAIL — зуммер не работает (нет канала)!");
  ledcWrite(PIN_BUZZER, 0);
 #else
  ledcSetup(BUZZER_LEDC_CH, BUZZER_ATTACH_HZ, BUZZER_BITS);     // core 2.x
  ledcAttachPin(PIN_BUZZER, BUZZER_LEDC_CH);
  ledcWrite(BUZZER_LEDC_CH, 0);
 #endif
#else
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
#endif
}

void buzzerTone(uint16_t freq, uint8_t vol) {
#if BUZZER_PASSIVE
  if (freq < TONE_MIN_HZ) freq = TONE_MIN_HZ;
  if (freq > TONE_MAX_HZ) freq = TONE_MAX_HZ;
 #if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PIN_BUZZER, 0);
  buzzerCurrentDuty = 0;
  if (!ledcChangeFrequency(PIN_BUZZER, freq, BUZZER_BITS)) return;
 #else
  ledcWrite(BUZZER_LEDC_CH, 0);
  buzzerCurrentDuty = 0;
  if (!ledcSetup(BUZZER_LEDC_CH, freq, BUZZER_BITS)) return;
 #endif
  buzzerSetDuty(dutyForVolume(vol));
#else
  (void)freq; (void)vol;           // активный зуммер: частоту задаёт его генератор
  digitalWrite(PIN_BUZZER, HIGH);
#endif
}

void buzzerSilence() {
#if BUZZER_PASSIVE
  buzzerSetDuty(0);
#else
  digitalWrite(PIN_BUZZER, LOW);
#endif
}


// =====================================================================================
//  ⚡ v6.0: МИНИ-ПЛЕЕР ПУЛЬТА (неблокирующий, по millis() в loop)
//  Одна «дорожка»: либо нота от мозга (TonePacket), либо локальная последовательность
//  (щелчок, «связь потеряна»). У каждой ноты/паузы есть длительность — по её
//  истечении updateSound() включает следующую ноту или глушит зуммер. Для нот мозга
//  это и есть автостоп: если следующий пакет потерялся, писк не «залипнет».
// =====================================================================================
void soundStop() {
  buzzerSilence();
  sndActive = false;
  sndSeq    = nullptr;
}

void soundBeginNote(uint16_t f, uint16_t d, unsigned long now) {
  // У локальной темы громкость поднимется в updateSound(): старт с нуля
  // устраняет щелчок. Звуки мозга и кнопок сохраняют прежнюю громкость.
  if (f > 0) buzzerTone(f, sndSeq == MEL_BOOMER ? 0 : sndVol);
  else       buzzerSilence();          // пауза внутри последовательности
  sndNoteStart = now;
  sndNoteDur   = d;
  sndActive    = true;
}

// Локальные звуки пульта звучат, только если мозг выводит звук на пульт.
bool localSoundAllowed() {
  return s_out() == SND_OUT_REMOTE || s_out() == SND_OUT_BOTH;
}

void soundPlayLocal(const Note* seq, uint8_t len, uint8_t prio) {
  if (!localSoundAllowed() || len == 0) return;
  if (sndActive && prio < sndPrio) return;     // не перебиваем более важный звук
  sndSeq    = seq;
  sndSeqLen = len;
  sndSeqIdx = 0;
  sndPrio   = prio;
  sndVol    = s_volume();
  soundBeginNote(seq[0].f, seq[0].d, millis());
}

// Нота от мозга: играет немедленно, прерывая любой локальный звук. freq=0 — стоп.
void soundPlayRemote(uint16_t freq, uint16_t durMs, uint8_t vol, unsigned long now) {
  lastBrainToneMs = now;               // «Бумер» не вклинивается в паузы мелодии мозга
  if (freq == 0 || durMs == 0) { soundStop(); return; }
  sndSeq  = nullptr;
  sndPrio = SPRIO_TONE;
  sndVol  = normVolume(vol);
  soundBeginNote(freq, durMs > TONE_MAX_MS ? TONE_MAX_MS : durMs, now);
}

void updateSound(unsigned long now) {
  if (!sndActive) return;
  unsigned long age = now - sndNoteStart;
  if (sndSeq == MEL_BOOMER && sndSeq[sndSeqIdx].f != 0 && age < sndNoteDur) {
    uint16_t gateAt = sndNoteDur > BOOMER_GATE_MS ? sndNoteDur - BOOMER_GATE_MS : sndNoteDur;
    uint16_t maxDuty = dutyForVolume(sndVol > BOOMER_MAX_VOL ? BOOMER_MAX_VOL : sndVol);
    uint16_t duty = 0;
    if (age < gateAt) {
      duty = maxDuty;
      if (age < BOOMER_ATTACK_MS) duty = maxDuty * age / BOOMER_ATTACK_MS;
      unsigned long left = gateAt - age;
      if (left < BOOMER_RELEASE_MS) {
        uint16_t fading = maxDuty * left / BOOMER_RELEASE_MS;
        if (fading < duty) duty = fading;
      }
    }
    buzzerSetDuty(duty);
  }
  if (age < sndNoteDur) return;
  if (sndSeq && ++sndSeqIdx < sndSeqLen) {
    soundBeginNote(sndSeq[sndSeqIdx].f, sndSeq[sndSeqIdx].d, now);
    return;
  }
  soundStop();                         // конец звука / автостоп ноты мозга
}

void toggleBoomer(unsigned long now) {
  boomerPlaying = !boomerPlaying;
  lastActivityMs = now;
  if (!boomerPlaying && sndSeq == MEL_BOOMER) soundStop();
  strncpy(lastCmdLabel, boomerPlaying ? "BOOMER ON" : "BOOMER OFF", sizeof(lastCmdLabel) - 1);
  lastCmdLabel[sizeof(lastCmdLabel) - 1] = '\0';
  Serial.printf("[MELODY] Boomer %s\n", boomerPlaying ? "ON" : "OFF");
}

void updateBoomer(unsigned long now) {
  if (!boomerPlaying || sndActive) return;  // тон мозга / тревога имеют приоритет
  // Мозг недавно играл на пульте: в паузах его мелодии/тревоги «Бумер» не стартуем
  if (lastBrainToneMs != 0 && now - lastBrainToneMs < T_BOOMER_RESUME) return;
  sndSeq    = MEL_BOOMER;
  sndSeqLen = MEL_BOOMER_LEN;
  sndSeqIdx = 0;
  sndPrio   = SPRIO_BOOMER;
  sndVol    = s_volume();
  soundBeginNote(sndSeq[0].f, sndSeq[0].d, now);
}


// =====================================================================================
//  ⚡ v6.0: LED TX — неблокирующие вспышки
// =====================================================================================
void ledTxBlink(uint8_t pulses, unsigned long onMs, unsigned long offMs) {
  ledTxPulses = pulses;
  ledTxOnMs   = onMs;
  ledTxOffMs  = offMs;
  ledTxLit    = true;
  digitalWrite(PIN_LED_TX, HIGH);
  ledTxNext   = millis() + onMs;
}

void updateLedTx(unsigned long now) {
  if (ledTxPulses == 0 || (long)(now - ledTxNext) < 0) return;
  if (ledTxLit) {
    digitalWrite(PIN_LED_TX, LOW);
    ledTxLit = false;
    if (--ledTxPulses == 0) return;
    ledTxNext = now + ledTxOffMs;
  } else {
    digitalWrite(PIN_LED_TX, HIGH);
    ledTxLit  = true;
    ledTxNext = now + ledTxOnMs;
  }
}


// =====================================================================================
//  ОТПРАВКА КОМАНДЫ
// =====================================================================================
void sendCmd(uint8_t cmd, const char* label) {
  lastActivityMs = millis();   // ⚡ v5.0: любое нажатие сбрасывает таймер сна
  // ⚡ v6.0: щелчок через неблокирующий мини-плеер — отзвучит сам в loop
  if (s_clicks()) soundPlayLocal(MEL_CLICK, MEL_CLICK_LEN, SPRIO_CLICK);

  CmdPacket pkt = { PKT_CMD, cmd };
  radio.stopListening();
  bool ok = radio.write(&pkt, sizeof(pkt));
  radio.startListening();

  strncpy(lastCmdLabel, label, sizeof(lastCmdLabel) - 1);
  lastCmdLabel[sizeof(lastCmdLabel) - 1] = '\0';

  // ⚡ v6.0: индикация без delay(): OK — одна вспышка, ошибка — две
  if (ok) ledTxBlink(1, T_LED_BLINK, 0);
  else    ledTxBlink(2, T_LED_FAIL_ON, T_LED_FAIL_OFF);
  Serial.printf("[TX] %s (%d) -> %s\n", label, cmd, ok ? "OK" : "FAIL");
}


// =====================================================================================
//  OLED — ПОЛНОЕ ДУБЛИРОВАНИЕ ЭКРАНА МОЗГА
//  Все экраны и тексты идентичны мозговой версии updateDisplay() для
//  максимальной предсказуемости: что видит человек у мозга — то же видит
//  и у пульта, без расхождений в формулировках.
//
//  Список экранов (в порядке приоритета):
//    1. ❌ Связь потеряна — спец-экран "МОЗГ НЕ ОТВЕЧАЕТ"
//    2. ⚙ TILT-калибровка идёт на мозге — "КАЛИБРОВКА MPU..."
//    3. 🎯 Только что изменилось целевое давление — крупный экран на 2.5 сек
//    4. 📊 Основной экран — заголовок, давление, угол, полоска, E/V/P, статус
// =====================================================================================
void updateDisplay() {
  if (!oledOk) return;
  unsigned long now = millis();

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);

  // ── 1. Нет связи с мозгом ───────────────────────────────────────────────
  if (!brainOnline) {
    oled.setTextSize(1);
    oled.setCursor(0, 0);  oled.print("H2 REMOTE");
    oled.setCursor(80, 0); oled.print("RF:--");
    oled.drawLine(0, 9, 127, 9, SSD1306_WHITE);
    oled.setCursor(0, 18); oled.print("BRAIN OFFLINE");
    oled.setCursor(0, 30); oled.print("Check power");
    oled.setCursor(0, 40); oled.print("and NRF24 link");
    oled.setCursor(0, 52); oled.printf("2 Mbps, ch %d", RF_CHANNELS[rfChannelIdx]);
    oled.display();
    return;
  }

  // ── 2. Мозг сейчас калибрует MPU6050 ───────────────────────────────────
  if (r_tiltCal()) {
    oled.setTextSize(1);
    oled.setCursor(0, 0);  oled.print("H2 REMOTE");
    oled.setCursor(80, 0); oled.print("RF:OK");
    oled.drawLine(0, 9, 127, 9, SSD1306_WHITE);
    oled.setCursor(0, 18); oled.print("BRAIN: CALIBRATING");
    oled.setCursor(0, 30); oled.print("MPU6050...");
    oled.setCursor(0, 44); oled.print("Do not move it!");
    oled.display();
    return;
  }

  // ── 3. Экран установки целевого давления (2.5 сек после изменения) ─────
  //      Полностью идентичен спецэкрану мозга
  if (now - lastTargetChange < T_SHOW_TARGET) {
    oled.setTextSize(1);
    oled.setCursor(8, 0); oled.print("TARGET PRESSURE");
    oled.drawLine(0, 9, 127, 9, SSD1306_WHITE);
    oled.setTextSize(3);
    char tBuf[8]; dtostrf(remTarget, 4, 1, tBuf);
    oled.setCursor(8, 14); oled.print(tBuf);
    oled.setTextSize(1);
    oled.setCursor(100, 22); oled.print("bar");
    int tw = constrain((int)(remTarget / MAX_TARGET_PRESSURE * 127.0f), 0, 127);
    oled.drawRect(0, 42, 128, 5, SSD1306_WHITE);
    if (tw > 0) oled.fillRect(1, 43, tw, 3, SSD1306_WHITE);
    oled.setCursor(0, 50);
    oled.printf("%.1f         %.1f bar", MIN_TARGET_PRESSURE, MAX_TARGET_PRESSURE);
    oled.display();
    return;
  }

  // ── 4. ОСНОВНОЙ ЭКРАН — копия мозгового ────────────────────────────────

  // Заголовок (как на мозге, но "H2 REMOTE" вместо "H2 PRO" чтобы человек
  // мог отличить, на какое устройство он смотрит)
  oled.setTextSize(1);
  oled.setCursor(0, 0); oled.print("H2 REMOTE");
  oled.setCursor(48, 0); oled.print("RF:OK");
  oled.setCursor(86, 0);
  if (r_tiltLock())        oled.print("TILT!");
  else if (r_ovLock())     oled.print("!OVP");
  else if (r_purging())    oled.print("PURG");
  else if (r_purgeShort()) oled.print("PURG");
  else                     oled.print(r_sysOn() ? (r_locked() ? "LOCK" : "ON") : "OFF");
  oled.drawLine(0, 9, 127, 9, SSD1306_WHITE);

  // Давление крупно — точь-в-точь как на мозге
  oled.setTextSize(2);
  oled.setCursor(0, 12);
  char pBuf[8]; dtostrf(remPressure, 4, 2, pBuf);
  oled.print(pBuf);
  oled.setTextSize(1);
  oled.setCursor(86, 18); oled.print("bar");

  // Угол наклона справа от давления
  oled.setCursor(108, 12); oled.printf("%3d", remTiltAngle);
  oled.setCursor(108, 22); oled.print("deg");

  // Полоска давления 0..6 бар + маркер цели
  const float SCALE = 7.0f;   // шкала давления 0..7 бар (запас над аварийным 6.8)
  int bw = constrain((int)(remPressure / SCALE * 127.0f), 0, 127);
  oled.drawRect(0, 30, 128, 5, SSD1306_WHITE);
  if (bw > 0) oled.fillRect(1, 31, bw, 3, SSD1306_WHITE);
  int tx = constrain((int)(remTarget / SCALE * 127.0f), 0, 127);
  oled.drawLine(tx, 28, tx, 36, SSD1306_WHITE);
  oled.drawLine(tx-1, 28, tx+1, 28, SSD1306_WHITE);
  oled.drawLine(0, 38, 127, 38, SSD1306_WHITE);

  // E/V/P + температура/вентиляторы (как на мозге v6.0)
  oled.setCursor(0, 41);
  oled.print("E:"); oled.print(r_electro() ? "ON " : "OFF");
  oled.print(" V:"); oled.print(r_valve()   ? "ON " : "OFF");
  oled.print(" P:"); oled.print(r_piezo()   ? "ON"  : "OF");
  char tgtBuf[6]; dtostrf(remTarget, 3, 1, tgtBuf);
  // Справа температура и управляемый выводной вентилятор.
  oled.setCursor(86, 41);
  oled.printf("%2d\xF8" "F%c", (int)remTemp, r_fanExh() ? '2' : '-');

  // Статус / режим — те же тексты что в мозге updateDisplay()
  oled.setCursor(0, 52);
  if (r_tiltLock())            { oled.print("TILT! "); oled.print(remTiltAngle); oled.print(" deg LOCK"); }
  else if (r_ovLock())         oled.print(">RELEASE PRESSURE!<");
  else if (r_psFault())        oled.print("!P-SENSOR FAULT!");     // ⚡ v6.2
  else if (r_leakLock())       oled.print("!LEAK! press LOCK");    // ⚡ v6.2
  else if (r_overheat())       { oled.print("!OVERHEAT "); oled.print((int)remTemp); oled.print("C!"); }
  else if (r_purging())        { oled.print("PURGE "); oled.print(pBuf); oled.print(" bar"); }
  else if (r_purgeShort())     oled.print("MINI-PURGE 200ms");
  else if (r_repulsor())       oled.print(">>REPULSOR FIRE!<<");
  else if (!r_sysOn())         { oled.print("OFF  T="); oled.print(tgtBuf); }
  else if (r_locked())         oled.print("LOCKED");
  else if (r_watchdog())       oled.print("!NO PRESSURE RISE!");
  else if (r_manual())         oled.print("MANUAL GAS");
  else if (r_leakWarn())       { oled.print("!LEAK? T="); oled.print(tgtBuf); }
  else if (r_maint())          { oled.print("SERVICE! T="); oled.print(tgtBuf); }
  else {
    // ⚡ v6.2: АВТО — цель (гистерезис убран: MOSFET ВКЛ при P < цели)
    oled.print("AUTO T="); oled.print(tgtBuf); oled.print(" bar");
  }

  oled.display();
}

// ⚡ ТЗ 7.3: перерисовывать OLED не чаще раза в T_DISPLAY_UPD и ТОЛЬКО при изменении
// отображаемых данных (либо принудительно раз в 1 с, либо на «живом» экране показа
// установки цели). Убирает мерцание и снижает нагрузку на шину I2C.
void maybeUpdateDisplay(unsigned long now) {
  if (!oledOk) return;
  if (now - lastDispCheck < T_DISPLAY_UPD) return;
  lastDispCheck = now;
  char sig[110];
  snprintf(sig, sizeof(sig), "%d|%u|%u|%.2f|%.1f|%u|%d|%u|%u",
    brainOnline ? 1 : 0, (unsigned)remFlags, (unsigned)remFlags2,
    remPressure, remTarget, (unsigned)remTiltAngle, (int)remTemp,
    (unsigned)remFans, (unsigned)remExt);
  bool animated = (now - lastTargetChange < T_SHOW_TARGET);   // «живой» экран цели
  bool forced   = (now - lastDispDraw >= 1000);
  if (animated || forced || strcmp(sig, lastDispSig) != 0) {
    updateDisplay();
    strncpy(lastDispSig, sig, sizeof(lastDispSig) - 1);
    lastDispSig[sizeof(lastDispSig) - 1] = '\0';
    lastDispDraw = now;
  }
}


// =====================================================================================
//  POWER: 1× = вкл/выкл, 2× = продувка; ⚡ v6.2: удержание ≥ 1 с = продувка
//  (как и двойной клик: первое нажатие уже переключило питание, удержание
//  добавляет продувку до нуля — результат тот же, что у привычного 2×).
// =====================================================================================
const unsigned long T_LONG_PRESS = 1000;
unsigned long pwrPressMs   = 0;      // момент нажатия (0 — отпущена)
bool          pwrLongSent  = false;  // удержание уже обработано
bool          pwrPurgeSent = false;  // продувка уже ушла двойным кликом

void handlePowerBtn(unsigned long now) {
  bool raw = digitalRead(PIN_BTN_POWER);
  if (raw != pwrLastRaw) pwrDebTime = now;
  if ((now - pwrDebTime) > T_DEBOUNCE && raw != pwrStable) {
    pwrStable = raw;
    if (pwrStable == LOW) {
      pwrPressMs  = now;
      pwrLongSent = false;
      pwrPurgeSent = (now - pwrLastClick) < T_DOUBLE_CLICK;
      if (pwrPurgeSent) sendCmd(99, "SAFE OFF");
      else              sendCmd(1,  "POWER");
      pwrLastClick = now;
    } else {
      pwrPressMs = 0;
    }
  }
  if (pwrStable == LOW && pwrPressMs && !pwrLongSent && now - pwrPressMs >= T_LONG_PRESS) {
    pwrLongSent = true;
    if (!pwrPurgeSent) sendCmd(99, "SAFE OFF");
  }
  pwrLastRaw = raw;
}


// =====================================================================================
//  Простые кнопки
// =====================================================================================
// ⚡ v6.2: LOCK (GPIO 17) срабатывает по ОТПУСКАНИЮ: короче 1 с — обычный LOCK
// (cmd 2), удержание ≥ 1 с — «заглушить тревогу» (cmd 12) без смены блокировки.
// Нельзя слать LOCK по нажатию: тогда попытка заглушить тревогу снимала бы
// блокировку и сбрасывала защёлки.
const int     PIN_BTN_LOCK  = 17;
unsigned long lockPressMs   = 0;
bool          lockLongSent  = false;

void handleSimpleBtns(unsigned long now) {
  for (uint8_t i = 0; i < NUM_SIMPLE_BTNS; i++) {
    bool raw = digitalRead(SIMPLE_BTN_PINS[i]);
    if (raw != simpleLastRaw[i]) simpleDebTime[i] = now;
    if ((now - simpleDebTime[i]) > T_DEBOUNCE && raw != simpleStable[i]) {
      simpleStable[i] = raw;
      bool isLock = SIMPLE_BTN_PINS[i] == PIN_BTN_LOCK;
      if (simpleStable[i] == LOW) {
        if (SIMPLE_BTN_PINS[i] == 34) toggleBoomer(now);
        else if (isLock) { lockPressMs = now; lockLongSent = false; lastActivityMs = now; }
        else sendCmd(SIMPLE_BTN_CMDS[i], SIMPLE_BTN_LABELS[i]);
      } else if (isLock && lockPressMs) {
        if (!lockLongSent) sendCmd(SIMPLE_BTN_CMDS[i], SIMPLE_BTN_LABELS[i]);   // короткое — LOCK
        lockPressMs = 0;
      }
    }
    simpleLastRaw[i] = raw;
  }
  // Удержание LOCK ≥ 1 с — заглушить тревогу (один раз за нажатие)
  if (lockPressMs && !lockLongSent && now - lockPressMs >= T_LONG_PRESS) {
    lockLongSent = true;
    sendCmd(CMD_SILENCE_ALARM, "MUTE ALARM");
    soundStop();                       // и свой звук пульта (например «связь потеряна»)
  }
}


// =====================================================================================
//  SETUP
// =====================================================================================
void setup() {
  // ⚡ v6.0: САМАЯ ПЕРВАЯ операция — LED статуса и таймер мигания загрузки
  // (мигает всё время, пока идёт setup() и пока не найден мозг).
  pinMode(PIN_LED_STATUS, OUTPUT);
  digitalWrite(PIN_LED_STATUS, HIGH);
  startBootBlink();

  Serial.begin(115200);
  Serial.println("\n=== H2 PRO Remote v6.0 ===");

  // ⚡ v6.0: зуммер — сразу в тишину
  buzzerInit();

  for (uint8_t i = 0; i < NUM_SIMPLE_BTNS; i++) {
    pinMode(SIMPLE_BTN_PINS[i], SIMPLE_BTN_PINS[i] == 34 ? INPUT : INPUT_PULLUP);
    // Стартуем с ФАКТИЧЕСКОГО уровня пина, а не HIGH: если проснулись от
    // удержанной кнопки TGT− (GPIO 33), фронта HIGH→LOW не возникает и
    // команда 9 (TGT −0.5) НЕ уходит — пробуждение лишь включает пульт,
    // как и заявлено. Команда уйдёт только при следующем полном нажатии.
    bool lvl = digitalRead(SIMPLE_BTN_PINS[i]);
    simpleLastRaw[i] = lvl;
    simpleStable[i]  = lvl;
    simpleDebTime[i] = 0;
  }
  pinMode(PIN_BTN_POWER, INPUT_PULLUP);

  pinMode(PIN_LED_TX,    OUTPUT); digitalWrite(PIN_LED_TX,    LOW);
  pinMode(PIN_LED_POWER, OUTPUT); digitalWrite(PIN_LED_POWER, HIGH);

  Wire.begin(21, 22);
  oledOk = oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!oledOk) Serial.println("OLED: нет");
  else {
    oled.cp437(true);   // корректная таблица CP437: \xF8 = знак градуса
    oled.clearDisplay(); oled.setTextSize(1); oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(12, 20); oled.print("H2 REMOTE v6.0");
    oled.setCursor(20, 36); oled.print("NRF24 @ 2 Mbps");
    oled.display();
    Serial.println("OLED: OK");
  }

  SPI.begin(18, 19, 23, PIN_NRF_CSN);
  if (!radio.begin()) {
    Serial.println("NRF24: FAIL");
    for (int i = 0; i < 3; i++) {
      digitalWrite(PIN_LED_TX, HIGH); delay(200);
      digitalWrite(PIN_LED_TX, LOW);  delay(200);
    }
  } else {
    radio.openWritingPipe(PIPE_R2B);
    radio.openReadingPipe(1, PIPE_B2R);
    radio.setPALevel(RF24_PA_MAX);
    radio.setDataRate(RF24_2MBPS);
    radio.setCRCLength(RF24_CRC_16);
    // ⚡ Ограничиваем автоповторы: при отсутствующем мозге radio.write() команды
    // не должен блокировать пульт надолго. 3 попытки × 500 мкс ≈ 1.5 мс макс
    // (вместо 15 попыток дефолтом). Команды редкие, надёжность сохраняется.
    radio.setRetries(2, 3);
    rfChannelIdx = 0;
    radio.setChannel(RF_CHANNELS[rfChannelIdx]);   // ⚡ v5.0: канал из массива
    radio.startListening();
    Serial.printf("NRF24: OK (2 Мбит/с, канал %d, +резерв)\n", RF_CHANNELS[rfChannelIdx]);
    for (int i = 0; i < 2; i++) {
      digitalWrite(PIN_LED_TX, HIGH); delay(80);
      digitalWrite(PIN_LED_TX, LOW);  delay(80);
    }
  }

  // ⚡ v5.0: инициализация таймеров RF и сна
  rfLastActivity = millis();
  lastActivityMs = millis();

  // Если проснулись из сна — сообщим в Serial
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("[WAKE] Пробуждение из сна кнопкой TGT-.");
  } else if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1) {
    Serial.println("[WAKE] Пробуждение из сна кнопкой МЕЛОДИЯ.");
    toggleBoomer(millis());
  }

  // ⚡ v6.2: сторожевой таймер loop — зависание дольше 5 с → перезагрузка пульта
  // (Arduino сам сбрасывает его после каждой итерации loop)
  esp_task_wdt_config_t wdtCfg = {};
  wdtCfg.timeout_ms     = 5000;
  wdtCfg.idle_core_mask = 0;
  wdtCfg.trigger_panic  = true;
  esp_err_t we = esp_task_wdt_reconfigure(&wdtCfg);
  if (we == ESP_ERR_INVALID_STATE) we = esp_task_wdt_init(&wdtCfg);
  if (we == ESP_OK) enableLoopWDT();
  else Serial.printf("WDT: FAIL (%d)\n", (int)we);

  Serial.println("Пульт готов:");
  Serial.println("  GPIO16 → POWER (1× вкл/выкл, 2× или удержание — продувка)");
  for (uint8_t i = 0; i < NUM_SIMPLE_BTNS; i++) {
    if (SIMPLE_BTN_PINS[i] == 34) Serial.println("  GPIO34 → MELODY (локально: Boomer)");
    else Serial.printf("  GPIO%d → %s (cmd=%d)\n",
      SIMPLE_BTN_PINS[i], SIMPLE_BTN_LABELS[i], SIMPLE_BTN_CMDS[i]);
  }
  Serial.printf("  GPIO%d → LED статуса, GPIO%d → зуммер (%s)\n",
    PIN_LED_STATUS, PIN_BUZZER, BUZZER_PASSIVE ? "пассивный" : "активный");
  Serial.printf("  Звук (последние настройки мозга): вывод=%u щелчки=%d громкость=%u\n",
    (unsigned)s_out(), s_clicks() ? 1 : 0, (unsigned)s_volume());
}


// =====================================================================================
//  ⚡ v5.0: ГЛУБОКИЙ СОН
// =====================================================================================
void goToSleep() {
  // ⚡ v6.0: тишина и остановка таймера мигания — до заставки сна (delay ниже)
  soundStop();
  stopBootBlink();
  // Показываем уведомление на OLED перед сном
  if (oledOk) {
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setCursor(10, 20); oled.print("SLEEP MODE");
    oled.setCursor(2, 34);  oled.print("TGT-/MELODY wake");
    oled.display();
    delay(1500);
    // Гасим панель напрямую командой (без clearDisplay+display перед DISPLAYOFF —
    // лишняя перерисовка не нужна, важно именно выключить контроллер дисплея)
    oled.ssd1306_command(SSD1306_DISPLAYOFF);
  }
  digitalWrite(PIN_LED_POWER, LOW);
  digitalWrite(PIN_LED_TX, LOW);
  digitalWrite(PIN_LED_STATUS, LOW);   // ⚡ v6.0
  // ⚡ v6.0: NRF24 в power-down (иначе модуль в режиме приёма тянет ~13 мА всё время
  // сна). После пробуждения radio.begin() в setup() снова включает его.
  radio.powerDown();
  // ⚡ ИСПРАВЛЕНО: ext1 с ALL_LOW будит только когда ВСЕ пины маски в LOW
  // одновременно (т.е. надо зажать обе кнопки). Это неверно для пробуждения
  // «по любой кнопке». На классическом ESP32 для пробуждения по нажатию ОДНОЙ
  // кнопки (активный уровень LOW) используем ext0 на одном RTC-пине.
  // Будим по TGT− (GPIO 33). Кнопка на GND, внутренняя подтяжка вверх, нажатие = LOW = 0.
  esp_sleep_enable_ext0_wakeup((gpio_num_t)33, 0);
  esp_sleep_enable_ext1_wakeup(1ULL << 34, ESP_EXT1_WAKEUP_ALL_LOW);
  Serial.println("[SLEEP] Пробуждение TGT- (GPIO33) или МЕЛОДИЯ (GPIO34).");
  Serial.flush();
  esp_deep_sleep_start();
  // после пробуждения выполнится setup() заново (это not a return point)
}


// =====================================================================================
//  LOOP
// =====================================================================================
void loop() {
  unsigned long now = millis();

  // ⚡ v6.0: приём — читаем весь payload (статическая длина, до 32 байт) и разбираем
  // по первому байту. По одной трубе идут статус (10 Гц) и ноты зуммера, поэтому за
  // проход вычитываем до 3 пакетов (глубина RX FIFO nRF24) — нота не ждёт лишний цикл.
  for (uint8_t n = 0; n < 3 && radio.available(); n++) {
    uint8_t buf[RF_PAYLOAD_MAX];
    uint8_t len = radio.getPayloadSize();
    if (len > RF_PAYLOAD_MAX) len = RF_PAYLOAD_MAX;
    radio.read(buf, len);

    if (buf[0] == PKT_STATUS && len >= sizeof(StatusPacket)) {
      StatusPacket pkt;
      memcpy(&pkt, buf, sizeof(pkt));
      remPressure  = pkt.pressure;
      // Если target изменился — запоминаем время для показа спецэкрана
      if (prevRemTarget < 0) {
        prevRemTarget = pkt.target;            // первый приём — без спецэкрана
      } else if (fabsf(pkt.target - prevRemTarget) > 0.01f) {
        lastTargetChange = now;                // мозг изменил target
        prevRemTarget    = pkt.target;
      }
      remTarget    = pkt.target;
      remFlags     = pkt.flags;
      remFlags2    = pkt.flags2;
      remTiltAngle = pkt.tiltAngle;
      remExt       = pkt.ext;         // ⚡ v6.2: доп. состояния EXT_*
      remTemp      = pkt.temp;        // ⚡ v5.0
      remFans      = pkt.fans;        // ⚡ v6.0: вентиляторы на реле
      remSound     = pkt.sound;       // ⚡ v6.0: настройки звука (в RTC — переживут сон)
      lastRxTime   = now;
      rfLastActivity = now;           // ⚡ v5.0: связь жива на этом канале
    } else if (buf[0] == PKT_TONE && len >= sizeof(TonePacket)) {
      TonePacket tp;
      memcpy(&tp, buf, sizeof(tp));
      soundPlayRemote(tp.freq, tp.durMs, tp.volume, now);   // ⚡ v6.0: нота от мозга
      rfLastActivity = now;           // мозг слышен на этом канале
      lastActivityMs = now;           // во время мелодии не уходим в глубокий сон
    }
  }

  bool wasOnline = brainOnline;
  brainOnline = (now - lastRxTime < T_RF_TIMEOUT) && (lastRxTime > 0);
  if (wasOnline && !brainOnline) {    // ⚡ v6.0: связь с мозгом пропала
    Serial.println("[RF] Связь с мозгом потеряна");
    soundPlayLocal(MEL_LINK_LOST, MEL_LINK_LOST_LEN, SPRIO_ALARM);
  }

  // ⚡ v5.0: резервный канал — если тишина, прыгаем на следующий.
  // Пока мозг ни разу не услышан (старт или пробуждение из deep sleep) —
  // сканируем быстро (1.2 с/канал): пока пульт спал, мозг мог прыгать по
  // каналам. Быстрый скан 3 каналов находит его за ≤4 с, и экран сразу
  // показывает актуальное состояние.
  unsigned long hopSilence = (lastRxTime == 0) ? RF_SCAN_FAST : RF_HOP_SILENCE;
  if (now - rfLastActivity >= hopSilence) {
    rfChannelIdx = (rfChannelIdx + 1) % RF_CHANNEL_CNT;
    radio.setChannel(RF_CHANNELS[rfChannelIdx]);
    rfLastActivity = now;
  }

  handlePowerBtn(now);
  handleSimpleBtns(now);

  updateLedTx(millis());     // ⚡ v6.0: вспышки LED TX без delay()
  updateSound(millis());     // ⚡ v6.0: мини-плеер и автостоп тона (millis(): щелчок мог
                             //          стартовать в sendCmd позже, чем снят now)
  updateBoomer(millis());    // зацикленная мелодия; не задерживает RF и кнопки
  updateStatusLed(now);      // ⚡ v6.0: LED статуса/связи

  maybeUpdateDisplay(now);

  // ⚡ v5.0: глубокий сон при длительном простое (экономия батареи)
  // ⚡ v6.0: millis(), а не now — sendCmd() ставит lastActivityMs = millis() уже после
  // снятия now; разность «now − lastActivityMs» ушла бы в переполнение, и пульт
  // засыпал бы сразу после нажатия кнопки.
  if (DEEP_SLEEP_ENABLE && !boomerPlaying && (millis() - lastActivityMs >= IDLE_SLEEP_MS)) {
    goToSleep();
  }
}

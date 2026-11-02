/*
 * =====================================================================================
 *  H2 CONTROL SYSTEM — МОЗГ v5.0  (ESP32 DevKit v1, WROOM-32D)
 * =====================================================================================
 *
 *  ⚡ НОВОЕ В v5.0 (большое обновление):
 *    • ОХЛАЖДЕНИЕ: 4-pin вентилятор (PWM + тахометр). ШИМ-сигнал 25 кГц на
 *      GPIO 32 задаёт скорость, тахометр на GPIO 16 даёт реальные обороты (RPM),
 *      которые видны в веб-интерфейсе. MOSFET НЕ нужен — у 4-pin вентилятора
 *      свой драйвер, управляет сигнальная линия. Скорость растёт линейно от
 *      FAN_TEMP_MIN до FAN_TEMP_MAX. Порог и кривую можно менять в веб-интерфейсе.
 *      Режимы: авто (по температуре) / ручной (фикс. скорость) / выкл.
 *    • ДАТЧИК ТЕМПЕРАТУРЫ: выбор типа через #define TEMP_SENSOR_TYPE —
 *      DS18B20 (цифровой, 1-Wire, GPIO 15) либо NTC-термистор (аналоговый,
 *      ADC GPIO 34). Калибруется температура и кривая вентилятора через веб.
 *    • КАЛИБРОВКА ДАВЛЕНИЯ через веб: 2-точечная (offset + scale), сохраняется
 *      в NVS. Эндпоинт /calibrate. Больше не нужно править формулу в коде.
 *    • СЧЁТЧИК ЦИКЛОВ РЕЛЕ: считает срабатывания каждого реле (электролизер,
 *      клапан, пьезо), сохраняет в NVS. Предупреждение при выработке ресурса.
 *      Видно в веб-интерфейсе, сброс кнопкой.
 *    • WEBSOCKET: дашборд реального времени вместо HTTP-polling. Мозг push-ит
 *      состояние мгновенно при изменении + раз в 1 сек как heartbeat. Резко
 *      снижает задержку и нагрузку. Polling /status оставлен как fallback.
 *    • ИСТОРИЯ час/день/неделя: данные копятся в БРАУЗЕРЕ ТЕЛЕФОНА (localStorage),
 *      не на ESP32. Авто-сохранение, экспорт в файл (CSV/JSON), импорт из
 *      ранее сохранённого файла для восстановления базы. Мозг отдаёт live-поток
 *      по WebSocket + короткий ring-буфер для backfill при подключении.
 *    • ЭКСПОРТ/ИМПОРТ данных кнопками в вебе (CSV и JSON).
 *    • СТАТИСТИКА: средн./макс./мин. давление и температура, число выстрелов
 *      репульсора, циклы реле, наработка, КПД-таймлайн — в отдельной вкладке.
 *    • РЕЗЕРВНЫЙ КАНАЛ NRF24: при росте счётчика потерь пакетов мозг и пульт
 *      синхронно перепрыгивают на резервную частоту (channel hopping). Список
 *      каналов RF_CHANNELS, переключение по таймауту тишины.
 *    • ТЁМНАЯ/СВЕТЛАЯ ТЕМА в вебе: переключатель, выбор хранится в localStorage.
 *    • Совместимо с пультом v5.0 (deep sleep). Протокол NRF24 расширен.
 *
 *  НАЗНАЧЕНИЕ
 *  ----------
 *  Прошивка центрального контроллера водородной установки H2 PRO. Объединяет:
 *    • Веб-интерфейс (HTTP polling) для управления со смартфона/планшета
 *    • Двустороннюю радиосвязь с физическим пультом по NRF24L01+ (2 Мбит/с)
 *    • Графический OLED-индикатор для локального наблюдения
 *    • Управление электролизером (MOSFET или РЕЛЕ), продувочным клапаном и пьезо
 *    • Двухуровневую защиту по давлению (предупреждение + аварийное отключение)
 *    • Защиту по наклону через датчик MPU6050 (TILT-protection) с автокалибровкой
 *    • Регулируемый гистерезис электролизера (Schmitt trigger)
 *    • Watchdog по давлению — предупреждение если бак не набирает за 2 минуты
 *    • OTA-обновление прошивки по Wi-Fi (без USB-кабеля)
 *    • Счётчик наработки электролизера (отображается в веб-интерфейсе)
 *
 *  ОТЛИЧИЯ ОТ v4.3
 *  ---------------
 *    • WATCHDOG ПО ДАВЛЕНИЮ. Когда электролизер активно качает (electroOn в
 *      авто-режиме), запускается таймер. Опорной точкой берётся ФАКТИЧЕСКОЕ
 *      давление на момент включения (wdRefPressure = currentPressure).
 *      Прогрессом считается рост давления на WD_PROGRESS_BAR (0.05 бар)
 *      относительно опорного: при этом таймер и опорное значение обновляются.
 *      Если за WD_TIMEOUT (2 минуты) прогресса нет — поднимается флаг
 *      pressureWatchdog, на вебе и OLED появляется предупреждение
 *      "БАК НЕ НАБИРАЕТ". Это сигнал о возможной неисправности: нет воды в
 *      электролизере, утечка газа, залипшее реле клапана, обрыв. Отсчёт от
 *      фактического давления (а не от target) исключает ложное срабатывание в
 *      нормальной паузе гистерезиса. Watchdog НЕ отключает систему — только
 *      предупреждает, решение за оператором.
 *    • OTA-ОБНОВЛЕНИЕ. Новая прошивка заливается через браузер по адресу
 *      http://192.168.4.1/update — выбрать .bin файл, загрузить, мозг
 *      перепрошьётся сам и перезагрузится. Реле во время OTA принудительно
 *      обесточиваются. ⚠ OTA доступно только когда система ВЫКЛЮЧЕНА
 *      (systemOn=false) — нельзя обновлять на работающей установке.
 *    • СЧЁТЧИК НАРАБОТКИ. Суммарное время работы электролизера в секундах
 *      накапливается и сохраняется в NVS каждые 30 секунд. В веб-интерфейсе
 *      отображается как "Наработка: Xч Yм". Сбрасывается кнопкой в вебе
 *      (эндпоинт /resetruntime). На пульт НЕ передаётся (только веб).
 *  ОТЛИЧИЯ ОТ v4.2
 *  ---------------
 *    • Режим ГОРЕЛКА (continuous gas) ЗАМЕНЁН на РЕПУЛЬСОР — одноразовый
 *      импульс: клапан открывается на 800 мс, через 700 мс после открытия
 *      запускается пьезоподжиг на 100 мс. После завершения цикла кнопка
 *      снова доступна для повторного выстрела.
 *    • Добавлен выбор драйвера электролизера:
 *        MOSFET — быстрая ШИМ-совместимая силовая часть. Гистерезис
 *                 настраивается оператором через веб от 0.00 до 1.00 бар
 *                 с шагом 0.05 бар.
 *        РЕЛЕ   — обычный электромеханический контактор. Гистерезис
 *                 принудительно фиксирован на 0.4 бар для защиты контактов
 *                 от частого щёлканья. Веб-слайдер блокируется.
 *    • Логика управления электролизером переведена с простого порога
 *      на Schmitt trigger: включаем когда P < (target − hysteresis),
 *      выключаем когда P ≥ target. Это устраняет дребезг около цели.
 *    • Все три флага состояния (driver, hysteresis, repulsorActive) сохраняются
 *      в EEPROM/NVS через библиотеку Preferences и восстанавливаются при
 *      старте мозга — оператору не нужно переустанавливать драйвер после
 *      перезагрузки.
 *
 *  ОТЛИЧИЯ ОТ v4.1
 *  ---------------
 *    • Добавлен датчик наклона MPU6050 (I2C-шина общая с OLED).
 *      Калибровка при включении мозга: 50 измерений за ~0.5 сек, усреднение
 *      даёт эталонный вектор гравитации. Дальнейшие отклонения сравниваются
 *      с этим эталоном.
 *    • Порог TILT-сработки: 30 градусов (можно изменить TILT_ANGLE_DEG).
 *    • При TILT-сработке принудительно отключается ПИТАНИЕ ЭЛЕКТРОЛИЗЕРА И КЛАПАНА.
 *      Пьезоподжиг при этом завершает свой цикл (если был запущен) сам.
 *    • Разблокировка после TILT: одной кнопкой LOCK (на пульте, мозге или в вебе).
 *      Если устройство всё ещё наклонено — на следующем тике защёлка взведётся
 *      снова. Это означает, что для разблокировки сначала надо физически
 *      вернуть устройство в исходное положение.
 *
 *  ПРИНЦИП РАБОТЫ РЕПУЛЬСОРА
 *  -------------------------
 *  Кнопка РЕПУЛЬСОР (cmd=3) запускает одноразовый автомат:
 *
 *      t=0    мс  ── открыть клапан, запустить таймер repulsorStart = millis()
 *      t<700  мс  ── клапан ОТКРЫТ, накопление газа
 *      t=700  мс  ── включить пьезоподжиг, клапан остаётся ОТКРЫТ
 *      t<800  мс  ── клапан ОТКРЫТ + пьезоподжиг ВКЛ (горение)
 *      t≥800  мс  ── оба реле ВЫКЛ, repulsorActive = false
 *
 *  Электролизер во время репульсора работает по СВОЕЙ обычной логике
 *  (Schmitt с гистерезисом) — это позволяет дозаправлять бак параллельно
 *  с выстрелом, если давление просело. Повторное нажатие во время активного
 *  цикла игнорируется. По завершении цикла кнопка снова доступна.
 *
 *  ПРИНЦИП РАБОТЫ ГИСТЕРЕЗИСА ЭЛЕКТРОЛИЗЕРА
 *  ----------------------------------------
 *  Классическая релейная регуляция с двумя порогами (Schmitt trigger):
 *
 *      electroOn = ВКЛ      P ≥ target    → electroOn = ВЫКЛ
 *      electroOn = ВЫКЛ     P < target−h  → electroOn = ВКЛ
 *
 *  где h — текущее значение гистерезиса. Это даёт пилу давления
 *  амплитудой h вокруг target вместо постоянного щёлканья на самом target.
 *
 *  Для MOSFET h по умолчанию = 0 (мгновенная реакция), оператор может
 *  поднять до 1.0 бар если нужна экономия циклов.
 *  Для РЕЛЕ h принудительно = 0.4 бар (≈260 циклов/час при типичной
 *  скорости газогенерации против ≈900 без гистерезиса — продлевает
 *  ресурс контактов реле в 3-4 раза).
 *
 *  ПРИНЦИП РАБОТЫ TILT-ЗАЩИТЫ
 *  --------------------------
 *  При старте мозга MPU6050 пробуждается и снимает 50 измерений вектора
 *  ускорения свободного падения. Усреднённый вектор сохраняется как baseline
 *  (опорное положение). Во время работы каждые 100 мс берётся новый вектор,
 *  и вычисляется угол между ним и baseline по формуле скалярного произведения:
 *
 *      cos(θ) = (a·b) / (|a|·|b|)
 *      θ      = acos(cos(θ)) · 180 / π
 *
 *  Это даёт угол отклонения от стартового положения, независимо от того,
 *  как именно устройство было ориентировано при старте — горизонтально,
 *  вертикально, или под углом. Если угол > 30° дольше одного цикла, поднимается
 *  защёлка tiltLock, и логика управления полностью обесточивает электролизер
 *  и клапан. Защёлка сбрасывается командой LOCK (cmd=2).
 *
 *  Для устойчивости к вибрации используется скользящее среднее по 5 измерениям.
 *
 *  СХЕМА ПОДКЛЮЧЕНИЯ
 *  -----------------
 *    NRF24L01+:
 *        CE      → GPIO4
 *        CSN     → GPIO5
 *        SCK     → GPIO18  (VSPI SCK)
 *        MISO    → GPIO19  (VSPI MISO)
 *        MOSI    → GPIO23  (VSPI MOSI)
 *        VCC     → 3.3 V
 *        IRQ     → не подключается
 *        ⚠ ОБЯЗАТЕЛЬНО электролитический конденсатор 470 мкФ × 10 В между
 *          VCC и GND модуля! Без него на MAX-мощности модуль сбоит.
 *
 *    OLED SSD1306 (I2C, 128×64):
 *        SDA     → GPIO21
 *        SCL     → GPIO22
 *        VCC     → 3.3 V
 *        Адрес   → 0x3C (если не работает — попробуйте 0x3D)
 *
 *    MPU6050 (I2C, 6-осевой акселерометр+гироскоп):
 *        SDA     → GPIO21  (на ТУ ЖЕ шину, что OLED)
 *        SCL     → GPIO22  (на ТУ ЖЕ шину, что OLED)
 *        VCC     → 3.3 V   (на модулях GY-521 встроенный регулятор LDO,
 *                          можно питать и от 5 V — смотрите по плате)
 *        GND     → GND
 *        AD0     → GND     → адрес 0x68 (по умолчанию)
 *                  VCC     → адрес 0x69 (если хотите изменить)
 *        INT     → не подключается
 *        Подтяжки SDA/SCL к 3.3 V — уже стоят на модуле GY-521, отдельные
 *        резисторы не нужны.
 *
 *    Реле (модули SRD-05VDC-SL-C или аналогичные, HIGH-active по умолчанию):
 *        Электролизер → GPIO25
 *        Клапан       → GPIO26
 *        Пьезоподжиг  → GPIO27
 *        ⚠ Реле должны питаться от ОТДЕЛЬНОГО источника 5 В. ESP32 не вытянет
 *          ток нескольких модулей одновременно. Общая земля с ESP32 ОБЯЗАТЕЛЬНА.
 *        Если у вас LOW-active модули — установите RELAY_ACTIVE_LOW = true.
 *
 *    Физические кнопки на корпусе мозга (INPUT_PULLUP, замыкаются на GND):
 *        POWER → GPIO13  — одиночный клик: вкл/выкл системы
 *                         — двойной клик в окне 400 мс: продувка до нуля
 *                           (открывает клапан, ждёт пока давление упадёт
 *                           ниже 0.05 бар или до тайм-аута 90 сек,
 *                           затем выключает систему)
 *        LOCK  → GPIO14  — переключает блокировку
 *                         — также используется для сброса защёлки TILT
 *                           и защёлки overpressure
 *        PURGE → GPIO33  — короткая ручная продувка: открывает клапан на 200 мс
 *                          для разового стравливания небольшого давления
 *
 *    Светодиоды (через резистор 220 Ом → GND):
 *        LED_POWER → GPIO17  — горит когда система работает (не в продувке)
 *        LED_LOCK  → GPIO12  — горит при блокировке  ⚠ strapping-pin!
 *        LED_MODE  → GPIO2   — мигает при ГОРЕЛКЕ, горит при MANUAL GAS
 *                              ⚠ strapping-pin! Это встроенный синий LED ESP32
 *
 *        О strapping-pins (GPIO0, GPIO2, GPIO12, GPIO15):
 *        Эти пины влияют на режим загрузки ESP32. Если плата не стартует
 *        с подключёнными светодиодами — отсоедините их временно, проверьте
 *        загрузку, и при необходимости перенесите LED на другие пины
 *        (например, GPIO16, GPIO19, GPIO22 — НО учтите, GPIO22 уже занят I2C).
 *
 *    Датчик давления (например, DXP-2-K-0-10-bar или аналог 0.5–4.5 В):
 *        Сигнал → GPIO36 (SENSOR_VP, только вход, 12-битный ADC)
 *
 *        ⚠ ВАЖНО про делитель напряжения:
 *        ESP32 ADC принимает 0–3.3 В. Если ваш датчик выдаёт 0.5–4.5 В,
 *        нужен делитель 1:1.5 (например, 10 кОм + 20 кОм или 10к + 15к).
 *        ⚡ v5.0: делитель учитывается КАЛИБРОВКОЙ через веб-интерфейс
 *        (offset = напряжение на пине при 0 бар, scale = бар/вольт с учётом
 *        делителя). Формулу в readPressure() править не нужно.
 *
 *  ИСПОЛЬЗУЕМЫЕ БИБЛИОТЕКИ (установить через Library Manager Arduino IDE)
 *  ---------------------------------------------------------------------
 *    RF24                  — by TMRh20
 *    ESPAsyncWebServer     — by me-no-dev / lacamera (fork)
 *    AsyncTCP              — by me-no-dev / esphome (ESP32-версия, НЕ ESPAsyncTCP!)
 *    Adafruit SSD1306      — by Adafruit
 *    Adafruit GFX Library  — by Adafruit (подтянется автоматически)
 *
 *    MPU6050 НЕ требует отдельной библиотеки — используется прямой I2C-доступ
 *    через стандартную Wire.h. Это упрощает зависимости и снимает конфликты
 *    с другими MPU-библиотеками.
 *
 *  WI-FI ТОЧКА ДОСТУПА
 *  -------------------
 *    SSID:    H2_PRO
 *    Пароль:  hydrogen_2024
 *    IP:      192.168.4.1
 *    После подключения к Wi-Fi откройте в браузере: http://192.168.4.1
 *
 *  СВЯЗЬ С ПУЛЬТОМ (NRF24L01+)
 *  ---------------------------
 *    Скорость:         2 Мбит/с (RF24_2MBPS)
 *    Мощность:         MAX (RF24_PA_MAX, ~0 dBm)
 *    CRC:              16 бит
 *    Канал:            76
 *    Адреса труб:
 *        "CMD01"  — пульт → мозг (команды)
 *        "STS01"  — мозг → пульт (статус)
 *    Статус отправляется каждые 100 мс (10 Гц).
 *    Команды от пульта принимаются по мере прихода (асинхронно).
 *
 *  ТАБЛИЦА КОМАНД (передаются от пульта или из веб-интерфейса)
 *  -----------------------------------------------------------
 *    cmd= 1  → POWER toggle (вкл/выкл системы)
 *    cmd= 2  → LOCK toggle (блокировка/разблокировка + сброс защёлок
 *                            TILT и overpressure)
 *    cmd= 3  → РЕПУЛЬСОР — одноразовый выстрел (бывший ГОРЕЛКА/CONST GAS).
 *                          Клапан 800 мс, пьезо со 700-й мс на 100 мс.
 *                          Повторный запуск возможен после завершения цикла.
 *    cmd= 4  → PIEZO ручной поджиг (импульс 500 мс)
 *    cmd= 5  → PURGE_SHORT (открыть клапан на 200 мс для стравливания)
 *    cmd= 7  → MANUAL GAS toggle (электролизер+клапан безусловно)
 *    cmd= 8  → TARGET +0.5 бар
 *    cmd= 9  → TARGET -0.5 бар
 *    cmd=10  → DRIVER toggle (MOSFET ↔ РЕЛЕ; при переходе в РЕЛЕ гистерезис
 *                              принудительно становится 0.4 бар)
 *    cmd=99  → SAFE OFF (продувка до давления ≤ 0.05 бар, потом выключение
 *                        системы; макс. время 90 секунд, потом принудительно)
 *
 *  HTTP-эндпоинты:
 *    GET /                 — главная страница (HTML)
 *    GET /status           — JSON с состоянием (для polling каждые 250 мс)
 *    GET /cmd?id=N         — выполнить команду N (см. таблицу выше)
 *    GET /setpressure?val=N.N — установить целевое давление (0.5–6.5 бар)
 *    GET /sethysteresis?val=N.NN — установить гистерезис электролизера
 *                                  (0.00–1.00 бар; игнорируется в режиме РЕЛЕ)
 *    GET /resetruntime     — обнулить счётчик наработки электролизера
 *    GET /update           — страница OTA-обновления прошивки (только при systemOn=false)
 *    POST /update          — приём .bin файла прошивки (multipart)
 *
 *  ПОРОГИ ДАВЛЕНИЯ И НАКЛОНА
 *  --------------------------
 *    MIN_TARGET_PRESSURE  = 0.5 бар  (минимум регулировки)
 *    MAX_TARGET_PRESSURE  = 6.5 бар  (максимум регулировки)
 *    WARN_PRESSURE        = 6.0 бар  (предупреждение, флаг ovWarn)
 *    MAX_SAFE_PRESSURE    = 6.8 бар  (АВАРИЯ — защёлка ovLock, электролизер OFF)
 *    PURGE_ZERO_BAR       = 0.05 бар (порог окончания продувки до нуля)
 *    TILT_ANGLE_DEG       = 30°      (порог TILT-сработки)
 *    HYST_MAX             = 1.0 бар  (макс. гистерезис в режиме MOSFET)
 *    HYST_RELAY_FIXED     = 0.4 бар  (фикс. гистерезис в режиме РЕЛЕ)
 *
 *  РЕПУЛЬСОР — ТАЙМИНГИ
 *  --------------------
 *    T_REPULSOR_TOTAL      = 800 мс  (общая длительность цикла)
 *    T_REPULSOR_PIEZO_AT   = 700 мс  (когда внутри цикла включить пьезо)
 *    Длительность пьезо в репульсоре = 800 − 700 = 100 мс
 *
 *  ПОВЕДЕНИЕ ПО УМОЛЧАНИЮ (стартовое состояние)
 *  --------------------------------------------
 *    • Система ВЫКЛЮЧЕНА   (systemOn = false)
 *    • Блокировка ВКЛЮЧЕНА (isLocked = true)
 *    • Целевое давление    = 1.0 бар
 *    • Драйвер             = MOSFET (загружается из NVS, либо MOSFET по умолч.)
 *    • Гистерезис          = 0.0 бар (для MOSFET; для РЕЛЕ всегда 0.4)
 *    • Все реле обесточиваются сразу в setup() до любой другой инициализации
 *    • MPU6050 калибруется один раз при старте мозга (~0.5 сек)
 *
 *  ПОВЕДЕНИЕ ПРИ АВАРИЯХ
 *  ---------------------
 *    Превышение 6.5 бар:
 *        → электролизер выключается принудительно
 *        → РЕПУЛЬСОР прерывается, клапан и пьезо обесточиваются
 *        → поднимается ovLock (защёлка)
 *        → снимается только нажатием LOCK при давлении < 5.5 бар
 *
 *    Наклон > 30°:
 *        → электролизер и клапан обесточиваются
 *        → поднимается tiltLock (защёлка) + isLocked = true
 *        → снимается нажатием LOCK; если угол всё ещё > 30°,
 *          защёлка снова взводится автоматически
 *
 *    Потеря связи с пультом (3 секунды без приёма):
 *        → флаг remoteOnline = false (отображается на OLED и в вебе),
 *          но управление продолжается (веб-интерфейс остаётся рабочим)
 *
 *  АВТОР: ChatGPT/Claude + пользователь, 2026
 * =====================================================================================
 */

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include <Update.h>
#include <math.h>
#include "h2_protocol.h"   // ⚡ общий протокол: CmdPacket/StatusPacket, трубы, каналы,
                           //    MIN/MAX_TARGET_PRESSURE (единый для мозга и пульта)

// =====================================================================================
//  ВЫБОР ДАТЧИКА ТЕМПЕРАТУРЫ (⚡ v5.0)
// =====================================================================================
//  Раскомментируйте ОДИН вариант:
#define TEMP_SENSOR_DS18B20    // цифровой 1-Wire (GPIO 15) — точный, рекомендуется
// #define TEMP_SENSOR_NTC     // аналоговый NTC-термистор (ADC GPIO 34)

#ifdef TEMP_SENSOR_DS18B20
  #include <OneWire.h>
  #include <DallasTemperature.h>
#endif


// =====================================================================================
//  WI-FI
// =====================================================================================
const char* AP_SSID     = "H2_PRO";
const char* AP_PASSWORD = "hydrogen_2024";


// =====================================================================================
//  ПИНЫ
// =====================================================================================
#define PIN_RELAY_ELECTRO   25
#define PIN_RELAY_VALVE     26
#define PIN_RELAY_PIEZO     27

#define PIN_BTN_POWER       13   // 1×=ON/OFF  2×=продувка до нуля
#define PIN_BTN_LOCK        14   // блокировка + сброс защёлок
#define PIN_BTN_PURGE       33   // короткая продувка 200 мс

#define PIN_LED_POWER       17
#define PIN_LED_LOCK        12   // ! strapping-pin
#define PIN_LED_MODE         2   // ! strapping-pin (встроенный LED)

#define PIN_PRESSURE        36   // ADC1_CH0, только вход
#define PIN_FAN_PWM         32   // ⚡ v5.0: ШИМ-сигнал 4-pin вентилятора (LEDC 25 кГц)
#define PIN_FAN_TACH        16   // ⚡ v5.0: тахометр 4-pin вентилятора (вход, 2 имп/об)
#define PIN_TEMP_DS18B20    15   // ⚡ v5.0: DS18B20 1-Wire (если выбран цифровой)
#define PIN_TEMP_NTC        34   // ⚡ v5.0: NTC термистор ADC (если выбран аналоговый)

#define PIN_NRF_CE           4
#define PIN_NRF_CSN          5
// VSPI: SCK=18, MISO=19, MOSI=23

#define I2C_SDA             21
#define I2C_SCL             22

#define MPU6050_ADDR        0x68


// =====================================================================================
//  РЕЛЕ — УРОВЕНЬ АКТИВНОСТИ
// =====================================================================================
#define RELAY_ACTIVE_LOW    false
#define RELAY_ON            (RELAY_ACTIVE_LOW ? LOW  : HIGH)
#define RELAY_OFF           (RELAY_ACTIVE_LOW ? HIGH : LOW)


// =====================================================================================
//  ПОРОГИ
// =====================================================================================
const float MAX_SAFE_PRESSURE   = 6.8f;   // аварийный порог (ovLock)
const float WARN_PRESSURE       = 6.0f;   // предупредительный порог (ovWarn)
// MIN_TARGET_PRESSURE / MAX_TARGET_PRESSURE — в общем h2_protocol.h (0.5 / 6.5 бар)
const float TARGET_STEP         = 0.5f;
const float PURGE_ZERO_BAR      = 0.05f;
const float TILT_ANGLE_DEG      = 30.0f;
const int   TILT_AVG_SAMPLES    = 5;     // Скользящее среднее
const int   TILT_CAL_SAMPLES    = 50;    // Калибровка

// ── Гистерезис электролизера ───────────────────────────────────────────────
const float HYST_MIN          = 0.00f;   // Минимальный гистерезис (MOSFET)
const float HYST_MAX          = 1.00f;   // Максимальный гистерезис (MOSFET)
const float HYST_STEP         = 0.05f;   // Шаг настройки в вебе
const float HYST_RELAY_FIXED  = 0.40f;   // Фиксированный гистерезис для РЕЛЕ

// ── Watchdog по давлению ───────────────────────────────────────────────────
const unsigned long WD_TIMEOUT      = 120000;  // 2 минуты без прогресса → предупреждение
const float         WD_PROGRESS_BAR = 0.05f;   // На столько должно вырасти давление, чтобы счесть "есть прогресс"

// ── Драйвер электролизера ──────────────────────────────────────────────────
enum ElectroDriver {
  DRV_MOSFET = 0,    // MOSFET: гистерезис настраивается оператором
  DRV_RELAY  = 1     // РЕЛЕ:   гистерезис принудительно 0.4 бар
};

// ── ⚡ v5.0: Вентилятор охлаждения — 4-pin (ШИМ + тахометр) ────────────────
// 4-pin вентилятор: питание 12В постоянно, скорость задаётся ШИМ-сигналом на
// PIN_FAN_PWM (25 кГц по спецификации Intel), реальные обороты читаются с
// тахометра PIN_FAN_TACH (2 импульса на оборот). MOSFET НЕ нужен — управляет
// сигнальная линия, у вентилятора свой драйвер.
const int   FAN_PWM_CHANNEL = 0;       // LEDC канал (только для core 2.x)
const int   FAN_PWM_FREQ    = 25000;   // 25 кГц — стандарт 4-pin PWM
const int   FAN_PWM_BITS    = 8;       // 0..255
const float FAN_TEMP_MIN_DEF = 40.0f;  // ниже этой t° вентилятор выключен (по умолч.)
const float FAN_TEMP_MAX_DEF = 70.0f;  // на этой t° и выше — 100% оборотов
const int   FAN_MIN_DUTY     = 50;     // мин. ШИМ % при котором 4-pin не глохнет
const int   FAN_TACH_PPR     = 2;      // импульсов тахометра на оборот (станд. для PC-вентиляторов)
enum FanMode { FAN_AUTO = 0, FAN_MANUAL = 1, FAN_OFF = 2 };

// ── ⚡ v5.0: Датчик температуры ────────────────────────────────────────────
const unsigned long T_TEMP_SAMP   = 1000;   // опрос температуры раз в секунду
const float TEMP_OVERHEAT         = 75.0f;  // аварийный перегрев → предупреждение
// NTC параметры (если выбран аналоговый): делитель + Бета-уравнение
const float NTC_NOMINAL_R    = 10000.0f;   // 10 кОм при 25°C
const float NTC_NOMINAL_T    = 25.0f;
const float NTC_BETA         = 3950.0f;
const float NTC_SERIES_R     = 10000.0f;   // последовательный резистор делителя

// ── ⚡ v5.0: Счётчик циклов реле ───────────────────────────────────────────
const unsigned long RELAY_LIFE_WARN = 100000UL;  // ресурс реле (циклов) → предупреждение

// ── ⚡ v5.0: Резервные каналы NRF24 (channel hopping) ──────────────────────
// RF_CHANNELS[] и RF_CHANNEL_CNT вынесены в общий h2_protocol.h.
const unsigned long RF_HOP_SILENCE = 4000;   // мс тишины до перехода на след. канал


// =====================================================================================
//  ТАЙМИНГИ (мс)
// =====================================================================================
const unsigned long T_PIEZO_WORK         = 500;     // обычный пьезо (cmd=4)
const unsigned long T_PURGE_MAX          = 90000;   // макс. продувка
const unsigned long T_PURGE_SHORT        = 200;     // импульс клапана
const unsigned long T_REPULSOR_TOTAL     = 800;     // ⚡ репульсор: общая длительность
const unsigned long T_REPULSOR_PIEZO_AT  = 700;     // ⚡ репульсор: момент включения пьезо
const unsigned long T_DOUBLE_CLICK       = 400;
const unsigned long T_DEBOUNCE           = 50;
const unsigned long T_STATUS_TX          = 100;
const unsigned long T_DISPLAY_UPD        = 200;
const unsigned long T_PRESSURE_SAMP      = 100;
const unsigned long T_TILT_SAMP          = 100;
const unsigned long T_RF_TIMEOUT    = 3000;
const unsigned long T_SHOW_TARGET   = 2500;   // OLED-экран установки давления
const unsigned long T_WS_HEARTBEAT  = 1000;   // ⚡ v5.0: WebSocket push раз в сек


// =====================================================================================
//  NRF24
// =====================================================================================
RF24 radio(PIN_NRF_CE, PIN_NRF_CSN);
// Трубы PIPE_R2B/PIPE_B2R и структуры CmdPacket/StatusPacket вынесены в общий
// h2_protocol.h (включён выше) — протокол байт-в-байт идентичен с прошивкой пульта.


// =====================================================================================
//  OLED
// =====================================================================================
Adafruit_SSD1306 oled(128, 64, &Wire, -1);
bool oledOk = false;


// =====================================================================================
//  ВЕБ + WEBSOCKET (⚡ v5.0)
// =====================================================================================
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");           // дашборд реального времени


// =====================================================================================
//  СОСТОЯНИЕ
// =====================================================================================
bool  systemOn         = false;
bool  isLocked         = true;
bool  isPurging        = false;
bool  isPurgeShort     = false;
bool  repulsorActive   = false;       // ⚡ v4.3: одноразовый импульс газ+поджиг
bool  manualGasActive  = false;
bool  piezoActive      = false;
bool  electroOn        = false;
bool  valveOn          = false;
bool  overpressureLock = false;
bool  tiltLock         = false;
bool  tiltCalibrating  = true;        // true пока идёт калибровка в setup
bool  mpuOk            = false;
float targetPressure   = 1.0f;
float currentPressure  = 0.0f;
float currentTiltDeg   = 0.0f;        // Текущий угол наклона относительно baseline

// ⚡ v4.3: драйвер электролизера и гистерезис
ElectroDriver electroDriver = DRV_MOSFET;
float         hysteresis    = 0.0f;   // в режиме РЕЛЕ принудительно = HYST_RELAY_FIXED

// ⚡ v4.4: Watchdog по давлению
bool          pressureWatchdog = false;  // true = бак не набирает давление
unsigned long wdStart          = 0;      // когда начали отсчёт текущего "качка"
float         wdRefPressure     = 0.0f;   // опорное давление на начало отсчёта

// ⚡ v4.4: Счётчик наработки электролизера
unsigned long runtimeSeconds   = 0;      // суммарное время работы электролизера (сек)
unsigned long electroOnSince    = 0;     // millis() момента включения электролизера (0 = выкл)
unsigned long runtimeMsAccum    = 0;     // накопленные миллисекунды < 1 сек (не теряем остаток)
unsigned long lastRuntimeSave    = 0;    // последнее сохранение в NVS
unsigned long savedRuntimeSeconds = 0;   // что реально записано в NVS (чтобы не писать зря)

// ⚡ v4.4: OTA
bool          otaInProgress     = false;

// ⚡ Отложенное сохранение настроек в NVS (защита от износа flash при частых
// изменениях, например при перетаскивании слайдеров). Реальная запись —
// в loop с задержкой SETTINGS_SAVE_DELAY после последнего изменения.
bool          settingsDirty     = false;
unsigned long settingsDirtyTime = 0;
const unsigned long SETTINGS_SAVE_DELAY = 10000;  // 10 сек тишины → запись

// ⚡ v5.0: Температура и вентилятор
float         currentTemp       = 25.0f;     // текущая температура °C
bool          tempSensorOk      = false;
bool          overheatWarn      = false;     // перегрев > TEMP_OVERHEAT
FanMode       fanMode           = FAN_AUTO;  // авто / ручной / выкл
int           fanDutyManual     = 50;        // ручная скорость 0..100 %
int           fanDutyCurrent    = 0;         // текущая фактическая скорость 0..100 %
float         fanTempMin        = FAN_TEMP_MIN_DEF;
float         fanTempMax        = FAN_TEMP_MAX_DEF;
unsigned long lastTempSamp      = 0;
// ⚡ v5.0: тахометр 4-pin вентилятора
volatile unsigned long fanTachCount = 0;     // счётчик импульсов (инкремент в ISR)
unsigned int  fanRPM            = 0;          // вычисленные обороты в минуту
unsigned long lastRpmCalc       = 0;          // время последнего расчёта RPM

// ⚡ v5.0: Калибровка датчика давления (P = (raw_volt - offset) * scale)
float         presCalOffset     = 0.5f;      // напряжение при 0 бар (датчик 0.5–4.5В)
float         presCalScale      = 2.5f;      // бар на вольт ((10бар)/(4.5-0.5)В = 2.5)

// ⚡ v5.0: Счётчики циклов реле
unsigned long relayCyclesElectro = 0;
unsigned long relayCyclesValve   = 0;
unsigned long relayCyclesPiezo   = 0;
bool          relayElectroPrev   = false;    // для детекта фронта
bool          relayValvePrev     = false;
bool          relayPiezoPrev     = false;
unsigned long lastCyclesSave     = 0;

// ⚡ v5.0: Резервный канал NRF24 (channel hopping)
uint8_t       rfChannelIdx       = 0;        // индекс текущего канала в RF_CHANNELS
unsigned long rfLastActivity     = 0;        // когда последний раз был приём
uint32_t      rfHopCount         = 0;        // сколько раз перепрыгивали

// ⚡ v5.0: WebSocket
unsigned long lastWsHeartbeat    = 0;
bool          wsStateChanged     = false;    // флаг "состояние изменилось, пушим немедленно"

// MPU6050 — baseline (эталонный вектор гравитации после калибровки)
float baseAx = 0, baseAy = 0, baseAz = 0;
// Скользящее среднее угла
float angleBuf[TILT_AVG_SAMPLES] = {0};
int   angleIdx = 0;
bool  angleBufFilled = false;

// RF
bool          remoteOnline = false;
bool          radioOk      = false;   // ⚡ NRF24 инициализирован успешно
unsigned long lastRfRx     = 0;

// Кнопки
bool          pwrLastRaw   = HIGH, pwrStable   = HIGH;
bool          lckLastRaw   = HIGH, lckStable   = HIGH;
bool          prgLastRaw   = HIGH, prgStable   = HIGH;
unsigned long pwrDebTime   = 0,    lckDebTime  = 0,   prgDebTime = 0;
unsigned long pwrLastClick = 0;

// Таймеры
unsigned long purgeStart       = 0;
unsigned long purgeShortStart  = 0;
unsigned long piezoStart       = 0;
unsigned long repulsorStart    = 0;   // ⚡ v4.3
unsigned long lastStatusTx     = 0;
// ⚡ ТЗ 7.3: перерисовка OLED только при изменении данных
unsigned long lastDispCheck    = 0;   // последняя проверка (троттлинг T_DISPLAY_UPD)
unsigned long lastDispDraw     = 0;   // время последней реальной перерисовки
char          lastDispSig[110] = "";  // сигнатура отображаемых полей
unsigned long lastPressureT    = 0;
unsigned long lastTiltT        = 0;
unsigned long bootMs           = 0;
unsigned long lastTargetChange = 0;

// NVS-хранилище для настроек
Preferences prefs;

// ⚡ v5.0: датчик температуры DS18B20 (если выбран цифровой)
#ifdef TEMP_SENSOR_DS18B20
OneWire oneWire(PIN_TEMP_DS18B20);
DallasTemperature tempSensor(&oneWire);
#endif


// =====================================================================================
//  HTML
// =====================================================================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="ru"><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>H2 PRO v5.0</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Exo+2:wght@300;600;800&display=swap');
  :root{
    --bg:#080810;--card:#131326;--border:#1e1e3a;
    --accent:#00d4ff;--ok:#00ff88;--warn:#ffcc00;--danger:#ff3355;--dim:#3a3a5a;--text:#c8d0e0;
    --purple:#bb88ff;--temp:#ff8844;
  }
  /* Светлая тема */
  body.light{
    --bg:#eef1f7;--card:#ffffff;--border:#d0d6e2;
    --accent:#0077cc;--ok:#00a050;--warn:#cc8800;--danger:#cc2244;--dim:#99a0b0;--text:#1a2030;
    --purple:#7744bb;--temp:#dd5500;
  }
  *{box-sizing:border-box;margin:0;padding:0}
  body{background:var(--bg);color:var(--text);font-family:'Exo 2',sans-serif;
    min-height:100vh;padding:14px 10px;transition:background .3s,color .3s;
    background-image:radial-gradient(ellipse at 20% 0%,rgba(0,212,255,.07) 0%,transparent 60%),
      radial-gradient(ellipse at 80% 100%,rgba(77,138,255,.07) 0%,transparent 60%);}
  body.light{background-image:none;}
  .topbar{max-width:480px;margin:0 auto 10px;display:flex;justify-content:space-between;align-items:center;}
  h1{font-size:1.05em;font-weight:800;letter-spacing:5px;color:var(--accent);
    text-transform:uppercase;text-shadow:0 0 20px rgba(0,212,255,.5);}
  body.light h1{text-shadow:none;}
  .theme-btn{background:var(--card);border:1px solid var(--border);color:var(--text);
    border-radius:8px;padding:6px 10px;cursor:pointer;font-size:.9em;transition:all .2s;}
  .theme-btn:hover{border-color:var(--accent);}
  .ws-dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-left:6px;
    background:var(--dim);transition:all .3s;vertical-align:middle;}
  .ws-dot.live{background:var(--ok);box-shadow:0 0 8px var(--ok);}

  .panel{max-width:480px;margin:0 auto;display:flex;flex-direction:column;gap:10px;}

  /* Вкладки */
  .tabs{display:flex;gap:4px;background:var(--card);border:1px solid var(--border);
    border-radius:10px;padding:4px;}
  .tab{flex:1;padding:8px 4px;border:none;background:transparent;color:var(--dim);
    border-radius:7px;cursor:pointer;font-family:'Exo 2',sans-serif;font-size:.7em;
    font-weight:600;letter-spacing:1px;text-transform:uppercase;transition:all .2s;}
  .tab.active{background:var(--accent);color:var(--bg);}
  .diag-row{display:flex;justify-content:space-between;align-items:center;gap:10px;
    padding:9px 2px;border-bottom:1px solid var(--border);font-size:.82em;}
  .diag-row:last-child{border-bottom:none;}
  .pill{font-size:.7em;font-weight:800;letter-spacing:1px;padding:3px 11px;border-radius:20px;
    border:1px solid var(--dim);color:var(--dim);white-space:nowrap;}
  .pill.ok{color:var(--ok);border-color:var(--ok);background:rgba(0,255,136,.08);}
  .pill.bad{color:var(--danger);border-color:var(--danger);background:rgba(255,51,85,.10);}
  .diag-hint{font-size:.8em;color:var(--text);line-height:1.6;}
  .page{display:none;flex-direction:column;gap:10px;}
  .page.active{display:flex;}

  .alert-bar{background:#1a1500;border:1px solid var(--warn);color:var(--warn);
    border-radius:10px;padding:8px 12px;text-align:center;font-size:.75em;
    letter-spacing:2px;text-transform:uppercase;font-family:'Share Tech Mono',monospace;display:none;}
  .alert-bar.show{display:block;animation:ap 1.5s infinite}
  .alert-bar.danger{background:#1a0008;border-color:var(--danger);color:var(--danger)}
  .alert-bar.tilt{background:#1a0a1a;border-color:#ff6bff;color:#ff6bff}
  @keyframes ap{0%,100%{opacity:.7}50%{opacity:1}}

  .pressure{background:var(--card);border:1px solid var(--border);border-radius:14px;
    padding:16px;text-align:center;position:relative;overflow:hidden;}
  .pressure::before{content:'';position:absolute;top:0;left:0;right:0;height:2px;
    background:linear-gradient(90deg,transparent,var(--accent),transparent);}
  .pressure.warn::before{background:linear-gradient(90deg,transparent,var(--warn),transparent)}
  .pressure.danger::before{background:linear-gradient(90deg,transparent,var(--danger),transparent)}
  .p-lbl{font-size:.65em;letter-spacing:4px;color:var(--dim);text-transform:uppercase;margin-bottom:6px}
  .p-num{font-family:'Share Tech Mono',monospace;font-size:3.4em;color:var(--accent);
    line-height:1;text-shadow:0 0 30px rgba(0,212,255,.6);transition:color .3s;}
  .p-num.warn{color:var(--warn)}.p-num.danger{color:var(--danger)}
  .p-unit{font-size:.4em;color:var(--dim);margin-left:6px}
  .p-bar-wrap{position:relative;margin-top:10px}
  .p-bar{height:6px;background:rgba(0,0,0,.3);border-radius:3px;overflow:hidden;position:relative;}
  body.light .p-bar{background:#e0e4ec;}
  .p-hyst{position:absolute;top:0;height:6px;background:rgba(255,204,0,.22);
    border-left:1px dashed rgba(255,204,0,.5);border-right:1px dashed rgba(255,204,0,.5);
    pointer-events:none;transition:all .3s;}
  .p-fill{height:100%;border-radius:3px;background:linear-gradient(90deg,var(--ok),var(--accent));
    transition:width .5s ease,background .3s;position:relative;z-index:1;}
  .p-fill.warn{background:linear-gradient(90deg,var(--accent),var(--warn))}
  .p-fill.danger{background:linear-gradient(90deg,var(--warn),var(--danger))}
  .p-tgt{position:absolute;top:-3px;width:2px;height:13px;background:var(--warn);
    border-radius:1px;transition:left .5s ease;pointer-events:none;z-index:2;}
  .p-scale{display:flex;justify-content:space-between;font-size:.55em;color:var(--dim);
    font-family:'Share Tech Mono',monospace;margin-top:4px;letter-spacing:1px}

  /* Температура */
  .temp-card{background:var(--card);border:1px solid var(--border);border-radius:12px;
    padding:12px 16px;display:flex;justify-content:space-between;align-items:center;}
  .temp-card::before{content:'';position:absolute;}
  .temp-left{display:flex;flex-direction:column;gap:2px;}
  .temp-lbl{font-size:.58em;letter-spacing:3px;color:var(--dim);text-transform:uppercase;}
  .temp-val{font-family:'Share Tech Mono',monospace;font-size:1.8em;color:var(--temp);line-height:1;}
  .temp-val.hot{color:var(--danger);animation:ap 1s infinite;}
  .fan-info{text-align:right;font-size:.62em;font-family:'Share Tech Mono',monospace;color:var(--dim);}
  .fan-rpm{font-size:1.5em;color:var(--accent);}
  .fan-spin{display:inline-block;animation:spin 2s linear infinite;}
  @keyframes spin{from{transform:rotate(0)}to{transform:rotate(360deg)}}

  .chart-box{background:var(--card);border:1px solid var(--border);border-radius:14px;padding:10px 12px;}
  .chart-hdr{display:flex;justify-content:space-between;align-items:center;margin-bottom:6px;}
  .chart-lbl{font-size:.6em;letter-spacing:2px;color:var(--dim);text-transform:uppercase;}
  .range-btns{display:flex;gap:3px;}
  .range-btn{padding:3px 8px;border:1px solid var(--border);border-radius:6px;background:transparent;
    color:var(--dim);font-family:'Share Tech Mono',monospace;font-size:.62em;cursor:pointer;transition:all .2s;}
  .range-btn.active{border-color:var(--accent);color:var(--accent);background:rgba(0,212,255,.1);}
  canvas{width:100%;display:block;}

  .tilt-row{display:flex;justify-content:space-between;align-items:center;
    background:var(--card);border:1px solid var(--border);border-radius:10px;
    padding:8px 12px;font-size:.65em;letter-spacing:2px;
    font-family:'Share Tech Mono',monospace;color:var(--dim);}
  .tilt-row.warn{border-color:var(--warn);color:var(--warn);}
  .tilt-row.danger{border-color:#ff6bff;color:#ff6bff;animation:pulse .9s infinite;}
  .tilt-val{color:var(--ok);font-size:1.2em;}
  .tilt-val.warn{color:var(--warn);}.tilt-val.danger{color:#ff6bff;}

  .badges{display:flex;flex-wrap:wrap;gap:6px}
  .badge{padding:4px 10px;border-radius:20px;font-size:.62em;font-weight:600;
    letter-spacing:1.5px;text-transform:uppercase;background:var(--card);color:var(--dim);
    border:1px solid var(--border);transition:all .25s;}
  .badge.ok{background:#001a0e;color:var(--ok);border-color:var(--ok);}
  body.light .badge.ok{background:#e0fff0;}
  .badge.warn{background:#1a1500;color:var(--warn);border-color:var(--warn);}
  body.light .badge.warn{background:#fff8e0;}
  .badge.danger{background:#1a0008;color:var(--danger);border-color:var(--danger);animation:pulse .9s infinite}
  .badge.tilt{background:#1a0a1a;color:#ff6bff;border-color:#ff6bff;animation:pulse .9s infinite}
  .badge.purple{background:#1a0a2a;color:var(--purple);border-color:var(--purple);animation:pulse .9s infinite}
  @keyframes pulse{0%,100%{box-shadow:0 0 6px rgba(255,51,85,.3)}50%{box-shadow:0 0 14px rgba(255,51,85,.7)}}

  .info-row{display:flex;justify-content:space-between;align-items:center;padding:0 4px;
    font-size:.62em;letter-spacing:2px;color:var(--dim);font-family:'Share Tech Mono',monospace;}
  .rf-indicator{display:flex;align-items:center;gap:6px;}
  .rf-dot{width:7px;height:7px;border-radius:50%;background:var(--dim);transition:all .3s}
  .rf-dot.ok{background:var(--ok);box-shadow:0 0 8px var(--ok)}
  .rf-dot.fail{background:var(--danger);animation:pulse .9s infinite}

  .btn-power{width:100%;padding:16px;border:none;border-radius:12px;cursor:pointer;
    font-family:'Exo 2',sans-serif;font-size:.9em;font-weight:800;letter-spacing:3px;
    text-transform:uppercase;transition:all .2s;
    background:var(--card);color:var(--text);border:1px solid var(--border);}
  .btn-power.on{background:linear-gradient(135deg,#1a0010,#330015);color:var(--danger);
    border-color:var(--danger);box-shadow:0 0 20px rgba(255,51,85,.2);}
  body.light .btn-power.on{background:#ffe0e6;}
  .btn-power:active{transform:scale(.97)}

  .grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}
  .ctrl{padding:14px 8px;border:1px solid var(--border);border-radius:11px;
    background:var(--card);cursor:pointer;font-family:'Exo 2',sans-serif;
    font-size:.7em;font-weight:600;letter-spacing:1px;text-transform:uppercase;
    color:var(--text);transition:all .2s;display:flex;flex-direction:column;
    align-items:center;gap:8px;}
  .ctrl .dot{width:8px;height:8px;border-radius:50%;background:var(--dim);transition:all .25s}
  .ctrl.wide{grid-column:1/-1}
  .ctrl:hover:not(:disabled){filter:brightness(1.2)}
  .ctrl:active:not(:disabled){transform:scale(.95)}
  .ctrl:disabled{opacity:.25;cursor:not-allowed}
  .ctrl.active{background:#001a0e;border-color:var(--ok)}
  .ctrl.active .dot{background:var(--ok);box-shadow:0 0 8px var(--ok)}
  .ctrl.active-warn{background:#1a1500;border-color:var(--warn)}
  .ctrl.active-warn .dot{background:var(--warn);box-shadow:0 0 8px var(--warn)}
  .ctrl.active-red{background:#1a0008;border-color:var(--danger)}
  .ctrl.active-red .dot{background:var(--danger);box-shadow:0 0 8px var(--danger)}
  .ctrl.active-purple{background:#1a0a2a;border-color:var(--purple)}
  .ctrl.active-purple .dot{background:var(--purple);box-shadow:0 0 8px var(--purple)}
  .ctrl.purge-btn{background:#0a1a0a;border-color:#2a4a2a;color:#8f8;}
  .ctrl.repulsor-btn{background:#150a2a;border-color:#3a2a5a;color:var(--purple);}

  .footer{text-align:center;font-size:.7em;letter-spacing:2px;color:var(--warn);
    min-height:16px;font-family:'Share Tech Mono',monospace;}

  .block{background:var(--card);border:1px solid var(--border);border-radius:12px;
    padding:14px;position:relative;overflow:hidden;}
  .block::before{content:'';position:absolute;top:0;left:0;right:0;height:2px;}
  .block.green::before{background:linear-gradient(90deg,transparent,#56d364,transparent);}
  .block.warn::before{background:linear-gradient(90deg,transparent,var(--warn),transparent);}
  .block.temp::before{background:linear-gradient(90deg,transparent,var(--temp),transparent);}
  .blk-lbl{font-size:.62em;letter-spacing:4px;color:var(--dim);text-transform:uppercase;
    text-align:center;margin-bottom:10px;}

  .seg{display:flex;gap:6px;margin-bottom:10px;}
  .seg-opt{flex:1;padding:9px 4px;border:1px solid var(--border);border-radius:8px;
    background:transparent;cursor:pointer;font-family:'Exo 2',sans-serif;
    font-size:.7em;font-weight:600;letter-spacing:1px;text-transform:uppercase;
    color:var(--dim);text-align:center;transition:all .2s;}
  .seg-opt.active{border-color:var(--accent);color:var(--accent);background:rgba(0,212,255,.1);}

  .row{display:flex;align-items:center;gap:8px;margin-bottom:8px;}
  .row label{font-size:.6em;letter-spacing:2px;color:var(--dim);text-transform:uppercase;min-width:62px;}
  .row .val{font-family:'Share Tech Mono',monospace;font-size:.95em;color:var(--accent);min-width:70px;text-align:right;}
  input[type=range]{flex:1;-webkit-appearance:none;height:4px;border-radius:2px;outline:none;cursor:pointer;
    background:linear-gradient(90deg,var(--accent) var(--pct,0%),var(--border) var(--pct,0%));}
  input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:16px;height:16px;
    border-radius:50%;background:var(--accent);cursor:pointer;}
  input[type=range]:disabled{opacity:.4;}

  .t-pm{width:38px;height:38px;border-radius:9px;border:1px solid var(--warn);
    background:rgba(255,204,0,.1);color:var(--warn);font-size:1.3em;font-weight:800;
    cursor:pointer;display:flex;align-items:center;justify-content:center;transition:all .2s;}
  .t-pm:active{transform:scale(.92);}
  .t-val{font-family:'Share Tech Mono',monospace;font-size:1.9em;color:var(--warn);min-width:88px;text-align:center;}
  .t-unit{font-size:.42em;color:var(--dim);}
  .qrow{display:flex;gap:5px;margin-bottom:10px;}
  .qb{flex:1;padding:6px 2px;border:1px solid var(--border);border-radius:7px;background:transparent;
    cursor:pointer;font-family:'Share Tech Mono',monospace;font-size:.7em;color:var(--dim);transition:all .2s;text-align:center;}
  .qb.active{border-color:var(--warn);color:var(--warn);background:rgba(255,204,0,.1);}
  .apply-btn{width:100%;padding:10px;border:1px solid var(--accent);border-radius:9px;
    background:rgba(0,212,255,.1);color:var(--accent);cursor:pointer;font-family:'Exo 2',sans-serif;
    font-size:.72em;font-weight:800;letter-spacing:2px;text-transform:uppercase;transition:all .2s;}
  .apply-btn:active{transform:scale(.97);}
  .ok-msg{text-align:center;font-size:.6em;font-family:'Share Tech Mono',monospace;
    color:var(--ok);min-height:14px;margin-top:5px;transition:opacity .4s;opacity:0;}

  /* Статистика */
  .stat-grid{display:grid;grid-template-columns:1fr 1fr;gap:8px;}
  .stat-item{background:var(--card);border:1px solid var(--border);border-radius:10px;padding:10px 12px;}
  .stat-k{font-size:.55em;letter-spacing:2px;color:var(--dim);text-transform:uppercase;margin-bottom:3px;}
  .stat-v{font-family:'Share Tech Mono',monospace;font-size:1.2em;color:var(--accent);}
  .stat-v.sm{font-size:.95em;}

  .data-btns{display:grid;grid-template-columns:1fr 1fr;gap:8px;}
  .data-btn{padding:11px;border:1px solid var(--border);border-radius:9px;background:var(--card);
    color:var(--text);cursor:pointer;font-family:'Exo 2',sans-serif;font-size:.66em;font-weight:600;
    letter-spacing:1px;text-transform:uppercase;transition:all .2s;}
  .data-btn:hover{border-color:var(--accent);color:var(--accent);}
  .data-btn:active{transform:scale(.96);}
  .data-note{font-size:.58em;color:var(--dim);text-align:center;line-height:1.6;
    font-family:'Share Tech Mono',monospace;margin-top:4px;}

  .ota-link{display:block;text-align:center;font-size:.62em;letter-spacing:2px;
    color:#4d8aff;text-decoration:none;padding:8px;border:1px dashed #1e3a5a;
    border-radius:8px;transition:all .2s;font-family:'Share Tech Mono',monospace;text-transform:uppercase;}
  .ota-link:hover{background:rgba(77,138,255,.1);border-color:#4d8aff;}
  .ota-link.disabled{color:var(--dim);border-color:var(--border);cursor:not-allowed;}
</style></head>
<body>
<div class="topbar">
  <h1>⚡ H2 PRO<span class="ws-dot" id="wsDot"></span></h1>
  <button class="theme-btn" id="themeBtn" onclick="toggleTheme()">🌙</button>
</div>

<div class="panel">
  <div class="tabs">
    <button class="tab active" onclick="showPage('main',this)">Управление</button>
    <button class="tab" onclick="showPage('graph',this)">Графики</button>
    <button class="tab" onclick="showPage('settings',this)">Настройки</button>
    <button class="tab" onclick="showPage('stats',this)">Данные</button>
    <button class="tab" onclick="showPage('diag',this)">Диагн.</button>
  </div>

  <!-- ════════ СТРАНИЦА: УПРАВЛЕНИЕ ════════ -->
  <div class="page active" id="page-main">
    <div class="alert-bar" id="alertBar"></div>

    <div class="pressure" id="pBox">
      <div class="p-lbl">Давление в баке</div>
      <div class="p-num" id="pNum"><span id="pv">0.00</span><span class="p-unit">бар</span></div>
      <div class="p-bar-wrap">
        <div class="p-bar">
          <div class="p-hyst" id="physt" style="left:16.6%;width:0%"></div>
          <div class="p-fill" id="pf" style="width:0%"></div>
        </div>
        <div class="p-tgt" id="ptgt" style="left:16.6%"></div>
        <div class="p-scale"><span>0</span><span>2</span><span>4</span><span>6 бар</span></div>
      </div>
    </div>

    <div class="temp-card block temp">
      <div class="temp-left">
        <div class="temp-lbl">Температура</div>
        <div class="temp-val" id="tempVal">--°</div>
      </div>
      <div class="fan-info">
        <div><span class="fan-spin" id="fanIcon">❄</span> вентилятор</div>
        <div class="fan-rpm"><span id="fanDuty">0</span>%</div>
        <div style="font-size:.85em;color:var(--accent);"><span id="fanRpm">0</span> об/мин</div>
      </div>
    </div>

    <div class="tilt-row" id="tiltRow">
      <span id="tiltLbl">НАКЛОН (порог 30°)</span>
      <span class="tilt-val" id="tiltVal">--°</span>
    </div>

    <div class="badges">
      <span class="badge" id="bPwr">POWER</span>
      <span class="badge" id="bLck">LOCK</span>
      <span class="badge" id="bTlt">TILT</span>
      <span class="badge" id="bPrg">PURGE</span>
      <span class="badge" id="bPsh">PURGE SHORT</span>
      <span class="badge" id="bRep">РЕПУЛЬСОР</span>
      <span class="badge" id="bMgs">MANUAL</span>
      <span class="badge" id="bPzo">PIEZO</span>
      <span class="badge" id="bEle">ELECTRO</span>
      <span class="badge" id="bVal">VALVE</span>
    </div>

    <div class="info-row">
      <div class="rf-indicator"><div class="rf-dot" id="rfDot"></div><span id="rfTxt">ПУЛЬТ: —</span></div>
      <div id="uptime">UP 00:00:00</div>
    </div>

    <button class="btn-power" id="btnPwr" onclick="handlePower()">ВКЛЮЧИТЬ СИСТЕМУ</button>

    <div class="grid">
      <button class="ctrl wide" id="cLck" onclick="cmd(2)"><div class="dot"></div>БЛОКИРОВКА / СБРОС АВАРИЙ</button>
      <button class="ctrl repulsor-btn" id="cRep" onclick="cmd(3)" disabled><div class="dot"></div>РЕПУЛЬСОР (800 мс)</button>
      <button class="ctrl" id="cMgs" onclick="cmd(7)" disabled><div class="dot"></div>РУЧНАЯ ПОДАЧА</button>
      <button class="ctrl active-warn wide" id="cPzo" onclick="cmd(4)" disabled><div class="dot"></div>ПОДЖИГ (500 мс)</button>
      <button class="ctrl purge-btn wide" id="cPsh" onclick="cmd(5)" disabled><div class="dot"></div>ПРОДУТЬ (200 мс клапан)</button>
    </div>

    <div class="block warn">
      <div class="blk-lbl">Целевое давление (max 6.0 бар)</div>
      <div class="row" style="justify-content:center;">
        <button class="t-pm" onclick="stepTgt(-0.5)">−</button>
        <span class="t-val"><span id="tval">1.0</span><span class="t-unit"> бар</span></span>
        <button class="t-pm" onclick="stepTgt(+0.5)">+</button>
      </div>
      <div class="row"><input type="range" id="tslider" min="0.5" max="6.0" step="0.1" value="1.0" oninput="onSlide(this.value)"></div>
      <div class="qrow">
        <button class="qb active" onclick="setQ(1)">1</button><button class="qb" onclick="setQ(2)">2</button>
        <button class="qb" onclick="setQ(3)">3</button><button class="qb" onclick="setQ(4)">4</button>
        <button class="qb" onclick="setQ(5)">5</button><button class="qb" onclick="setQ(6)">6</button>
      </div>
      <button class="apply-btn" onclick="applyTgt()">УСТАНОВИТЬ ДАВЛЕНИЕ</button>
      <div class="ok-msg" id="tok"></div>
    </div>

    <div class="footer" id="foot"></div>
  </div>

  <!-- ════════ СТРАНИЦА: ГРАФИКИ ════════ -->
  <div class="page" id="page-graph">
    <div class="chart-box">
      <div class="chart-hdr">
        <span class="chart-lbl">Давление, бар</span>
        <div class="range-btns">
          <button class="range-btn active" onclick="setRange('1h',this)">1ч</button>
          <button class="range-btn" onclick="setRange('1d',this)">1д</button>
          <button class="range-btn" onclick="setRange('1w',this)">1н</button>
        </div>
      </div>
      <canvas id="chartP" height="120"></canvas>
    </div>
    <div class="chart-box">
      <div class="chart-hdr"><span class="chart-lbl">Температура, °C</span></div>
      <canvas id="chartT" height="90"></canvas>
    </div>
    <div class="data-note" id="histInfo">Точек в базе: 0</div>
  </div>

  <!-- ════════ СТРАНИЦА: НАСТРОЙКИ ════════ -->
  <div class="page" id="page-settings">
    <!-- Драйвер + гистерезис -->
    <div class="block green">
      <div class="blk-lbl">Драйвер электролизера + гистерезис</div>
      <div class="seg">
        <button class="seg-opt" id="drvMos" onclick="cmd(10)">⚡ MOSFET</button>
        <button class="seg-opt" id="drvRel" onclick="cmd(10)">🔲 РЕЛЕ</button>
      </div>
      <div class="row">
        <label>Гистер.</label>
        <input type="range" id="hslider" min="0" max="1.0" step="0.05" value="0" oninput="onHyst(this.value)">
        <span class="val" id="hval">0.00 бар</span>
      </div>
      <div class="data-note" id="hnote">MOSFET: настраивается 0…1 бар</div>
    </div>

    <!-- Вентилятор -->
    <div class="block temp">
      <div class="blk-lbl">Охлаждение — вентилятор</div>
      <div class="seg">
        <button class="seg-opt" id="fanAuto" onclick="setFanMode(0)">Авто</button>
        <button class="seg-opt" id="fanMan" onclick="setFanMode(1)">Ручной</button>
        <button class="seg-opt" id="fanOff" onclick="setFanMode(2)">Выкл</button>
      </div>
      <div class="row" id="fanManRow" style="display:none;">
        <label>Скорость</label>
        <input type="range" id="fanDutySlider" min="0" max="100" step="5" value="50" oninput="onFanDuty(this.value)">
        <span class="val" id="fanDutyVal">50%</span>
      </div>
      <div class="row">
        <label>t° старт</label>
        <input type="range" id="fanTminSlider" min="20" max="60" step="1" value="35" oninput="onFanTmin(this.value)">
        <span class="val" id="fanTminVal">35°C</span>
      </div>
      <div class="row">
        <label>t° 100%</label>
        <input type="range" id="fanTmaxSlider" min="40" max="90" step="1" value="60" oninput="onFanTmax(this.value)">
        <span class="val" id="fanTmaxVal">60°C</span>
      </div>
      <button class="apply-btn" onclick="applyFan()">ПРИМЕНИТЬ</button>
      <div class="ok-msg" id="fanOk"></div>
    </div>

    <!-- Калибровка давления -->
    <div class="block green">
      <div class="blk-lbl">Калибровка датчика давления</div>
      <div class="data-note" style="margin-bottom:10px;">2 точки: при 0 бар запишите текущее напряжение (offset), затем масштаб (бар/В). Текущее давление: <b id="calCur">0.00</b> бар</div>
      <div class="row">
        <label>Offset В</label>
        <input type="range" id="calOffSlider" min="0" max="1.0" step="0.01" value="0.5" oninput="onCalOff(this.value)">
        <span class="val" id="calOffVal">0.50 В</span>
      </div>
      <div class="row">
        <label>Бар/В</label>
        <input type="range" id="calSclSlider" min="0.5" max="5.0" step="0.05" value="2.5" oninput="onCalScl(this.value)">
        <span class="val" id="calSclVal">2.50</span>
      </div>
      <button class="apply-btn" onclick="applyCal()">СОХРАНИТЬ КАЛИБРОВКУ</button>
      <div class="ok-msg" id="calOk"></div>
    </div>

    <a class="ota-link" id="otaLink" href="/update">⬆ OTA-обновление прошивки</a>
  </div>

  <!-- ════════ СТРАНИЦА: ДАННЫЕ/СТАТИСТИКА ════════ -->
  <div class="page" id="page-stats">
    <div class="stat-grid">
      <div class="stat-item"><div class="stat-k">Давление сейчас</div><div class="stat-v" id="sPnow">0.00</div></div>
      <div class="stat-item"><div class="stat-k">Давление макс</div><div class="stat-v" id="sPmax">0.00</div></div>
      <div class="stat-item"><div class="stat-k">Темп. сейчас</div><div class="stat-v" id="sTnow">0.0°</div></div>
      <div class="stat-item"><div class="stat-k">Темп. макс</div><div class="stat-v" id="sTmax">0.0°</div></div>
      <div class="stat-item"><div class="stat-k">Наработка</div><div class="stat-v sm" id="sRt">0ч 0м</div></div>
      <div class="stat-item"><div class="stat-k">Аптайм</div><div class="stat-v sm" id="sUp">0:00:00</div></div>
    </div>

    <div class="block warn">
      <div class="blk-lbl">Циклы реле <span style="font-size:.8em;color:var(--dim)">(ресурс ~100k)</span></div>
      <div class="stat-grid">
        <div class="stat-item"><div class="stat-k">Электролизер</div><div class="stat-v sm" id="sRce">0</div></div>
        <div class="stat-item"><div class="stat-k">Клапан</div><div class="stat-v sm" id="sRcv">0</div></div>
        <div class="stat-item"><div class="stat-k">Пьезо</div><div class="stat-v sm" id="sRcp">0</div></div>
        <div class="stat-item"><div class="stat-k">RF-каналов</div><div class="stat-v sm" id="sRfh">к76 / 0</div></div>
      </div>
      <button class="apply-btn" style="margin-top:10px;border-color:var(--danger);color:var(--danger);background:rgba(255,51,85,.1);" onclick="resetCycles()">СБРОСИТЬ ЦИКЛЫ РЕЛЕ</button>
    </div>

    <div class="block green">
      <div class="blk-lbl">База данных истории (на телефоне)</div>
      <div class="data-btns">
        <button class="data-btn" onclick="exportCSV()">⬇ Экспорт CSV</button>
        <button class="data-btn" onclick="exportJSON()">⬇ Экспорт JSON</button>
        <button class="data-btn" onclick="document.getElementById('importFile').click()">⬆ Импорт файла</button>
        <button class="data-btn" style="border-color:var(--danger);color:var(--danger);" onclick="clearHistory()">🗑 Очистить базу</button>
      </div>
      <input type="file" id="importFile" accept=".json,.csv" style="display:none" onchange="importData(this)">
      <div class="data-note" id="dbInfo">Точек: 0 · Размер: 0 КБ · Авто-сохранение: вкл</div>
    </div>
    <button class="apply-btn" onclick="resetRT()" style="border-color:var(--danger);color:var(--danger);background:rgba(255,51,85,.1);">СБРОСИТЬ СЧЁТЧИК НАРАБОТКИ</button>
  </div>

  <div class="page" id="page-diag">
    <div class="block">
      <div class="blk-lbl">Состояние модулей (обновляется в реальном времени)</div>
      <div class="diag-row"><span>OLED-дисплей · I²C 0x3C</span><span class="pill" id="dgOled">…</span></div>
      <div class="diag-row"><span>MPU-6050 (наклон) · I²C 0x68</span><span class="pill" id="dgMpu">…</span></div>
      <div class="diag-row"><span>Радио NRF24L01 · SPI</span><span class="pill" id="dgNrf">…</span></div>
      <div class="diag-row"><span>Датчик температуры</span><span class="pill" id="dgTemp">…</span></div>
      <div class="diag-row"><span>Связь с пультом</span><span class="pill" id="dgRf">…</span></div>
      <div class="diag-row"><span>Вентилятор (по тахометру)</span><span class="pill" id="dgFan">…</span></div>
    </div>
    <div class="block">
      <div class="blk-lbl">Что проверить</div>
      <div class="diag-hint" id="dgHint">Загрузка состояния…</div>
    </div>
    <div class="block">
      <div class="blk-lbl">Подсказка по шине I²C</div>
      <div class="diag-hint">OLED и MPU-6050 сидят на одной шине I²C (пины 21/22). Если не работают оба сразу — причина общая: SDA→21, SCL→22 (на дисплее этот контакт подписан «SCK»), питание 3,3 В, общая земля, провода не задели TX/RX. Полная диагностика — в файле H2_Assembly_Full.html, раздел «ЭТАП E · Диагностика».</div>
    </div>
  </div>
</div>

<script>
/* ══════════ СОСТОЯНИЕ И WEBSOCKET ══════════ */
let st={}, pendingTgt=1.0, purging=false;
let ws=null, wsAlive=false, lastMsg=0;
let chartRange='1h';

/* История хранится в localStorage телефона. Структура: массив {ts,p,t}.
   Разрешение прореживаем по диапазону для экономии места. */
let history=[];
const HIST_KEY='h2pro_history';
const MAX_POINTS=20000;       // ~ неделя при записи раз в 30 сек
let lastHistPush=0;

function loadHistory(){
  try{ const s=localStorage.getItem(HIST_KEY); if(s) history=JSON.parse(s)||[]; }
  catch(e){ history=[]; }
}
function saveHistory(){
  try{ localStorage.setItem(HIST_KEY,JSON.stringify(history)); }catch(e){}
}
function pushHistory(p,t){
  const now=Date.now();
  if(now-lastHistPush < 30000) return;   // не чаще раза в 30 сек
  lastHistPush=now;
  history.push({ts:now,p:+p.toFixed(2),t:+t.toFixed(1)});
  if(history.length>MAX_POINTS) history=history.slice(-MAX_POINTS);
  saveHistory();
}

/* ══════════ ТЕМА ══════════ */
function toggleTheme(){
  document.body.classList.toggle('light');
  const light=document.body.classList.contains('light');
  document.getElementById('themeBtn').innerText=light?'☀':'🌙';
  localStorage.setItem('h2pro_theme',light?'light':'dark');
}
if(localStorage.getItem('h2pro_theme')==='light'){
  document.body.classList.add('light');
  document.getElementById('themeBtn').innerText='☀';
}

/* ══════════ ВКЛАДКИ ══════════ */
function showPage(p,btn){
  document.querySelectorAll('.page').forEach(e=>e.classList.remove('active'));
  document.querySelectorAll('.tab').forEach(e=>e.classList.remove('active'));
  document.getElementById('page-'+p).classList.add('active');
  btn.classList.add('active');
  if(p==='graph') drawCharts();
}

/* ══════════ WEBSOCKET ══════════ */
function wsConnect(){
  ws=new WebSocket('ws://'+location.host+'/ws');
  ws.onopen=()=>{ wsAlive=true; document.getElementById('wsDot').classList.add('live'); };
  ws.onclose=()=>{ wsAlive=false; document.getElementById('wsDot').classList.remove('live'); setTimeout(wsConnect,2000); };
  ws.onerror=()=>{ ws.close(); };
  ws.onmessage=(ev)=>{ lastMsg=Date.now(); try{ st=JSON.parse(ev.data); render(); }catch(e){} };
}
function cmd(id){ if(wsAlive&&ws.readyState===1) ws.send('{"cmd":'+id+'}'); else fetch('/cmd?id='+id).catch(()=>{}); }

/* ══════════ УПРАВЛЕНИЕ ДАВЛЕНИЕМ ══════════ */
function onSlide(v){ v=+v; pendingTgt=v; document.getElementById('tval').innerText=v.toFixed(1);
  document.getElementById('tslider').style.setProperty('--pct',((v-0.5)/5.5*100).toFixed(1)+'%'); updateQ(v); }
function setQ(v){ pendingTgt=v; document.getElementById('tslider').value=v; onSlide(v); }
function stepTgt(d){ let v=Math.max(0.5,Math.min(6.0,Math.round((pendingTgt+d)*2)/2)); setQ(v); }
function updateQ(v){ document.querySelectorAll('#page-main .qb').forEach((b,i)=>b.classList.toggle('active',(i+1)===v)); }
function applyTgt(){ fetch('/setpressure?val='+pendingTgt.toFixed(1)).catch(()=>{}); flash('tok','✓ установлено: '+pendingTgt.toFixed(1)+' бар'); }
function handlePower(){ if(purging)return;
  if(!window._pt){ window._pt=setTimeout(()=>{window._pt=null;cmd(1);},380); }
  else{ clearTimeout(window._pt); window._pt=null; cmd(99); } }

/* ══════════ ГИСТЕРЕЗИС ══════════ */
function onHyst(v){ if(st.drv===1)return; const h=+v;
  document.getElementById('hval').innerText=h.toFixed(2)+' бар';
  document.getElementById('hslider').style.setProperty('--pct',(h*100)+'%');
  fetch('/sethysteresis?val='+h.toFixed(2)).catch(()=>{}); }

/* ══════════ ВЕНТИЛЯТОР ══════════ */
let fanCfg={mode:0,duty:50,tmin:35,tmax:60};
function setFanMode(m){ fanCfg.mode=m;
  document.getElementById('fanAuto').classList.toggle('active',m===0);
  document.getElementById('fanMan').classList.toggle('active',m===1);
  document.getElementById('fanOff').classList.toggle('active',m===2);
  document.getElementById('fanManRow').style.display=(m===1)?'flex':'none'; }
function onFanDuty(v){ fanCfg.duty=+v; document.getElementById('fanDutyVal').innerText=v+'%';
  document.getElementById('fanDutySlider').style.setProperty('--pct',v+'%'); }
function onFanTmin(v){ fanCfg.tmin=+v; document.getElementById('fanTminVal').innerText=v+'°C';
  document.getElementById('fanTminSlider').style.setProperty('--pct',((v-20)/40*100)+'%'); }
function onFanTmax(v){ fanCfg.tmax=+v; document.getElementById('fanTmaxVal').innerText=v+'°C';
  document.getElementById('fanTmaxSlider').style.setProperty('--pct',((v-40)/50*100)+'%'); }
function applyFan(){ fetch('/setfan?mode='+fanCfg.mode+'&duty='+fanCfg.duty+'&tmin='+fanCfg.tmin+'&tmax='+fanCfg.tmax).catch(()=>{});
  flash('fanOk','✓ настройки вентилятора применены'); }

/* ══════════ КАЛИБРОВКА ══════════ */
let calCfg={off:0.5,scl:2.5};
function onCalOff(v){ calCfg.off=+v; document.getElementById('calOffVal').innerText=(+v).toFixed(2)+' В';
  document.getElementById('calOffSlider').style.setProperty('--pct',(v/1.0*100)+'%'); }
function onCalScl(v){ calCfg.scl=+v; document.getElementById('calSclVal').innerText=(+v).toFixed(2);
  document.getElementById('calSclSlider').style.setProperty('--pct',((v-0.5)/4.5*100)+'%'); }
function applyCal(){ fetch('/calibrate?offset='+calCfg.off.toFixed(2)+'&scale='+calCfg.scl.toFixed(2)).catch(()=>{});
  flash('calOk','✓ калибровка сохранена'); }

/* ══════════ СБРОСЫ ══════════ */
function resetRT(){ if(confirm('Обнулить счётчик наработки?')) fetch('/resetruntime').catch(()=>{}); }
function resetCycles(){ if(confirm('Обнулить счётчики циклов реле?')) fetch('/resetcycles').catch(()=>{}); }

/* ══════════ ЭКСПОРТ/ИМПОРТ ══════════ */
function download(name,content,type){
  const blob=new Blob([content],{type:type});
  const a=document.createElement('a');
  a.href=URL.createObjectURL(blob); a.download=name;
  document.body.appendChild(a); a.click(); document.body.removeChild(a);
}
function exportCSV(){
  let csv='timestamp,iso,pressure_bar,temp_c\n';
  history.forEach(r=>{ csv+=r.ts+','+new Date(r.ts).toISOString()+','+r.p+','+r.t+'\n'; });
  download('h2pro_'+Date.now()+'.csv',csv,'text/csv');
}
function exportJSON(){ download('h2pro_'+Date.now()+'.json',JSON.stringify(history),'application/json'); }
function importData(inp){
  const f=inp.files[0]; if(!f)return;
  const r=new FileReader();
  r.onload=e=>{
    try{
      let data;
      if(f.name.endsWith('.json')){ data=JSON.parse(e.target.result); }
      else{ // CSV
        data=[]; const lines=e.target.result.split('\n');
        for(let i=1;i<lines.length;i++){ const c=lines[i].split(','); if(c.length>=4) data.push({ts:+c[0],p:+c[2],t:+c[3]}); }
      }
      if(Array.isArray(data)){
        // объединяем с текущей базой, сортируем по времени, убираем дубли
        const map={}; [...history,...data].forEach(r=>{ if(r&&r.ts) map[r.ts]=r; });
        history=Object.values(map).sort((a,b)=>a.ts-b.ts);
        if(history.length>MAX_POINTS) history=history.slice(-MAX_POINTS);
        saveHistory(); updateDbInfo(); drawCharts();
        alert('Импортировано. Всего точек: '+history.length);
      }
    }catch(err){ alert('Ошибка импорта: '+err); }
  };
  r.readAsText(f); inp.value='';
}
function clearHistory(){ if(confirm('Очистить всю базу истории на телефоне?')){ history=[]; saveHistory(); updateDbInfo(); drawCharts(); } }
function updateDbInfo(){
  const kb=(JSON.stringify(history).length/1024).toFixed(1);
  const el=document.getElementById('dbInfo'); if(el) el.innerText='Точек: '+history.length+' · Размер: '+kb+' КБ · Авто-сохранение: вкл';
  const hi=document.getElementById('histInfo'); if(hi) hi.innerText='Точек в базе: '+history.length;
}

/* ══════════ ГРАФИКИ ══════════ */
function setRange(r,btn){ chartRange=r;
  document.querySelectorAll('#page-graph .range-btn').forEach(b=>b.classList.remove('active'));
  btn.classList.add('active'); drawCharts(); }
function rangeMs(){ return chartRange==='1h'?3600e3:chartRange==='1d'?86400e3:604800e3; }
function drawLine(cv,data,key,color,maxY,unit){
  const ctx=cv.getContext('2d'); const w=cv.width=cv.clientWidth*2; const h=cv.height=cv.clientHeight*2;
  ctx.scale(1,1); ctx.clearRect(0,0,w,h);
  const css=getComputedStyle(document.body);
  ctx.strokeStyle=css.getPropertyValue('--border'); ctx.lineWidth=1;
  for(let i=0;i<=4;i++){ const y=i*(h-20)/4+10; ctx.beginPath(); ctx.moveTo(40,y); ctx.lineTo(w,y); ctx.stroke();
    ctx.fillStyle=css.getPropertyValue('--dim'); ctx.font='16px monospace'; ctx.textAlign='right';
    ctx.fillText((maxY*(1-i/4)).toFixed(1),36,y+5); }
  if(data.length<2){ ctx.fillStyle=css.getPropertyValue('--dim'); ctx.font='18px monospace'; ctx.textAlign='center';
    ctx.fillText('Накопление данных...',w/2,h/2); return; }
  const t0=data[0].ts, t1=data[data.length-1].ts, span=Math.max(1,t1-t0);
  ctx.strokeStyle=color; ctx.lineWidth=2.5; ctx.beginPath();
  data.forEach((r,i)=>{ const x=40+(r.ts-t0)/span*(w-40); const y=10+(1-Math.min(r[key]/maxY,1))*(h-20);
    i===0?ctx.moveTo(x,y):ctx.lineTo(x,y); });
  ctx.stroke();
}
function drawCharts(){
  const cut=Date.now()-rangeMs();
  const data=history.filter(r=>r.ts>=cut);
  drawLine(document.getElementById('chartP'),data,'p','#00d4ff',6.5,'бар');
  drawLine(document.getElementById('chartT'),data,'t','#ff8844',90,'°C');
  updateDbInfo();
}

/* ══════════ РЕНДЕР ══════════ */
function cls(id,v,c){ const e=document.getElementById(id); if(e) e.className='badge'+(v?' '+c:''); }
function setPill(id,ok,a,b){ const e=document.getElementById(id); if(!e)return; e.className='pill '+(ok?'ok':'bad'); e.innerText=ok?a:b; }
function flash(id,msg){ const e=document.getElementById(id); if(!e)return; e.innerText=msg; e.style.opacity='1'; setTimeout(()=>e.style.opacity='0',2500); }
function fmtUp(s){ return [Math.floor(s/3600),Math.floor(s%3600/60),s%60].map(n=>String(n).padStart(2,'0')).join(':'); }

function render(){
  const d=st; if(d.p===undefined)return;
  const p=d.p;
  document.getElementById('pv').innerText=p.toFixed(2);
  document.getElementById('pf').style.width=Math.min(p/6.5*100,100)+'%';
  document.getElementById('ptgt').style.left='calc('+Math.min(d.tgt/6.5*100,100)+'% - 1px)';
  const ph=document.getElementById('physt');
  if(d.h>0){ const l=Math.max(0,(d.tgt-d.h)/6.5*100); ph.style.left=l+'%'; ph.style.width=Math.min(d.h/6.5*100,100-l)+'%'; }
  else ph.style.width='0%';

  const pBox=document.getElementById('pBox'),pNum=document.getElementById('pNum'),pf=document.getElementById('pf');
  pBox.className='pressure';pNum.className='p-num';pf.className='p-fill';
  if(d.ovl){pBox.classList.add('danger');pNum.classList.add('danger');pf.classList.add('danger');}
  else if(d.ovw){pBox.classList.add('warn');pNum.classList.add('warn');pf.classList.add('warn');}

  // Температура + вентилятор
  const tv=document.getElementById('tempVal');
  tv.innerText=(d.tok?d.t.toFixed(1):'--')+'°';
  tv.className='temp-val'+(d.oh?' hot':'');
  document.getElementById('fanDuty').innerText=d.fan||0;
  const fr=document.getElementById('fanRpm'); if(fr)fr.innerText=d.rpm||0;
  const fi=document.getElementById('fanIcon');
  fi.style.animationDuration=(d.fan>0?(2-d.fan/100*1.7):0)+'s';
  fi.style.opacity=d.fan>0?1:0.3;

  // Tilt
  const tRow=document.getElementById('tiltRow'),tVal=document.getElementById('tiltVal'),tLbl=document.getElementById('tiltLbl');
  tRow.className='tilt-row';tVal.className='tilt-val';
  if(d.tcal){tLbl.innerText='КАЛИБРОВКА MPU6050...';tVal.innerText='wait';}
  else if(d.tlk){tRow.classList.add('danger');tVal.classList.add('danger');tLbl.innerText='НАКЛОН — АВАРИЯ (LOCK)';tVal.innerText=d.ta+'°';}
  else if(d.ta>=20){tRow.classList.add('warn');tVal.classList.add('warn');tLbl.innerText='НАКЛОН (порог 30°)';tVal.innerText=d.ta+'°';}
  else {tLbl.innerText='НАКЛОН (порог 30°)';tVal.innerText=d.ta+'°';}

  // Алерты (приоритет)
  const ab=document.getElementById('alertBar');
  if(d.tlk){ab.className='alert-bar show tilt';ab.innerText='⚠ НАКЛОН '+d.ta+'° — ЭЛЕКТРОЛИЗЕР И КЛАПАН ОБЕСТОЧЕНЫ';}
  else if(d.ovl){ab.className='alert-bar show danger';ab.innerText='⚠ АВАРИЯ '+p.toFixed(2)+' БАР';}
  else if(d.oh){ab.className='alert-bar show danger';ab.innerText='🔥 ПЕРЕГРЕВ '+d.t.toFixed(0)+'°C';}
  else if(d.ovw){ab.className='alert-bar show';ab.innerText='⚠ Приближение к аварии: '+p.toFixed(2)+' / 6.5 бар';}
  else if(d.wd){ab.className='alert-bar show';ab.innerText='⚠ БАК НЕ НАБИРАЕТ ДАВЛЕНИЕ';}
  else if(d.relw){ab.className='alert-bar show';ab.innerText='⚙ РЕСУРС РЕЛЕ НА ИСХОДЕ — проверьте контакты';}
  else ab.className='alert-bar';

  // Бейджи
  cls('bPwr',d.pwr,'ok');cls('bLck',d.lck,'warn');cls('bTlt',d.tlk,'tilt');
  cls('bPrg',d.prg,'danger');cls('bPsh',d.psh,'warn');cls('bRep',d.rep,'purple');
  cls('bMgs',d.mgs,'ok');cls('bPzo',d.pzo,'warn');cls('bEle',d.ele,'ok');cls('bVal',d.val,'ok');

  // Power btn + controls
  const bp=document.getElementById('btnPwr');
  bp.className='btn-power'+(d.pwr?' on':'');
  bp.innerText=d.pwr?'ВЫКЛЮЧИТЬ  (2× = ПРОДУВКА)':'ВКЛЮЧИТЬ СИСТЕМУ';
  const dis=!d.pwr||d.prg||d.lck||d.tlk;
  ['cRep','cMgs','cPzo','cPsh'].forEach(i=>document.getElementById(i).disabled=dis);
  document.getElementById('cLck').className='ctrl wide'+(d.lck?' active-warn':'');
  document.getElementById('cRep').className='ctrl repulsor-btn'+(d.rep?' active-purple':'');
  document.getElementById('cMgs').className='ctrl'+(d.mgs?' active':'');
  document.getElementById('cPzo').className='ctrl active-warn wide'+(d.pzo?' active-red':'');
  document.getElementById('cPsh').className='ctrl purge-btn wide'+(d.psh?' active-red':'');

  // Драйвер
  document.getElementById('drvMos').classList.toggle('active',d.drv===0);
  document.getElementById('drvRel').classList.toggle('active',d.drv===1);
  const hs=document.getElementById('hslider'),hn=document.getElementById('hnote');
  if(d.drv===1){ hs.disabled=true; hn.innerText='РЕЛЕ: гистерезис фикс. 0.40 бар'; }
  else{ hs.disabled=false; hn.innerText='MOSFET: настраивается 0…1 бар'; }
  if(document.activeElement!==hs){ hs.value=d.h; document.getElementById('hval').innerText=d.h.toFixed(2)+' бар'; hs.style.setProperty('--pct',(d.h*100)+'%'); }

  // Целевое (если не редактируем)
  const sl=document.getElementById('tslider');
  if(document.activeElement!==sl){ const v=+d.tgt; sl.value=v; document.getElementById('tval').innerText=v.toFixed(1);
    sl.style.setProperty('--pct',((v-0.5)/5.5*100).toFixed(1)+'%'); pendingTgt=v; updateQ(Math.round(v)); }

  // RF / uptime / footer
  document.getElementById('rfDot').className='rf-dot'+(d.rf?' ok':' fail');
  document.getElementById('rfTxt').innerText='ПУЛЬТ: '+(d.rf?'ОНЛАЙН':'ОФФЛАЙН');
  document.getElementById('uptime').innerText='UP '+fmtUp(d.up);

  const ota=document.getElementById('otaLink');
  if(d.pwr){ ota.className='ota-link disabled'; ota.removeAttribute('href'); ota.innerText='⬆ OTA — выключите систему'; }
  else{ ota.className='ota-link'; ota.setAttribute('href','/update'); ota.innerText='⬆ OTA-обновление прошивки'; }

  purging=d.prg;
  const foot=document.getElementById('foot');
  if(d.prg)foot.innerText='⏳ ПРОДУВКА ДО НУЛЯ — '+p.toFixed(2)+' бар';
  else if(d.psh)foot.innerText='⚡ МИНИ-ПРОДУВКА 200 мс';
  else if(d.rep)foot.innerText='🔥 РЕПУЛЬСОР — ВЫСТРЕЛ';
  else if(d.tlk)foot.innerText='⚠ TILT — выровняйте и нажмите LOCK';
  else if(d.ovl)foot.innerText='⚠ АВАРИЯ — СБРОСЬТЕ ДАВЛЕНИЕ';
  else foot.innerText='';

  // Статистика
  document.getElementById('sPnow').innerText=p.toFixed(2);
  document.getElementById('sTnow').innerText=(d.tok?d.t.toFixed(1):'--')+'°';
  document.getElementById('sRt').innerText=Math.floor(d.rt/3600)+'ч '+Math.floor((d.rt%3600)/60)+'м';
  document.getElementById('sUp').innerText=fmtUp(d.up);
  document.getElementById('sRce').innerText=(d.rce||0).toLocaleString();
  document.getElementById('sRcv').innerText=(d.rcv||0).toLocaleString();
  document.getElementById('sRcp').innerText=(d.rcp||0).toLocaleString();
  document.getElementById('sRfh').innerText='к'+(d.rfc||76)+' / '+(d.rfh||0);
  document.getElementById('calCur').innerText=p.toFixed(2);
  // макс из истории
  if(history.length){ let pm=0,tm=0; history.forEach(r=>{if(r.p>pm)pm=r.p;if(r.t>tm)tm=r.t;});
    document.getElementById('sPmax').innerText=Math.max(pm,p).toFixed(2);
    document.getElementById('sTmax').innerText=Math.max(tm,d.tok?d.t:0).toFixed(1)+'°'; }
  else { document.getElementById('sPmax').innerText=p.toFixed(2);
    document.getElementById('sTmax').innerText=(d.tok?d.t.toFixed(1):'--')+'°'; }

  // ── Диагностика модулей ──
  setPill('dgOled',d.oledok,'OK','НЕ НАЙДЕН');
  setPill('dgMpu',d.mpuok,'OK','НЕ НАЙДЕН');
  setPill('dgNrf',d.nrfok,'OK','НЕ НАЙДЕН');
  setPill('dgTemp',d.tok,'OK','НЕТ ДАННЫХ');
  setPill('dgRf',d.rf,'ОНЛАЙН','ОФФЛАЙН');
  { const e=document.getElementById('dgFan'); if(e){
      if(d.fan>0&&(d.rpm|0)===0){ e.className='pill bad'; e.innerText='НЕТ ОБОРОТОВ'; }
      else if(d.fan>0){ e.className='pill ok'; e.innerText=(d.rpm|0)+' об/мин'; }
      else{ e.className='pill'; e.innerText='ВЫКЛ (0%)'; } } }
  { const hints=[];
    if(!d.oledok && !d.mpuok) hints.push('Не работают сразу OLED и MPU — они на ОДНОЙ шине I²C. Проверьте: SDA→пин 21, SCL→пин 22 (на дисплее подписан «SCK»), VCC→3.3В, общая земля GND, провода не перепутаны с TX/RX.');
    else { if(!d.oledok) hints.push('OLED не найден: SDA→21, SCL(SCK)→22, VCC→3.3В, GND. Если провода верны — адрес дисплея может быть 0x3D вместо 0x3C.');
           if(!d.mpuok)  hints.push('MPU-6050 не найден: та же шина I²C (21/22), AD0→GND (адрес 0x68), VCC→3.3В.'); }
    if(!d.nrfok) hints.push('NRF24 не найден: SCK→18, MISO→19, MOSI→23, CE→4, CSN→5; VCC→3.3В (НЕ 5В!), конденсатор 470мкФ у модуля.');
    if(!d.tok)   hints.push('Нет данных с датчика температуры: DS18B20 DATA→15 (+резистор 4,7к на 3,3В), либо NTC→34.');
    const dh=document.getElementById('dgHint'); if(dh) dh.innerHTML = hints.length ? hints.map(h=>'⚠ '+h).join('<br><br>') : '✅ Все основные модули в норме.'; }

  // в историю (с прореживанием) — только при работе датчиков
  pushHistory(p, d.tok?d.t:0);
}

/* ══════════ ИНИЦИАЛИЗАЦИЯ ══════════ */
loadHistory(); updateDbInfo();
document.getElementById('tslider').style.setProperty('--pct','9.1%');
wsConnect();
// fallback на polling если WebSocket долго молчит
setInterval(()=>{ if(!wsAlive || Date.now()-lastMsg>5000){
  fetch('/status').then(r=>r.json()).then(d=>{st=d;render();}).catch(()=>{}); } },3000);
// перерисовка графика раз в 10 сек если открыт
setInterval(()=>{ if(document.getElementById('page-graph').classList.contains('active')) drawCharts(); },10000);
</script>
</body></html>
)rawliteral";


// =====================================================================================
//  HTML страницы OTA-обновления (⚡ v4.4)
// =====================================================================================
const char OTA_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="ru"><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>H2 PRO — OTA обновление</title>
<style>
  body{background:#080810;color:#c8d0e0;font-family:sans-serif;padding:30px 16px;text-align:center;}
  h1{color:#00d4ff;font-size:1.2em;letter-spacing:3px;margin-bottom:8px;}
  .sub{color:#666;font-size:.8em;margin-bottom:24px;}
  .box{max-width:380px;margin:0 auto;background:#131326;border:1px solid #1e1e3a;
    border-radius:14px;padding:24px;}
  .warn{background:#1a1500;border:1px solid #ffcc00;color:#ffcc00;border-radius:8px;
    padding:10px;font-size:.78em;margin-bottom:18px;line-height:1.5;}
  input[type=file]{width:100%;padding:10px;background:#0e0e1c;border:1px dashed #3a3a5a;
    border-radius:8px;color:#c8d0e0;font-size:.8em;margin-bottom:16px;cursor:pointer;}
  .btn{width:100%;padding:13px;border:none;border-radius:10px;cursor:pointer;
    font-size:.85em;font-weight:800;letter-spacing:2px;text-transform:uppercase;
    background:linear-gradient(135deg,#003a4a,#005a7a);color:#00d4ff;
    border:1px solid #00d4ff;transition:all .2s;}
  .btn:active{transform:scale(.97);}
  .bar{height:8px;background:#111128;border-radius:4px;overflow:hidden;margin-top:16px;display:none;}
  .bar.show{display:block;}
  .fill{height:100%;width:0;background:linear-gradient(90deg,#00ff88,#00d4ff);transition:width .3s;}
  #pct{font-family:monospace;font-size:.8em;color:#00d4ff;margin-top:8px;}
  a{color:#00d4ff;font-size:.8em;text-decoration:none;display:inline-block;margin-top:18px;}
</style></head><body>
<h1>⬆ OTA ОБНОВЛЕНИЕ</h1>
<div class="sub">H2 PRO Brain v5.0</div>
<div class="box">
  <div class="warn">⚠ Загрузите файл <b>.bin</b>, собранный в Arduino IDE
    (Sketch → Export Compiled Binary). НЕ выключайте питание во время прошивки!</div>
  <form id="f" method="POST" action="/update" enctype="multipart/form-data">
    <input type="file" name="firmware" id="file" accept=".bin" required>
    <button class="btn" type="submit">ПРОШИТЬ</button>
  </form>
  <div class="bar" id="bar"><div class="fill" id="fill"></div></div>
  <div id="pct"></div>
  <a href="/">← На главную</a>
</div>
<script>
  const f=document.getElementById('f');
  f.addEventListener('submit',function(e){
    e.preventDefault();
    const file=document.getElementById('file').files[0];
    if(!file)return;
    const xhr=new XMLHttpRequest();
    const fd=new FormData();
    fd.append('firmware',file);
    document.getElementById('bar').className='bar show';
    xhr.upload.addEventListener('progress',function(ev){
      if(ev.lengthComputable){
        const p=Math.round(ev.loaded/ev.total*100);
        document.getElementById('fill').style.width=p+'%';
        document.getElementById('pct').innerText=p+'%  ('+Math.round(ev.loaded/1024)+' / '+Math.round(ev.total/1024)+' КБ)';
      }
    });
    xhr.addEventListener('load',function(){
      document.getElementById('pct').innerText='✅ Готово! Мозг перезагружается...';
      setTimeout(()=>{location.href='/';},6000);
    });
    xhr.addEventListener('error',function(){
      document.getElementById('pct').innerText='❌ Ошибка загрузки';
    });
    xhr.open('POST','/update');
    xhr.send(fd);
  });
</script>
</body></html>
)rawliteral";


// =====================================================================================
//  MPU6050 — ПРЯМОЙ I2C
// =====================================================================================
bool mpuWriteReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return (Wire.endTransmission() == 0);
}

bool mpuReadAccel(float &ax, float &ay, float &az) {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x3B);  // ACCEL_XOUT_H
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)MPU6050_ADDR, (uint8_t)6) != 6) return false;
  int16_t rx = (Wire.read() << 8) | Wire.read();
  int16_t ry = (Wire.read() << 8) | Wire.read();
  int16_t rz = (Wire.read() << 8) | Wire.read();
  // Делитель для ±2g (LSB sensitivity = 16384) — для угла масштаб не важен,
  // считаем в "сырых" единицах для скорости
  ax = (float)rx;
  ay = (float)ry;
  az = (float)rz;
  return true;
}

bool mpuInit() {
  // Пробуждение (PWR_MGMT_1 = 0)
  if (!mpuWriteReg(0x6B, 0x00)) return false;
  delay(50);
  // Диапазон акселерометра ±2g (ACCEL_CONFIG = 0)
  if (!mpuWriteReg(0x1C, 0x00)) return false;
  // Лёгкий фильтр (CONFIG = 3, ~44 Hz)
  if (!mpuWriteReg(0x1A, 0x03)) return false;
  delay(50);
  // Проверка чтения
  float ax, ay, az;
  if (!mpuReadAccel(ax, ay, az)) return false;
  return true;
}

void mpuCalibrate() {
  // Снимаем TILT_CAL_SAMPLES измерений, усредняем
  Serial.print("[MPU] Калибровка ");
  double sx = 0, sy = 0, sz = 0;
  int got = 0;
  for (int i = 0; i < TILT_CAL_SAMPLES; i++) {
    float ax, ay, az;
    if (mpuReadAccel(ax, ay, az)) {
      sx += ax; sy += ay; sz += az;
      got++;
    }
    if (i % 10 == 0) Serial.print('.');
    delay(10);
  }
  if (got > 0) {
    baseAx = sx / got;
    baseAy = sy / got;
    baseAz = sz / got;
    Serial.printf(" OK (samples=%d, baseline=[%.0f,%.0f,%.0f])\n", got, baseAx, baseAy, baseAz);
  } else {
    Serial.println(" FAIL");
  }
}

// Угол между текущим вектором гравитации и baseline, в градусах
float mpuAngleVsBaseline() {
  float ax, ay, az;
  if (!mpuReadAccel(ax, ay, az)) return -1.0f;
  float dot = ax*baseAx + ay*baseAy + az*baseAz;
  float magA = sqrtf(ax*ax + ay*ay + az*az);
  float magB = sqrtf(baseAx*baseAx + baseAy*baseAy + baseAz*baseAz);
  if (magA < 1.0f || magB < 1.0f) return 0;
  float c = dot / (magA * magB);
  if (c > 1.0f)  c = 1.0f;
  if (c < -1.0f) c = -1.0f;
  return acosf(c) * 180.0f / (float)M_PI;
}


// =====================================================================================
//  УТИЛИТЫ
// =====================================================================================
// Прототипы — гарантируют корректную сборку независимо от порядка определения
// (Arduino обычно генерит их сам, но явное объявление надёжнее).
void  setRelay(int pin, bool on);
void  accrueRuntime();
void  allRelaysOff();
void  cutElectroAndValve();
void  resetProcesses();
float readPressure();
void  startPiezo();
void  setTarget(float v);
void  bumpTarget(float delta);
void  saveSettings();
void  commitSettings();
void  loadSettings();
void  saveRuntime();
void  resetRuntime();
void  updateRuntimeOnElectroChange(bool nowOn);
void  wdReset();
void  wdCheck();
void  setHysteresis(float h);
void  toggleDriver();
void  startRepulsor();
void  executeCommand(uint8_t cmd);
void  safetyCheck();
void  tiltCheck();
void  updateDisplay();
void  maybeUpdateDisplay(unsigned long now);
void  updateLEDs();
void  sendStatusToRemote();
// ⚡ v5.0 прототипы
float readTemperature();
void  updateFan();
void  updateRPM(unsigned long now);
void  IRAM_ATTR fanTachISR();
void  countRelayCycles();
void  saveCycles();
void  saveCalibration();
void  saveFanConfig();
void  rfApplyChannel();
void  rfCheckHop();
void  wsBroadcastState();
void  wsTick(unsigned long now);
void  onWsEvent(AsyncWebSocket* s, AsyncWebSocketClient* c, AwsEventType type, void* arg, uint8_t* data, size_t len);

void setRelay(int pin, bool on) { digitalWrite(pin, on ? RELAY_ON : RELAY_OFF); }

// ⚡ Зафиксировать наработку электролизера и ЗАКРЫТЬ текущий интервал работы.
// Единая точка учёта (DRY): вызывается всюду, где электролизер гасится. Накапливает
// время без потери остатка миллисекунд и обнуляет electroOnSince, чтобы при
// следующем включении в счётчик НЕ попал интервал простоя под блокировкой.
void accrueRuntime() {
  if (electroOn && electroOnSince != 0) {
    runtimeMsAccum += (millis() - electroOnSince);
    runtimeSeconds += runtimeMsAccum / 1000UL;
    runtimeMsAccum %= 1000UL;
    electroOnSince = 0;
  }
}

void allRelaysOff() {
  setRelay(PIN_RELAY_ELECTRO, false);
  setRelay(PIN_RELAY_VALVE,   false);
  setRelay(PIN_RELAY_PIEZO,   false);
  accrueRuntime();
  electroOn = false; valveOn = false;
}

// Только электролизер и клапан (для TILT-сработки во время продувки)
void cutElectroAndValve() {
  setRelay(PIN_RELAY_ELECTRO, false);
  setRelay(PIN_RELAY_VALVE,   false);
  accrueRuntime();
  electroOn = false; valveOn = false;
}

void resetProcesses() {
  repulsorActive  = false;
  manualGasActive = false;
  piezoActive     = false;
}

float readPressure() {
  // ⚡ Усреднение нескольких отсчётов ADC: ESP32 ADC шумный, одиночное
  // измерение даёт дребезг ±0.05–0.1 бар, что заставляет гистерезис лишний
  // раз срабатывать. 8 отсчётов сглаживают шум, занимают ~0.1 мс.
  uint32_t acc = 0;
  for (uint8_t i = 0; i < 8; i++) acc += analogRead(PIN_PRESSURE);
  float v = (acc / 8.0f) / 4095.0f * 3.3f;
  // ⚡ v5.0: калибровка через NVS (2-точечная): P = (V - offset) * scale.
  // offset и scale настраиваются в веб-интерфейсе, формулу в коде менять не нужно.
  // Если стоит делитель 10к/20к — учтите его множитель в scale при калибровке.
  return max(0.0f, (v - presCalOffset) * presCalScale);
}

// ── ⚡ v5.0: ЧТЕНИЕ ТЕМПЕРАТУРЫ (цифровой DS18B20 или аналоговый NTC) ───────
float readTemperature() {
#ifdef TEMP_SENSOR_DS18B20
  // АСИНХРОННОЕ чтение: setWaitForConversion(false) выставлен в setup(), поэтому
  // requestTemperatures() возвращается мгновенно. Здесь мы сначала забираем
  // результат ПРЕДЫДУЩЕГО запроса (он уже готов — между вызовами проходит
  // T_TEMP_SAMP=1000 мс, а преобразование занимает ~94 мс), затем запускаем
  // новое преобразование. Так loop не блокируется на время конверсии.
  float t = tempSensor.getTempCByIndex(0);
  tempSensor.requestTemperatures();      // запуск следующего (неблокирующий)
  if (t <= -100.0f || t == 85.0f) {      // DEVICE_DISCONNECTED или power-on reset
    tempSensorOk = false;
    return currentTemp;                  // держим прошлое значение
  }
  tempSensorOk = true;
  return t;
#else
  // NTC термистор: делитель NTC—последовательный резистор, уравнение Бета
  uint32_t acc = 0;
  for (uint8_t i = 0; i < 8; i++) acc += analogRead(PIN_TEMP_NTC);
  float adc = acc / 8.0f;
  if (adc < 1 || adc > 4094) { tempSensorOk = false; return currentTemp; }
  // Делитель по схеме (см. H2_Wiring_Full): NTC к 3.3В, резистор к GND,
  // ADC меряет напряжение на резисторе → R_ntc = R_series * (4095 - adc) / adc.
  // (Прежняя формула adc/(4095-adc) соответствовала ОБРАТНОМУ включению и
  // давала инвертированную температуру — вентилятор замедлялся бы при нагреве.)
  float r = NTC_SERIES_R * (4095.0f - adc) / adc;
  float steinhart = r / NTC_NOMINAL_R;
  steinhart = logf(steinhart);
  steinhart /= NTC_BETA;
  steinhart += 1.0f / (NTC_NOMINAL_T + 273.15f);
  steinhart = 1.0f / steinhart - 273.15f;
  tempSensorOk = true;
  return steinhart;
#endif
}

// ── ⚡ v5.0: УПРАВЛЕНИЕ 4-PIN ВЕНТИЛЯТОРОМ (ШИМ по температуре) ─────────────
void updateFan() {
  int duty = 0;   // 0..100 %
  if (fanMode == FAN_OFF) {
    duty = 0;
  } else if (!tempSensorOk) {
    // ⚡ ТЗ 5.3: датчик температуры недостоверен — вентилятор на 100% (безопасная
    // реакция: при неизвестной температуре охлаждаем максимально). Кроме FAN_OFF.
    duty = 100;
  } else if (fanMode == FAN_MANUAL) {
    duty = fanDutyManual;
  } else {
    // АВТО: линейная кривая от fanTempMin (0%) до fanTempMax (100%)
    if (currentTemp <= fanTempMin)      duty = 0;
    else if (currentTemp >= fanTempMax) duty = 100;
    else duty = (int)((currentTemp - fanTempMin) / (fanTempMax - fanTempMin) * 100.0f);
  }
  fanDutyCurrent = duty;
  // Перевод 0..100% в ШИМ 0..255 с учётом FAN_MIN_DUTY (чтобы вентилятор не глох).
  // ledcWrite в core 3.x пишет в ПИН, в core 2.x — в КАНАЛ.
  int pwm = 0;
  if (duty > 0) pwm = map(duty, 1, 100, FAN_MIN_DUTY, 255);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PIN_FAN_PWM, pwm);
#else
  ledcWrite(FAN_PWM_CHANNEL, pwm);
#endif
}

// ── ⚡ v5.0: ПРЕРЫВАНИЕ ТАХОМЕТРА (считаем импульсы) ───────────────────────
void IRAM_ATTR fanTachISR() {
  fanTachCount++;
}

// Расчёт оборотов: вызывается раз в секунду. RPM = (импульсы / PPR) * 60 / период_сек.
void updateRPM(unsigned long now) {
  unsigned long dt = now - lastRpmCalc;
  if (dt < 1000) return;                  // считаем раз в секунду
  noInterrupts();
  unsigned long pulses = fanTachCount;
  fanTachCount = 0;
  interrupts();
  // обороты в минуту: импульсы за dt мс → имп/мин → /PPR
  fanRPM = (unsigned int)((pulses * 60000UL) / (dt * (unsigned long)FAN_TACH_PPR));
  lastRpmCalc = now;
}

// ── ⚡ v5.0: СЧЁТЧИК ЦИКЛОВ РЕЛЕ (детект фронта OFF→ON) ─────────────────────
void countRelayCycles() {
  if (electroOn && !relayElectroPrev) relayCyclesElectro++;
  if (valveOn   && !relayValvePrev)   relayCyclesValve++;
  if (piezoActive && !relayPiezoPrev) relayCyclesPiezo++;
  relayElectroPrev = electroOn;
  relayValvePrev   = valveOn;
  relayPiezoPrev   = piezoActive;
}

// ── ⚡ v5.0: РЕЗЕРВНЫЙ КАНАЛ NRF24 (channel hopping) ────────────────────────
void rfApplyChannel() {
  radio.setChannel(RF_CHANNELS[rfChannelIdx]);
}

void rfCheckHop() {
  // Если давно не было приёма от пульта — пробуем следующий канал.
  // Пульт делает то же самое синхронно, так что они встретятся на общем канале.
  if (millis() - rfLastActivity >= RF_HOP_SILENCE) {
    rfChannelIdx = (rfChannelIdx + 1) % RF_CHANNEL_CNT;
    rfApplyChannel();
    rfHopCount++;
    rfLastActivity = millis();   // даём время на этом канале
    Serial.printf("[RF] hop -> канал %d\n", RF_CHANNELS[rfChannelIdx]);
  }
}

void startPiezo() {
  if (!piezoActive && systemOn && !isLocked && !isPurging && !tiltLock && !repulsorActive) {
    piezoActive = true; piezoStart = millis();
    setRelay(PIN_RELAY_PIEZO, true);
  }
}

void setTarget(float v) {
  v = constrain(roundf(v * 10.0f) / 10.0f, MIN_TARGET_PRESSURE, MAX_TARGET_PRESSURE);
  targetPressure   = v;
  lastTargetChange = millis();
  saveSettings();
}

void bumpTarget(float delta) {
  setTarget(roundf((targetPressure + delta) * 2.0f) / 2.0f);
}

// ── ⚡ v4.3: УПРАВЛЕНИЕ ДРАЙВЕРОМ И ГИСТЕРЕЗИСОМ ────────────────────────────
// Фактическая запись настроек в NVS. NVS сам пропускает запись поля, если его
// значение не изменилось, так что повторные вызовы с теми же данными безопасны.
void commitSettings() {
  prefs.begin("h2cfg", false);
  prefs.putUChar("drv",   (uint8_t)electroDriver);
  prefs.putFloat("hyst",  hysteresis);
  prefs.putFloat("tgt",   targetPressure);
  prefs.putULong("rt",    runtimeSeconds);   // ⚡ v4.4: наработка
  prefs.end();
  settingsDirty = false;
}

// Отложенная пометка «настройки изменились». Реальная запись произойдёт в loop
// через SETTINGS_SAVE_DELAY после последнего изменения — это защищает flash от
// износа при частых правках (перетаскивание слайдера давления/гистерезиса даёт
// десятки промежуточных значений; пишем только финальное).
void saveSettings() {
  settingsDirty     = true;
  settingsDirtyTime = millis();
}

void loadSettings() {
  prefs.begin("h2cfg", true);
  electroDriver  = (ElectroDriver)prefs.getUChar("drv",   DRV_MOSFET);
  hysteresis     = prefs.getFloat("hyst",  0.0f);
  targetPressure = prefs.getFloat("tgt",   1.0f);
  runtimeSeconds = prefs.getULong("rt",    0);     // ⚡ v4.4
  // ⚡ v5.0: калибровка давления, вентилятор, счётчики реле
  presCalOffset  = prefs.getFloat("pco",   0.5f);
  presCalScale   = prefs.getFloat("pcs",   2.5f);
  fanTempMin     = prefs.getFloat("ftmn",  FAN_TEMP_MIN_DEF);
  fanTempMax     = prefs.getFloat("ftmx",  FAN_TEMP_MAX_DEF);
  fanMode        = (FanMode)prefs.getUChar("fmode", FAN_AUTO);
  fanDutyManual  = prefs.getInt("fdm",     50);
  relayCyclesElectro = prefs.getULong("rce", 0);
  relayCyclesValve   = prefs.getULong("rcv", 0);
  relayCyclesPiezo   = prefs.getULong("rcp", 0);
  prefs.end();
  // Защита от мусора
  if (electroDriver != DRV_MOSFET && electroDriver != DRV_RELAY) electroDriver = DRV_MOSFET;
  if (electroDriver == DRV_RELAY) hysteresis = HYST_RELAY_FIXED;
  hysteresis     = constrain(hysteresis, HYST_MIN, HYST_MAX);
  targetPressure = constrain(targetPressure, MIN_TARGET_PRESSURE, MAX_TARGET_PRESSURE);
  if (fanMode > FAN_OFF) fanMode = FAN_AUTO;
  fanDutyManual  = constrain(fanDutyManual, 0, 100);
  if (fanTempMax <= fanTempMin) { fanTempMin = FAN_TEMP_MIN_DEF; fanTempMax = FAN_TEMP_MAX_DEF; }
  savedRuntimeSeconds = runtimeSeconds;  // ⚡ v4.4: то что в NVS — уже сохранено
}

// ⚡ v5.0: сохранение калибровки давления
void saveCalibration() {
  prefs.begin("h2cfg", false);
  prefs.putFloat("pco", presCalOffset);
  prefs.putFloat("pcs", presCalScale);
  prefs.end();
}

// ⚡ v5.0: сохранение настроек вентилятора
void saveFanConfig() {
  prefs.begin("h2cfg", false);
  prefs.putFloat("ftmn",  fanTempMin);
  prefs.putFloat("ftmx",  fanTempMax);
  prefs.putUChar("fmode", (uint8_t)fanMode);
  prefs.putInt("fdm",     fanDutyManual);
  prefs.end();
}

// ⚡ v5.0: сохранение счётчиков реле
void saveCycles() {
  prefs.begin("h2cfg", false);
  prefs.putULong("rce", relayCyclesElectro);
  prefs.putULong("rcv", relayCyclesValve);
  prefs.putULong("rcp", relayCyclesPiezo);
  prefs.end();
}

// ── ⚡ v4.4: СЧЁТЧИК НАРАБОТКИ ──────────────────────────────────────────────
// Сохраняем runtime отдельно (только это поле), чтобы не трогать остальное.
// Пишем во flash ТОЛЬКО если значение секунд реально изменилось с прошлой
// записи — это защищает NVS от лишнего износа.
void saveRuntime() {
  if (runtimeSeconds == savedRuntimeSeconds) return;  // нечего сохранять
  prefs.begin("h2cfg", false);
  prefs.putULong("rt", runtimeSeconds);
  prefs.end();
  savedRuntimeSeconds = runtimeSeconds;
}

void resetRuntime() {
  runtimeSeconds = 0;
  runtimeMsAccum = 0;
  electroOnSince = electroOn ? millis() : 0;
  savedRuntimeSeconds = 1;  // форсируем запись нуля (1 != 0)
  saveRuntime();
  Serial.println("[RUNTIME] Счётчик наработки обнулён");
}

// Вызывается при каждом изменении electroOn — учитывает накопленное время
void updateRuntimeOnElectroChange(bool nowOn) {
  unsigned long ms = millis();
  if (nowOn && electroOnSince == 0) {
    // включился
    electroOnSince = ms;
  } else if (!nowOn && electroOnSince != 0) {
    // выключился — добавляем интервал без потери остатка миллисекунд
    runtimeMsAccum += (ms - electroOnSince);
    runtimeSeconds += runtimeMsAccum / 1000UL;
    runtimeMsAccum %= 1000UL;
    electroOnSince = 0;
  }
}

// ── ⚡ v4.4: WATCHDOG ПО ДАВЛЕНИЮ ──────────────────────────────────────────
// Сбросить watchdog (вызывается когда есть прогресс или электролизер выкл)
void wdReset() {
  pressureWatchdog = false;
  wdStart       = millis();
  wdRefPressure = currentPressure;
}

// Проверка watchdog — вызывается в loop когда электролизер активен в авто-режиме
void wdCheck() {
  unsigned long now = millis();
  // Если давление выросло на WD_PROGRESS_BAR от опорного — есть прогресс, сброс
  if (currentPressure >= wdRefPressure + WD_PROGRESS_BAR) {
    wdStart       = now;
    wdRefPressure = currentPressure;
    pressureWatchdog = false;
    return;
  }
  // Прогресса нет — проверяем тайм-аут
  if (now - wdStart >= WD_TIMEOUT) {
    if (!pressureWatchdog) {
      Serial.printf("[WATCHDOG] Бак не набирает давление за %lu сек! P=%.2f, опорное=%.2f\n",
                    WD_TIMEOUT/1000UL, currentPressure, wdRefPressure);
    }
    pressureWatchdog = true;
  }
}

void setHysteresis(float h) {
  if (electroDriver == DRV_RELAY) return;  // в режиме РЕЛЕ изменение запрещено
  // Округление до шага HYST_STEP (0.05)
  h = roundf(h / HYST_STEP) * HYST_STEP;
  hysteresis = constrain(h, HYST_MIN, HYST_MAX);
  saveSettings();
}

void toggleDriver() {
  if (electroDriver == DRV_MOSFET) {
    electroDriver = DRV_RELAY;
    hysteresis    = HYST_RELAY_FIXED;  // принудительно
    Serial.println("[DRV] -> РЕЛЕ (гистерезис принудительно 0.40 бар)");
  } else {
    electroDriver = DRV_MOSFET;
    // hysteresis оставляем как было (загруженное или последнее MOSFET-значение).
    // Если он равен релейному фиксированному — сбрасываем в 0. Сравниваем
    // через допуск, а не ==, т.к. float напрямую сравнивать ненадёжно.
    if (fabsf(hysteresis - HYST_RELAY_FIXED) < 0.001f) hysteresis = 0.0f;
    Serial.println("[DRV] -> MOSFET");
  }
  saveSettings();
}

// ── ⚡ v4.3: ЗАПУСК РЕПУЛЬСОРА ─────────────────────────────────────────────
void startRepulsor() {
  if (repulsorActive) return;  // повторное нажатие во время цикла игнорируем
  if (!systemOn || isLocked || tiltLock || isPurging || isPurgeShort) return;
  repulsorActive = true;
  repulsorStart  = millis();
  manualGasActive = false;     // взаимоисключение с manual gas
  // Открываем клапан сразу. Пьезо включится в loop() при t >= 700 мс.
  setRelay(PIN_RELAY_VALVE, true);
  valveOn = true;
  Serial.println("[REPULSOR] FIRE!");
}


// =====================================================================================
//  КОМАНДЫ
// =====================================================================================
void executeCommand(uint8_t cmd) {
  if (isPurging && cmd != 2) return;
  wsStateChanged = true;   // ⚡ v5.0: любая команда → немедленный push в WebSocket

  switch (cmd) {
    case 1:  // POWER
      if (!systemOn) {
        if (overpressureLock && currentPressure < WARN_PRESSURE) overpressureLock = false;
        systemOn = true;
      } else {
        systemOn = false; resetProcesses(); allRelaysOff();
      }
      break;

    case 2:  // LOCK toggle + сброс защёлок TILT/overpressure
      if (isLocked || tiltLock || overpressureLock) {
        // Попытка разблокировки: сбрасываем всё, что можем
        tiltLock = false;
        if (currentPressure < WARN_PRESSURE) overpressureLock = false;
        isLocked = false;
        // Если устройство всё ещё наклонено, на следующем тике tiltLock
        // снова взведётся — и isLocked тоже
      } else {
        isLocked = true;
        resetProcesses();
        // LOCK во время продувки = аборт продувки: иначе клапан закрывается
        // (allRelaysOff), а isPurging ещё до 90 сек держит систему в продувке.
        if (isPurging) { isPurging = false; systemOn = false; }
        allRelaysOff();
      }
      break;

    case 3:  // РЕПУЛЬСОР — одноразовый выстрел
      startRepulsor();
      break;

    case 4: startPiezo(); break;

    case 5:  // PURGE SHORT
      if (systemOn && !isLocked && !tiltLock && !isPurging && !isPurgeShort && !repulsorActive) {
        isPurgeShort    = true;
        purgeShortStart = millis();
        setRelay(PIN_RELAY_VALVE, true);
        valveOn = true;
      }
      break;

    case 7:  // MANUAL GAS
      if (systemOn && !isLocked && !tiltLock && !repulsorActive) {
        manualGasActive = !manualGasActive;
        if (manualGasActive) repulsorActive = false;
      }
      break;

    case 8: bumpTarget(+TARGET_STEP); break;
    case 9: bumpTarget(-TARGET_STEP); break;

    case 10: // DRIVER toggle (MOSFET ↔ РЕЛЕ)
      toggleDriver();
      break;

    case 99: // SAFE OFF — продувка до нуля
      // БЕЗ проверки systemOn: при двойном клике POWER (пульт и кнопка мозга)
      // первый клик уже выключил систему, и cmd 99 приходил в systemOn=false —
      // продувка молча не запускалась. Продувка-сброс должна работать из
      // любого состояния (это аварийное стравливание бака).
      resetProcesses();
      setRelay(PIN_RELAY_ELECTRO, false);
      setRelay(PIN_RELAY_PIEZO,   false);
      setRelay(PIN_RELAY_VALVE,   true);
      accrueRuntime();        // ⚡ зафиксировать наработку перед гашением
      electroOn = false; valveOn = true;
      isPurging = true; purgeStart = millis();
      break;
  }
}


// =====================================================================================
//  БЕЗОПАСНОСТЬ — ДАВЛЕНИЕ
// =====================================================================================
void safetyCheck() {
  if (currentPressure > MAX_SAFE_PRESSURE && systemOn) {
    setRelay(PIN_RELAY_ELECTRO, false);
    setRelay(PIN_RELAY_PIEZO,   false);
    // Во время продувки клапан НЕ закрываем: открытый клапан — единственный
    // путь сброса давления. Закрыть его при сверхдавлении значило бы
    // заблокировать стравливание до 90-сек таймаута продувки.
    if (!isPurging) {
      setRelay(PIN_RELAY_VALVE, false);
      valveOn = false;
    }
    accrueRuntime();           // ⚡ зафиксировать наработку перед гашением
    electroOn = false;
    piezoActive = false;
    repulsorActive = false;
    overpressureLock = true;
  }
}


// =====================================================================================
//  БЕЗОПАСНОСТЬ — НАКЛОН
// =====================================================================================
void tiltCheck() {
  if (!mpuOk || tiltCalibrating) return;

  float ang = mpuAngleVsBaseline();
  if (ang < 0) return;  // ошибка чтения — пропускаем тик

  // Скользящее среднее
  angleBuf[angleIdx] = ang;
  angleIdx = (angleIdx + 1) % TILT_AVG_SAMPLES;
  if (angleIdx == 0) angleBufFilled = true;
  int n = angleBufFilled ? TILT_AVG_SAMPLES : angleIdx;
  if (n == 0) return;
  float sum = 0;
  for (int i = 0; i < n; i++) sum += angleBuf[i];
  currentTiltDeg = sum / n;

  if (currentTiltDeg > TILT_ANGLE_DEG) {
    if (!tiltLock) Serial.printf("[TILT] Сработка: %.1f° > %.1f°\n", currentTiltDeg, TILT_ANGLE_DEG);
    tiltLock = true;
    cutElectroAndValve();
    setRelay(PIN_RELAY_PIEZO, false);
    piezoActive     = false;
    repulsorActive  = false;
    manualGasActive = false;
  }
}


// =====================================================================================
//  OLED
// =====================================================================================
void updateDisplay() {
  if (!oledOk) return;
  unsigned long now = millis();
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);

  // ── Экран установки целевого давления ──────────────────────────────────
  if (now - lastTargetChange < T_SHOW_TARGET) {
    oled.setTextSize(1);
    oled.setCursor(8, 0); oled.print("TARGET PRESSURE");
    oled.drawLine(0, 9, 127, 9, SSD1306_WHITE);
    oled.setTextSize(3);
    char tBuf[8]; dtostrf(targetPressure, 4, 1, tBuf);
    oled.setCursor(8, 14); oled.print(tBuf);
    oled.setTextSize(1);
    oled.setCursor(100, 22); oled.print("bar");
    int tw = constrain((int)(targetPressure / MAX_TARGET_PRESSURE * 127.0f), 0, 127);
    oled.drawRect(0, 42, 128, 5, SSD1306_WHITE);
    if (tw > 0) oled.fillRect(1, 43, tw, 3, SSD1306_WHITE);
    oled.setCursor(0, 50);
    oled.printf("%.1f         %.1f bar", MIN_TARGET_PRESSURE, MAX_TARGET_PRESSURE);
    oled.display();
    return;
  }

  // ── Заголовок ───────────────────────────────────────────────────────────
  oled.setTextSize(1);
  oled.setCursor(0, 0); oled.print("H2 PRO");
  oled.setCursor(48, 0); oled.print("RF:"); oled.print(remoteOnline ? "OK" : "--");
  oled.setCursor(86, 0);
  if (tiltCalibrating)    oled.print("CAL..");
  else if (tiltLock)      oled.print("TILT!");
  else if (overpressureLock) oled.print("!OVP");
  else if (isPurging)     oled.print("PURG");
  else if (isPurgeShort)  oled.print("PURG");
  else                    oled.print(systemOn ? (isLocked ? "LOCK" : "ON") : "OFF");
  oled.drawLine(0, 9, 127, 9, SSD1306_WHITE);

  // ── Давление крупно ─────────────────────────────────────────────────────
  oled.setTextSize(2);
  oled.setCursor(0, 12);
  char pBuf[8]; dtostrf(currentPressure, 4, 2, pBuf);
  oled.print(pBuf);
  oled.setTextSize(1);
  oled.setCursor(86, 18); oled.print("bar");

  // ── Угол наклона справа от давления ──────────────────────────────────────
  oled.setCursor(108, 12);
  if (tiltCalibrating) { oled.print("CAL"); }
  else if (mpuOk)      { oled.printf("%3d", (int)currentTiltDeg); oled.setCursor(108, 22); oled.print("deg"); }
  else                 { oled.print("---"); }

  // ── Полоска давления 0..6 бар + маркер цели ─────────────────────────────
  const float SCALE = 7.0f;   // шкала давления 0..7 бар (запас над аварийным 6.8)
  int bw = constrain((int)(currentPressure / SCALE * 127.0f), 0, 127);
  oled.drawRect(0, 30, 128, 5, SSD1306_WHITE);
  if (bw > 0) oled.fillRect(1, 31, bw, 3, SSD1306_WHITE);
  int tx = constrain((int)(targetPressure / SCALE * 127.0f), 0, 127);
  oled.drawLine(tx, 28, tx, 36, SSD1306_WHITE);
  oled.drawLine(tx-1, 28, tx+1, 28, SSD1306_WHITE);
  oled.drawLine(0, 38, 127, 38, SSD1306_WHITE);

  // ── E/V/P + температура/вентилятор ──────────────────────────────────────
  oled.setCursor(0, 41);
  oled.print("E:"); oled.print(electroOn   ? "ON " : "OFF");
  oled.print(" V:"); oled.print(valveOn     ? "ON " : "OFF");
  oled.print(" P:"); oled.print(piezoActive ? "ON"  : "OF");
  // Температура и обороты вентилятора справа
  oled.setCursor(86, 41);
  if (tempSensorOk) { oled.printf("%2d", (int)roundf(currentTemp)); oled.print("\xF8"); oled.printf("%d", fanDutyCurrent); }  // \xF8 = ° (CP437)
  else              { oled.print("t--"); }

  // ── Статус / режим ──────────────────────────────────────────────────────
  char tgtBuf[6]; dtostrf(targetPressure, 3, 1, tgtBuf);
  oled.setCursor(0, 52);
  if (tiltCalibrating)        oled.print("MPU CALIBRATING...");
  else if (tiltLock)          { oled.print("TILT! "); oled.print((int)currentTiltDeg); oled.print(" deg LOCK"); }
  else if (overpressureLock)  oled.print(">RELEASE PRESSURE!<");
  else if (isPurging)         { oled.print("PURGE "); oled.print(pBuf); oled.print(" bar"); }
  else if (isPurgeShort)      oled.print("MINI-PURGE 200ms");
  else if (repulsorActive)    {
    unsigned long dt = millis() - repulsorStart;
    if (dt < T_REPULSOR_PIEZO_AT) { oled.print(">REPULSOR GAS "); oled.print(dt); oled.print("ms"); }
    else                          { oled.print(">>REPULSOR FIRE!<<"); }
  }
  else if (!systemOn)         { oled.print("OFF  T="); oled.print(tgtBuf); }
  else if (isLocked)          oled.print("LOCKED");
  else if (pressureWatchdog)  { oled.print("!NO PRESSURE RISE!"); }
  else if (manualGasActive)   oled.print("MANUAL GAS");
  else {
    // АВТО — показываем драйвер и гистерезис
    char hBuf[8]; dtostrf(hysteresis, 3, 2, hBuf);
    oled.print(electroDriver == DRV_MOSFET ? "MOS h=" : "REL h=");
    oled.print(hBuf);
    oled.print(" T="); oled.print(tgtBuf);
  }

  oled.display();
}

// ⚡ ТЗ 7.3: перерисовывать OLED не чаще раза в T_DISPLAY_UPD и ТОЛЬКО при изменении
// отображаемых данных (либо принудительно раз в 1 с, либо на «живых» экранах —
// показ установки цели и репульсор). Убирает мерцание и нагрузку на шину I2C.
void maybeUpdateDisplay(unsigned long now) {
  if (!oledOk) return;
  if (now - lastDispCheck < T_DISPLAY_UPD) return;
  lastDispCheck = now;
  char sig[110];
  snprintf(sig, sizeof(sig),
    "%d%d%d%d%d%d%d%d%d%d%d%d%d|%.2f|%.1f|%d|%d|%d|%.2f|%d|%d",
    systemOn, isLocked, isPurging, isPurgeShort, repulsorActive, manualGasActive,
    electroOn, valveOn, piezoActive, overpressureLock, tiltLock, tiltCalibrating,
    pressureWatchdog, currentPressure, targetPressure, (int)roundf(currentTiltDeg),
    (int)roundf(currentTemp), fanDutyCurrent, hysteresis, (int)electroDriver,
    remoteOnline ? 1 : 0);
  bool animated = (now - lastTargetChange < T_SHOW_TARGET) || repulsorActive;
  bool forced   = (now - lastDispDraw >= 1000);
  if (animated || forced || strcmp(sig, lastDispSig) != 0) {
    updateDisplay();
    strncpy(lastDispSig, sig, sizeof(lastDispSig) - 1);
    lastDispSig[sizeof(lastDispSig) - 1] = '\0';
    lastDispDraw = now;
  }
}


// =====================================================================================
//  LED
// =====================================================================================
void updateLEDs() {
  digitalWrite(PIN_LED_POWER, (systemOn && !isPurging) ? HIGH : LOW);
  digitalWrite(PIN_LED_LOCK,  (isLocked || tiltLock || overpressureLock) ? HIGH : LOW);
  if (tiltLock)
    digitalWrite(PIN_LED_MODE, (millis() / 150) % 2 ? HIGH : LOW);   // быстрое мигание при TILT
  else if (repulsorActive)
    digitalWrite(PIN_LED_MODE, (millis() / 80)  % 2 ? HIGH : LOW);   // очень быстрое — стробит
  else
    digitalWrite(PIN_LED_MODE, manualGasActive ? HIGH : LOW);
}


// =====================================================================================
//  NRF24 — отправка статуса
// =====================================================================================
void sendStatusToRemote() {
  StatusPacket pkt;
  pkt.type     = 0xAA;
  pkt.pressure = currentPressure;
  pkt.target   = targetPressure;

  uint8_t f = 0;
  if (systemOn)        f |= (1<<0);
  if (isLocked)        f |= (1<<1);
  if (isPurging)       f |= (1<<2);
  if (repulsorActive)  f |= (1<<3);   // ⚡ v4.3: бит3 теперь repulsor
  if (manualGasActive) f |= (1<<4);
  if (electroOn)       f |= (1<<5);
  if (valveOn)         f |= (1<<6);
  if (piezoActive)     f |= (1<<7);
  pkt.flags = f;

  uint8_t f2 = 0;
  if (currentPressure > WARN_PRESSURE)  f2 |= (1<<0);
  if (overpressureLock)                 f2 |= (1<<1);
  if (isPurgeShort)                     f2 |= (1<<2);
  if (tiltLock)                         f2 |= (1<<3);
  if (tiltCalibrating)                  f2 |= (1<<4);
  if (electroDriver == DRV_RELAY)       f2 |= (1<<5);  // ⚡ v4.3: бит5 = driver
  if (pressureWatchdog)                 f2 |= (1<<6);  // ⚡ v5.0: watchdog
  if (overheatWarn)                     f2 |= (1<<7);  // ⚡ v5.0: перегрев
  pkt.flags2 = f2;

  int a = (int)roundf(currentTiltDeg);
  if (a < 0) a = 0; if (a > 180) a = 180;
  pkt.tiltAngle = (uint8_t)a;

  // ⚡ v4.3: гистерезис × 10 (0.0..1.0 → 0..10)
  int hx = (int)roundf(hysteresis * 10.0f);
  if (hx < 0) hx = 0; if (hx > 10) hx = 10;
  pkt.hystX10 = (uint8_t)hx;

  // ⚡ v5.0: температура и обороты вентилятора
  int tc = (int)roundf(currentTemp);
  tc = constrain(tc, -40, 125);
  pkt.temp    = (int8_t)tc;
  pkt.fanDuty = (uint8_t)constrain(fanDutyCurrent, 0, 100);

  if (!radioOk) return;   // NRF24 не инициализирован — пропускаем
  radio.stopListening();
  bool acked = radio.write(&pkt, sizeof(pkt));
  radio.startListening();
  // Аппаратный ACK пульта = канал жив. Без этого мозг считал канал «мёртвым»
  // (пульт сам передаёт ТОЛЬКО при нажатии кнопок) и прыгал по каналам каждые
  // RF_HOP_SILENCE, постоянно разрывая связь с молчащим пультом.
  if (acked) rfLastActivity = millis();
}


// =====================================================================================
//  КНОПКИ
// =====================================================================================
void handlePowerBtn(unsigned long now) {
  bool raw = digitalRead(PIN_BTN_POWER);
  if (raw != pwrLastRaw) pwrDebTime = now;
  if ((now - pwrDebTime) > T_DEBOUNCE && raw != pwrStable) {
    pwrStable = raw;
    if (pwrStable == LOW) {
      if ((now - pwrLastClick) < T_DOUBLE_CLICK) executeCommand(99);
      else                                        executeCommand(1);
      pwrLastClick = now;
    }
  }
  pwrLastRaw = raw;
}

void handleLockBtn(unsigned long now) {
  bool raw = digitalRead(PIN_BTN_LOCK);
  if (raw != lckLastRaw) lckDebTime = now;
  if ((now - lckDebTime) > T_DEBOUNCE && raw != lckStable) {
    lckStable = raw;
    if (lckStable == LOW) executeCommand(2);
  }
  lckLastRaw = raw;
}

void handlePurgeBtn(unsigned long now) {
  bool raw = digitalRead(PIN_BTN_PURGE);
  if (raw != prgLastRaw) prgDebTime = now;
  if ((now - prgDebTime) > T_DEBOUNCE && raw != prgStable) {
    prgStable = raw;
    if (prgStable == LOW) executeCommand(5);
  }
  prgLastRaw = raw;
}


// =====================================================================================
//  ⚡ v5.0: ФОРМИРОВАНИЕ JSON СОСТОЯНИЯ (общее для /status и WebSocket)
// =====================================================================================
void buildStatusJson(char* buf, size_t bufLen) {
  unsigned long up = (millis() - bootMs) / 1000UL;
  bool ovw = currentPressure > WARN_PRESSURE;
  unsigned long rt = runtimeSeconds;
  if (electroOn && electroOnSince != 0)
    rt += (runtimeMsAccum + (millis() - electroOnSince)) / 1000UL;
  // Предупреждение о ресурсе реле — если любой счётчик превысил порог
  bool relWarn = (relayCyclesElectro > RELAY_LIFE_WARN ||
                  relayCyclesValve   > RELAY_LIFE_WARN ||
                  relayCyclesPiezo   > RELAY_LIFE_WARN);
  snprintf(buf, bufLen,
    "{\"p\":%.2f,\"tgt\":%.1f,\"h\":%.2f,\"drv\":%d,"
    "\"pwr\":%d,\"lck\":%d,\"prg\":%d,\"psh\":%d,\"rep\":%d,\"mgs\":%d,"
    "\"pzo\":%d,\"ele\":%d,\"val\":%d,\"rf\":%d,\"ovw\":%d,\"ovl\":%d,"
    "\"tlk\":%d,\"tcal\":%d,\"wd\":%d,\"rt\":%lu,\"ta\":%d,\"up\":%lu,"
    "\"t\":%.1f,\"tok\":%d,\"oh\":%d,\"fan\":%d,\"rpm\":%u,\"fmode\":%d,\"fmin\":%.0f,\"fmax\":%.0f,"
    "\"rce\":%lu,\"rcv\":%lu,\"rcp\":%lu,\"relw\":%d,"
    "\"pco\":%.3f,\"pcs\":%.3f,\"rfc\":%d,\"rfh\":%lu,"
    "\"oledok\":%d,\"mpuok\":%d,\"nrfok\":%d}",
    currentPressure, targetPressure, hysteresis, (int)electroDriver,
    systemOn?1:0, isLocked?1:0, isPurging?1:0, isPurgeShort?1:0,
    repulsorActive?1:0, manualGasActive?1:0, piezoActive?1:0, electroOn?1:0,
    valveOn?1:0, remoteOnline?1:0, ovw?1:0, overpressureLock?1:0,
    tiltLock?1:0, tiltCalibrating?1:0, pressureWatchdog?1:0,
    rt, (int)roundf(currentTiltDeg), up,
    currentTemp, tempSensorOk?1:0, overheatWarn?1:0, fanDutyCurrent, fanRPM, (int)fanMode,
    fanTempMin, fanTempMax,
    relayCyclesElectro, relayCyclesValve, relayCyclesPiezo, relWarn?1:0,
    presCalOffset, presCalScale, RF_CHANNELS[rfChannelIdx], (unsigned long)rfHopCount,
    oledOk?1:0, mpuOk?1:0, radioOk?1:0);
}

// ⚡ v5.0: рассылка состояния всем WebSocket-клиентам
void wsBroadcastState() {
  if (ws.count() == 0) return;
  char buf[768];
  buildStatusJson(buf, sizeof(buf));
  ws.textAll(buf);
}

// ⚡ v5.0: периодический WebSocket-тик (push при изменении + heartbeat 1 Гц +
// очистка отвалившихся клиентов). Вынесен в функцию, чтобы вызывать его и в
// ветках loop с ранним return (продувка, блокировка, OTA) — иначе веб-дашборд
// переставал бы получать обновления по WebSocket в этих состояниях.
void wsTick(unsigned long now) {
  if (wsStateChanged || (now - lastWsHeartbeat >= T_WS_HEARTBEAT)) {
    wsBroadcastState();
    wsStateChanged  = false;
    lastWsHeartbeat = now;
  }
  ws.cleanupClients();
}

// ⚡ v5.0: обработчик событий WebSocket
void onWsEvent(AsyncWebSocket* s, AsyncWebSocketClient* c, AwsEventType type,
               void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    // Новому клиенту сразу шлём полное состояние
    char buf[768];
    buildStatusJson(buf, sizeof(buf));
    c->text(buf);
  } else if (type == WS_EVT_DATA) {
    // Клиент может прислать команду: {"cmd":N} или {"set":"...","val":...}
    // Парсим простым способом без библиотеки JSON.
    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      // ВАЖНО: не писать data[len]=0 — это выход за границу буфера AsyncWebSocket
      // (heap corruption). Конструктор String с длиной копирует ровно len байт.
      String msg((char*)data, len);
      int ci = msg.indexOf("\"cmd\":");
      if (ci >= 0) {
        int cmd = msg.substring(ci + 6).toInt();
        executeCommand((uint8_t)cmd);
        wsStateChanged = true;
      }
    }
  }
}


// =====================================================================================
//  SETUP
// =====================================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== H2 PRO Brain v5.0 ===");
  bootMs = millis();

  // Реле — сразу выключить (до любых других инициализаций)
  pinMode(PIN_RELAY_ELECTRO, OUTPUT);
  pinMode(PIN_RELAY_VALVE,   OUTPUT);
  pinMode(PIN_RELAY_PIEZO,   OUTPUT);
  allRelaysOff();

  // ⚡ v4.3: Загружаем сохранённые настройки из NVS (драйвер, гистерезис, цель)
  loadSettings();

  // ⚡ v5.0: ШИМ вентилятора (LEDC) — сразу выключен
  // ESP32 Arduino core 3.x сменил LEDC API: ledcSetup+ledcAttachPin → ledcAttach,
  // и ledcWrite теперь принимает ПИН вместо канала. Поддерживаем обе версии ядра.
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  if (!ledcAttach(PIN_FAN_PWM, FAN_PWM_FREQ, FAN_PWM_BITS))   // core 3.x
    Serial.println("LEDC: FAIL — ШИМ вентилятора не работает (нет канала)!");
  ledcWrite(PIN_FAN_PWM, 0);
#else
  ledcSetup(FAN_PWM_CHANNEL, FAN_PWM_FREQ, FAN_PWM_BITS); // core 2.x
  ledcAttachPin(PIN_FAN_PWM, FAN_PWM_CHANNEL);
  ledcWrite(FAN_PWM_CHANNEL, 0);
#endif
  // ⚡ v5.0: тахометр 4-pin вентилятора — вход с подтяжкой + прерывание по фронту.
  // Тахометр open-collector, поэтому нужна подтяжка (внутренняя INPUT_PULLUP).
  pinMode(PIN_FAN_TACH, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_FAN_TACH), fanTachISR, FALLING);
  lastRpmCalc = millis();

  // АЦП — явно 12 бит: readPressure() (GPIO 36) и NTC (GPIO 34) делят на 4095.
  // Не полагаемся на дефолт ядра — давление участвует в контуре безопасности.
  analogReadResolution(12);

  // ⚡ v5.0: датчик температуры
#ifdef TEMP_SENSOR_DS18B20
  tempSensor.begin();
  tempSensor.setResolution(10);          // 10 бит ≈ 0.25°C, конверсия ~94 мс
  tempSensor.setWaitForConversion(false); // АСИНХРОННО — не блокировать loop
  tempSensor.requestTemperatures();       // запускаем первое преобразование
  currentTemp  = 25.0f;                    // стартовое значение до первого готового замера
  tempSensorOk = false;
  Serial.println("Темп. датчик: DS18B20 (1-Wire, GPIO 15, асинхронно)");
#else
  Serial.println("Темп. датчик: NTC (ADC, GPIO 34)");
  currentTemp = readTemperature();
#endif

  // LED
  pinMode(PIN_LED_POWER, OUTPUT); digitalWrite(PIN_LED_POWER, LOW);
  pinMode(PIN_LED_LOCK,  OUTPUT); digitalWrite(PIN_LED_LOCK,  HIGH);
  pinMode(PIN_LED_MODE,  OUTPUT); digitalWrite(PIN_LED_MODE,  LOW);

  // Кнопки
  pinMode(PIN_BTN_POWER, INPUT_PULLUP);
  pinMode(PIN_BTN_LOCK,  INPUT_PULLUP);
  pinMode(PIN_BTN_PURGE, INPUT_PULLUP);

  // I2C (общая шина: OLED + MPU6050)
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  // OLED
  oledOk = oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!oledOk) {
    Serial.println("OLED: нет (проверьте I2C, адрес 0x3C/0x3D)");
  } else {
    oled.cp437(true);   // корректная таблица CP437: \xF8 = знак градуса
    oled.clearDisplay(); oled.setTextSize(1); oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(8, 16); oled.print("H2 PRO  Brain v5.0");
    oled.setCursor(8, 32); oled.print("MPU6050 calibrating");
    oled.setCursor(8, 44); oled.print("Do not move!");
    oled.display();
    Serial.println("OLED: OK");
  }

  // MPU6050 — инициализация и калибровка
  tiltCalibrating = true;
  mpuOk = mpuInit();
  if (mpuOk) {
    Serial.println("MPU6050: OK");
    mpuCalibrate();
  } else {
    Serial.println("MPU6050: НЕТ — TILT-защита отключена!");
    if (oledOk) {
      oled.clearDisplay(); oled.setCursor(8, 24);
      oled.print("MPU6050: NOT FOUND");
      oled.setCursor(8, 40);
      oled.print("TILT protection OFF");
      oled.display();
      delay(1500);
    }
  }
  tiltCalibrating = false;

  // NRF24
  SPI.begin(18, 19, 23, PIN_NRF_CSN);
  if (radio.begin()) {
    radio.openWritingPipe(PIPE_B2R);
    radio.openReadingPipe(1, PIPE_R2B);
    radio.setPALevel(RF24_PA_MAX);
    radio.setDataRate(RF24_2MBPS);
    radio.setCRCLength(RF24_CRC_16);
    radio.setChannel(76);
    // ⚡ ОПТИМИЗАЦИЯ: статус шлётся широковещательно 10 раз/сек, потеря
    // отдельного пакета некритична (пульт сам отслеживает таймаут связи).
    // Минимизируем автоповторы, чтобы radio.write() статуса при выключенном
    // пульте не блокировал loop надолго: 1 попытка вместо 15, без задержки.
    // Команды пульт→мозг (приём) это не затрагивает — там свой ACK.
    radio.setRetries(0, 1);
    rfChannelIdx   = 0;
    rfApplyChannel();          // ⚡ v5.0: канал из RF_CHANNELS
    rfLastActivity = millis();
    radio.startListening();
    radioOk = true;
    Serial.printf("NRF24: OK (2 Мбит/с, канал %d, +резерв)\n", RF_CHANNELS[rfChannelIdx]);
  } else { radioOk = false; Serial.println("NRF24: FAIL — работа только через веб-интерфейс"); }

  // Wi-Fi AP
  bool apOk = WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("WiFi AP: "); Serial.print(AP_SSID);
  if (apOk) { Serial.print("  IP: "); Serial.println(WiFi.softAPIP()); }
  else      Serial.println("  ОШИБКА запуска точки доступа!");

  // HTTP
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send_P(200, "text/html", INDEX_HTML);
  });
  server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (req->hasParam("id")) executeCommand((uint8_t)req->getParam("id")->value().toInt());
    req->send(200, "text/plain", "OK");
  });
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest* req) {
    char buf[768];
    buildStatusJson(buf, sizeof(buf));
    req->send(200, "application/json", buf);
  });
  server.on("/setpressure", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (req->hasParam("val")) setTarget(req->getParam("val")->value().toFloat());
    wsStateChanged = true;   // ⚡ немедленный push изменения остальным веб-клиентам
    req->send(200, "text/plain", "OK");
  });
  server.on("/sethysteresis", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (req->hasParam("val")) {
      float h = req->getParam("val")->value().toFloat();
      setHysteresis(h);
    }
    wsStateChanged = true;   // ⚡ немедленный push изменения остальным веб-клиентам
    req->send(200, "text/plain", "OK");
  });
  // ⚡ v5.0: настройка вентилятора — режим, ручная скорость, температурная кривая
  server.on("/setfan", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (req->hasParam("mode")) {
      int m = req->getParam("mode")->value().toInt();
      if (m >= 0 && m <= 2) fanMode = (FanMode)m;
    }
    if (req->hasParam("duty")) fanDutyManual = constrain(req->getParam("duty")->value().toInt(), 0, 100);
    if (req->hasParam("tmin")) fanTempMin = req->getParam("tmin")->value().toFloat();
    if (req->hasParam("tmax")) fanTempMax = req->getParam("tmax")->value().toFloat();
    if (fanTempMax <= fanTempMin) fanTempMax = fanTempMin + 1.0f;
    saveFanConfig();
    updateFan();
    wsStateChanged = true;
    req->send(200, "text/plain", "OK");
  });
  // ⚡ v5.0: калибровка датчика давления — offset (В при 0 бар) и scale (бар/В)
  server.on("/calibrate", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (req->hasParam("offset")) presCalOffset = req->getParam("offset")->value().toFloat();
    if (req->hasParam("scale"))  presCalScale  = req->getParam("scale")->value().toFloat();
    // защита от абсурдных значений
    presCalOffset = constrain(presCalOffset, 0.0f, 2.0f);
    presCalScale  = constrain(presCalScale, 0.1f, 10.0f);
    saveCalibration();
    wsStateChanged = true;
    req->send(200, "text/plain", "OK");
  });
  // ⚡ v5.0: сброс счётчиков циклов реле
  server.on("/resetcycles", HTTP_GET, [](AsyncWebServerRequest* req) {
    relayCyclesElectro = relayCyclesValve = relayCyclesPiezo = 0;
    saveCycles();
    wsStateChanged = true;
    req->send(200, "text/plain", "OK");
  });
  // ⚡ v4.4: сброс счётчика наработки
  server.on("/resetruntime", HTTP_GET, [](AsyncWebServerRequest* req) {
    resetRuntime();
    req->send(200, "text/plain", "OK");
  });

  // ⚡ v4.4: OTA — страница загрузки прошивки
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (systemOn) {
      req->send(403, "text/html",
        "<html><head><meta charset='utf-8'></head><body style='font-family:sans-serif;"
        "background:#0d1117;color:#e0e0e0;padding:40px;text-align:center;'>"
        "<h2 style='color:#ff3355;'>⛔ OTA недоступно</h2>"
        "<p>Сначала ВЫКЛЮЧИТЕ систему (POWER), затем обновляйте прошивку.</p>"
        "<a href='/' style='color:#00d4ff;'>← На главную</a></body></html>");
      return;
    }
    req->send(200, "text/html", OTA_HTML);
  });

  // ⚡ v4.4: OTA — приём .bin файла
  server.on("/update", HTTP_POST,
    [](AsyncWebServerRequest* req) {
      // !systemOn: если загрузку отклонили (система была включена), Update
      // даже не стартовал и hasError()==false — без этой проверки страница
      // ответила бы «успех» и перезагрузила мозг без прошивки.
      bool ok = !Update.hasError() && !systemOn;
      AsyncWebServerResponse* resp = req->beginResponse(200, "text/html",
        ok ? "<html><head><meta charset='utf-8'></head><body style='font-family:sans-serif;"
             "background:#0d1117;color:#56d364;padding:40px;text-align:center;'>"
             "<h2>✅ Прошивка загружена</h2><p>Мозг перезагружается...</p></body></html>"
           : "<html><head><meta charset='utf-8'></head><body style='font-family:sans-serif;"
             "background:#0d1117;color:#ff3355;padding:40px;text-align:center;'>"
             "<h2>❌ Ошибка прошивки</h2><a href='/update' style='color:#00d4ff;'>← Назад</a></body></html>");
      resp->addHeader("Connection", "close");
      req->send(resp);
      if (ok) { delay(500); ESP.restart(); }
    },
    [](AsyncWebServerRequest* req, String filename, size_t index, uint8_t* data, size_t len, bool final) {
      if (index == 0) {
        if (systemOn) {            // та же защита, что на GET /update:
          Serial.println("[OTA] Отклонено: система включена — сначала POWER OFF");
          return;                  // нельзя прошивать работающую установку
        }
        Serial.printf("[OTA] Старт: %s\n", filename.c_str());
        otaInProgress = true;
        allRelaysOff();            // безопасность — гасим всё на время прошивки
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
      }
      if (!otaInProgress) return;  // загрузка была отклонена на первом чанке
      if (Update.write(data, len) != len) Update.printError(Serial);
      if (final) {
        if (Update.end(true)) Serial.printf("[OTA] Успех: %u байт\n", index + len);
        else                  Update.printError(Serial);
        otaInProgress = false;
      }
    }
  );

  // ⚡ v5.0: WebSocket для дашборда реального времени
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.begin();
  Serial.println("WebServer + WebSocket: OK  →  http://192.168.4.1");
  Serial.println("Старт: systemOn=false, isLocked=true");
  Serial.printf("Загружено из NVS: drv=%s, hyst=%.2f, target=%.1f, наработка=%lu сек\n",
    electroDriver == DRV_MOSFET ? "MOSFET" : "RELAY", hysteresis, targetPressure, runtimeSeconds);

  // ⚡ v4.4 + v5.0: инициализация таймеров
  wdStart         = millis();
  wdRefPressure   = 0.0f;
  lastRuntimeSave = millis();
  electroOnSince  = 0;
  lastTempSamp    = millis();
  lastCyclesSave  = millis();
  lastWsHeartbeat = millis();
}


// =====================================================================================
//  LOOP
// =====================================================================================
void loop() {
  unsigned long now = millis();

  // ⚡ v4.4: во время OTA-прошивки ничего не трогаем — реле уже обесточены
  // обработчиком загрузки, а любая работа loop может помешать записи флеша.
  if (otaInProgress) {
    return;
  }

  // Давление каждые 100 мс
  if (now - lastPressureT >= T_PRESSURE_SAMP) {
    lastPressureT   = now;
    currentPressure = readPressure();
    safetyCheck();
  }

  // Угол наклона каждые 100 мс
  if (now - lastTiltT >= T_TILT_SAMP) {
    lastTiltT = now;
    tiltCheck();
  }

  // ⚡ v5.0: Температура + вентилятор раз в секунду
  if (now - lastTempSamp >= T_TEMP_SAMP) {
    lastTempSamp = now;
    currentTemp  = readTemperature();
    overheatWarn = (currentTemp >= TEMP_OVERHEAT);
    updateFan();
  }
  // ⚡ v5.0: расчёт оборотов с тахометра (функция сама считает раз в секунду)
  updateRPM(now);

  // Кнопки
  handlePowerBtn(now);
  handleLockBtn(now);
  handlePurgeBtn(now);

  // NRF24 — приём (только если модуль инициализирован)
  if (radioOk && radio.available()) {
    CmdPacket pkt; radio.read(&pkt, sizeof(pkt));
    if (pkt.type == 0x55) {
      executeCommand(pkt.cmd);
      lastRfRx = now;
      rfLastActivity = now;          // ⚡ v5.0: связь жива, канал хороший
      wsStateChanged = true;         // ⚡ v5.0: пушим в WebSocket немедленно
      Serial.print("[RF] cmd="); Serial.println(pkt.cmd);
    }
  }
  remoteOnline = (now - lastRfRx < T_RF_TIMEOUT) && (lastRfRx > 0);

  // ⚡ v5.0: резервный канал — если давно тишина, прыгаем на следующий
  if (radioOk) rfCheckHop();

  // NRF24 — отправка статуса
  if (now - lastStatusTx >= T_STATUS_TX) {
    sendStatusToRemote(); lastStatusTx = now;
  }

  // ── ПРОДУВКА ДО НУЛЯ ───────────────────────────────────────────────────
  if (isPurging) {
    // TILT во время продувки — продувка продолжается, но электролизер
    // и так выключен. Просто принудительно убедимся.
    if (tiltLock) {
      // ⚡ ТЗ 5.2: при наклоне корпуса (> 30°) во время продувки клапан
      // ЗАКРЫВАЕТСЯ — защита от утечки газа в опасном положении установки.
      // Делаем закрытие явным, не полагаясь на порядок вызова tiltCheck().
      setRelay(PIN_RELAY_ELECTRO, false);
      electroOn = false;
      setRelay(PIN_RELAY_VALVE, false);
      valveOn = false;
    }
    bool zeroed  = currentPressure <= PURGE_ZERO_BAR;
    bool timeout = (now - purgeStart) >= T_PURGE_MAX;
    if (zeroed || timeout) {
      setRelay(PIN_RELAY_VALVE, false);
      valveOn = false; isPurging = false; systemOn = false;
      Serial.println(timeout ? "[PURGE] Таймаут — выкл." : "[PURGE] ≤0.05 бар — выкл.");
    }
    updateLEDs();
    maybeUpdateDisplay(now);
    wsTick(now);
    return;
  }

  // ── МИНИ-ПРОДУВКА 200 мс ────────────────────────────────────────────────
  if (isPurgeShort) {
    if (tiltLock || (now - purgeShortStart) >= T_PURGE_SHORT) {
      setRelay(PIN_RELAY_VALVE, false);
      valveOn = false; isPurgeShort = false;
    }
    updateLEDs();
    maybeUpdateDisplay(now);
    wsTick(now);
    return;
  }

  // ── ВЫКЛ / БЛОКИРОВКА / АВАРИЯ ДАВЛЕНИЯ / TILT ────────────────────────────
  if (!systemOn || isLocked || overpressureLock || tiltLock) {
    if (tiltLock && !isLocked) {
      // TILT принудительно блокирует, чтобы пользователь увидел причину
      isLocked = true;
    }
    allRelaysOff();
    if (!systemOn || isLocked) resetProcesses();
    pressureWatchdog = false;   // ⚡ v4.4: снять watchdog в неактивном состоянии
    // Если есть несохранённые настройки — записать сейчас (система простаивает,
    // самый безопасный момент; защищает от потери при отключении питания).
    if (settingsDirty) commitSettings();
    updateLEDs();
    maybeUpdateDisplay(now);
    wsTick(now);
    return;
  }

  // ── ⚡ РЕПУЛЬСОР — конечный автомат на 800 мс ──────────────────────────────
  // Если репульсор активен — он управляет клапаном и пьезо. Электролизер
  // продолжает работать по своей обычной гистерезисной логике параллельно.
  if (repulsorActive) {
    unsigned long dt = now - repulsorStart;

    if (dt >= T_REPULSOR_TOTAL) {
      // Цикл завершён — гасим клапан и пьезо
      setRelay(PIN_RELAY_VALVE, false);
      setRelay(PIN_RELAY_PIEZO, false);
      valveOn        = false;
      piezoActive    = false;
      repulsorActive = false;
      Serial.println("[REPULSOR] done");
    } else {
      // Клапан открыт всё время репульсора
      setRelay(PIN_RELAY_VALVE, true);
      valveOn = true;
      // Пьезо включается на 700-й мс и горит до конца (= 100 мс)
      if (dt >= T_REPULSOR_PIEZO_AT && !piezoActive) {
        piezoActive = true;
        piezoStart  = now;          // чтобы общий таймер тоже видел
        setRelay(PIN_RELAY_PIEZO, true);
        Serial.println("[REPULSOR] piezo ON");
      }
    }
  }

  // ── ОСНОВНАЯ ЛОГИКА ЭЛЕКТРОЛИЗЕРА (Schmitt trigger с гистерезисом) ─────────
  bool needElectro;
  bool needValve;
  bool wasElectroOn = electroOn;   // запомнить предыдущее состояние для runtime

  if (manualGasActive) {
    // Ручная подача — электролизер и клапан принудительно ВКЛ
    needElectro = true;
    needValve   = true;
  } else {
    // Schmitt trigger:
    //   electroOn=ВКЛ:  выключаем когда P >= target
    //   electroOn=ВЫКЛ: включаем когда P < (target − hysteresis)
    if (electroOn) {
      needElectro = (currentPressure < targetPressure);
    } else {
      needElectro = (currentPressure < (targetPressure - hysteresis));
    }
    // Клапан в авто-режиме закрыт (если только репульсор не открыл его)
    needValve = repulsorActive ? true : false;
  }

  // Не перезаписываем valveOn если репульсор управляет клапаном
  setRelay(PIN_RELAY_ELECTRO, needElectro);
  electroOn = needElectro;

  if (!repulsorActive) {
    setRelay(PIN_RELAY_VALVE, needValve);
    valveOn = needValve;
  }

  // ── ⚡ v4.4: Учёт наработки при изменении состояния электролизера ─────────
  if (electroOn != wasElectroOn) {
    updateRuntimeOnElectroChange(electroOn);
  }

  // ── ⚡ v4.4: WATCHDOG ПО ДАВЛЕНИЮ ────────────────────────────────────────
  // Watchdog активен только когда электролизер реально качает в авто-режиме
  // (не manual, не репульсор — там клапан открыт и давление падать должно).
  if (electroOn && !manualGasActive && !repulsorActive) {
    if (!wasElectroOn) wdReset();   // только что включился — начинаем отсчёт заново
    wdCheck();
  } else {
    // Электролизер выключен (достигли цели) или manual/репульсор — снимаем watchdog
    if (pressureWatchdog || wasElectroOn) wdReset();
    pressureWatchdog = false;
  }

  // Обычный пьезо-поджиг (cmd=4) — гаснет через 500 мс
  // Внутри репульсора пьезо гасится автоматом самого репульсора (см. выше)
  if (piezoActive && !repulsorActive && (now - piezoStart >= T_PIEZO_WORK)) {
    piezoActive = false;
    setRelay(PIN_RELAY_PIEZO, false);
  }

  // ── ⚡ v4.4: периодическое сохранение наработки (раз в 5 минут) ───────────
  // Реже пишем = меньше износ flash. saveRuntime() сам не пишет если значение
  // не изменилось. При штатном выключении наработка фиксируется в allRelaysOff.
  if (now - lastRuntimeSave >= 300000) {   // 5 минут
    lastRuntimeSave = now;
    // Фиксируем накопленный интервал без сброса electroOnSince (без потери остатка)
    if (electroOn && electroOnSince != 0) {
      runtimeMsAccum += (now - electroOnSince);
      runtimeSeconds += runtimeMsAccum / 1000UL;
      runtimeMsAccum %= 1000UL;
      electroOnSince = now;
    }
    saveRuntime();
  }

  // ── ⚡ v5.0: Счётчик циклов реле (детект фронтов каждую итерацию) ─────────
  countRelayCycles();
  // Периодическое сохранение счётчиков (раз в 5 минут, пишет только при изменении)
  if (now - lastCyclesSave >= 300000) {
    lastCyclesSave = now;
    saveCycles();
  }

  // ── ⚡ Отложенная запись настроек (debounce против износа flash) ──────────
  if (settingsDirty && (now - settingsDirtyTime >= SETTINGS_SAVE_DELAY)) {
    commitSettings();
  }

  // ── ⚡ v5.0: WebSocket — push + heartbeat + очистка клиентов ──────────────
  wsTick(now);

  updateLEDs();
  maybeUpdateDisplay(now);
}

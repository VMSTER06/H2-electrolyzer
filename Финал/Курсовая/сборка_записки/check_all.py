"""ПОЛНАЯ АВТОПРОВЕРКА КУРСОВОГО (одна команда, выводит только проблемы).
  py -3 check_all.py [папка_сборки]      (по умолчанию – самая свежая _build/b* с full.pdf)
Группы проверок:
  1. Исходник записки (pz_spec.txt): СТП-текст (stp_check.py) + правила преподавателя.
  2. docx: поля, шрифт, интервал, заголовки, таблицы (stp_docx_check.py).
  3. PDF: заполненность страниц, висячие заголовки/подписи, номера страниц, объём ≥ 35 с.
  4. Обозначения: текст ↔ Э3 ↔ ПЭ3 ↔ Э2 (одинаковые, без пропусков в нумерации).
  5. Схемы Э1/Э2: без питания на Э1, вольты на Э2 только у блоков питания, без примечаний.
  6. Листинг: нет веба/Wi-Fi, копия прошивки свежее оригинала, файлы на месте.
  7. Лист задания: «не менее», п. 4 = заголовки записки, п. 5 = приложения, даты п. 8, без Wi-Fi.
  8. Комплект: Д1 = число страниц записки, КП3-папка = zip, копии в ГОТОВО одинаковые.
Код возврата 1, если есть ЗАМЕЧАНИЯ."""
import glob, hashlib, io, json, os, re, subprocess, sys, zipfile
sys.stdout.reconfigure(encoding='utf-8')
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)                               # Курсовая
G = os.path.join(ROOT, 'ГОТОВО_курсовая_H2')
K3 = '434701_Мурачев_ВК_КП3'
DES = 'ГУИР.6-05-0611-05.114'
POP = r'C:\Users\vladm\AppData\Local\Microsoft\WinGet\Packages\oschwartz10612.Poppler_Microsoft.Winget.Source_8wekyb3d8bbwe\poppler-25.07.0\Library\bin'
FIRMWARE = r'S:\Projects\1Electrolizer\Финал'

R, W = [], []                                              # замечания / посмотреть
def rem(g, s): R.append(f'[{g}] {s}')
def warn(g, s): W.append(f'[{g}] {s}')
def rd(p): return io.open(p, encoding='utf-8-sig').read()
def md5(p): return hashlib.md5(open(p, 'rb').read()).hexdigest()
def run(args): return subprocess.run(args, capture_output=True, text=True, encoding='utf-8', errors='replace').stdout

builds = sorted(glob.glob(os.path.join(HERE, '_build', 'b*', 'full.pdf')), key=os.path.getmtime)
B = sys.argv[1] if len(sys.argv) > 1 else (os.path.dirname(builds[-1]) if builds else None)
print('сборка:', B)

# ── 1. исходник записки ─────────────────────────────────────────────────────────
SPEC = os.path.join(HERE, 'pz_spec.txt')
S = rd(SPEC); L = S.split('\n')
app0 = next(i for i, l in enumerate(L) if l.startswith('#APP '))
body = [l for l in L[:app0]]
TEXTTAGS = ('#P ', '#L ', '#N ', '#W ', '#W2 ', '#PL ', '#R ', '#T ', '#F ', '#FIG', '#H1', '#H2')
out = run(['py', '-3', os.path.join(HERE, 'stp_check.py'), SPEC])
for l in out.splitlines():
    if l.startswith('('): rem('СТП-текст', l)
    if 'без ссылок: [' in l and not l.rstrip().endswith('[]'): rem('СТП-текст', l)
plain = '\n'.join(l.replace('\u00a0', ' ') for l in body if l.startswith(TEXTTAGS))
FORBID = [(r'\bузл\w*', 'слово «узлы» (преподаватель)'), (r'\bвеб\w*|\bweb\b', 'веб-интерфейс'),
          (r'Wi-?Fi|Bluetooth', 'Wi-Fi/Bluetooth'), (r'\bпульт\w*', 'старое название «пульт»'),
          (r'\bмозг\w*', 'старое название «мозг»'), (r'блок\w* управлени', 'старое название «блок управления»'),
          (r'\bмы\b|\bнаш\w*', 'изложение от первого лица'), (r'\bU ?пит\b(?!.*\|)', '«Uпит» в тексте')]
for pat, why in FORBID:
    for m in re.finditer(pat, plain, re.I if 'Wi' not in pat else 0):
        ctx = plain[max(0, m.start() - 30):m.end() + 20].replace('\n', ' ')
        rem('преподаватель', f'{why}: …{ctx}…')
for m in re.finditer(r'(?<![\d.,])\d+,\d{3,}(?![\d])', plain):
    rem('преподаватель', f'больше 2 знаков после запятой: …{plain[max(0, m.start() - 25):m.end() + 5]}…'.replace('\n', ' '))
for m in re.finditer(r'\[\d+(?:[,–]\d+)*, \d', plain):
    rem('преподаватель', f'ссылка с пробелом (нужно [2,3]): {plain[m.start():m.end() + 4]}')
# порядок первого упоминания источников и все ли процитированы
src_n = len([l for l in L if re.match(r'#SRC |#S \[', l)]) or len(re.findall(r'^\s*#SRC', S, re.M))
cites = []
for m in re.finditer(r'\[(\d+(?:[,–]\d+)*)\]', plain):
    for part in m.group(1).split(','):
        if '–' in part: a, b = map(int, part.split('–')); cites += list(range(a, b + 1))
        else: cites.append(int(part))
first = []
for c in cites:
    if c not in first: first.append(c)
if first != sorted(first): warn('СТП-источники', f'порядок первого упоминания не по возрастанию: {first[:30]}')
if first and set(range(1, max(first) + 1)) - set(first):
    rem('СТП-источники', f'источники без ссылок в тексте: {sorted(set(range(1, max(first) + 1)) - set(first))}')
# разделы 1–3 начинаются с X.1 Требования; в разделе 4 требований нет
h1 = [(i, l) for i, l in enumerate(body) if l.startswith('#H1 ')]
for k, (i, l) in enumerate(h1):
    m = re.match(r'#H1 (\d)', l)
    if not m: continue
    n = int(m.group(1))
    nxt = next((x for x in body[i + 1:] if x.startswith(('#H2', '#P ', '#H1'))), '')
    if n in (1, 2, 3) and not re.match(rf'#H2 {n}\.1\s*(<TAB>|\t| )Требования к проектируемому устройству', nxt):
        rem('преподаватель', f'раздел {n} начинается не с «{n}.1 Требования к проектируемому устройству»: {nxt[:60]}')
    if nxt.startswith('#P '): rem('преподаватель', f'текст сразу после заголовка раздела {n} (нужен подраздел)')
    end = h1[k + 1][0] if k + 1 < len(h1) else len(body)
    if n == 4 and any('Требования к проектируемому' in x for x in body[i:end]):
        rem('преподаватель', 'в разделе 4 есть «Требования к проектируемому устройству»')
# после таблиц питания – «Для источника питания выбрано»
for i, l in enumerate(body):
    if l.startswith('#T ') and re.search(r'Расчет мощности', l):
        j = next(j for j in range(i + 1, len(body)) if body[j].startswith('#TEND'))
        p = next((x for x in body[j + 1:] if x.startswith('#P ')), '')
        if not p.startswith('#P Для источника питания'):
            rem('преподаватель', f'после «{l[3:40]}» первые слова не «Для источника питания выбрано…»: {p[3:50]}')
        rows = body[i + 2:j]
        if not any(r.startswith('#R Итого') for r in rows): rem('преподаватель', f'нет строки «Итого» в {l[3:20]}')
        if not any('подсветк' in r.lower() for r in rows) and 'LCD' in ''.join(rows):
            rem('преподаватель', f'в {l[3:20]} нет тока подсветки дисплея')
# нумерация таблиц/рисунков/формул подряд внутри раздела
for kind, pat in (('таблиц', r'^#T Таблица (\d+)\.(\d+)'), ('рисунков', r'^#FIG.*Рисунок (\d+)\.(\d+)'),
                  ('формул', r'^#F .*\((\d+)\.(\d+)\)\s*$')):
    seen = {}
    for l in body:
        m = re.match(pat, l)
        if m:
            s_, n_ = int(m.group(1)), int(m.group(2))
            if n_ != seen.get(s_, 0) + 1: rem('СТП-нумерация', f'нумерация {kind}: {s_}.{n_} после {s_}.{seen.get(s_, 0)}')
            seen[s_] = n_
# пробелы, точки в конце заголовков
for l in body:
    if l.startswith(('#H1 ', '#H2 ')) and l.rstrip().endswith('.'): rem('СТП-заголовки', f'точка в конце заголовка: {l[:60]}')
    if l.startswith('#T ') and l.rstrip().endswith('.'): rem('СТП-таблицы', f'точка в конце названия таблицы: {l[:60]}')

# ── 2. docx ─────────────────────────────────────────────────────────────────────
for name in ('full', 'kp3'):
    p = os.path.join(B, name + '.docx') if B else ''
    if not os.path.exists(p): rem('сборка', f'нет {name}.docx в сборке'); continue
    for l in run(['py', '-3', os.path.join(HERE, 'stp_docx_check.py'), p]).splitlines():
        if not l.startswith('('): continue
        if l.startswith("('поля раздела'") and '(0, 0, 0, 0)' in l: continue   # вставленные листы ПЭ3/Д1 без полей – так и надо
        if l.startswith("(('"): continue                                         # статистика
        rem(f'СТП-docx {name}', l)

    # продолжение таблицы: те же столбцы, ширина и выравнивание, что у первой части; головка повторена
    try:
        from docx import Document as _D
        from docx.text.paragraph import Paragraph as _P
        dd = _D(p); prev = None
        def geom(tb):
            g = [round(int(x.get('{http://schemas.openxmlformats.org/wordprocessingml/2006/main}w')) / 56.7, 1) for x in tb._tbl.tblGrid]
            jc = tb._tbl.tblPr.find('{http://schemas.openxmlformats.org/wordprocessingml/2006/main}jc')
            return g, (jc.get('{http://schemas.openxmlformats.org/wordprocessingml/2006/main}val') if jc is not None else None)
        for tb in dd.tables:
            el = tb._tbl.getprevious()
            cap = _P(el, None).text.strip() if el is not None and el.tag.endswith('}p') else ''
            if cap.startswith('Продолжение таблицы') and prev is not None:
                (g1, j1), (g2, j2) = geom(prev), geom(tb)
                if len(g1) != len(g2) or any(a and b and abs(a - b) > 1 for a, b in zip(g1, g2)) or j1 != j2:
                    rem(f'таблицы {name}', f'«{cap}»: столбцы/положение не как у первой части ({g1}, {j1} ≠ {g2}, {j2})')
                if [c.text for c in prev.rows[0].cells] != [c.text for c in tb.rows[0].cells]:
                    rem(f'таблицы {name}', f'«{cap}»: головка не повторена')
            if cap.startswith(('Таблица', 'Продолжение таблицы')): prev = tb
            # таблица целиком «не отрывать» (кроме таблиц с объединенными ячейками) – будет прыгать на новую страницу
            rows = tb.rows
            if len(rows) > 3 and all(r.cells[0].paragraphs[0].paragraph_format.keep_with_next for r in rows[1:-1]) \
                    and 'vMerge' not in tb._tbl.xml:
                warn(f'таблицы {name}', f'«{cap[:40]}» держится целиком (keep_with_next во всех строках)')
    except Exception as e:
        warn(f'таблицы {name}', f'проверка не выполнена: {e}')

# ── 3. PDF ──────────────────────────────────────────────────────────────────────
pdfpages = {}
for name in ('full', 'kp3'):
    p = os.path.join(B, name + '.pdf') if B else ''
    if not os.path.exists(p): continue
    info = run([os.path.join(POP, 'pdfinfo.exe'), p])
    pdfpages[name] = int(re.search(r'Pages:\s+(\d+)', info).group(1))
    sec = None
    for l in run(['py', '-3', os.path.join(HERE, 'stp_pdf_check.py'), p]).splitlines():
        if l.startswith('ЗАМЕЧАНИЯ'): sec = rem
        elif l.startswith('ПОСМОТРЕТЬ'): sec = warn
        elif l.startswith('  ') and l.strip() != 'нет' and sec: sec(f'PDF {name}', l.strip())
    txt = run([os.path.join(POP, 'pdftotext.exe'), p, '-'])
    cut = txt.find('\nЛистинг программ\n')          # заголовок приложения Е (не строка содержания)
    for pat, why in FORBID[:6]:
        old = 0
        for m in re.finditer(pat, txt, re.I if 'Wi' not in pat else 0):
            ctx = txt[max(0, m.start() - 30):m.end() + 20].replace('\n', ' ')
            if 'http' in ctx or 'Режим доступа' in ctx: continue
            if cut > 0 and m.start() > cut and why.startswith('старое'): old += 1; continue
            rem(f'PDF {name}', f'{why}: …{ctx}…')
        if old: warn(f'PDF {name}', f'в комментариях листинга {why}: {old} раз')

# ── 4. обозначения ──────────────────────────────────────────────────────────────
DESIG = r'(?<![-\w])(?:A|BK|BP|C|DD|GB|HA|HG|HL|K|M|R|SA|SB|XP)\d{1,2}'
def expand(s):
    res = set()
    for part in re.split(r',\s*', s):
        m = re.match(r'([A-Z]+)(\d+)[–-]([A-Z]+)?(\d+)$', part.strip())
        if m: res |= {f'{m.group(1)}{k}' for k in range(int(m.group(2)), int(m.group(4)) + 1)}
        elif re.match(DESIG + '$', part.strip()): res.add(part.strip())
    return res
ns = {}
src = rd(os.path.join(HERE, 'чертежи', 'draw_docs.py'))
pe_block = src[src.index('PE = ['):src.index(']\n', src.index('PE = ['))]
PE = set()
for m in re.finditer(r"\('([^'#][^']*)', '", pe_block): PE |= expand(m.group(1))
E3 = set(re.findall(r'["\'](' + DESIG + r')["\']', rd(os.path.join(HERE, 'чертежи', 'draw_e3.py'))))
TXT = set()
# координаты зон схемы («по координатам B18», «K04, лист 1») — не обозначения элементов
plain_d = re.sub(r'координат\w*(?:\s+[A-Z]\d\d(?:\s+(?:и|первого|второго|листа|на|обоих|листах)|,)*)+', ' ', plain)
plain_d = re.sub(r'\b[A-Z]\d\d, лист \d', ' ', plain_d)
for m in re.finditer(r'\b(' + DESIG + r')(?:[–-](' + DESIG + r'))?\b', plain_d):
    TXT |= expand(m.group(0)) if m.group(2) else {m.group(1)}
e2 = json.load(io.open(os.path.join(HERE, 'чертежи', 'e2_a3.json'), encoding='utf-8'))
E2txt = ' '.join(it.get('text', '') for it in e2['items'])
E2 = set()
for m in re.finditer(r'\b(' + DESIG + r')(?:[–-](' + DESIG + r'))?\b', E2txt.replace('\\n', ' ')):
    E2 |= expand(m.group(0)) if m.group(2) else {m.group(1)}
for name, st in (('Э3', E3), ('текст', TXT), ('Э2', E2)):
    extra = sorted(st - PE)
    if extra: rem('обозначения', f'{name}: нет в ПЭ3 → {extra}')
for x in sorted(PE - E3): rem('обозначения', f'{x} есть в ПЭ3, но не на Э3')
for x in sorted(PE - TXT): warn('обозначения', f'{x} из ПЭ3 не упомянут в тексте записки')
groups = {}
for x in PE: groups.setdefault(re.match(r'[A-Z]+', x).group(0), []).append(int(re.search(r'\d+', x).group(0)))
for g, v in groups.items():
    if sorted(v) != list(range(1, max(v) + 1)): rem('обозначения', f'пропуск в нумерации {g}: {sorted(v)}')

# ── 5. схемы Э1/Э2 ──────────────────────────────────────────────────────────────
e1 = json.load(io.open(os.path.join(HERE, 'чертежи', 'e1_a3.json'), encoding='utf-8'))
E1txt = ' '.join(it.get('text', '') for it in e1['items'])
if re.search(r'U ?пит|питани|\d ?В\b|аккумул', E1txt): rem('Э1', 'на Э1 есть питание (преподаватель: без питания)')
for it in e2['items']:
    t = it.get('text', '')
    if re.search(r'\d+(,\d+)? ?В\b', t) and 'питания' not in t and not t.startswith('+') and 'бар' not in t:
        rem('Э2', f'вольты вне блока питания: {t!r}')
    if re.search(r'Примечани|U ?пит', t): rem('Э2', f'примечание/Uпит на Э2: {t!r}')
sig = [it['text'] for it in e2['items'] if it.get('t') == 'text' and re.search(r'GPIO|SPI|I2C|ШИМ|1-Wire', it.get('text', ''))]
bad = [s for s in sig if not s.replace('\n', ' ').startswith('Цифровой сигнал')]
if bad: rem('Э2', f'подписи сигналов не «Цифровой сигнал …»: {bad}')

# ── 6. листинг ──────────────────────────────────────────────────────────────────
WEB = re.compile(r'веб|Веб|web|Web|WEB|Wi-?Fi|WIFI|WiFi|браузер|смартфон|(?<![A-Za-z])OTA(?![A-Za-z])|captive|WebSocket|ESPAsync|AsyncTCP|DNSServer|softAP')
for l in L:
    if not l.startswith('#LISTING '): continue
    p = l[9:].strip()
    if not os.path.exists(p): rem('листинг', f'нет файла {p}'); continue
    if 'листинг' not in p: rem('листинг', f'берется из настоящей прошивки (с вебом), а не из очищенной копии: {p}')
    for n, s in enumerate(rd(p).split('\n'), 1):
        if WEB.search(s): rem('листинг', f'{os.path.basename(p)}:{n}: {s.strip()[:80]}')
    orig = os.path.join(FIRMWARE, os.path.basename(os.path.dirname(p)), os.path.basename(p))
    if os.path.exists(orig) and os.path.getmtime(orig) > os.path.getmtime(p):
        rem('листинг', f'{os.path.basename(p)}: прошивка изменена после очистки – запусти make_listing_src.py и пересобери')
inc = set(re.findall(r'#include "([^"]+)"', ''.join(rd(l[9:].strip()) for l in L if l.startswith('#LISTING ') and os.path.exists(l[9:].strip()))))
listed = {os.path.basename(l[9:].strip()) for l in L if l.startswith('#LISTING ')}
miss = inc - listed - {'h2_sound.h', 'h2_events.h', 'remote_game.h'}
if miss: warn('листинг', f'подключаются, но не приведены в листинге: {sorted(miss)}')

# ── 7. лист задания ─────────────────────────────────────────────────────────────
try:
    from docx import Document
    H1 = [re.sub(r'^#H1 \d+\s*(<TAB>)?', '', l).strip() for l in body if re.match(r'#H1 \d', l)]
    for path, stage in ((os.path.join(G, '4_Лист задания.docx'), 'full'),
                        (os.path.join(G, '3_КП3_сдача', K3, 'Лист задания.docx'), 'kp3'),
                        (os.path.join(ROOT, K3, 'Лист задания.docx'), 'kp3')):
        if not os.path.exists(path): rem('задание', f'нет {path}'); continue
        T = '\n'.join(p.text for p in Document(path).paragraphs)
        tag = os.path.relpath(path, ROOT)
        p9 = next((x for x in T.split('\n') if x.startswith('Питание')), '')
        if 'не более' in p9: rem('задание', f'{tag}: п. 9 «не более» (нужно «ток не менее»)')
        if not re.search(r'ток не менее 1,5 А.*ток не менее 0,4 А', T, re.S): rem('задание', f'{tag}: п. 9 не «ток не менее 1,5 А / 0,4 А»')
        if re.search(r'Wi-?Fi|веб|Bluetooth', T): rem('задание', f'{tag}: упоминается Wi-Fi/веб')
        for d in ('к 23.09', 'к 13.10', 'к 04.11', 'к 10.11', 'к 18.11'):
            if d not in T: rem('задание', f'{tag}: в п. 8 нет «{d}»')
        want = H1 if stage == 'full' else H1[:4]
        for h in want:
            hh = h.lower().replace('«', '').replace('»', '')
            if hh[:40] not in T.lower().replace('«', '').replace('»', ''): rem('задание', f'{tag}: в п. 4 нет раздела «{h[:50]}»')
        if stage == 'kp3' and re.search(r'5\. Разработка программного|Заключение', T): rem('задание', f'{tag}: в КП3 лишний раздел 5/Заключение')
        for k, need in (('Структурная схема', 1), ('Функциональная', 1), ('Принципиальная', 1), ('Перечень элементов', 1),
                        ('Схема программы', stage == 'full')):
            if need and k not in T: rem('задание', f'{tag}: в п. 5 нет «{k}»')
except ImportError:
    warn('задание', 'python-docx не установлен')

# ── 8. комплект ─────────────────────────────────────────────────────────────────
pp = os.path.join(HERE, '_sheets', 'doc_Д1.pages')
if 'full' in pdfpages:
    total = pdfpages['full'] + 2                    # + титульный лист и лист задания
    d1 = rd(pp).strip() if os.path.exists(pp) else '?'
    if d1 != str(total): rem('Д1', f'в Д1 записка {d1} с., а фактически {total} с. (draw_docs.py ../_sheets/doc {total})')
    readme = os.path.join(G, 'ПРОЧТИ.txt')
    if os.path.exists(readme) and f'{total} с.' not in rd(readme): rem('ПРОЧТИ', f'в ПРОЧТИ.txt не {total} с.')
pe_pdf = os.path.join(HERE, '_sheets', 'doc_ПЭ3.pdf')
if os.path.exists(pe_pdf):
    n = int(re.search(r'Pages:\s+(\d+)', run([os.path.join(POP, 'pdfinfo.exe'), pe_pdf])).group(1))
    if f"' ПЭ3', 'Микропроцессорное устройство', '{n} с.'" not in src: rem('Д1', f'ПЭ3 {n} с., в Д1 указано иначе')
pairs = []
if B:
    pairs += [(os.path.join(B, 'full.pdf'), os.path.join(G, '1_Пояснительная записка (полная).pdf')),
              (os.path.join(B, 'full.docx'), os.path.join(G, '1_Пояснительная записка (полная).docx')),
              (os.path.join(B, 'full.pdf'), os.path.join(ROOT, 'Пояснительная записка H2 (СТП).pdf')),
              (os.path.join(B, 'kp3.pdf'), os.path.join(G, '3_КП3_сдача', K3, 'Пояснительная записка.pdf')),
              (os.path.join(B, 'kp3.pdf'), os.path.join(ROOT, K3, 'Пояснительная записка.pdf'))]
SH = os.path.join(HERE, '_sheets')
for f, dst in (('E2n.pdf', f'{DES} Э2 Схема электрическая функциональная (А3).pdf'), ('E1n.pdf', f'{DES} Э1 Схема электрическая структурная (А3).pdf'),
               ('E3.pdf', f'{DES} Э3 Схема электрическая принципиальная (А3, 2 листа).pdf'), ('doc_ПЭ3.pdf', f'{DES} ПЭ3 Перечень элементов.pdf')):
    pairs.append((os.path.join(SH, f), os.path.join(G, '2_Чертежи', dst)))
for f, dst in (('E1n.pdf', 'Приложение А (Структурная схема).pdf'), ('E2n.pdf', 'Приложение Б (Функциональная схема).pdf'),
               ('E3.pdf', 'Приложение В (Принципиальная схема).pdf'), ('doc_ПЭ3.pdf', 'Приложение Г (Перечень элементов).pdf'),
               ('pd1.pdf', 'Приложение Д (Схема программы).pdf'), ('doc_Д1.pdf', 'Приложение Ж (Ведомость документов).pdf')):
    for d in (os.path.join(G, '3_КП3_сдача', K3), os.path.join(ROOT, K3)):
        pairs.append((os.path.join(SH, f), os.path.join(d, dst)))
for f, dst in (('doc_Д1.pdf', f'{DES} Д1 Ведомость документов.pdf'), ('pd1.pdf', f'{DES} ПД1 Схема программы (А3).pdf')):
    pairs.append((os.path.join(SH, f), os.path.join(G, '2_Чертежи', dst)))
for a, b in pairs:
    if not os.path.exists(a): continue
    if not os.path.exists(b): rem('комплект', f'нет {os.path.relpath(b, ROOT)}')
    elif md5(a) != md5(b) and os.path.getmtime(b) < os.path.getmtime(a):
        rem('комплект', f'устарела копия {os.path.relpath(b, ROOT)}')
for a, b in ((os.path.join(G, '4_Лист задания.docx'), os.path.join(G, '4_Лист задания.pdf')),
             (os.path.join(G, '3_КП3_сдача', K3, 'Лист задания.docx'), os.path.join(G, '3_КП3_сдача', K3, 'Лист задания.pdf'))):
    if os.path.exists(a) and os.path.exists(b) and os.path.getmtime(a) > os.path.getmtime(b) + 5:
        rem('комплект', f'PDF старее docx: {os.path.relpath(b, ROOT)}')
zp = os.path.join(G, '3_КП3_сдача', K3 + '.zip'); fd = os.path.join(G, '3_КП3_сдача', K3)
if os.path.exists(zp):
    z = zipfile.ZipFile(zp)
    zn = {os.path.basename(n): z.getinfo(n).CRC for n in z.namelist() if not n.endswith('/')}
    fn = {f: zipfile.crc32(open(os.path.join(fd, f), 'rb').read()) for f in os.listdir(fd) if f.endswith('.pdf')}
    if set(zn) != set(fn): rem('комплект', f'zip ≠ папка КП3: {sorted(set(zn) ^ set(fn))}')
    for f in set(zn) & set(fn):
        if zn[f] != fn[f]: rem('комплект', f'в zip устаревший файл: {f}')
    bad = [f for f in fn if re.search(r'\.(bak|tmp)$|~\$|до_', f)]
    if bad: rem('комплект', f'мусор в папке КП3: {bad}')

# ── итог ────────────────────────────────────────────────────────────────────────
print('\nЗАМЕЧАНИЯ:' if R else '\nЗАМЕЧАНИЙ НЕТ')
for x in R: print('  ' + x)
if W:
    print('ПОСМОТРЕТЬ:')
    for x in W: print('  ' + x)
print(f'ИТОГО: замечаний {len(R)}, посмотреть {len(W)}')
sys.exit(1 if R else 0)

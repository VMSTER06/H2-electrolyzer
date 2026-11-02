"""Проверка текста записки (pz_spec.txt) на типовые нарушения СТП 01–2024, раздел 2."""
import io, re, sys
sys.stdout.reconfigure(encoding='utf-8')
L = io.open(sys.argv[1] if len(sys.argv) > 1 else 'pz_spec.txt', encoding='utf-8-sig').read().split('\n')
end = next(i for i, l in enumerate(L) if l.startswith('#APP '))
UN = r'(?:кбит/с|Мбит/с|мкА|мА·ч|мА|А|мВт|Вт|мВ|В|кГц|МГц|ГГц|Гц|бар|мм|см|м|°C|кОм|мОм|Ом|дБм|дБ|мкс|мс|нс|с|ч|мин|%|КБ|МБ|Кбайт|бит|байт|мкФ|нФ|пФ|г|кг|Па|кПа|МПа)'
issues = []
def add(i, kind, frag): issues.append((i + 1, kind, frag))

TEXT = ('#P ', '#L ', '#N ', '#W ', '#W2 ', '#PL ')
fig_ref, tab_ref, app_ref, figs, tabs = set(), set(), set(), [], []
for i, l in enumerate(L[:end]):
    tag = l.split(' ', 1)[0]
    body = l[len(tag) + 1:] if ' ' in l else ''
    plain = body.replace(' ', ' ')
    if l.startswith('#FIG'):
        m = re.search(r'Рисунок (\d+\.\d+)', l); figs.append(m.group(1) if m else '?')
    if l.startswith('#T '):
        m = re.search(r'Таблица (\d+\.\d+)', l); tabs.append(m.group(1) if m else '?')
        if ' – ' not in l: add(i, 'заголовок таблицы без тире', l[:80])
    if not l.startswith(TEXT): continue
    for m in re.finditer(r'рисунк\w*\s+(\d+\.\d+)', plain): fig_ref.add(m.group(1))
    for m in re.finditer(r'таблиц\w*\s+(\d+\.\d+(?:(?:,\s*|\s+и\s+)\d+\.\d+)*)', plain):
        tab_ref.update(re.findall(r'\d+\.\d+', m.group(1)))   # «в таблицах 4.1 и 4.2»
    for m in re.finditer(r'приложени\w*\s+([А-Я])\b', plain): app_ref.add(m.group(1))
    for m in re.finditer(r'приложени\w*\s+([А-Я])(?:\s*(?:и|,)\s*([А-Я])\b)', plain): app_ref.add(m.group(2))
    if l.startswith('#PL '): continue
    # 2.1.1/СТП-шрифт: число и единица — неразрывный пробел (наше правило), без пробела — ошибка
    for m in re.finditer(r'\d(' + UN + r')(?=[\s.,;:)»]|$)', body):
        if not re.search(r'[A-Za-z]\d' + re.escape(m.group(1)), body[max(0, m.start() - 1):m.end()]):
            add(i, 'нет пробела перед единицей', body[max(0, m.start() - 15):m.end() + 5])
    for m in re.finditer(r'\d (' + UN + r')(?=[\s.,;:)»/]|$)', body):
        add(i, 'обычный пробел (возможен перенос) перед единицей', body[max(0, m.start() - 15):m.end() + 5])
    # 2.3.12 диапазон через тире в тексте -> «от … до …»
    for m in re.finditer(r'\d[\d,]*\s?–\s?\d[\d,]*\s?' + UN, plain):
        ctx = plain[max(0, m.start() - 25):m.end() + 3]
        if l.startswith(('#P ', '#L ', '#N ')): add(i, 'диапазон через тире (СТП: «от … до …»)', ctx)
    # «в» перед числом с размерностью
    for m in re.finditer(r'(?:мощностью|током|напряжением|емкостью|частотой|сопротивлением|длиной|массой|объемом) в \d', plain):
        add(i, '«в» перед числом с размерностью', plain[max(0, m.start() - 10):m.end() + 10])
    # 2.3.12 числа 1..9 без единиц — словами
    for m in re.finditer(r'(?<![\w.,/–\-+×·(\[])([1-9])(?![\d,.:×\-–\]%])(?:\s+)(?!' + UN + r'\b)([а-яё]+)', plain):
        w = m.group(2)
        if w in ('и', 'или', 'до', 'из', 'на', 'по', 'с', 'в', 'к', 'от', 'бар', 'раз'):
            if w != 'раз': continue
        ctx = plain[max(0, m.start() - 20):m.end() + 5]
        if re.search(r'(GPIO|канал|SB|HL|R|C|рисунк|таблиц|формул|раздел|подраздел|пункт|прил|блок\w*|ядро|уров|версии|тип|IO|=)\s*$', plain[max(0, m.start() - 12):m.start()]): continue
        add(i, 'число 1–9 цифрой без единицы', ctx)
    # кавычки
    if '"' in plain or '“' in plain or '”' in plain: add(i, 'кавычки не «ёлочки»', plain[:80])
    # дефис вместо тире
    for m in re.finditer(r'\s-\s', plain): add(i, 'дефис вместо тире', plain[max(0, m.start() - 20):m.end() + 20])
    # градусы Цельсия без пробела
    for m in re.finditer(r'\d°C', plain): add(i, '«°C» без пробела', plain[max(0, m.start() - 10):m.end() + 3])
    # ссылка-источник: [1, 2] с пробелом (у нас формат [1,2])
    for m in re.finditer(r'\[\d+, \d', plain): add(i, 'пробел в ссылке', plain[m.start():m.end() + 4])
    # пробел перед знаками препинания
    for m in re.finditer(r'\s[,.;:](?!\d)', plain): add(i, 'пробел перед знаком препинания', plain[max(0, m.start() - 15):m.end() + 5])
    # иностранные слова при наличии русских
    for w in ('девайс', 'контроллер ', 'дисплей'):
        pass
# перечисления
for i, l in enumerate(L[:end]):
    if l.startswith('#L '):
        nxt = L[i + 1] if i + 1 < len(L) else ''
        last = not nxt.startswith('#L ')
        if not l.rstrip().endswith(('.' if last else ';')): add(i, 'конец элемента простого перечисления', l[-50:])
        if not l[3:].startswith('–'): add(i, 'элемент перечисления не с «–»', l[:40])
    if l.startswith('#N '):
        if not re.match(r'#N \d+ [А-ЯA-Z]', l): add(i, 'сложное перечисление: «N Текст»', l[:40])
        if not l.rstrip().endswith('.'): add(i, 'сложное перечисление без точки', l[-40:])
    if l.startswith('#W '):
        if not l.startswith('#W где '): add(i, 'расшифровка не со слова «где»', l[:40])
        if l.startswith('#W где:'): add(i, '«где» с двоеточием', l[:40])
    if l.startswith(('#W ', '#W2 ')):
        nxt = L[i + 1] if i + 1 < len(L) else ''
        last = not nxt.startswith('#W2 ')
        if not l.rstrip().endswith('.' if last else ';'): add(i, 'конец строки расшифровки', l[-40:])
    if l.startswith('#P ') and re.search(r'(?:\b(?:из|на|то|как)):\s*$', l): add(i, 'вводная фраза обрывается на предлоге', l[-40:])
    if l.startswith('#P ') and l.rstrip().endswith(':'):
        nxt = L[i + 1] if i + 1 < len(L) else ''
        if nxt.startswith('#P '): add(i, 'двоеточие, а дальше не перечисление', l[-60:])
print('рисунки:', figs, 'ссылки:', sorted(fig_ref))
print('таблицы:', len(tabs), 'без ссылок:', [t for t in tabs if t not in tab_ref])
print('рисунки без ссылок:', [f for f in figs if f not in fig_ref])
apps = [l.split('|')[0][5:] for l in L if l.startswith('#APP ')]
print('приложения:', apps, 'без ссылок:', [a for a in apps if a not in app_ref])
for t in issues: print(t)
print('ВСЕГО замечаний:', len(issues))

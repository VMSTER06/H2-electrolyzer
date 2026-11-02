"""Merge: Введение + разделы 1–2 + источники [1]–[26] из записки КП1 (последняя версия) в полную
СТП-записку (pz_spec.txt): разделы 3–5, заключение и приложения остаются, ссылки перенумеровываются."""
import sys, re, io
from docx import Document
from docx.oxml.ns import qn
from docx.text.paragraph import Paragraph
from docx.table import Table
sys.stdout.reconfigure(encoding='utf-8')

kp1 = Document('kp1.docx')
old = io.open('pz_spec.txt', encoding='utf-8-sig').read().split('\n')

# ---------------- 1. KP1 intro + sections 1–2 -> markup ----------------
out = []
state = None
caption = None
for el in kp1.element.body.iterchildren():
    if el.tag == qn('w:p'):
        p = Paragraph(el, kp1)
        st = p.style.name if p.style is not None else ''
        t = p.text.strip()
        if st == 'Heading 1':
            if t.lower() == 'введение':
                state = 'on'; out.append('#H1C Введение'); continue
            if t.lower().startswith('список'):
                break
            if state:
                num, title = p.text.split('\t', 1)
                out.append(f'#H1 {num}<TAB>{title}'); continue
        if not state:
            continue
        if st == 'Heading 2':
            num, title = p.text.split('\t', 1)
            out.append(f'#H2 {num}<TAB>{title}'); continue
        if not t:
            out.append('#E'); continue
        if t.startswith('Таблица '):
            caption = t; continue
        out.append(('#L ' if t.startswith('– ') else '#P ') + t)
    elif el.tag == qn('w:tbl') and state:
        tb = Table(el, kp1)
        out.append('#T ' + caption)
        for r in tb.rows:
            out.append('#R ' + '|'.join(c.text.strip().replace('\n', ' ') for c in r.cells))
        out.append('#TEND')
        caption = None

# collapse duplicate #E and drop #E right before #H1
clean = []
for l in out:
    if l == '#E' and clean and clean[-1] == '#E':
        continue
    if l.startswith('#H1') and clean and clean[-1] == '#E':
        clean.pop()
    clean.append(l)
new12 = clean

def sub(lines, pat, rep, must=True):
    n = 0
    for i, l in enumerate(lines):
        if pat in l:
            lines[i] = l.replace(pat, rep); n += 1
    if must and n == 0:
        raise SystemExit('pattern not found: ' + pat)

# 1.5: only the digital sensor is used
sub(new12, 'Для получения более подробной информации о рассмотренных датчиках температуры использовались источники [12, 13].',
    'В устройстве применяется только цифровой датчик DS18B20; аналоговый термистор рассмотрен как альтернатива и не используется. '
    'Для получения более подробной информации о рассмотренных датчиках температуры использовались источники [12, 13].')

# ---------------- 2. old sections 3–5, conclusion, appendices ----------------
i3 = next(i for i, l in enumerate(old) if l.startswith('#H1 3<TAB>'))
isrc = next(i for i, l in enumerate(old) if l.startswith('#H1C Список'))
iapp = next(i for i, l in enumerate(old) if l.startswith('#APP'))
body = old[i3:isrc]
apps_old = old[iapp:]

# citations: old number -> new number
cmap = {1: 1, 2: 3, 3: 4, 4: 8, 5: 9, 6: 10, 7: 11, 8: 12, 9: 13, 10: 14, 11: 16, 12: 21, 13: 25,
        14: 17, 15: 18, 16: 7, 17: 26, 18: 20, 19: 27}
def remap_cite(m):
    nums = []
    for part in m.group(1).split(','):
        part = part.strip()
        if re.fullmatch(r'\d+[–-]\d+', part):
            a, b = map(int, re.split('[–-]', part)); nums += list(range(a, b + 1))
        else:
            nums.append(int(part))
    new = sorted(dict.fromkeys(cmap[n] for n in nums))
    return '[' + ', '.join(map(str, new)) + ']'
cite_re = re.compile(r'\[(\d+(?:\s*[–-]\s*\d+)?(?:\s*,\s*\d+(?:\s*[–-]\s*\d+)?)*)\]')
# "[4], [13]" style -> single bracket first
def merge_adjacent(l):
    return re.sub(r'\[(\d+)\],\s*\[(\d+)\]', r'[\1, \2]', l)
body = [cite_re.sub(remap_cite, merge_adjacent(merge_adjacent(l))) for l in body]

# appendix letters (old -> new, ordered by first reference in text)
amap = {'А': 'Е', 'Б': 'А', 'В': 'Б', 'Г': 'В', 'Д': 'Д', 'Е': 'Г', 'Ж': 'Ж'}
app_re = re.compile(r'(приложени[еияюй]|приложениях)\s+([А-Ж])\b')
body = [app_re.sub(lambda m: m.group(1) + ' ' + amap[m.group(2)], l) for l in body]

# section 3 per assignment/lecturer
sub(body, '#H1 3<TAB>Обоснование выбора узлов и элементов функциональной схемы устройства',
    '#H1 3<TAB>Обоснование выбора элементов функциональной схемы устройства')
sub(body, '#H2 3.1<TAB>Требования к элементам функциональной схемы', '#H2 3.1<TAB>Требования к проектируемому устройству')
sub(body, '#P Выбор каждого элемента в соответствии с этими требованиями обоснован в последующих подразделах.',
    '#P Выбор каждого элемента в соответствии с этими требованиями обоснован в последующих подразделах. '
    'Схема электрическая функциональная устройства приведена в приложении Б.')
# display: brain uses LCD2004 only
body = [l for l in body if not l.startswith('#P Программное обеспечение блока управления поддерживает оба типа дисплея')]
i555 = next(i for i, l in enumerate(body) if l.startswith('#P Вывод на дисплей организован через слой абстракции'))
body[i555] = ('#P Вывод на дисплей блока управления выполняет функция обновления состояния, которая формирует четыре '
              'текстовые строки. Первая строка содержит заголовок, состояние связи и режим, вторая – текущее давление, '
              'температуру и угол наклона, третья – состояние электролизера, клапана и поджига, четвертая – целевое '
              'давление и признаки защит; при изменении целевого давления на несколько секунд выводится шкала из 20 '
              'знакомест. Экраны обоих блоков обновляются только при изменении отображаемых данных, что исключает '
              'мерцание и снижает нагрузку на шину I2C.')
# temperature: DS18B20 only
body = [l for l in body if 'NTC' not in l or l.startswith('#P ')]
for l in body:
    if re.search('NTC|термистор', l):
        print('WARN NTC mention left:', l[:150])
# conclusion: reference to the list of documents
sub(body, '#P К слабым сторонам проекта', '#P Комплект конструкторской документации проекта приведен в ведомости документов (приложение Ж). К слабым сторонам проекта')
# words "узел" in body
for i, l in enumerate(body):
    if re.search(r'\bузл', l):
        print('INFO узел in body:', l[:160])

# ---------------- 3. sources ----------------
kp1_src = [p.text.strip() for p in kp1.paragraphs if re.match(r'^\[\d+\] ', p.text.strip())]
assert len(kp1_src) == 26, len(kp1_src)
gost19 = next(l for l in old if l.startswith('#PL [19] ГОСТ 19.701'))
sources = ['#PL ' + s for s in kp1_src] + ['#PL [27]' + gost19[len('#PL [19]'):]]

# ---------------- 4. appendices in new order ----------------
blocks = {}
cur = None
for l in apps_old:
    if l.startswith('#APP'):
        cur = l.split(' ', 1)[1].split('|')[0]; blocks[cur] = [l]
    elif cur:
        blocks[cur].append(l)
apps = []
for oldL in ['Б', 'В', 'Г', 'Е', 'Д', 'А', 'Ж']:
    blk = blocks[oldL][:]
    head = blk[0].split(' ', 1)[1].split('|')
    blk[0] = '#APP ' + amap[oldL] + '|' + head[1] + '|' + head[2]
    # remap appendix letters inside appendix texts, except references to the STP's own appendices
    fixed = []
    for l in blk[1:]:
        if 'стандарта предприятия' not in l:
            l = app_re.sub(lambda m: m.group(1) + ' ' + amap[m.group(2)], l)
        l = re.sub(r'функциональные узлы', 'функциональные части', l)
        l = re.sub(r'функциональных узлов', 'функциональных частей', l)
        l = re.sub(r'периферийных узлов', 'периферийных устройств', l)
        l = re.sub(r'\bузлом\b', 'элементом', l)
        l = re.sub(r'\bузлами\b', 'элементами', l)
        l = re.sub(r'\bузлов\b', 'элементов', l)
        l = re.sub(r'\bузлы\b', 'элементы', l)
        fixed.append(l)
    if oldL == 'Б':   # structural scheme: A3 sheet placeholder, inserted after the Word build
        fixed.append('#E')
        fixed.append('#P Схема выполнена на листе формата А3 и приведена на следующей странице.')
    apps += [blk[0]] + fixed
    for l in fixed:
        if re.search(r'\bузл', l): print('WARN узел in appendix:', l[:120])

head = old[:2]   # #TITLE, #TOC
res = head + new12 + ['#E'] if False else head + new12
res += body + ['#H1C Список использованных источников', '#E'] + sources + apps
io.open('pz_spec_new.txt', 'w', encoding='utf-8-sig').write('\n'.join(res) + '\n')

# ---------------- checks ----------------
text = '\n'.join(l for l in res if not l.startswith('#PL') and not l.startswith('#APP'))
order = []
for m in cite_re.finditer(text):
    for part in m.group(1).split(','):
        part = part.strip()
        rng = range(*[int(x) + k for k, x in enumerate(re.split('[–-]', part))]) if re.search('[–-]', part) else [int(part)]
        for n in rng:
            if n not in order: order.append(n)
print('citation order:', order)
print('uncited:', [n for n in range(1, 28) if n not in order])
aorder = []
for l in res:
    if l.startswith('#APP'): break
    for m in app_re.finditer(l):
        if m.group(2) not in aorder: aorder.append(m.group(2))
print('appendix reference order:', aorder)
print('lines:', len(res))

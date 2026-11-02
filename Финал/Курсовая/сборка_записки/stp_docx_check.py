"""Проверка оформления docx по СТП 01–2024 (п. 2.1.1, 2.2, 2.6, 2.7): поля, шрифт, интервал, отступ,
выравнивание, заголовки, таблицы. Usage: py -3 stp_docx_check.py file.docx"""
import sys, re
from collections import Counter
from docx import Document
from docx.shared import Mm, Pt
from docx.enum.text import WD_ALIGN_PARAGRAPH as A
sys.stdout.reconfigure(encoding='utf-8')
d = Document(sys.argv[1])
bad = []
for i, s in enumerate(d.sections):
    w, h = round(s.page_width.mm), round(s.page_height.mm)
    if (w, h) == (210, 297):
        m = tuple(round(x.mm) for x in (s.left_margin, s.right_margin, s.top_margin, s.bottom_margin))
        if m != (30, 15, 20, 20): bad.append(('поля раздела', i, m))
print('разделы:', [(round(s.page_width.mm), round(s.page_height.mm)) for s in d.sections])

def eff(p, attr):
    """effective paragraph format value through style chain"""
    v = getattr(p.paragraph_format, attr)
    st = p.style
    while v is None and st is not None:
        v = getattr(st.paragraph_format, attr); st = st.base_style
    return v

def font_of(p):
    names, sizes = Counter(), Counter()
    for r in p.runs:
        if not r.text.strip(): continue
        n = r.font.name; sz = r.font.size
        st = r.style if r.style is not None and r.style.name != 'Default Paragraph Font' else None
        ps = p.style
        while n is None and ps is not None: n = ps.font.name; ps = ps.base_style
        ps = p.style
        while sz is None and ps is not None: sz = ps.font.size; ps = ps.base_style
        names[n] += len(r.text); sizes[sz.pt if sz else None] += len(r.text)
    return names, sizes

stats = Counter()
body_started = False
for k, p in enumerate(d.paragraphs):
    t = p.text.strip()
    if not t: continue
    if t.upper() == 'ВВЕДЕНИЕ': body_started = True
    if not body_started: continue
    if t.startswith('ПРИЛОЖЕНИЕ Е'): break
    sty = p.style.name
    names, sizes = font_of(p)
    for n in names:
        if n not in ('Times New Roman', None): bad.append(('шрифт', k, n, t[:50]))
    for z in sizes:
        if z not in (14.0, None) and not sty.startswith('TOC'):
            stats[('размер', z, sty)] += 1
    ls = eff(p, 'line_spacing'); rule = eff(p, 'line_spacing_rule')
    stats[('интервал', str(ls), sty[:12])] += 1
    al = eff(p, 'alignment'); fi = eff(p, 'first_line_indent')
    is_h1 = re.match(r'^\d+\s+[А-ЯЁ ]{6,}', t) is not None and t == t.upper()
    is_h2 = re.match(r'^\d+\.\d+\s', t) is not None and len(t) < 160 and not t.endswith('.')
    is_cent = t.upper() in ('ВВЕДЕНИЕ', 'ЗАКЛЮЧЕНИЕ', 'СПИСОК ИСПОЛЬЗОВАННЫХ ИСТОЧНИКОВ', 'СОДЕРЖАНИЕ') or t.startswith('ПРИЛОЖЕНИЕ')
    if is_h1 or is_h2:
        bold = all((r.bold or (p.style.font.bold)) for r in p.runs if r.text.strip())
        if not bold: bad.append(('заголовок не полужирный', k, t[:60]))
        if al not in (None, A.LEFT, A.JUSTIFY): bad.append(('заголовок не слева', k, t[:60], al))
        stats[('отступ заголовка', round(fi.mm, 1) if fi is not None else None)] += 1
    elif is_cent:
        if al != A.CENTER: bad.append(('ненумерованный заголовок не по центру', k, t[:40], al))
    elif t.startswith('Таблица') and ' – ' in t:
        if al not in (None, A.LEFT, A.JUSTIFY) or (fi is not None and abs(fi.mm) > 0.5): bad.append(('заголовок таблицы', k, t[:50], al, fi))
    elif p.style.name.startswith('List') or t.startswith(('–', 'где')):
        pass
    elif len(t) > 120:
        if al != A.JUSTIFY: bad.append(('абзац не по ширине', k, t[:50], al))
        if fi is None or abs(fi.mm - 12.5) > 0.3: bad.append(('абзацный отступ', k, t[:50], fi.mm if fi is not None else None))
for x in sorted(stats.items(), key=lambda kv: -kv[1])[:25]: print(x)
# tables
for ti, tb in enumerate(d.tables):
    hdr = [c.text.strip() for c in tb.rows[0].cells]
    if any(re.match(r'^№', h) for h in hdr): bad.append(('графа №', ti, hdr))
    for r in tb.rows[1:]:
        for c in r.cells:
            if not c.text.strip(): bad.append(('пустая ячейка таблицы', ti, hdr[:2])); break
print('замечаний:', len(bad))
for b in bad[:80]: print(b)

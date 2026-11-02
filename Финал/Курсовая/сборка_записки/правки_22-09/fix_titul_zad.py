import sys, copy
from docx import Document
sys.stdout.reconfigure(encoding='utf-8')

def set_text(p, text):
    runs = p.runs
    if not runs:
        p.add_run(text); return
    runs[0].text = text
    for r in runs[1:]:
        r._r.getparent().remove(r._r)

def replace_in(p, old, new):
    if old in p.text:
        # try run-level first (keeps formatting), else whole paragraph
        for r in p.runs:
            if old in r.text:
                r.text = r.text.replace(old, new); return True
        set_text(p, p.text.replace(old, new)); return True
    return False

# ---------------- title page ----------------
t = Document(sys.argv[1])
n = 0
for p in t.paragraphs:
    n += replace_in(p, 'МИКРОПРОЦЕССОРНОЕ УСТРОЙСТВО КОНТРОЛЯ ПАРАМЕТРОВ СУПЕРМАРКЕТА',
                    'МИКРОПРОЦЕССОРНОЕ УСТРОЙСТВО ДИСТАНЦИОННОГО УПРАВЛЕНИЯ ОБЪЕКТОМ')
    n += replace_in(p, 'БГУИР КП 6-05-0611-05 111 ПЗ', 'БГУИР КП 6-05-0611-05 113 ПЗ')
for tb in t.tables:
    for row in tb.rows:
        for c in row.cells:
            for p in c.paragraphs:
                n += replace_in(p, 'Д.Д. Левшук', 'В.К. Мурачев')
print('title replacements:', n)
t.save(sys.argv[2])

# ---------------- assignment sheet ----------------
z = Document(sys.argv[3])
P = z.paragraphs
n = 0
for p in P:
    n += replace_in(p, 'с 07.12.2026 по 16.12.2026', 'с 23.11.2026 по 26.11.2026')
    n += replace_in(p, '(формат А4)', '(формат А3)')
    n += replace_in(p, 'разделы 1, 2 к 23.09 – 20 %', 'разделы 1, 2 к 01.10 – 20 %;')
    n += replace_in(p, 'раздел 3 к 13.10 – 20 %;', 'раздел 3 к 15.10 – 20 %;')
    n += replace_in(p, 'раздел 5 к 10.11 – 20 %. ', 'раздел 5 к 12.11 – 20 %;')
    n += replace_in(p, 'к 18.11 – 15 %', 'к 20.11 – 15 %;')
print('assignment replacements:', n)
# item 2 of the initial data, verbatim from the department template
src = [p for p in z.paragraphs if p.text.startswith('Сложность электрической схемы')][0]
new = copy.deepcopy(src._p)
src._p.addnext(new)
from docx.text.paragraph import Paragraph
np_ = Paragraph(new, src._parent)
set_text(np_, 'В момент начала работы устройства отдельный дискретный светодиод должен мигать 3–5 с с частотой не более 2–3 Гц.\t')
z.save(sys.argv[4])
print('saved')

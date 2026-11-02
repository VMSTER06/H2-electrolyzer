from pathlib import Path
from shutil import copy2
from copy import deepcopy
import re
import json
import xml.etree.ElementTree as ET

from docx import Document
from docx.shared import Mm
from docx.enum.section import WD_ORIENT

BASE = Path(__file__).resolve().parents[2]
SOURCE = BASE / '434701_Мурачев_В_К_КП2'
OUT = BASE / '434701_Мурачев_В_К_КП2_проверено_09-10'
OUT.mkdir(exist_ok=True)

# Copy only this checkpoint; keep the source and the open full note intact.
for path in SOURCE.iterdir():
    if path.is_file() and '(А4)' not in path.name:
        copy2(path, OUT / path.name)

def replace_span(paragraph, start, end, replacement):
    runs = paragraph.runs
    cursor = 0
    spans = []
    for run in runs:
        spans.append((run, cursor, cursor + len(run.text)))
        cursor += len(run.text)
    inserted = False
    for run, left, right in spans:
        if right <= start or left >= end:
            continue
        a, b = max(start - left, 0), min(end - left, len(run.text))
        run.text = run.text[:a] + (replacement if not inserted else '') + run.text[b:]
        inserted = True
    assert inserted

def replace(paragraph, before, after):
    start = paragraph.text.index(before)
    replace_span(paragraph, start, start + len(before), after)

# The assignment change is one character and preserves underlined tab leaders.
assignment = Document(OUT / 'Лист задания.docx')
matches = [p for p in assignment.paragraphs if 'Структурная схема устройства (формат А4)' in p.text]
assert len(matches) == 1
replace(matches[0], 'формат А4', 'формат А3')
assignment.save(OUT / 'Лист задания.docx')

# Reuse the already authored A3 version of the same seven-block diagram.
drawings = BASE / 'чертежи'
stem = 'ГУИР.6-05-0611-05.113 Э1 Схема электрическая структурная'
sheet_png = drawings / (stem + ' (А3).png')
copy2(drawings / (stem + ' (А3).pdf'), OUT / 'Схема электрическая структурная (А3).pdf')
tree = ET.parse(drawings / (stem + '.drawio'))
source_a3 = next(d for d in tree.getroot().findall('diagram') if d.get('name') == 'Э1 А3')
root = ET.Element('mxfile', {'host': 'Electron', 'agent': 'Codex'})
root.append(deepcopy(source_a3))
ET.indent(root)
ET.ElementTree(root).write(OUT / 'Схема электрическая структурная (А3).drawio', encoding='utf-8', xml_declaration=True)

note = Document(OUT / 'Пояснительная записка.docx')
assert len(note.sections) == 4 and len(note.inline_shapes) == 2
# Section 1 is precisely the structural drawing sheet. The following A4
# appendix title and the A3 functional drawing retain their own sections.
note.sections[1].orientation = WD_ORIENT.LANDSCAPE
note.sections[1].page_width = Mm(420)
note.sections[1].page_height = Mm(297)
shape = note.inline_shapes[0]
rel_id = shape._inline.graphic.graphicData.pic.blipFill.blip.embed
note.part.related_parts[rel_id]._blob = sheet_png.read_bytes()
shape.width = Mm(419)
shape.height = Mm(296.3)

# Put the reference itself at the end of section 2, not only in its last paragraph.
reference = next(p for p in note.paragraphs if p.text.startswith('Схема электрическая структурная устройства приведена в приложении А;'))
prefix, body = reference.text.split(' Устройство состоит', 1)
replace_span(reference, 0, len(reference.text), 'Устройство состоит' + body + ' ' + prefix)

citations_changed = 0
for paragraph in note.paragraphs:
    # Preserve numbers and range semantics; only standardize comma spacing.
    for match in reversed(list(re.finditer(r'\[(\d[\d\s,–-]*)\]', paragraph.text))):
        fixed = re.sub(r'\s*,\s*', ',', match.group())
        if fixed != match.group():
            replace_span(paragraph, match.start(), match.end(), fixed)
            citations_changed += 1
note.save(OUT / 'Пояснительная записка.docx')

paras = note.paragraphs
intro_start = next(i for i, p in enumerate(paras) if p.text.strip().upper() == 'ВВЕДЕНИЕ')
intro_end = next(i for i in range(intro_start + 1, len(paras)) if paras[i].style.name == 'Heading 1')
intro = [p.text for p in paras[intro_start+1:intro_end] if p.text.strip()]
all_text = '\n'.join(p.text for p in paras)
section3 = next(i for i,p in enumerate(paras) if p.style.name == 'Heading 1' and re.match(r'^3\s',p.text))
before3 = [p.text for p in paras[:section3] if p.text.strip()][-1]
audit = {
    'source': str(SOURCE), 'output': str(OUT),
    'intro_functions': sum(t.startswith('– ') for t in intro),
    'intro_parts': 3,
    'uzly_occurrences': len(re.findall(r'\bуз(?:ел|л\w*)\b', all_text, re.I)),
    'board_one_element': 'Плата с установленным микроконтроллером рассматривается как один элемент.' in all_text,
    'reference_ends_section_2': before3.endswith(prefix),
    'structural_drawing_blocks': sum(c.get('id') in {f'c{i}' for i in range(48,55)} for c in source_a3.findall('.//mxCell')),
    'word_sheet_mm': [[round(s.page_width.mm,2),round(s.page_height.mm,2)] for s in note.sections],
    'citation_groups_normalized': citations_changed,
    'scope': 'Only the checkpoint package is edited. Full note and prior checkpoints are preserved.'
}
assert audit['intro_functions']==7 and audit['uzly_occurrences']==0 and audit['reference_ends_section_2']
assert audit['structural_drawing_blocks']==7
Path(__file__).with_name('audit.json').write_text(json.dumps(audit,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(audit,ensure_ascii=False,indent=2))

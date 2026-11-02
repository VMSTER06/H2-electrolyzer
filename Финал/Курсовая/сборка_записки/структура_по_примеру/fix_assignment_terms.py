from pathlib import Path
from shutil import copy2
from docx import Document

source = Path(r"S:\1Electrolizer\Финал\Курсовая\Лист задания H2 PRO.docx")
backup = Path(r"S:\1Electrolizer\Финал\Курсовая\Архив\Лист задания H2 PRO до удаления слова узлы.docx")
backup.parent.mkdir(parents=True, exist_ok=True)
if not backup.exists():
    copy2(source, backup)

doc = Document(source)
changed = 0


def replace_in_paragraph(paragraph):
    global changed
    for run in paragraph.runs:
        if "узлов и элементов" in run.text:
            run.text = run.text.replace("узлов и элементов", "элементов")
            changed += 1


for paragraph in doc.paragraphs:
    replace_in_paragraph(paragraph)
for table in doc.tables:
    for row in table.rows:
        for cell in row.cells:
            for paragraph in cell.paragraphs:
                replace_in_paragraph(paragraph)

if changed != 1:
    raise RuntimeError(f"Expected one replacement, got {changed}")
doc.save(source)
print(f"saved={source}; replacements={changed}")

from copy import deepcopy
from pathlib import Path
import shutil

from docx import Document
from docx.enum.text import WD_TAB_ALIGNMENT, WD_TAB_LEADER
from docx.oxml.ns import qn


DOCX = Path(r"S:\1Electrolizer\Финал\Курсовая\Лист задания H2 PRO.docx")
BACKUP = Path(r"S:\1Electrolizer\Финал\Курсовая\Архив\Лист задания H2 PRO до продления линий.docx")

BACKUP.parent.mkdir(parents=True, exist_ok=True)
if not BACKUP.exists():
    shutil.copy2(DOCX, BACKUP)

# Always rebuild from the clean pre-edit copy so failed layout experiments cannot
# accumulate in the form.
shutil.copy2(BACKUP, DOCX)
doc = Document(DOCX)
section = doc.sections[0]
text_width = section.page_width - section.left_margin - section.right_margin

changed = []
for index, paragraph in enumerate(doc.paragraphs):
    if not any(run.font.underline is True for run in paragraph.runs):
        continue
    # These paragraphs already contain the template's own tab-based blank lines.
    # Adding another right tab stop would reflow every existing tab onto a new row.
    if "\t" in paragraph.text:
        continue

    # Remove an earlier generated terminal tab to keep repeated runs idempotent.
    for run in list(paragraph.runs):
        if run._element.get(qn("w:rsidRPr")) == "00C0D3A1":
            paragraph._element.remove(run._element)

    left = paragraph.paragraph_format.left_indent or 0
    right = paragraph.paragraph_format.right_indent or 0
    stop = text_width - left - right
    paragraph.paragraph_format.tab_stops.add_tab_stop(
        stop, WD_TAB_ALIGNMENT.RIGHT, WD_TAB_LEADER.SPACES
    )

    tail = paragraph.add_run("\t")
    tail.font.underline = True
    tail._element.set(qn("w:rsidRPr"), "00C0D3A1")
    if paragraph.runs:
        source = next((r for r in reversed(paragraph.runs[:-1]) if r._element.rPr is not None), None)
        if source is not None and source._element.rPr is not None:
            old = tail._element.rPr
            if old is not None:
                tail._element.remove(old)
            tail._element.insert(0, deepcopy(source._element.rPr))
            tail.font.underline = True
    changed.append(index)

doc.save(DOCX)
print("extended", ",".join(map(str, changed)))

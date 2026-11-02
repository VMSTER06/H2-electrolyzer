"""Insert the A3 landscape sheet (structural scheme) right after appendix A text, as its own section
without page-number footer; the rest of the document continues on A4 with numbering."""
import sys, copy
from docx import Document
from docx.shared import Mm
from docx.oxml.ns import qn
from docx.oxml import OxmlElement
from docx.text.paragraph import Paragraph
sys.stdout.reconfigure(encoding='utf-8')
SRC, DST, PNG = sys.argv[1], sys.argv[2], sys.argv[3]
d = Document(SRC)
body_sect = d.element.body.find(qn('w:sectPr'))

marker = next(p for p in d.paragraphs if p.text.startswith('Схема выполнена на листе формата А3'))
# paragraph that ends the first (A4) section
end1 = OxmlElement('w:p')
marker._p.addnext(end1)
ppr = OxmlElement('w:pPr'); end1.append(ppr)
s1 = copy.deepcopy(body_sect); ppr.append(s1)
# paragraph with the picture, ends the A3 section
pic_p = OxmlElement('w:p'); end1.addnext(pic_p)
P = Paragraph(pic_p, marker._parent)
pf = P.paragraph_format
pf.first_line_indent = Mm(0); pf.left_indent = Mm(0); pf.space_before = 0; pf.space_after = 0
pf.line_spacing = 1.0
run = P.add_run()
run.add_picture(PNG, width=Mm(419), height=Mm(296.3))
ppr2 = pic_p.find(qn('w:pPr'))
s2 = copy.deepcopy(body_sect)
for tag in ('w:headerReference', 'w:footerReference', 'w:pgNumType', 'w:titlePg'):
    for e in s2.findall(qn(tag)): s2.remove(e)
pgSz = s2.find(qn('w:pgSz')); pgSz.set(qn('w:w'), '23811'); pgSz.set(qn('w:h'), '16838'); pgSz.set(qn('w:orient'), 'landscape')
pgMar = s2.find(qn('w:pgMar'))
for k in ('top', 'right', 'bottom', 'left', 'header', 'footer', 'gutter'): pgMar.set(qn('w:' + k), '0')
ppr2.append(s2)
# the remaining (last) section must continue numbering, not restart
for e in body_sect.findall(qn('w:pgNumType')): body_sect.remove(e)
# the A3 section gets its own empty footer/header
secs = d.sections
a3 = next(s for s in secs if s._sectPr is s2)
a3.footer.is_linked_to_previous = False
a3.header.is_linked_to_previous = False
for p in a3.footer.paragraphs:
    for r in list(p.runs): r._r.getparent().remove(r._r)
    for f in list(p._p.iter(qn('w:fldSimple'))): f.getparent().remove(f)
print('sections:', [(round(s.page_width.mm), round(s.page_height.mm)) for s in d.sections])
d.save(DST)
print('saved', DST)

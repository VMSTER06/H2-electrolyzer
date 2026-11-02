"""Insert drawing sheets after appendix titles, each sheet as its own section without page-number footer.
Usage: python insert_sheets.py in.docx out.docx А=path.png:A4 Б=path.png:A3 ..."""
import sys, copy
from docx import Document
from docx.shared import Mm, Pt
from docx.oxml.ns import qn
from docx.oxml import OxmlElement
from docx.text.paragraph import Paragraph
sys.stdout.reconfigure(encoding='utf-8')
SRC, DST = sys.argv[1], sys.argv[2]
jobs = []
for a in sys.argv[3:]:
    letter, rest = a.split('=', 1)
    png, fmt = rest.rsplit(':', 1)
    jobs.append((letter, png, fmt))
SIZES = {'A4': (210, 297, False), 'A3': (420, 297, True)}

d = Document(SRC)
body_sect = d.element.body.find(qn('w:sectPr'))
main = copy.deepcopy(body_sect)

def to_anchor(p_el):
    """Inline picture -> floating picture at the page origin (no clipping by line height)."""
    WP = 'http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing'
    inline = p_el.find('.//{%s}inline' % WP)
    anchor = OxmlElement('wp:anchor')
    for k, v in {'distT': '0', 'distB': '0', 'distL': '0', 'distR': '0', 'simplePos': '0', 'relativeHeight': '0',
                 'behindDoc': '0', 'locked': '0', 'layoutInCell': '1', 'allowOverlap': '1'}.items():
        anchor.set(k, v)
    sp = OxmlElement('wp:simplePos'); sp.set('x', '0'); sp.set('y', '0'); anchor.append(sp)
    for d_ in ('H', 'V'):
        pos = OxmlElement('wp:position' + d_); pos.set('relativeFrom', 'page')
        off = OxmlElement('wp:posOffset'); off.text = '0'; pos.append(off); anchor.append(pos)
    anchor.append(copy.deepcopy(inline.find('{%s}extent' % WP)))
    ee = OxmlElement('wp:effectExtent')
    for side in 'ltrb': ee.set(side, '0')
    anchor.append(ee)
    anchor.append(OxmlElement('wp:wrapNone'))
    for tag in ('docPr', 'cNvGraphicFramePr'):
        e = inline.find('{%s}%s' % (WP, tag))
        if e is not None: anchor.append(copy.deepcopy(e))
    anchor.append(copy.deepcopy(inline.find('{http://schemas.openxmlformats.org/drawingml/2006/main}graphic')))
    inline.getparent().replace(inline, anchor)

first = True
sheet_sects = []

def strip(s, tags):
    for tag in tags:
        for e in s.findall(qn(tag)): s.remove(e)

for letter, png, fmt in jobs:
    paras = d.paragraphs
    k = next(i for i, p in enumerate(paras) if p.text.strip() == 'ПРИЛОЖЕНИЕ ' + letter)
    title = paras[k + 2]
    # drop empty paragraphs that follow the title (they would land on a separate page)
    j = k + 3
    while j < len(paras) and not paras[j].text.strip() and not paras[j]._p.findall('.//' + qn('w:drawing')):
        paras[j]._p.getparent().remove(paras[j]._p); j += 1
    end1 = OxmlElement('w:p'); title._p.addnext(end1)
    ppr = OxmlElement('w:pPr'); end1.append(ppr)
    s1 = copy.deepcopy(main)
    if not first: strip(s1, ['w:pgNumType'])
    first = False
    ppr.append(s1)
    prev = end1
    pngs = png.split(';')
    for k, one in enumerate(pngs):
        pic_p = OxmlElement('w:p'); prev.addnext(pic_p); prev = pic_p
        P = Paragraph(pic_p, title._parent)
        pf = P.paragraph_format
        pf.first_line_indent = Mm(0); pf.left_indent = Mm(0); pf.space_before = 0; pf.space_after = 0
        w, h, land = SIZES[fmt]
        run = P.add_run(); run.add_picture(one, width=Mm(w), height=Mm(h)); run.font.size = Pt(1)
        pf.line_spacing = Pt(1)
        to_anchor(pic_p)
        s2 = copy.deepcopy(main)
        strip(s2, ['w:headerReference', 'w:footerReference', 'w:pgNumType', 'w:titlePg'])
        pgSz = s2.find(qn('w:pgSz'))
        pgSz.set(qn('w:w'), str(round(w * 56.7))); pgSz.set(qn('w:h'), str(round(h * 56.7)))
        if land: pgSz.set(qn('w:orient'), 'landscape')
        elif qn('w:orient') in pgSz.attrib: del pgSz.attrib[qn('w:orient')]
        pgMar = s2.find(qn('w:pgMar'))
        for kk in ('top', 'right', 'bottom', 'left', 'header', 'footer', 'gutter'): pgMar.set(qn('w:' + kk), '0')
        if k < len(pngs) - 1:
            pic_p.find(qn('w:pPr')).append(s2); sheet_sects.append(s2)
    # if nothing follows the sheet, it becomes the last section (no trailing empty page)
    rest = []
    nxt = pic_p.getnext()
    while nxt is not None and nxt.tag != qn('w:sectPr'):
        rest.append(nxt); nxt = nxt.getnext()
    if all(e.tag == qn('w:p') and not ''.join(e.itertext()).strip() for e in rest):
        for e in rest: e.getparent().remove(e)
        for ch in list(body_sect): body_sect.remove(ch)
        for ch in list(s2): body_sect.append(ch)
        for k_, v_ in s2.attrib.items(): body_sect.set(k_, v_)
        sheet_sects.append(body_sect)
    else:
        pic_p.find(qn('w:pPr')).append(s2)
        sheet_sects.append(s2)

if body_sect not in sheet_sects: strip(body_sect, ['w:pgNumType'])
# Таблицы больше не держатся целиком: по СТП таблица переносится с надписью «Продолжение
# таблицы N.M» – ее ставит upd.ps1 после разбиения на страницы (build_pz.ps1 держит вместе
# только подпись, головку и первую строку; таблицы с объединенными ячейками – целиком).
for s in d.sections:
    if any(s._sectPr is x for x in sheet_sects):
        s.footer.is_linked_to_previous = False
        s.header.is_linked_to_previous = False
        for p in s.footer.paragraphs:
            for r in list(p.runs): r._r.getparent().remove(r._r)
print('sections:', [(round(s.page_width.mm), round(s.page_height.mm)) for s in d.sections])
d.save(DST)
print('saved', DST)

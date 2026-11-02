from copy import deepcopy
from pathlib import Path

from docx import Document
from docx.oxml import OxmlElement
from docx.shared import Pt

base = Path(__file__).resolve().parents[2]
path = base / '434701_Мурачев_В_К_КП2_проверено_09-10' / 'Пояснительная записка.docx'
doc = Document(path)
sheets = [doc.sections[1], doc.sections[3]]
for index, p in enumerate(p for p in doc.paragraphs if p._p.xpath('.//wp:inline')):
    inline = p._p.xpath('.//wp:inline')[0]
    anchor = OxmlElement('wp:anchor')
    for name, value in {
        'distT': '0', 'distB': '0', 'distL': '0', 'distR': '0',
        'simplePos': '0', 'relativeHeight': '0', 'behindDoc': '0',
        'locked': '0', 'layoutInCell': '1', 'allowOverlap': '1',
    }.items():
        anchor.set(name, value)
    simple = OxmlElement('wp:simplePos')
    simple.set('x', '0')
    simple.set('y', '0')
    anchor.append(simple)
    for direction in ['H', 'V']:
        position = OxmlElement('wp:position' + direction)
        position.set('relativeFrom', 'page')
        offset = OxmlElement('wp:posOffset')
        offset.text = '0'
        position.append(offset)
        anchor.append(position)
    width = str(int(sheets[index].page_width))
    height = str(int(sheets[index].page_height))
    extent = deepcopy(inline.find('{http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing}extent'))
    extent.set('cx', width)
    extent.set('cy', height)
    anchor.append(extent)
    effect = OxmlElement('wp:effectExtent')
    for side in ['l', 't', 'r', 'b']:
        effect.set(side, '0')
    anchor.append(effect)
    anchor.append(OxmlElement('wp:wrapNone'))
    for tag in ['wp:docPr', 'wp:cNvGraphicFramePr', 'a:graphic']:
        anchor.append(deepcopy(inline.xpath('./' + tag)[0]))
    for ext in anchor.xpath('.//a:xfrm/a:ext'):
        ext.set('cx', width)
        ext.set('cy', height)
    for name in anchor.xpath('.//pic:cNvPr'):
        name.set('name', 'e' + str(index + 1) + '_a3.png')
    inline.getparent().replace(inline, anchor)
    fmt = p.paragraph_format
    fmt.space_before = Pt(0)
    fmt.space_after = Pt(0)
    fmt.first_line_indent = Pt(0)
    fmt.left_indent = Pt(0)
    fmt.right_indent = Pt(0)
    fmt.line_spacing = Pt(1)
    for run in p.runs:
        run.font.size = Pt(1)
doc.save(path)
print('Both A3 drawings anchored to page origin at full page size.')

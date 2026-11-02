"""Лист задания: новые названия частей устройства и единое начертание пунктов перечня графического материала."""
import sys
from docx import Document
sys.stdout.reconfigure(encoding='utf-8')
p_in, p_out = sys.argv[1], sys.argv[2]
d = Document(p_in)
NEW = {
    'Двусторонний беспроводной обмен': 'Двусторонний беспроводной обмен командами и данными между устройством контроля и устройством '
                                       'удаленного управления на расстоянии не менее 30 м в помещении.',
    'Отображение параметров': 'Отображение параметров на устройствах вывода информации устройства контроля и устройства '
                              'удаленного управления, звуковая сигнализация аварий.',
    'Питание блока управления': 'Питание устройства контроля – 12 В, ток не более 1 А; питание устройства удаленного управления '
                                '– 3,7 В, ток не более 0,3 А.',
}
done = set()
in_graph = False
for p in d.paragraphs:
    t = p.text
    for k, v in NEW.items():
        if t.startswith(k):
            p.runs[0].text = v + '\t'
            for r in p.runs[1:]: r.text = ''
            done.add(k)
    if t.startswith('5. Перечень графического'): in_graph = True; continue
    if t.startswith('6. Консультант'): in_graph = False
    if in_graph:
        for r in p.runs: r.italic = None
assert done == set(NEW), done
d.save(p_out)
for p in Document(p_out).paragraphs:
    if any(w in p.text for w in ('пульт', 'блок управления', 'блока управления', 'блоком управления')): print('ОСТАЛОСЬ:', p.text)
print('ok')

"""Лист задания, п. 3.3: без Wi-Fi (требование 31.10.2026). Usage: py -3 fix_zad_nowifi.py <docx>..."""
import sys
from docx import Document
sys.stdout.reconfigure(encoding='utf-8')
NEW = ('Микроконтроллерная часть – отладочные платы на 32-разрядных микроконтроллерах с логическими '
       'уровнями 3,3 В и интерфейсами SPI, I2C, АЦП и ШИМ.')
for fn in sys.argv[1:]:
    d = Document(fn); hit = 0
    for p in d.paragraphs:
        if p.text.startswith('Микроконтроллерная часть'):
            tail = '\t' if p.text.endswith('\t') else ''
            p.runs[0].text = NEW + tail
            for r in p.runs[1:]: r.text = ''
            hit += 1
    assert hit == 1, (fn, hit)
    d.save(fn)
    left = [p.text for p in Document(fn).paragraphs if 'Wi-Fi' in p.text or 'веб' in p.text.lower()]
    print(fn, 'ok' if not left else left)

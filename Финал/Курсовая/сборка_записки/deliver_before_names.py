"""Раскладка собранного по папкам сдачи и пересборка zip КП3.
Usage: py -3 deliver.py <папка_сборки>   (в ней full.docx/pdf и kp3.docx/pdf)"""
import os, shutil, sys, zipfile
sys.stdout.reconfigure(encoding='utf-8')
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
G = os.path.join(ROOT, 'ГОТОВО_курсовая_H2')
K3 = '434701_Мурачев_В_К_КП3'
DES = 'ГУИР.6-05-0611-05.114'
B = sys.argv[1]
SH = os.path.join(HERE, '_sheets')
for f in ('full.pdf', 'full.docx', 'kp3.pdf', 'kp3.docx'):
    if not os.path.getsize(os.path.join(B, f)): raise SystemExit('пустой ' + f)
CP = [(os.path.join(B, 'full.docx'), [os.path.join(ROOT, 'Пояснительная записка H2 (СТП).docx'), os.path.join(G, '1_Пояснительная записка (полная).docx')]),
      (os.path.join(B, 'full.pdf'), [os.path.join(ROOT, 'Пояснительная записка H2 (СТП).pdf'), os.path.join(G, '1_Пояснительная записка (полная).pdf')])]
K3DIRS = [os.path.join(G, '3_КП3_сдача', K3), os.path.join(ROOT, K3)]
for ext in ('docx', 'pdf'):
    CP.append((os.path.join(B, 'kp3.' + ext), [os.path.join(d, 'Пояснительная записка.' + ext) for d in K3DIRS]))
SHEETS = {'E1n.pdf': f'{DES} Э1 Схема электрическая структурная (А3).pdf', 'E2n.pdf': f'{DES} Э2 Схема электрическая функциональная (А3).pdf',
          'E2.drawio': f'{DES} Э2 Схема электрическая функциональная (А3).drawio',
          'E3.pdf': f'{DES} Э3 Схема электрическая принципиальная (А3, 2 листа).pdf', 'doc_ПЭ3.pdf': f'{DES} ПЭ3 Перечень элементов.pdf'}
CHERT = [os.path.join(G, '2_Чертежи'), os.path.join(ROOT, 'чертежи')]
for s, d in SHEETS.items():
    CP.append((os.path.join(SH, s), [os.path.join(x, d) for x in K3DIRS + CHERT]))
for s, d in {'doc_Д1.pdf': f'{DES} Д1 Ведомость документов.pdf', 'pd1.pdf': f'{DES} ПД1 Схема программы (А3).pdf'}.items():
    CP.append((os.path.join(SH, s), [os.path.join(x, d) for x in CHERT]))
PNG = {'E1n.png': f'{DES} Э1 Схема электрическая структурная (А3).png', 'E2n.png': f'{DES} Э2 Схема электрическая функциональная (А3).png',
       'E3_1.png': f'{DES} Э3 лист 1.png', 'E3_2.png': f'{DES} Э3 лист 2.png', 'pd1.png': f'{DES} ПД1 Схема программы (А3).png'}
for s, d in PNG.items(): CP.append((os.path.join(SH, s), [os.path.join(ROOT, 'чертежи', d)]))
CP.append((os.path.join(SH, 'stickers.pdf'), [os.path.join(G, '6_Наклейки в корпус (печать А4).pdf'), os.path.join(ROOT, 'чертежи', 'Наклейки в корпус.pdf')]))
n = 0
for src, dsts in CP:
    if not os.path.exists(src): print('нет источника', src); continue
    for d in dsts:
        shutil.copyfile(src, d); n += 1
# zip КП3 из папки сдачи
d = K3DIRS[0]; z = d + '.zip'
with zipfile.ZipFile(z + '.tmp', 'w', zipfile.ZIP_DEFLATED) as Z:
    for f in sorted(os.listdir(d)): Z.write(os.path.join(d, f), K3 + '/' + f)
os.replace(z + '.tmp', z)
print(f'скопировано файлов: {n}; zip: {len(zipfile.ZipFile(z).namelist())} файлов')

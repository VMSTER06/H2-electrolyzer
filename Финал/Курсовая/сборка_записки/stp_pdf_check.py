"""Проверка готового PDF записки по вёрстке (то, что не видно в исходнике и docx):
заполненность страниц, «висячие» заголовки и подписи таблиц, пустоты внутри страницы,
продолжение таблиц, нумерация страниц, объём от введения до заключения (≥ 35 с.).
Usage: py -3 stp_pdf_check.py note.pdf [первый_номер_страницы=3]"""
import html, os, re, subprocess, sys, tempfile
sys.stdout.reconfigure(encoding='utf-8')
POP = r'C:\Users\vladm\AppData\Local\Microsoft\WinGet\Packages\oschwartz10612.Poppler_Microsoft.Winget.Source_8wekyb3d8bbwe\poppler-25.07.0\Library\bin'
pdf = sys.argv[1]; first = int(sys.argv[2]) if len(sys.argv) > 2 else 3
MM = 25.4 / 72
LINE = 18 * MM                     # строка 18 пт ≈ 6,35 мм
BOTTOM = 297 - 20                  # нижняя граница поля текста, мм

tmp = os.path.join(tempfile.gettempdir(), 'stp_pdf_check.html')
subprocess.run([os.path.join(POP, 'pdftotext.exe'), '-bbox-layout', pdf, tmp], check=True)
s = open(tmp, encoding='utf-8').read()
pages = []
for w, h, body in re.findall(r'<page width="([\d.]+)" height="([\d.]+)">(.*?)</page>', s, re.S):
    L = []
    for m in re.finditer(r'<line xMin="([\d.]+)" yMin="([\d.]+)" xMax="([\d.]+)" yMax="([\d.]+)">(.*?)</line>', body, re.S):
        t = ' '.join(html.unescape(x) for x in re.findall(r'>([^<]*)</word>', m.group(5))).strip()
        L.append((float(m.group(2)) * MM, float(m.group(4)) * MM, float(m.group(1)) * MM, t))
    pages.append((float(w) > float(h), sorted(L)))

NEWSEC = re.compile(r'^(\d+ [А-ЯЁ][А-ЯЁ ]|ВВЕДЕНИЕ|ЗАКЛЮЧЕНИЕ|СПИСОК|ПРИЛОЖЕНИЕ|СОДЕРЖАНИЕ)')
H2 = re.compile(r'^\d+\.\d+ [А-ЯЁA-Z]')
rem, warn = [], []
pnum = {}
intro = concl = None
for i, (land, L) in enumerate(pages):
    n = i + first
    if land: continue
    # номер страницы: отдельное число внизу справа
    nums = [x for x in L if re.fullmatch(r'\d{1,3}', x[3]) and x[0] > BOTTOM - 2]
    if nums: pnum[n] = int(nums[0][3])
    elif L: warn.append(f'с.{n}: нет номера страницы')
    T = [x for x in L if x not in nums]
    if not T: continue
    txt0 = T[0][3]
    if txt0 == 'ВВЕДЕНИЕ': intro = n
    if intro is None: continue              # титульная часть и содержание
    if txt0 == 'ЗАКЛЮЧЕНИЕ': concl = n
    if txt0.startswith('ПРИЛОЖЕНИЕ') and len(T) <= 3: continue        # титульный лист приложения
    # следующая текстовая страница
    nxt = next((pages[j][1] for j in range(i + 1, len(pages)) if not pages[j][0]), None)
    nxt0 = ''
    if nxt:
        nT = [x for x in nxt if not (re.fullmatch(r'\d{1,3}', x[3]) and x[0] > BOTTOM - 2)]
        nxt0 = nT[0][3] if nT else ''
    gap = BOTTOM - max(x[1] for x in T)
    last = max(T, key=lambda x: x[1])[3]
    if not NEWSEC.match(nxt0) and nxt is not None and pages[i + 1][0] is False:
        # подраздел переносится вместе с пустой строкой и двумя строками текста (СТП: заголовок
        # не остается внизу без текста) – до 6 строк пустоты перед ним допустимо
        if H2.match(nxt0) and gap <= 6 * LINE:
            if gap > 4 * LINE: warn.append(f'с.{n}: внизу {gap:.0f} мм перед подразделом «{nxt0[:40]}» (перенос заголовка с текстом)')
        elif nxt0.startswith('Таблица') and gap <= 6 * LINE:
            # таблица уходит целиком, только если подпись + головка + первая строка не помещаются
            # (build_pz/upd иначе разрывают её с «Продолжением таблицы»)
            if gap > 4 * LINE: warn.append(f'с.{n}: внизу {gap:.0f} мм – {nxt0[:14]} перенесена целиком (не помещаются подпись, головка и 1 строка)')
        elif gap > 4 * LINE:
            rem.append(f'с.{n}: незаполнено {gap:.0f} мм внизу (следующая начинается «{nxt0[:40]}»)')
        elif gap > 2.2 * LINE and not H2.match(nxt0):
            warn.append(f'с.{n}: внизу {gap:.0f} мм (≈{gap / LINE:.0f} строки), следующая «{nxt0[:40]}»')
    # висячие заголовок / подпись таблицы
    if H2.match(last) or re.match(r'^(Таблица|Продолжение таблицы) \d', last):
        rem.append(f'с.{n}: «{last[:40]}» стоит последней строкой страницы')
    # продолжение таблицы без надписи: страница начинается со строки таблицы (узкие колонки),
    # а предыдущая страница кончилась строкой таблицы
    # пустоты внутри страницы, если ниже нет подписи рисунка
    for a, b in zip(T, T[1:]):
        g = b[0] - a[1]
        if g > 4 * LINE and not b[3].startswith('Рисунок'):
            rem.append(f'с.{n}: пустота {g:.0f} мм между «{a[3][:30]}» и «{b[3][:30]}»')
for n, v in pnum.items():
    if v != n: rem.append(f'с.{n}: напечатан номер {v}'); break
if intro and concl:
    vol = concl - intro + 1
    (rem if vol < 35 else warn).append(f'объём от введения (с.{intro}) до заключения (с.{concl}): {vol} с.' + (' – меньше 35!' if vol < 35 else ''))
print('ЗАМЕЧАНИЯ:'); [print('  ' + r) for r in rem] or print('  нет')
print('ПОСМОТРЕТЬ:'); [print('  ' + r) for r in warn] or print('  нет')
print('ВСЕГО замечаний:', len(rem))

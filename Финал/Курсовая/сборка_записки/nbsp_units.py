"""СТП: число и единица измерения не разрываются переносом строки -> неразрывный пробел."""
import io, re, sys
p = sys.argv[1]
s = io.open(p, encoding='utf-8-sig').read().split('\n')
units = r'(?:кбит/с|Мбит/с|мкА|мА·ч|мА|А|мВт|Вт|мВ|В|кГц|МГц|ГГц|Гц|бар|мм|°C|°|кОм|мОм|Ом|дБм|дБ|мкс|мс|с|мин|ч|м|%|КБ|МБ|бит|байт|мкФ|нФ|пФ|g)'
rx = re.compile(r'(\d)[ ](' + units + r')(?=[\s.,;:)»/–-]|$)')
n = 0
out = []
for l in s:
    if l.startswith(('#P ', '#L ', '#N ', '#W ', '#W2 ', '#F ', '#R ')):
        l2, k = rx.subn(lambda m: m.group(1) + '\u00a0' + m.group(2), l); n += k; l = l2
    out.append(l)
io.open(p, 'w', encoding='utf-8-sig').write('\n'.join(out))
print('nbsp inserted:', n)

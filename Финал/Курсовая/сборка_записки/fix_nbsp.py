import io, re, sys
sys.stdout.reconfigure(encoding='utf-8')
units = r'(?:кбит/с|Мбит/с|мкА|мА|А|мВт|Вт|мВ|В|кГц|МГц|ГГц|Гц|бар|мм|°C|°|кОм|мОм|Ом|дБм|дБ|мкс|мс|с|ч|%|КБ|МБ|бит|байт|мкФ|нФ|пФ|g)'
src = io.open('sections_1_3_v6.txt', encoding='utf-8-sig').read() + '\n' + io.open('pz_spec_before_v6.txt', encoding='utf-8-sig').read()
src = src.replace('\u00a0', ' ')
M = '\x01\u00a0\x02'
def solve(line, i):
    pre, post = line[:i], line[i + 3:]
    pre_c = pre.replace(M, '#')
    for k in (40, 30, 22, 16, 11, 7, 4):
        p = pre_c[-k:]
        if '#' in p: p = p[p.rindex('#') + 1:]
        for q in (post[:25].split(M)[0], post[:12].split(M)[0], post[:5].split(M)[0], ''):
            rx = re.escape(p) + r'(\d) (' + units + r')' + re.escape(q) + ('' if q else r'(?=[\s.,;:)»/–-]|$)')
            c = set(re.findall(rx, src))
            if len(c) == 1: return c.pop()
            if len(c) > 1 and k <= 7: return None
    return None
for fn in ('pz_spec.txt', 'pz_spec_kp2.txt'):
    s = io.open(fn, encoding='utf-8-sig').read()
    bad = 0
    while M in s:
        i = s.index(M)
        ls = s.rfind('\n', 0, i) + 1
        le = s.find('\n', i); le = len(s) if le < 0 else le
        line = s[ls:le]
        r = solve(line, i - ls)
        if not r:
            bad += 1; print('UNRESOLVED', fn, repr(line[max(0, i - ls - 50): i - ls + 30])); r = ('?', '??')
        s = s[:i] + r[0] + '\u00a0' + r[1] + s[i + 3:]
    io.open(fn, 'w', encoding='utf-8-sig').write(s)
    print(fn, 'unresolved', bad, 'ctrl left', s.count('\x01') + s.count('\x02'))

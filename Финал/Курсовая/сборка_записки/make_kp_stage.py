"""Make a checkpoint spec (sections up to N) from the full pz_spec.txt: drops later sections and the
conclusion, keeps only cited sources (renumbered in order of first citation) and the listed appendices.
Usage: python make_kp_stage.py pz_spec.txt out.txt 3 А,Б"""
import sys, io, re
sys.stdout.reconfigure(encoding='utf-8')
src, dst, last, apps = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4].split(',')
L = io.open(src, encoding='utf-8-sig').read().split('\n')
cut = next(i for i, l in enumerate(L) if l.startswith(f'#H1 {last + 1}<TAB>'))
isrc = next(i for i, l in enumerate(L) if l.startswith('#H1C Список'))
iapp = next(i for i, l in enumerate(L) if l.startswith('#APP'))
body = L[:cut]
sources = [l for l in L[isrc:iapp] if l.startswith('#PL [')]
appx = L[iapp:]
cite_re = re.compile(r'\[(\d+(?:\s*[–-]\s*\d+)?(?:\s*,\s*\d+(?:\s*[–-]\s*\d+)?)*)\]')
def nums(g):
    out = []
    for part in g.split(','):
        part = part.strip()
        if re.search('[–-]', part):
            a, b = map(int, re.split('[–-]', part)); out += list(range(a, b + 1))
        else:
            out.append(int(part))
    return out
order = []
for l in body:
    for m in cite_re.finditer(l):
        for n in nums(m.group(1)):
            if n not in order: order.append(n)
newnum = {o: i + 1 for i, o in enumerate(order)}
def fmt(ns):
    ns = sorted(set(ns)); out = []; i = 0
    while i < len(ns):
        j = i
        while j + 1 < len(ns) and ns[j + 1] == ns[j] + 1: j += 1
        out.append(f'{ns[i]}–{ns[j]}' if j - i >= 2 else ','.join(map(str, ns[i:j + 1])))
        i = j + 1
    return '[' + ','.join(out) + ']'
body = [cite_re.sub(lambda m: fmt([newnum[n] for n in nums(m.group(1))]), l) for l in body]
bynum = {int(re.match(r'#PL \[(\d+)\]', s).group(1)): s for s in sources}
newsrc = ['#PL [' + str(newnum[o]) + ']' + bynum[o].split(']', 1)[1] for o in order]
keep = []; on = False
for l in appx:
    if l.startswith('#APP'):
        on = l.split(' ', 1)[1].split('|')[0] in apps
    if on: keep.append(l)
res = body + ['#H1C Список использованных источников', '#E'] + newsrc + keep
io.open(dst, 'w', encoding='utf-8-sig').write('\n'.join(res) + '\n')
print('sources kept:', len(newsrc), 'of', len(sources), '; dropped:', sorted(set(bynum) - set(order)))

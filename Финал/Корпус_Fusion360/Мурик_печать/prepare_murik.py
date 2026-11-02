"""Orient STL exported from мурик.f3d for printing and verify the meshes.
base: as modelled (bottom on the bed); lid and fascia: flipped 180 deg about X
so the outer face lies on the bed. Every part is moved so min X/Y/Z = 0."""
import struct, math, pathlib, collections, json, sys

RAW = pathlib.Path(sys.argv[1])
OUT = pathlib.Path(__file__).resolve().parent
PARTS = [('base_full', 'Корпус_основание.stl', False),
         ('lid', 'Корпус_крышка.stl', True),
         ('control_fascia', 'Вставка_кнопки_LED.stl', True)]


def read_stl(path):
    data = path.read_bytes()
    n = struct.unpack_from('<I', data, 80)[0]
    assert len(data) == 84 + n * 50, path
    return [[list(struct.unpack_from('<3f', data, 84 + i * 50 + 12 + j * 12)) for j in range(3)] for i in range(n)]


def write_stl(path, tris, header):
    with path.open('wb') as f:
        f.write(header.encode('ascii').ljust(80, b' ')[:80])
        f.write(struct.pack('<I', len(tris)))
        for a, b, c in tris:
            u = [b[i] - a[i] for i in range(3)]
            v = [c[i] - a[i] for i in range(3)]
            n = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
            norm = math.sqrt(sum(x * x for x in n))
            n = [x / norm for x in n] if norm else [0, 0, 0]
            f.write(struct.pack('<12fH', *(n + a + b + c), 0))


def verify(tris):
    edges = collections.Counter()
    vol = 0.0
    for t in tris:
        keys = [tuple(round(x, 4) for x in v) for v in t]
        for a, b in zip(keys, keys[1:] + keys[:1]):
            edges[(a, b) if a < b else (b, a)] += 1
        a, b, c = t
        vol += (a[0] * (b[1] * c[2] - b[2] * c[1]) + a[1] * (b[2] * c[0] - b[0] * c[2]) + a[2] * (b[0] * c[1] - b[1] * c[0])) / 6
    bad = sum(1 for n in edges.values() if n != 2)
    return bad, vol


report = []
for src, dst, flip in PARTS:
    tris = read_stl(RAW / (src + '.stl'))
    if flip:
        for t in tris:
            for q in t:
                q[1], q[2] = -q[1], -q[2]
    low = [min(q[i] for t in tris for q in t) for i in range(3)]
    for t in tris:
        for q in t:
            for i in range(3):
                q[i] -= low[i]
    dims = [max(q[i] for t in tris for q in t) for i in range(3)]
    bad, vol = verify(tris)
    write_stl(OUT / dst, tris, 'H2 murik print orientation ' + src)
    item = {'file': dst, 'source_body': src, 'triangles': len(tris), 'flipped_about_x': flip,
            'size_mm': [round(x, 2) for x in dims], 'volume_cm3': round(vol / 1000, 3),
            'non_manifold_edges': bad}
    report.append(item)
    print(json.dumps(item, ensure_ascii=False))
(OUT / 'dimensions.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')

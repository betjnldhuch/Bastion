# Собирает свои маркеры (пикапы) для gta3.img: плоская вращающаяся «монетка» с нашим значком
# вместо стандартных пикапов (домики, «i», $, футболка и т.д.).
#   python build_markers.py <папка для результата>
# Значки: ../markers/src/*.png (нарезаны из картинки ChatGPT). Оригинальные файлы GTA не нужны:
# каждый пикап берёт текстуры из своего txd (icons1..icons9 и т.д.), мы пишем эти txd целиком заново.
# Результат кладётся в models/gta3.img (заменить записи с теми же именами) или в pack\models\gta3.img\ лаунчера.
import math, os, struct, sys
from PIL import Image
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
from txdpatch import hdr, native
import mapping_textures
SRC = os.path.join(HERE, '..', 'markers', 'src')
VER = 0x1803FFFF            # RW 3.6.0.3 (GTA SA PC)
TEX = 128                   # размер текстуры
R = 0.45                    # радиус монетки, м
SEG = 40                    # сегментов окружности

# модель SA-MP -> (dff, txd из dynamic.ide, значок)
MODELS = [
    (1241, 'adrenaline',      'icons8',    'enter'),
    (1247, 'bribe',           'icons9',    'exit'),
    (1273, 'property_fsale',  'icons3',    'house_sale'),
    (1272, 'property_locked', 'icons4',    'house_owned'),
    (1277, 'pickupsave',      'icons4',    'house_owned'),   # делит icons4, иначе будет без текстуры
    (1314, 'twoplayer',       'icons4',    'house_owned'),
    (1239, 'info',            'icons6',    'business'),
    (1274, 'bigdollar',       'icons2',    'cash'),
    (1275, 'clothesp',        'icons1',    'job'),
    (1242, 'bodyarmour',      'icons7',    'warehouse'),
    (1212, 'money',           'dyn_cash',  'atm'),
    (1210, 'briefcase',       'breifcase', 'fuel'),
    (1240, 'health',          'icons5',    'hospital'),
    (1254, 'killfrenzy',      'icons',     'school'),
    (1253, 'camerapickup',    'icons',     'school'),        # делит icons
    (1313, 'killfrenzy2plyr', 'icons',     'school'),
]

def ch(t, data): return hdr(t, data, VER)
def rwstr(s):
    b = s.encode() + b'\0'
    return ch(0x02, b.ljust((len(b) + 3) & ~3, b'\0'))

def texture_image(key):
    im = Image.open(os.path.join(SRC, key + '.png')).convert('RGBA')
    bg = Image.new('RGBA', im.size, (17, 17, 17, 255)); bg.alpha_composite(im)   # вне круга — цвет обводки
    return bg.resize((TEX, TEX), Image.LANCZOS)

def disc_geometry(texname):
    k = 0.985
    verts, uvs, nrm, tris = [], [], [], []
    for side, (y, ny, mir) in enumerate(((-0.004, -1.0, 1), (0.004, 1.0, -1))):
        base = len(verts)
        verts.append((0.0, y, 0.0)); uvs.append((0.5, 0.5)); nrm.append((0.0, ny, 0.0))
        for i in range(SEG):
            t = 2 * math.pi * i / SEG; c, s = math.cos(t), math.sin(t)
            verts.append((R * c, y, R * s)); nrm.append((0.0, ny, 0.0))
            uvs.append((0.5 + 0.5 * k * c * mir, 0.5 - 0.5 * k * s))
        for i in range(SEG):
            a, b = base + 1 + i, base + 1 + (i + 1) % SEG
            # лицевая сторона смотрит по нормали (правило правой руки)
            tris.append((base, a, b) if ny < 0 else (base, b, a))
    flags = 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | (1 << 16)   # positions, textured, prelit, normals, light; 1 uv set
    st = struct.pack('<iiii', flags, len(tris), len(verts), 1)
    st += b''.join(struct.pack('<BBBB', 255, 255, 255, 255) for _ in verts)
    st += b''.join(struct.pack('<ff', u, v) for u, v in uvs)
    st += b''.join(struct.pack('<HHHH', b, a, 0, c) for a, b, c in tris)   # v1, v0, mat, v2
    st += struct.pack('<ffffii', 0, 0, 0, R + 0.01, 1, 1)
    st += b''.join(struct.pack('<fff', *v) for v in verts)
    st += b''.join(struct.pack('<fff', *n) for n in nrm)
    tex = ch(0x06, ch(0x01, struct.pack('<HH', 0x1106, 0)) + rwstr(texname) + rwstr('') + ch(0x03, b''))
    mat = ch(0x07, ch(0x01, struct.pack('<i4Biifff', 0, 255, 255, 255, 255, 0, 1, 1.0, 1.0, 1.0)) + tex + ch(0x03, b''))
    matlist = ch(0x08, ch(0x01, struct.pack('<ii', 1, -1)) + mat)
    binmesh = struct.pack('<III', 0, 1, len(tris) * 3) + struct.pack('<II', len(tris) * 3, 0)
    binmesh += b''.join(struct.pack('<III', *t) for t in tris)
    return ch(0x0F, ch(0x01, st) + matlist + ch(0x03, ch(0x50E, binmesh)))

def dff(name, texname):
    frame = struct.pack('<9f3fii', 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, -1, 0)
    framelist = ch(0x0E, ch(0x01, struct.pack('<i', 1) + frame) + ch(0x03, ch(0x253F2FE, name.encode())))
    geolist = ch(0x1A, ch(0x01, struct.pack('<i', 1)) + disc_geometry(texname))
    atomic = ch(0x14, ch(0x01, struct.pack('<iiii', 0, 0, 5, 0)) + ch(0x03, b''))
    return ch(0x10, ch(0x01, struct.pack('<iii', 1, 0, 0)) + framelist + geolist + atomic + ch(0x03, b''))

def txd(textures):
    body = ch(0x01, struct.pack('<HH', len(textures), 0))
    for name, img in textures: body += native(name, '', img, 0x1106, VER)
    return ch(0x16, body + ch(0x03, b''))

def main():
    out = sys.argv[1]; os.makedirs(out, exist_ok=True)
    txds = {}
    for mid, dname, tname, key in MODELS:
        texname = 'bst_' + key
        open(os.path.join(out, dname + '.dff'), 'wb').write(dff(dname, texname))
        txds.setdefault(tname, {})[texname] = key
    for tname, texs in txds.items():
        extra = mapping_textures.textures() if tname == mapping_textures.TXD else []   # текстуры для /mt
        open(os.path.join(out, tname + '.txd'), 'wb').write(txd([(n, texture_image(k)) for n, k in texs.items()] + extra))
    print('готово:', len(MODELS), 'dff,', len(txds), 'txd ->', out)
if __name__ == '__main__': main()

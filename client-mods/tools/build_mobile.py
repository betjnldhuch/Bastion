# Переносит нашу графику из ПК-пака лаунчера в APK телефона.
#   python build_mobile.py            (из любой папки)
# Берёт launcher/client-pack/pack/models (то, что ставит ПК-лаунчер) и пишет
# mobile/launcher/app/src/main/assets/bastion-mods/:
#   tex/<имя>.btx  — наши текстуры: 'BTX1', ширина, высота, размер, zlib(RGBA)
#   img/gta3/*.dff — модели маркеров (лаунчер телефона дописывает их в gta3.img)
#   version.txt    — хэш содержимого: по нему лаунчер понимает, что пора обновить файлы
# На телефоне текстуры GTA лежат в texdb, а не в txd, поэтому клиент подменяет их по имени
# (samp-mobile-patches/cpp/bastion_textures.cpp). Запускать после каждой сборки txd для ПК.
import hashlib, os, shutil, struct, sys, zlib
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import rw, txdpatch
from build_radar import MAP as RADAR_MAP

ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
PACK = os.path.join(ROOT, 'launcher', 'client-pack', 'pack', 'models')
OUT = os.path.join(ROOT, 'mobile', 'launcher', 'app', 'src', 'main', 'assets', 'bastion-mods')

def ours(path):
    """Имена текстур, которые записал txdpatch.native (A8R8G8B8 с мипмапами), — это наши."""
    b = open(path, 'rb').read(); names = []
    for t, s, e in rw.chunks(b, 0, len(b)):
        if t != 0x16: continue
        for ct, cs, ce in rw.chunks(b, s, e):
            if ct != 0x15: continue
            ss = next(x for x in rw.chunks(b, cs, ce) if x[0] == 1)[1]
            name = b[ss + 8:ss + 40].split(b'\0')[0].decode('latin1')
            fmt, d3d = struct.unpack_from('<II', b, ss + 72)
            if fmt == 0x8500 and d3d == 21: names.append(name)
    return names

def btx(im):
    im = im.convert('RGBA'); raw = im.tobytes()
    z = zlib.compress(raw, 9)
    return b'BTX1' + struct.pack('<III', im.width, im.height, len(z)) + z

def main():
    shutil.rmtree(OUT, ignore_errors=True)
    os.makedirs(os.path.join(OUT, 'tex')); os.makedirs(os.path.join(OUT, 'img', 'gta3'))
    radar = {tex.lower() for _, tex, _ in RADAR_MAP}
    textures = {}
    for dirpath, _, files in os.walk(PACK):
        for f in sorted(files):
            p = os.path.join(dirpath, f)
            if f.endswith('.dff'):
                shutil.copy(p, os.path.join(OUT, 'img', 'gta3', f)); continue
            if not f.endswith('.txd'): continue
            names = ours(p)
            if f == 'hud.txd': names = [n for n in names if n.lower() in radar]
            imgs = txdpatch.read(p)
            for n in names:
                key = n.lower()
                if key in textures: continue          # одна и та же картинка в двух txd (24/7)
                textures[key] = imgs[n]
    h = hashlib.sha256()
    for key in sorted(textures):
        data = btx(textures[key])
        open(os.path.join(OUT, 'tex', key + '.btx'), 'wb').write(data)
        h.update(key.encode() + data)
    for f in sorted(os.listdir(os.path.join(OUT, 'img', 'gta3'))):
        h.update(f.encode() + open(os.path.join(OUT, 'img', 'gta3', f), 'rb').read())
    open(os.path.join(OUT, 'version.txt'), 'w').write(h.hexdigest()[:16] + '\n')
    print('готово:', len(textures), 'текстур,', len(os.listdir(os.path.join(OUT, 'img', 'gta3'))), 'dff ->', OUT)

if __name__ == '__main__': main()

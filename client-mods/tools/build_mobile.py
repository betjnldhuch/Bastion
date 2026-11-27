# Переносит нашу графику из ПК-пака лаунчера в APK телефона.
#   python build_mobile.py            (из любой папки)
# Берёт launcher/client-pack/pack/models (то, что ставит ПК-лаунчер) и пишет
# mobile/launcher/app/src/main/assets/bastion-mods/:
#   tex/<имя>.btx  — наши текстуры (и значки оружия из ../weapons/src): 'BTX1', ширина, высота, размер, zlib(RGBA)
#   img/gta3/*.dff — модели маркеров (лаунчер телефона дописывает их в gta3.img)
#   version.txt    — хэш содержимого: по нему лаунчер понимает, что пора обновить файлы
# На телефоне текстуры GTA лежат в texdb, а не в txd, поэтому клиент подменяет их по имени
# (samp-mobile-patches/cpp/bastion_textures.cpp). Запускать после каждой сборки txd для ПК.
import hashlib, os, shutil, struct, sys, zlib
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import rw, txdpatch
from PIL import Image
from build_radar import MAP as RADAR_MAP, HUD_ART
from build_weapons import WEAPONS, SRC as WEAPON_SRC
from transport_textures import mobile_transport

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
            if (fmt == 0x8500 and d3d == 21) or name.startswith('bst_v_'):
                names.append(name)
    return names

# The client retains override textures in memory. Cap character atlases on Android.
SKIN_TEXTURES = frozenset((
    "lapd1 sfpd1 lvpd1 csher dsher tenpenny fbi wmomib bmymib "
    "army wmysgrd laemt lvemt sfemt wmosci fam1 fam2 fam3 "
    "sweet ballas1 ballas2 ballas3 vla1 vla2 vla3 omyst ofyst "
    "male01 wmybmx wmydrug hmyri swmyhp1 bfyri hfyri wfyst "
).split())

def btx(im, key=None):
    im = im.convert('RGBA')
    if (key in SKIN_TEXTURES or (key or '').startswith('bst_v_')) and max(im.size) > 512:
        im.thumbnail((512, 512), Image.Resampling.LANCZOS)
    raw = im.tobytes()
    z = zlib.compress(raw, 9)
    return b'BTX1' + struct.pack('<III', im.width, im.height, len(z)) + z

def main():
    shutil.rmtree(OUT, ignore_errors=True)
    os.makedirs(os.path.join(OUT, 'tex')); os.makedirs(os.path.join(OUT, 'img', 'gta3'))
    radar = {tex.lower() for _, tex, _ in RADAR_MAP} | {tex.lower() for tex, _ in HUD_ART}
    textures = {}
    for dirpath, _, files in os.walk(PACK):
        for f in sorted(files):
            p = os.path.join(dirpath, f)
            if f.endswith('.dff'):
                shutil.copy(p, os.path.join(OUT, 'img', 'gta3', f)); continue
            if not f.endswith('.txd'): continue
            names = ours(p)
            if f == 'hud.txd': names = [n for n in names if n.lower() in radar]
            if not names: continue
            imgs = txdpatch.read(p)
            for n in names:
                key = n.lower()
                if key in textures: continue          # одна и та же картинка в двух txd (24/7)
                textures[key] = imgs[n]
    # значки оружия: на ПК лаунчер вписывает их в txd оружия (build_weapons.py), здесь — по имени текстуры
    for model in WEAPONS:
        textures.setdefault((model + 'icon').lower(), Image.open(os.path.join(WEAPON_SRC, model + '.png')).convert('RGBA'))
    for key, image in mobile_transport(PACK):
        if key in textures:
            raise ValueError('Vehicle texture name is already in use: ' + key)
        textures[key] = image
    vehicle_models = os.path.join(ROOT, 'client-mods', 'transport', 'android-models')
    if os.path.isdir(vehicle_models):
        for filename in sorted(os.listdir(vehicle_models)):
            if filename.endswith('.dff'):
                shutil.copyfile(os.path.join(vehicle_models, filename),
                                os.path.join(OUT, 'img', 'gta3', filename))
    h = hashlib.sha256()
    for key in sorted(textures):
        data = btx(textures[key], key)
        open(os.path.join(OUT, 'tex', key + '.btx'), 'wb').write(data)
        h.update(key.encode() + data)
    for f in sorted(os.listdir(os.path.join(OUT, 'img', 'gta3'))):
        h.update(f.encode() + open(os.path.join(OUT, 'img', 'gta3', f), 'rb').read())
    open(os.path.join(OUT, 'version.txt'), 'w').write(h.hexdigest()[:16] + '\n')
    print('готово:', len(textures), 'текстур,', len(os.listdir(os.path.join(OUT, 'img', 'gta3'))), 'dff ->', OUT)

if __name__ == '__main__': main()

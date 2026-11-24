# Мини-карта и карта в меню паузы в стиле Bastion (ПК).
# Карта в игре собрана из 144 тайлов radar00..radar143.txd (12x12, по 128 px) в models/gta3.img.
# Геометрию берём из оригинальных тайлов игрока, а цвета переносим с образца, который ChatGPT
# перекрасил (../map/src/style_sample.png = кусок SAMPLE_BOX оригинальной карты). Дороги, берега и
# кварталы остаются на своих местах, поэтому метки и GPS не съезжают.
# Рамка, «N» и стрелка игрока — в hud.txd (build_radar.py, HUD_ART).
#   python build_map.py <папка с оригинальными radarNN.txd>
# Пишет launcher/client-pack/pack/models/gta3.img/radarNN.txd и ../map/preview.png.
# Потом: build_mobile.py (телефон) и make-client-pack.ps1 (client-pack.json).
import os, sys
import numpy as np
from PIL import Image
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import rw, txdpatch
SRC = os.path.join(HERE, '..', 'map', 'src')
SAMPLE_BOX = (880, 960, 1392, 1472)   # кусок карты 1536x1536, который отдавали в ChatGPT
DESERT = (156, 133, 115)              # пустыня в оригинале; в образце её нет, цвет из промпта
DESERT_TO = np.array([58, 51, 38])

def is_road(a): return a.max(-1) < 75

def load_tiles(d):
    full = Image.new('RGB', (1536, 1536)); names = []
    for i in range(144):
        b = open(os.path.join(d, 'radar%02d.txd' % i), 'rb').read()
        name, im = next(iter(rw.txd_images(b).items()))
        names.append(name)
        full.paste(im.convert('RGB').resize((128, 128)), ((i % 12) * 128, (i // 12) * 128))
    return full, names

def restyle(full):
    O = np.asarray(full).astype(np.int32)
    x0, y0, x1, y1 = SAMPLE_BOX
    oc = O[y0:y1, x0:x1]
    G = np.asarray(Image.open(os.path.join(SRC, 'style_sample.png')).convert('RGB')
                   .resize((x1 - x0, y1 - y0), Image.LANCZOS)).astype(np.int32)
    Q = 16
    key = lambda a: (a[..., 0] // Q) * 256 + (a[..., 1] // Q) * 16 + (a[..., 2] // Q)
    cen = lambda k: np.stack([(k // 256) * Q + 8, (k // 16 % 16) * Q + 8, (k % 16) * Q + 8], 1)
    rm = is_road(oc)
    k, g = key(oc)[~rm], G[~rm]
    keys, vals = [], []
    for kk in np.unique(k):
        sel = g[k == kk]
        if len(sel) >= 20: keys.append(kk); vals.append(np.median(sel, 0))
    keys, vals = np.array(keys), np.array(vals)
    road_c = np.median(G[rm & (G.min(-1) > 120)], 0)
    fk = key(O)
    uk, inv = np.unique(fk, return_inverse=True)
    near = ((cen(uk)[:, None, :] - cen(keys)[None]) ** 2).sum(-1).argmin(1)
    out = vals[near][inv.reshape(fk.shape)]
    R = is_road(O)
    des = (np.abs(O - np.array(DESERT)).max(-1) < 22) & ~R
    out[des] = (DESERT_TO * (O.mean(-1, keepdims=True) / np.mean(DESERT)))[des]
    out[R] = road_c
    return Image.fromarray(out.clip(0, 255).astype(np.uint8))

PACK = os.path.join(HERE, '..', '..', 'launcher', 'client-pack', 'pack', 'models', 'gta3.img')

def main():
    tiles_dir = sys.argv[1]
    full, names = load_tiles(tiles_dir)
    styled = restyle(full)
    styled.resize((768, 768), Image.LANCZOS).save(os.path.join(SRC, '..', 'preview.png'))
    for i in range(144):
        x, y = (i % 12) * 128, (i // 12) * 128
        txdpatch.patch(os.path.join(tiles_dir, 'radar%02d.txd' % i),
                       os.path.join(PACK, 'radar%02d.txd' % i), {names[i]: styled.crop((x, y, x + 128, y + 128))})
    print('готово: 144 тайла в', os.path.normpath(PACK))

if __name__ == '__main__': main()

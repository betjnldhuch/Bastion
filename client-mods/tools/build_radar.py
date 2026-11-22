# Собирает hud.txd с нашими значками фракций на радаре и карте.
# Берёт оригинальный models/hud.txd игрока и заменяет в нём иконки на ../radar/src/*.png.
#   python build_radar.py <оригинальный hud.txd> <папка для результата>
# Оригинальные файлы GTA в репозиторий не кладём.
import os, sys
from PIL import Image
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import rw, txdpatch
S = os.path.join(HERE, '..', 'radar', 'src')
# номер значка SA-MP (SetPlayerMapIcon) -> текстура в hud.txd -> наша картинка
MAP = [
    (62, 'radar_gangG', 'grove'), (59, 'radar_gangP', 'ballas'), (60, 'radar_gangY', 'vagos'),
    (58, 'radar_gangB', 'aztecas'), (30, 'radar_police', 'lspd'), (22, 'radar_hostpital', 'hospital'),
    (61, 'radar_gangN', 'fbi'), (40, 'radar_THETRUTH', 'army'), (8, 'radar_BIGSMOKE', 'cityhall'),
    (23, 'radar_LOCO', 'news'),
]
def main():
    src, out_dir = sys.argv[1], sys.argv[2]
    names = [t[0] for t in rw.txd_list(open(src, 'rb').read())]
    real = {n.lower(): n for n in names}
    repl = {}
    for icon, tex, key in MAP:
        n = real.get(tex.lower())
        if not n: sys.exit('в hud.txd нет текстуры ' + tex)
        repl[n] = Image.open(os.path.join(S, key + '.png')).convert('RGBA')
    os.makedirs(out_dir, exist_ok=True)
    txdpatch.patch(src, os.path.join(out_dir, 'hud.txd'), repl)
    print('готово:', os.path.join(out_dir, 'hud.txd'))
if __name__ == '__main__': main()

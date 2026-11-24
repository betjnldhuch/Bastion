# Значки оружия в HUD (ПК): готовит заплатки для txd оружия в gta3.img.
#   python build_weapons.py            (из любой папки)
# Оригинальных txd оружия в репозитории нет, поэтому целиком их не подменяем:
# пишем одну текстуру <модель>icon (A8R8G8B8 с мипмапами, как txdpatch.native) в
#   launcher/client-pack/pack/models/gta3.img/<модель>.txd/<модель>icon.tex
# Лаунчер ПК находит эту текстуру в txd игрока и меняет только её (ClientPack.PatchTxd),
# модель и её текстуры остаются родными. Кулак — текстура fist в hud.txd (build_radar.py).
# На телефоне значки идут в bastion-mods/tex как <модель>icon.btx (build_mobile.py).
import os, shutil, sys
from PIL import Image
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import txdpatch
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
SRC = os.path.join(ROOT, 'client-mods', 'weapons', 'src')
OUT = os.path.join(ROOT, 'launcher', 'client-pack', 'pack', 'models', 'gta3.img')
VER = 0x1803FFFF   # RenderWare 3.6 (GTA SA PC)
# модели оружия (weapon.ide): имя txd = имя модели, значок = <модель>icon
WEAPONS = [
    'brassknuckle', 'golfclub', 'nitestick', 'knifecur', 'bat', 'shovel', 'poolcue', 'katana', 'chnsaw',
    'flowera', 'gun_cane', 'grenade', 'teargas', 'molotov', 'bomb',
    'colt45', 'silenced', 'desert_eagle', 'chromegun', 'sawnoff', 'shotgspa', 'micro_uzi', 'mp5lng',
    'tec9', 'ak47', 'm4', 'cuntgun', 'sniper', 'rocketla', 'heatseek', 'flame',
    'minigun', 'satchel', 'spraycan', 'fire_ex', 'camera', 'nvgoggles', 'irgoggles', 'gun_para',
    'gun_dildo1', 'gun_dildo2', 'gun_vibe1', 'gun_vibe2',
]
def main():
    for model in WEAPONS:
        d = os.path.join(OUT, model + '.txd')
        shutil.rmtree(d, ignore_errors=True); os.makedirs(d)
        im = Image.open(os.path.join(SRC, model + '.png')).convert('RGBA')
        # filter 0x1106: линейная фильтрация с мипмапами, без повтора краёв
        open(os.path.join(d, model + 'icon.tex'), 'wb').write(txdpatch.native(model + 'icon', '', im, 0x1106, VER))
    print('готово:', len(WEAPONS), 'значков ->', OUT)
if __name__ == '__main__': main()

# Наши текстуры для маппинга: сервер натягивает их на объекты через SetObjectMaterial (/mt).
# build_markers.py дописывает их в txd пикапа «больница» (модель 1240, icons5.txd), который мы
# и так пишем целиком сами. На сервере: SetObjectMaterial(obj, слот, 1240, "icons5", "bst_...", 0).
# Телефон берёт эти же текстуры по имени (build_mobile.py -> .btx), txd там не важен.
# Размеры урезаны до 256/512, чтобы txd не съедал память игры.
import os
from PIL import Image
SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
MODEL, TXD = 1240, 'icons5'

# текстура в игре, исходник (папка/файл), размер
TEXTURES = [
    ('bst_hosp_wall',      'hospital/hospital_wall_green',   (256, 256)),
    ('bst_vault_door',     'bank/bank_vault_door',           (512, 512)),
    ('bst_bank_counter',   'bank/bank_teller_counter',       (256, 256)),
    ('bst_marble_beige',   'bank/bank_marble_beige',         (256, 256)),
    ('bst_marble_dark',    'bank/bank_marble_dark',          (256, 256)),
    ('bst_shop_shelves',   'shop/shop_grocery_shelves',      (512, 512)),
    ('bst_shop_fridge',    'shop/shop_drinks_fridge',        (512, 512)),
    ('bst_shop_floor',     'shop/shop_floor_grey_tile',      (256, 256)),
    ('bst_shop_sign',      'shop/shop_sign_247',             (512, 256)),
    ('bst_bar_shelves',    'bar/bar_back_shelves',           (512, 512)),
    ('bst_bar_counter',    'bar/bar_counter_wood',           (256, 256)),
    ('bst_dealer_sign',    'dealer/dealer_sign',             (512, 256)),
    ('bst_ballas_tag',     'gangs/ballas_graffiti',          (512, 256)),
    ('bst_grove_flag',     'gangs/grove_flag',               (256, 512)),
    ('bst_yellow_brick',   'gangs/yellow_brick_worn',        (256, 256)),
    ('bst_asphalt',        'common/asphalt',                 (256, 256)),
    ('bst_brick_red',      'common/brick_red',               (256, 256)),
    ('bst_carpet_blue',    'common/carpet_blue',             (256, 256)),
    ('bst_concrete',       'common/concrete_tiles',          (256, 256)),
    ('bst_corrugated',     'common/corrugated_metal',        (256, 256)),
    ('bst_garage_floor',   'common/garage_concrete_tyres',   (256, 256)),
    ('bst_parquet',        'common/parquet',                 (256, 256)),
    ('bst_plaster',        'common/plaster_beige',           (256, 256)),
    ('bst_leather',        'lspd/bpd4_black_leather',        (256, 256)),
    ('bst_fabric_blue',    'lspd/bpd4_blue_fabric',          (256, 256)),
    ('bst_planks',         'lspd/bpd4_wood_planks',          (256, 256)),
    ('bst_height_chart',   'lspd/bpd4_height_chart',         (256, 512)),
    ('bst_sign_cells',     'lspd/bpd4_sign_cells',           (512, 256)),
    ('bst_sign_interrog',  'lspd/bpd4_sign_interrogation',   (512, 256)),
    ('bst_sign_lockers',   'lspd/bpd4_sign_lockers',         (512, 256)),
    ('bst_sign_armory',    'lspd/bpd4_sign_armory',          (512, 256)),
]

def image(src, size):
    d, f = src.split('/')
    im = Image.open(os.path.join(SRC, d, 'src', f + '.png')).convert('RGBA')
    w, h = size
    # вывески 3:1 -> 2:1 без искажения: добиваем сверху и снизу цветом края
    if abs(im.width / im.height - w / h) > 0.2:
        nw, nh = im.width, round(im.width * h / w)
        if nh < im.height: nw, nh = round(im.height * w / h), im.height
        bg = Image.new('RGBA', (nw, nh), im.getpixel((0, 0)))
        bg.alpha_composite(im, ((nw - im.width) // 2, (nh - im.height) // 2)); im = bg
    return im.resize(size, Image.LANCZOS)

def textures():
    return [(name, image(src, size)) for name, src, size in TEXTURES]

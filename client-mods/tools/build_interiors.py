# Собирает txd интерьеров банка, бара Ten Green Bottles и 24/7 из оригиналов gta_int.img
# и наших картинок из ../bank/src, ../bar/src, ../shop/src.
#   python build_interiors.py <папка с оригинальными txd> <папка для результата>
# Нужны оригиналы: papaerchaseoffice.txd, cj_bar2.txd, genintint711_1.txd, 711c.txd,
# cj_ss_1.txd, cj_ss_2.txd. Оригинальные файлы GTA в репозиторий не кладём.
import os, sys
from PIL import Image
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import txdpatch
ROOT = os.path.join(HERE, '..')
L = lambda d, n: Image.open(os.path.join(ROOT, d, 'src', n)).convert('RGB')
src_dir, out_dir = sys.argv[1], sys.argv[2]
os.makedirs(out_dir, exist_ok=True)
sq = lambda im, s: im.resize((s, s), Image.LANCZOS)
def P(name, repl):
    txdpatch.patch(os.path.join(src_dir, name), os.path.join(out_dir, name), repl)

# Банк (интерьер «Планировочного отдела», int 3): пол — мрамор, деревянные стойки — панели стойки кассира.
teller = L('bank', 'bank_teller_counter.png')
P('papaerchaseoffice.txd', {
    'gun_floor1': sq(L('bank', 'bank_marble_beige.png'), 512),
    'ab_mottleGrey': sq(L('bank', 'bank_marble_dark.png'), 512),
    'cof_wood2': sq(teller.crop((0, 560, 512, 1000)), 256),
})

# Бар Ten Green Bottles (модель smllbarinterior): GB_nastybar01 — фасад L-образной стойки.
P('cj_bar2.txd', {'GB_nastybar01': sq(L('bar', 'bar_counter_wood.png'), 256)})

# 24/7: плитка пола и фирменная полоса «24-7» на окнах, стенах и полках.
sign = L('shop', 'shop_sign_247.png')
def band(orig):
    o = orig.convert('RGB').resize((512, 256), Image.LANCZOS)
    o.paste(Image.new('RGB', (512, 140), (232, 232, 228)), (0, 58))
    o.paste(sign.resize((360, 120), Image.LANCZOS), (76, 68))
    return o
def edge():
    e = Image.new('RGB', (128, 32), (180, 20, 25))
    e.paste(sign.resize((96, 32), Image.LANCZOS), (16, 0))
    return e
tile = sq(L('shop', 'shop_floor_grey_tile.png'), 256)
for n in ('genintint711_1.txd', '711c.txd'):
    o = txdpatch.read(os.path.join(src_dir, n))
    P(n, {'CJ_7_11_TILE': tile, 'CJ_7_11_win': band(o['CJ_7_11_win'])})
for n in ('cj_ss_1.txd', 'cj_ss_2.txd'):
    P(n, {'CJ_7_11_edge': edge()})
print('ok')

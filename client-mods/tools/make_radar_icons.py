# Упрощённые значки фракций для радара/карты (64x64) из эмблем UI.
# Форма щита берётся из эмблемы, внутри — цвет фракции и простой знак, чтобы читалось в 16–20 px.
# python client-mods/tools/make_radar_icons.py
import os, math
from PIL import Image, ImageDraw, ImageFont, ImageFilter
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
EMB = os.path.join(R, 'mobile/launcher/app/src/main/assets/ui/emblems')
OUT = os.path.join(R, 'client-mods/radar/src')
FONT = os.path.join(R, 'launcher/fonts/RussoOne-Regular.ttf')
GOLD = (232, 186, 92)
ICONS = {  # ключ: (цвет щита, знак)
    'grove': ((24, 120, 40), 'G'), 'ballas': ((120, 40, 170), 'B'), 'vagos': ((210, 170, 20), 'V'),
    'aztecas': ((0, 150, 190), 'A'), 'lspd': ((30, 70, 170), '*'), 'hospital': ((235, 235, 235), '+'),
    'fbi': ((20, 30, 60), 'FBI'), 'army': ((85, 110, 35), '*'), 'cityhall': ((40, 50, 80), 'col'),
    'news': ((180, 30, 30), 'N'),
}
S = 256
def star(d, cx, cy, r, fill):
    pts = [(cx + (r if i % 2 == 0 else r * .42) * math.sin(i * math.pi / 5), cy - (r if i % 2 == 0 else r * .42) * math.cos(i * math.pi / 5)) for i in range(10)]
    d.polygon(pts, fill=fill)
def main():
    os.makedirs(OUT, exist_ok=True)
    for k, (col, sym) in ICONS.items():
        a = Image.open(os.path.join(EMB, k + '.png')).convert('RGBA').getchannel('A').resize((S, S), Image.LANCZOS)
        a = a.point(lambda v: 255 if v > 128 else 0).filter(ImageFilter.MaxFilter(5))
        inner = a.filter(ImageFilter.MinFilter(33))
        img = Image.new('RGBA', (S, S), (0, 0, 0, 0))
        img.paste(Image.new('RGBA', (S, S), (30, 20, 10, 255)), (0, 0), a)                   # тёмный кант
        img.paste(Image.new('RGBA', (S, S), GOLD + (255,)), (0, 0), a.filter(ImageFilter.MinFilter(9)))  # золотая рамка
        img.paste(Image.new('RGBA', (S, S), col + (255,)), (0, 0), inner)                   # поле цвета фракции
        d = ImageDraw.Draw(img)
        bb = inner.getbbox(); cx = (bb[0] + bb[2]) / 2; cy = (bb[1] + bb[3]) / 2 + 4
        fg = (200, 20, 30) if k == 'hospital' else (255, 236, 170)
        if sym == '*': star(d, cx, cy, 70, fg)
        elif sym == '+': d.rectangle((cx - 20, cy - 66, cx + 20, cy + 66), fill=fg); d.rectangle((cx - 66, cy - 20, cx + 66, cy + 20), fill=fg)
        elif sym == 'col':
            d.polygon([(cx - 78, cy - 34), (cx, cy - 78), (cx + 78, cy - 34)], fill=fg)
            for i in range(4): d.rectangle((cx - 64 + i * 38, cy - 26, cx - 44 + i * 38, cy + 46), fill=fg)
            d.rectangle((cx - 80, cy + 52, cx + 80, cy + 68), fill=fg)
        else:
            f = ImageFont.truetype(FONT, 150 if len(sym) == 1 else 82)
            d.text((cx, cy), sym, font=f, fill=fg, anchor='mm', stroke_width=6, stroke_fill=(20, 12, 6))
        img.resize((64, 64), Image.LANCZOS).save(os.path.join(OUT, k + '.png'), optimize=True)
        print(k)
if __name__ == '__main__': main()

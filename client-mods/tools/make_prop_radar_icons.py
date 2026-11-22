# Упрощённые значки имущества для радара/карты (64x64) из значков UI (ui/property).
# Как make_job_radar_icons.py: форма щита из значка, внутри цвет поля и простой знак, чтобы читалось в 16–20 px.
# python client-mods/tools/make_prop_radar_icons.py
import os
from PIL import Image, ImageDraw, ImageFilter
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
SRC = os.path.join(R, 'mobile/launcher/app/src/main/assets/ui/property')
OUT = os.path.join(R, 'client-mods/radar/src')
GOLD = (232, 186, 92)
FG = (255, 236, 170)
DARK = (20, 12, 6)
FIELD = {
    'house': (25, 100, 40), 'apartment': (20, 90, 150), 'mansion': (190, 140, 25), 'garage': (90, 92, 98),
    'car': (25, 55, 150), 'boat': (10, 120, 125), 'heli': (80, 82, 88),
}
S = 256


def rr(d, box, r, fill):
    d.rounded_rectangle(box, r, fill=fill)


def glyph(d, k, cx, cy):
    f, bg = FG, FIELD[k]
    if k == 'house':  # домик с крышей
        d.polygon([(cx - 82, cy - 6), (cx, cy - 80), (cx + 82, cy - 6)], fill=f)
        d.rectangle((cx - 58, cy - 12, cx + 58, cy + 66), fill=f)
        d.rectangle((cx - 16, cy + 18, cx + 16, cy + 66), fill=bg)
    elif k == 'apartment':  # многоэтажка с окнами
        d.rectangle((cx - 48, cy - 84, cx + 48, cy + 74), fill=f)
        for y in range(-70, 40, 30):
            for x in (-34, 8): d.rectangle((cx + x, cy + y, cx + x + 26, cy + y + 18), fill=bg)
        d.rectangle((cx - 14, cy + 46, cx + 14, cy + 74), fill=bg)
    elif k == 'mansion':  # фронтон и колонны, тёмные на золоте
        f = DARK
        d.polygon([(cx - 86, cy - 34), (cx, cy - 84), (cx + 86, cy - 34)], fill=f)
        d.rectangle((cx - 86, cy + 56, cx + 86, cy + 74), fill=f)
        for x in (-70, -34, 2, 38): d.rectangle((cx + x, cy - 28, cx + x + 30, cy + 52), fill=f)
        for x in (-50, -14, 22, 58): d.rectangle((cx + x - 4, cy - 28, cx + x + 4, cy + 52), fill=bg)
    elif k == 'garage':  # гараж с воротами
        d.polygon([(cx - 86, cy - 20), (cx, cy - 78), (cx + 86, cy - 20)], fill=f)
        d.rectangle((cx - 70, cy - 22, cx + 70, cy + 70), fill=f)
        d.rectangle((cx - 48, cy, cx + 48, cy + 70), fill=bg)
        for y in range(8, 66, 16): d.rectangle((cx - 44, cy + y, cx + 44, cy + y + 8), fill=f)
    elif k == 'car':  # легковая машина
        d.polygon([(cx - 50, cy - 6), (cx - 30, cy - 50), (cx + 34, cy - 50), (cx + 56, cy - 6)], fill=f)
        rr(d, (cx - 84, cy - 12, cx + 84, cy + 32), 14, f)
        d.polygon([(cx - 36, cy - 10), (cx - 22, cy - 40), (cx - 4, cy - 40), (cx - 4, cy - 10)], fill=bg)
        d.polygon([(cx + 6, cy - 10), (cx + 6, cy - 40), (cx + 28, cy - 40), (cx + 42, cy - 10)], fill=bg)
        for x in (cx - 48, cx + 48):
            d.ellipse((x - 22, cy + 10, x + 22, cy + 54), fill=bg); d.ellipse((x - 15, cy + 17, x + 15, cy + 47), fill=f)
    elif k == 'boat':  # катер на волне
        d.polygon([(cx - 86, cy - 4), (cx + 88, cy - 4), (cx + 56, cy + 36), (cx - 70, cy + 36)], fill=f)
        d.polygon([(cx - 40, cy - 6), (cx - 20, cy - 50), (cx + 36, cy - 50), (cx + 44, cy - 6)], fill=f)
        d.rectangle((cx - 12, cy - 40, cx + 26, cy - 18), fill=bg)
        for x in (-80, -26, 28):
            d.arc((cx + x, cy + 44, cx + x + 54, cy + 74), 200, 340, fill=f, width=10)
    elif k == 'heli':  # вертолёт
        d.rectangle((cx - 86, cy - 72, cx + 86, cy - 62), fill=f)
        d.rectangle((cx - 6, cy - 68, cx + 6, cy - 40), fill=f)
        d.ellipse((cx - 70, cy - 46, cx + 26, cy + 30), fill=f)
        d.polygon([(cx + 10, cy - 20), (cx + 78, cy - 10), (cx + 78, cy + 2), (cx + 10, cy + 10)], fill=f)
        d.rectangle((cx + 70, cy - 32, cx + 84, cy + 4), fill=f)
        d.ellipse((cx - 56, cy - 34, cx - 14, cy - 2), fill=bg)
        for x in (-46, 4): d.rectangle((cx + x - 4, cy + 26, cx + x + 4, cy + 46), fill=f)
        rr(d, (cx - 74, cy + 44, cx + 32, cy + 56), 6, f)


def main():
    os.makedirs(OUT, exist_ok=True)
    for k, col in FIELD.items():
        a = Image.open(os.path.join(SRC, k + '.png')).convert('RGBA').getchannel('A').resize((S, S), Image.LANCZOS)
        a = a.point(lambda v: 255 if v > 128 else 0).filter(ImageFilter.MaxFilter(5))
        inner = a.filter(ImageFilter.MinFilter(33))
        img = Image.new('RGBA', (S, S), (0, 0, 0, 0))
        img.paste(Image.new('RGBA', (S, S), (30, 20, 10, 255)), (0, 0), a)                            # тёмный кант
        img.paste(Image.new('RGBA', (S, S), GOLD + (255,)), (0, 0), a.filter(ImageFilter.MinFilter(9)))  # золотая рамка
        img.paste(Image.new('RGBA', (S, S), col + (255,)), (0, 0), inner)                            # поле
        d = ImageDraw.Draw(img)
        bb = inner.getbbox(); cx = (bb[0] + bb[2]) / 2; cy = (bb[1] + bb[3]) / 2 + 4
        glyph(d, k, cx, cy)
        img.resize((64, 64), Image.LANCZOS).save(os.path.join(OUT, 'prop_' + k + '.png'), optimize=True)
        print(k)


if __name__ == '__main__': main()

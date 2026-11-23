# Упрощённые значки работ для радара/карты (64x64) из значков UI (ui/jobs).
# Как make_biz_radar_icons.py: форма щита из значка, внутри цвет поля и простой знак, чтобы читалось в 16–20 px.
# python client-mods/tools/make_job_radar_icons.py
import os, math
from PIL import Image, ImageDraw, ImageFont, ImageFilter
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
SRC = os.path.join(R, 'mobile/launcher/app/src/main/assets/ui/jobs')
OUT = os.path.join(R, 'client-mods/radar/src')
FONT = os.path.join(R, 'launcher/fonts/RussoOne-Regular.ttf')
GOLD = (232, 186, 92)
FG = (255, 236, 170)
DARK = (20, 12, 6)
FIELD = {
    'taxi': (215, 165, 10), 'trucker': (25, 55, 150), 'mechanic': (95, 98, 104), 'fisher': (10, 95, 165),
    'loader': (200, 90, 15), 'collector': (20, 105, 40), 'farmer': (110, 120, 20), 'garbage': (25, 90, 50),
    'bus': (225, 105, 15), 'pizza': (170, 25, 20),
}
S = 256


def rr(d, box, r, fill):
    d.rounded_rectangle(box, r, fill=fill)


def wheel(d, x, y, r, f):
    d.ellipse((x - r, y - r, x + r, y + r), fill=f, outline=DARK, width=6)


def glyph(d, k, cx, cy):
    f = FG
    if k == 'taxi':  # машина с шашкой, тёмная на жёлтом
        f = DARK
        rr(d, (cx - 22, cy - 70, cx + 22, cy - 50), 4, f)
        d.polygon([(cx - 46, cy - 6), (cx - 28, cy - 44), (cx + 28, cy - 44), (cx + 46, cy - 6)], fill=f)
        rr(d, (cx - 74, cy - 12, cx + 74, cy + 30), 14, f)
        for x in (cx - 44, cx + 44): d.ellipse((x - 18, cy + 14, x + 18, cy + 50), fill=f)
    elif k == 'trucker':  # тягач с прицепом
        rr(d, (cx - 80, cy - 52, cx + 18, cy + 22), 6, f)
        rr(d, (cx + 24, cy - 26, cx + 78, cy + 22), 10, f)
        d.rectangle((cx + 46, cy - 18, cx + 68, cy - 2), fill=FIELD[k])
        for x in (cx - 58, cx - 22, cx + 54): wheel(d, x, cy + 32, 17, f)
    elif k == 'mechanic':  # гаечный ключ
        d.line([(cx - 44, cy + 44), (cx + 30, cy - 30)], fill=f, width=30)
        d.ellipse((cx + 6, cy - 78, cx + 78, cy - 6), fill=f)
        d.polygon([(cx + 42, cy - 42), (cx + 90, cy - 90), (cx + 90, cy - 42)], fill=FIELD[k])
        d.ellipse((cx - 72, cy + 16, cx - 16, cy + 72), fill=f)
        d.ellipse((cx - 54, cy + 34, cx - 34, cy + 54), fill=FIELD[k])
    elif k == 'fisher':  # рыба
        d.ellipse((cx - 64, cy - 36, cx + 44, cy + 36), fill=f)
        d.polygon([(cx + 34, cy), (cx + 76, cy - 40), (cx + 76, cy + 40)], fill=f)
        d.ellipse((cx - 44, cy - 14, cx - 28, cy + 2), fill=FIELD[k])
    elif k == 'loader':  # ящики
        rr(d, (cx - 62, cy - 4, cx + 6, cy + 62), 6, f)
        rr(d, (cx + 2, cy - 4, cx + 70, cy + 62), 6, f)
        rr(d, (cx - 30, cy - 70, cx + 38, cy - 6), 6, f)
        for x0, y0 in ((cx - 62, cy - 4), (cx + 2, cy - 4), (cx - 30, cy - 70)):
            d.rectangle((x0 + 28, y0, x0 + 40, y0 + 22), fill=FIELD[k])
    elif k == 'collector':  # мешок с $
        d.ellipse((cx - 62, cy - 30, cx + 62, cy + 70), fill=f)
        d.polygon([(cx - 24, cy - 70), (cx + 24, cy - 70), (cx + 14, cy - 26), (cx - 14, cy - 26)], fill=f)
        d.line([(cx - 22, cy - 34), (cx + 22, cy - 34)], fill=FIELD[k], width=8)
        d.text((cx, cy + 22), '$', font=ImageFont.truetype(FONT, 100), fill=FIELD[k], anchor='mm')
    elif k == 'farmer':  # колос
        d.line([(cx, cy + 74), (cx, cy - 40)], fill=f, width=12)
        for i in range(4):
            y = cy - 56 + i * 28
            d.ellipse((cx - 44, y - 12, cx - 4, y + 16), fill=f)
            d.ellipse((cx + 4, y - 12, cx + 44, y + 16), fill=f)
        d.ellipse((cx - 14, cy - 86, cx + 14, cy - 40), fill=f)
    elif k == 'garbage':  # мусорный бак
        rr(d, (cx - 66, cy - 58, cx + 66, cy - 38), 6, f)
        rr(d, (cx - 18, cy - 74, cx + 18, cy - 54), 4, f)
        d.polygon([(cx - 56, cy - 30), (cx + 56, cy - 30), (cx + 44, cy + 70), (cx - 44, cy + 70)], fill=f)
        for x in (cx - 24, cx, cx + 24): d.line([(x, cy - 16), (x, cy + 56)], fill=FIELD[k], width=9)
    elif k == 'bus':  # автобус сбоку, тёмный на жёлтом
        f = DARK
        rr(d, (cx - 84, cy - 50, cx + 84, cy + 30), 14, f)
        for x in range(-70, 60, 34): d.rectangle((cx + x, cy - 38, cx + x + 26, cy - 10), fill=FIELD[k])
        d.rectangle((cx + 62, cy - 38, cx + 74, cy + 18), fill=FIELD[k])
        for x in (cx - 50, cx + 46): wheel(d, x, cy + 32, 17, FIELD[k]); d.ellipse((x - 12, cy + 20, x + 12, cy + 44), fill=f)
    elif k == 'pizza':  # кусок пиццы
        d.polygon([(cx - 70, cy - 56), (cx + 70, cy - 56), (cx, cy + 82)], fill=f)
        rr(d, (cx - 78, cy - 76, cx + 78, cy - 46), 14, f)
        for x, y in ((cx - 26, cy - 22), (cx + 24, cy - 18), (cx - 2, cy + 22)):
            d.ellipse((x - 13, y - 13, x + 13, y + 13), fill=FIELD[k])


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
        img.resize((64, 64), Image.LANCZOS).save(os.path.join(OUT, 'job_' + k + '.png'), optimize=True)
        print(k)


if __name__ == '__main__': main()

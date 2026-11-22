# Упрощённые значки бизнесов для радара/карты (64x64) из значков UI (ui/biz).
# Форма щита берётся из значка, внутри — цвет поля и простой знак, чтобы читалось в 16–20 px.
# python client-mods/tools/make_biz_radar_icons.py
import os, math
from PIL import Image, ImageDraw, ImageFont, ImageFilter
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
SRC = os.path.join(R, 'mobile/launcher/app/src/main/assets/ui/biz')
OUT = os.path.join(R, 'client-mods/radar/src')
FONT = os.path.join(R, 'launcher/fonts/RussoOne-Regular.ttf')
GOLD = (232, 186, 92)
FG = (255, 236, 170)
DARK = (20, 12, 6)
FIELD = {
    'shop': (25, 55, 150), 'fuel': (170, 25, 25), 'dealer': (55, 58, 64), 'bar': (150, 75, 15),
    'fastfood': (215, 95, 10), 'guns': (20, 105, 40), 'sto': (95, 98, 104), 'casino': (130, 15, 25),
    'hotel': (105, 35, 150), 'rental': (0, 130, 130), 'clothes': (190, 25, 105), 'bank': (120, 80, 10),
}
S = 256


def rr(d, box, r, fill):
    d.rounded_rectangle(box, r, fill=fill)


def glyph(d, k, cx, cy):
    f = FG
    if k == 'shop':
        d.text((cx, cy), '24', font=ImageFont.truetype(FONT, 104), fill=f, anchor='mm', stroke_width=6, stroke_fill=DARK)
    elif k == 'bank':
        d.text((cx, cy), '$', font=ImageFont.truetype(FONT, 150), fill=f, anchor='mm', stroke_width=6, stroke_fill=DARK)
    elif k == 'hotel':
        d.text((cx, cy), 'H', font=ImageFont.truetype(FONT, 140), fill=f, anchor='mm', stroke_width=6, stroke_fill=DARK)
    elif k == 'fuel':  # колонка с пистолетом
        rr(d, (cx - 52, cy - 62, cx + 14, cy + 62), 10, f)
        d.rectangle((cx - 40, cy - 48, cx + 2, cy - 18), fill=FIELD[k])
        d.line([(cx + 14, cy - 30), (cx + 44, cy - 30), (cx + 44, cy + 30), (cx + 60, cy + 30), (cx + 60, cy - 40)], fill=f, width=12, joint='curve')
    elif k == 'dealer':  # машина
        d.polygon([(cx - 50, cy - 8), (cx - 30, cy - 46), (cx + 30, cy - 46), (cx + 50, cy - 8)], fill=f)
        rr(d, (cx - 76, cy - 14, cx + 76, cy + 34), 14, f)
        for x in (cx - 44, cx + 44): d.ellipse((x - 20, cy + 16, x + 20, cy + 56), fill=f, outline=DARK, width=6)
    elif k == 'bar':  # кружка
        rr(d, (cx - 50, cy - 50, cx + 24, cy + 62), 10, f)
        d.ellipse((cx - 58, cy - 76, cx + 32, cy - 36), fill=f)
        d.arc((cx + 2, cy - 28, cx + 66, cy + 40), -90, 90, fill=f, width=16)
    elif k == 'fastfood':  # бургер
        d.chord((cx - 70, cy - 66, cx + 70, cy + 20), 180, 360, fill=f)
        rr(d, (cx - 74, cy - 14, cx + 74, cy + 10), 10, (90, 45, 20))
        rr(d, (cx - 70, cy + 18, cx + 70, cy + 52), 16, f)
    elif k == 'guns':  # пистолет
        d.polygon([(cx - 74, cy - 50), (cx + 72, cy - 50), (cx + 72, cy - 14), (cx - 8, cy - 14), (cx - 12, cy + 2),
                   (cx - 30, cy + 2), (cx - 40, cy + 64), (cx - 80, cy + 64), (cx - 62, cy - 14), (cx - 74, cy - 14)], fill=f)
        d.arc((cx - 34, cy - 30, cx + 10, cy + 22), 0, 180, fill=f, width=9)   # скоба спуска
    elif k == 'sto':  # шестерня
        for i in range(8):
            a = i * math.pi / 4
            d.polygon([(cx + 72 * math.cos(a + s) , cy + 72 * math.sin(a + s)) for s in (-.2, .2)] +
                      [(cx + 40 * math.cos(a + s), cy + 40 * math.sin(a + s)) for s in (.35, -.35)], fill=f)
        d.ellipse((cx - 56, cy - 56, cx + 56, cy + 56), fill=f)
        d.ellipse((cx - 22, cy - 22, cx + 22, cy + 22), fill=FIELD[k])
    elif k == 'casino':  # кубик
        rr(d, (cx - 60, cy - 60, cx + 60, cy + 60), 18, f)
        for x, y in ((-30, -30), (30, -30), (0, 0), (-30, 30), (30, 30)):
            d.ellipse((cx + x - 13, cy + y - 13, cx + x + 13, cy + y + 13), fill=FIELD[k])
    elif k == 'rental':  # ключ
        d.ellipse((cx - 70, cy - 34, cx - 2, cy + 34), fill=f)
        d.ellipse((cx - 48, cy - 12, cx - 24, cy + 12), fill=FIELD[k])
        d.rectangle((cx - 6, cy - 10, cx + 72, cy + 10), fill=f)
        d.rectangle((cx + 40, cy + 10, cx + 54, cy + 34), fill=f)
        d.rectangle((cx + 60, cy + 10, cx + 72, cy + 28), fill=f)
    elif k == 'clothes':  # футболка
        d.polygon([(cx - 30, cy - 62), (cx - 76, cy - 34), (cx - 56, cy - 2), (cx - 40, cy - 12), (cx - 40, cy + 62),
                   (cx + 40, cy + 62), (cx + 40, cy - 12), (cx + 56, cy - 2), (cx + 76, cy - 34), (cx + 30, cy - 62),
                   (cx, cy - 44)], fill=f)


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
        img.resize((64, 64), Image.LANCZOS).save(os.path.join(OUT, 'biz_' + k + '.png'), optimize=True)
        print(k)


if __name__ == '__main__': main()

"""Рисует иконку лаунчера (B на тёмном фоне с оранжевым) во все плотности."""
import os, sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter

here = os.path.dirname(os.path.abspath(__file__))
font_path = os.path.join(here, "..", "app", "src", "main", "assets", "fonts", "RussoOne-Regular.ttf")
res = os.path.join(here, "..", "app", "src", "main", "res")
sizes = {"mdpi": 48, "hdpi": 72, "xhdpi": 96, "xxhdpi": 144, "xxxhdpi": 192}

S = 512
img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
d = ImageDraw.Draw(img)
# тёмная плитка со скруглением
d.rounded_rectangle([16, 16, S - 16, S - 16], radius=96, fill=(18, 20, 24, 255), outline=(46, 49, 56, 255), width=6)
# оранжевая полоса снизу
d.rounded_rectangle([96, S - 130, S - 96, S - 106], radius=12, fill=(255, 122, 26, 255))
font = ImageFont.truetype(font_path, 300)
txt = "B"
bb = d.textbbox((0, 0), txt, font=font)
x = (S - (bb[2] - bb[0])) / 2 - bb[0]
y = (S - 60 - (bb[3] - bb[1])) / 2 - bb[1]
shadow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
ImageDraw.Draw(shadow).text((x, y + 10), txt, font=font, fill=(0, 0, 0, 200))
img = Image.alpha_composite(img, shadow.filter(ImageFilter.GaussianBlur(10)))
# металлический градиент на букве
mask = Image.new("L", (S, S), 0)
ImageDraw.Draw(mask).text((x, y), txt, font=font, fill=255)
grad = Image.new("RGBA", (S, S))
gd = ImageDraw.Draw(grad)
for i in range(S):
    t = i / S
    if t < 0.5:
        c = int(255 - 60 * (t / 0.5))
    else:
        c = int(150 + 50 * ((t - 0.5) / 0.5))
    gd.line([(0, i), (S, i)], fill=(c, c, min(255, c + 6), 255))
img.paste(grad, (0, 0), mask)

for name, px in sizes.items():
    out = os.path.join(res, "mipmap-" + name)
    os.makedirs(out, exist_ok=True)
    img.resize((px, px), Image.LANCZOS).save(os.path.join(out, "ic_launcher.png"))
print("ok")

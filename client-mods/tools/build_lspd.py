# Собирает наш LSPD: берёт оригинальные genintintpolicea.txd / genintintpoliceb.txd
# (из models/gta_int.img игрока) и заменяет в них текстуры на наши из ../lspd/src.
#   python build_lspd.py <папка с оригинальными txd> <папка для результата>
# Оригинальные файлы GTA в репозиторий не кладём.
import os, sys
from PIL import Image, ImageFilter
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import txdpatch
S = os.path.join(HERE, '..', 'lspd', 'src')
L = lambda n: Image.open(os.path.join(S, n))
src_dir, out_dir = sys.argv[1], sys.argv[2]
os.makedirs(out_dir, exist_ok=True)
sheet = L('_sheet_chatgpt.png').convert('RGB')
wall = L('bpd_wall.png').resize((256, 256), Image.LANCZOS)
wains = sheet.crop((6, 524, 370, 702)).resize((256, 256), Image.LANCZOS)
floor = L('bpd_floor.png').resize((256, 256), Image.LANCZOS)
bars = L('bpd2_bars.png').resize((256, 256), Image.LANCZOS)
# Доска объявлений: модель int_policea02 берёт из cop_notice только две четверти
# (правая верхняя и левая нижняя) и натягивает их зеркально, поэтому каждую
# картинку отражаем по горизонтали.
mirror = lambda im: im.transpose(Image.FLIP_LEFT_RIGHT)
logo = L('bpd_logo.png'); bg = Image.new('RGB', (256, 256), (30, 45, 90)); bg.paste(logo, (0, 0), logo)
def poster(n, crop=None):
    # плакат 1:2 по центру четверти доски, по бокам пробковый фон
    im = L(n).convert('RGB')
    if crop: im = im.crop(crop)
    q = cork()
    im = im.resize((256, 492), Image.LANCZOS)
    sh = Image.new('RGB', (256, 492), (25, 18, 12))          # тень под плакатом
    q.paste(sh, (134, 16)); q.paste(im, (128, 10))
    return q
def cork():
    # тёмная пробка с рамкой: в игре свет её высветляет, светлая выглядела оранжевой
    import random
    random.seed(7)
    q = Image.new('RGB', (512, 512), (62, 45, 30)); px = q.load()
    for _ in range(9000):
        x, y = random.randrange(512), random.randrange(512); d = random.randint(-14, 14)
        px[x, y] = (62 + d, 45 + d, 30 + d)
    q = q.filter(ImageFilter.GaussianBlur(0.8))
    fr = Image.new('RGB', (512, 512), (28, 34, 52)); fr.paste(q.crop((6, 6, 506, 506)), (6, 6))
    return fr
bg = bg.resize((512, 512), Image.LANCZOS)
def notes():
    # пробка с разными листками и кнопками, без читаемого текста
    from PIL import ImageDraw
    import random
    random.seed(3); q = cork(); d = ImageDraw.Draw(q)
    for x, y, w, h, c in [(40, 40, 180, 230, (235, 232, 220)), (260, 30, 200, 150, (240, 236, 200)),
                          (250, 210, 220, 260, (230, 230, 230)), (50, 300, 170, 170, (215, 228, 240))]:
        d.rectangle((x + 5, y + 6, x + w + 5, y + h + 6), fill=(25, 18, 12))
        d.rectangle((x, y, x + w, y + h), fill=c)
        d.rectangle((x + 12, y + 14, x + w - 12, y + 30), fill=(40, 60, 110))
        yy = y + 46
        while yy < y + h - 14:
            d.rectangle((x + 12, yy, x + 12 + random.randint(w // 3, w - 24), yy + 5), fill=(90, 90, 95)); yy += 16
        d.ellipse((x + w // 2 - 7, y + 2, x + w // 2 + 7, y + 16), fill=(180, 30, 30))
    return q
board = Image.new('RGB', (1024, 1024), (120, 90, 60))
board.paste(mirror(bg), (0, 0))                                      # не используется моделью
board.paste(mirror(notes()), (512, 0))                              # объявления (правила висели на каждой доске)
board.paste(mirror(poster('bpd2_wanted.png', (0, 0, 512, 992))), (0, 512))   # «Разыскивается»
board.paste(mirror(bg), (512, 512))                                  # не используется моделью
desk = L('bpd2_reception_wood.png').convert('RGB').crop((160, 330, 860, 1000)).resize((128, 128), Image.LANCZOS)
# Второй пак (отдельные картинки из ChatGPT)
sq = lambda n, size=256: L(n).convert('RGB').resize((size, size), Image.LANCZOS)
combo = L('bpd_wall_combo.png').convert('RGB')
base = combo.crop((0, 810, 888, 1254)).resize((128, 64), Image.LANCZOS)       # синяя панель низа
bench = L('bpd_bench.png').convert('RGB')
bench = bench.crop((667, 0, 1334, 667)).resize((128, 128), Image.LANCZOS).rotate(90)  # в модели доски идут поперёк — поворачиваем
surr = L('bpd3_door.png').convert('RGB').crop((80, 28, 540, 60)).resize((128, 32), Image.LANCZOS)  # синяя рамка двери вместо оранжевой
# пак 3: дверь без куска стены слева/сверху
door = L('bpd3_door.png').convert('RGB').crop((66, 66, 572, 1024)).resize((256, 256), Image.LANCZOS)
# Дверь двустворчатая: модель кладёт текстуру на каждую створку, вторую зеркально, ручками к центру.
# Рисуем зазор у края с ручкой (справа) и тонкий шов у петель, чтобы створки не сливались в одну.
from PIL import ImageDraw
# Модель показывает текстуру не до самого края, поэтому створку сжимаем, а зазор делаем широким.
door = door.resize((237, 256), Image.LANCZOS)
_c = Image.new('RGB', (256, 256), (10, 12, 18)); _c.paste(door, (0, 0)); door = _c
_d = ImageDraw.Draw(door)
_d.line((237, 0, 237, 255), fill=(80, 95, 125), width=2)   # кромка створки
_d.rectangle((0, 0, 2, 255), fill=(15, 18, 26))            # шов у петель
txdpatch.patch(os.path.join(src_dir, 'genintintpoliceb.txd'), os.path.join(out_dir, 'genintintpoliceb.txd'),
               {'coptop_blue': wall, 'copbtm_green': wains, 'p_floor4': floor, 'p_countr': desk,
                'breezewall': sq('bpd_corridor_wall.png'), 'breezewall2': sq('bpd_corridor_wall.png'),
                'breezewallbse': base,
                'coptop_pink': sq('bpd3_wall_blue.png'), 'coptop_brown': sq('bpd3_wall_greyblue.png'),
                'pol_flr3': sq('bpd3_floor_checker.png'), 'p_floor2': sq('bpd_floor_lino.png'),
                'cop_cellwall': sq('bpd3_cell_tiles.png'), 'cop_cellwall_btm': sq('bpd_cell_wall_btm.png'),
                'cop_cellfloor': sq('bpd3_cell_floor.png'),
                'p_door1': door, 'lockers': sq('bpd3_lockers.png'), 'cop_bench': bench, 'pol_dor_surr2': surr})
txdpatch.patch(os.path.join(src_dir, 'genintintpolicea.txd'), os.path.join(out_dir, 'genintintpolicea.txd'),
               {'copcell_bars': bars, 'cop_notice': board})

# Лампы на потолке (int_policea03a.txd, текстура striplight1)
lamp = L('bpd2_ceiling.png').convert('RGB').crop((295, 280, 765, 605)).resize((128, 128), Image.LANCZOS)
txdpatch.patch(os.path.join(src_dir, 'int_policea03a.txd'), os.path.join(out_dir, 'int_policea03a.txd'),
               {'striplight1': lamp})
print('ok')

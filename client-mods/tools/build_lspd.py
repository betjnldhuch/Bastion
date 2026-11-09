# Собирает наш LSPD: берёт оригинальные genintintpolicea.txd / genintintpoliceb.txd
# (из models/gta_int.img игрока) и заменяет в них текстуры на наши из ../lspd/src.
#   python build_lspd.py <папка с оригинальными txd> <папка для результата>
# Оригинальные файлы GTA в репозиторий не кладём.
import os, sys
from PIL import Image
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
bars = L('bpd_bars.png').resize((256, 256), Image.LANCZOS)
# Доска объявлений: модель int_policea02 берёт из cop_notice только две четверти
# (правая верхняя и левая нижняя) и натягивает их зеркально, поэтому каждую
# картинку отражаем по горизонтали.
mirror = lambda im: im.transpose(Image.FLIP_LEFT_RIGHT)
logo = L('bpd_logo.png'); bg = Image.new('RGB', (256, 256), (30, 45, 90)); bg.paste(logo, (0, 0), logo)
board = Image.new('RGB', (512, 512), (120, 90, 60))
board.paste(mirror(bg), (0, 0))                          # не используется моделью
board.paste(mirror(L('bpd_rules.png')), (256, 0))        # правила
board.paste(mirror(L('bpd_wanted.png')), (0, 256))       # «Разыскивается»
board.paste(mirror(bg), (256, 256))                      # не используется моделью
desk = L('bpd_desk.png').crop((200, 60, 330, 190)).resize((128, 128), Image.LANCZOS)
# Второй пак (отдельные картинки из ChatGPT)
sq = lambda n, size=256: L(n).convert('RGB').resize((size, size), Image.LANCZOS)
combo = L('bpd_wall_combo.png').convert('RGB')
base = combo.crop((0, 810, 888, 1254)).resize((128, 64), Image.LANCZOS)       # синяя панель низа
bench = L('bpd_bench.png').convert('RGB')
bench = bench.crop((667, 0, 1334, 667)).resize((128, 128), Image.LANCZOS)
txdpatch.patch(os.path.join(src_dir, 'genintintpoliceb.txd'), os.path.join(out_dir, 'genintintpoliceb.txd'),
               {'coptop_blue': wall, 'copbtm_green': wains, 'p_floor4': floor, 'p_countr': desk,
                'breezewall': sq('bpd_corridor_wall.png'), 'breezewall2': sq('bpd_corridor_wall.png'),
                'breezewallbse': base,
                'coptop_pink': sq('bpd_wall_blue.png'), 'coptop_brown': sq('bpd_wall_greyblue.png'),
                'pol_flr3': sq('bpd_floor_checker.png'), 'p_floor2': sq('bpd_floor_lino.png'),
                'cop_cellwall': sq('bpd_cell_wall.png'), 'cop_cellwall_btm': sq('bpd_cell_wall_btm.png'),
                'cop_cellfloor': sq('bpd_cell_floor.png'),
                'p_door1': sq('bpd_door.png'), 'lockers': sq('bpd_lockers.png'), 'cop_bench': bench})
txdpatch.patch(os.path.join(src_dir, 'genintintpolicea.txd'), os.path.join(out_dir, 'genintintpolicea.txd'),
               {'copcell_bars': bars, 'cop_notice': board})
print('ok')

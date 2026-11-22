# Клиентские моды Bastion (наши собственные файлы)

Здесь лежат только наши файлы: картинки, сгенерированные для сервера, и
собранные из них `.txd`. Оригинальные файлы GTA (gta3.img и т.п.) сюда не кладём.

Как это попадает к игрокам:
- телефон — через `mobile/installer/manifest.json` (лаунчер скачивает и проверяет sha256);
- ПК — через лаунчер (будет добавлено в `feature/launcher-pc`).

## lspd/ — интерьер полиции Лос-Сантоса

`src/` — исходные картинки (из листа ChatGPT `_sheet_chatgpt.png`, нарезаны и
выровнены по размерам степени двойки):

| Файл | Размер | Что | Прозрачность |
|---|---|---|---|
| bpd_floor.png | 512×512 | пол, бесшовный | нет |
| bpd_wall.png | 512×256 | стена, бесшовная | нет |
| bpd_wainscot.png | 512×128 | синяя панель низа стены | нет |
| bpd_ceiling.png | 512×128 | потолочные плиты с лампой | нет |
| bpd_desk.png | 512×256 | стойка дежурного | нет |
| bpd_logo.png | 256×256 | эмблема | да |
| bpd_bars.png | 512×256 | решётка камеры | да |
| bpd_wanted.png | 256×256 | плакат «Разыскивается» | нет |
| bpd_rules.png | 256×256 | плакат «Правила участка» | нет |

### Какие текстуры игры заменяются

Интерьер полиции берёт текстуры из `genintintpolicea.txd` и `genintintpoliceb.txd`
(архив `models/gta_int.img`). Мы меняем:

| Текстура GTA | Файл | Наша картинка |
|---|---|---|
| coptop_blue | genintintpoliceb.txd | стена (bpd_wall) |
| copbtm_green | genintintpoliceb.txd | синяя панель низа стены |
| p_floor4 | genintintpoliceb.txd | пол (bpd_floor) |
| p_countr | genintintpoliceb.txd | дерево стойки |
| copcell_bars | genintintpolicea.txd | решётка камеры (с прозрачностью) |
| cop_notice | genintintpolicea.txd | доска объявлений: правила и «Разыскивается» (зеркально — модель натягивает текстуру отражённой) |

Сборка: `python client-mods/tools/build_lspd.py <папка с оригинальными txd> <папка для результата>`.
Готовые `.txd` в репозиторий не кладём: в них остаются оригинальные текстуры Rockstar.

## radar/ — значки фракций, бизнесов и работ на радаре и карте (ПК)

`src/*.png` (64×64) — упрощённые значки из эмблем фракций (`mobile/launcher/app/src/main/assets/ui/emblems`),
рисует их `tools/make_radar_icons.py`. Полные эмблемы в 16–20 px не читаются, поэтому щит цвета фракции и знак.

| Значок SA-MP | Текстура hud.txd | Наш значок |
|---|---|---|
| 62 | radar_gangG | Grove |
| 59 | radar_gangP | Ballas |
| 60 | radar_gangY | Vagos |
| 58 | radar_gangB | Aztecas |
| 30 | radar_police | LSPD |
| 22 | radar_hostpital | Больница |
| 61 | radar_gangN | ФБР |
| 40 | radar_THETRUTH | Армия |
| 8 | radar_BIGSMOKE | Мэрия |
| 23 | radar_LOCO | СМИ |

Бизнесы (`src/biz_*.png`, рисует `tools/make_biz_radar_icons.py` из `ui/biz`):

| Значок SA-MP | Текстура hud.txd | Наш значок |
|---|---|---|
| 24 | radar_MADDOG | Магазин 24/7 |
| 13 | radar_CESARVIAPANDO | Заправка |
| 55 | radar_impound | Автосалон |
| 49 | radar_dateDrink | Бар |
| 10 | radar_burgerShot | Закусочная |
| 6 | radar_ammugun | Оружейный |
| 27 | radar_modGarage | СТО |
| 25 | radar_mafiaCasino | Казино |
| 26 | radar_MCSTRAP | Отель |
| 34 | radar_RYDER | Аренда авто |
| 45 | radar_tshirt | Одежда |
| 52 | radar_cash | Банк |

Работы (`src/job_*.png`, рисует `tools/make_job_radar_icons.py` из `ui/jobs`):

| Значок SA-MP | Текстура hud.txd | Наш значок |
|---|---|---|
| 12 | radar_CATALINAPINK | Таксист |
| 11 | radar_bulldozer | Дальнобойщик |
| 47 | radar_ZERO | Механик |
| 38 | radar_SWEET | Рыбак |
| 46 | radar_WOOZIE | Грузчик |
| 18 | radar_emmetGun | Инкассатор |
| 42 | radar_TorenoRanch | Фермер |
| 28 | radar_OGLOC | Мусорщик |

Сборка: `python client-mods/tools/build_radar.py <оригинальный models/hud.txd> <папка для результата>`,
готовый `hud.txd` кладётся в `models/` игры. Значок на точку ставит админ в `/markers`.

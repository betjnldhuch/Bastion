# Клиентские моды Bastion (наши собственные файлы)

Здесь лежат только наши файлы: картинки, сгенерированные для сервера, и
собранные из них `.txd`. Оригинальные файлы GTA (gta3.img и т.п.) сюда не кладём.

Как это попадает к игрокам:
- телефон — внутри APK: `python client-mods/tools/build_mobile.py` берёт наши текстуры и dff из ПК-пака
  (`launcher/client-pack`) и кладёт в `mobile/launcher/app/src/main/assets/bastion-mods/`; лаунчер копирует их
  в игру, клиент подменяет текстуры texdb по имени, маркеры дописываются в `gta3.img`. Запускать после каждой
  пересборки txd для ПК, иначе на телефоне останется старая графика;
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
| cop_notice | genintintpolicea.txd | доска объявлений: «Правила участка» (bpd4_rules) и пустой «Разыскивается» (bpd4_wanted_empty) (зеркально — модель натягивает текстуру отражённой) |
| copstuff | genintintpolicea.txd | атлас мелочей: шкаф, клавиатура с ноутбуком, рация, монитор, лоток (bpd4_*) |
| filing_cabnu | genintintpolicea.txd | боковина шкафа: тёмно-серый металл (bpd4_metal_panel) |
| la_kitch1/2/3 | genintintpolicea.txd | кухня: серая столешница, светло-серые фасады (bpd4_kitchen_cabinet, bpd4_terrazzo) |
| cops_chrome | genintintpolicea.txd | шлифованная сталь (bpd4_brushed_steel) |
| mp_shop_window | genintintpoliceb.txd | армированное стекло в раме (bpd4_wired_glass) |
| pol_flr164 | genintintpoliceb.txd | пол коридора, терраццо (bpd4_terrazzo3) |
| p_floor3 | genintintpoliceb.txd | пол кабинетов, тёмный ковролин (bpd4_carpet) |
| pol_flr2 | genintintpoliceb.txd | серая плитка по диагонали (bpd4_floor_diamond) |

Лежат в `src/`, но пока не используются (этих текстур нет в txd полиции, нужен файл, где они есть):
bpd4_blue_fabric (BLUE_FABRIC), bpd4_black_leather (CJ_CUSHION2), bpd4_wood_planks (CJ_WOOD1),
bpd4_subway_tile, bpd4_concrete, bpd4_sign_cells, bpd4_sign_interrogation, bpd4_sign_lockers, bpd4_sign_armory, bpd4_height_chart (таблички и ростомер — текстом или объектом с сервера).

Сборка: `python client-mods/tools/build_lspd.py <папка с оригинальными txd> <папка для результата>`.
Готовые `.txd` в репозиторий не кладём: в них остаются оригинальные текстуры Rockstar.

## Банк, бар, 24/7 — замена текстур в `models/gta_int.img`

Сборка: `python client-mods/tools/build_interiors.py <папка с оригинальными txd> <папка для результата>`.

| Интерьер | Файл | Текстура GTA | Наша картинка |
|---|---|---|---|
| Банк (зал «Планировочного отдела», int 3) | papaerchaseoffice.txd | gun_floor1 | пол, bank_marble_beige |
| | papaerchaseoffice.txd | ab_mottleGrey | пол, bank_marble_dark |
| | papaerchaseoffice.txd | cof_wood2 | деревянные стойки, панели из bank_teller_counter |
| Бар Ten Green Bottles (модель smllbarinterior) | cj_bar2.txd | GB_nastybar01 | фасад стойки, bar_counter_wood |
| 24/7 | genintint711_1.txd, 711c.txd | CJ_7_11_TILE | пол, shop_floor_grey_tile |
| | genintint711_1.txd, 711c.txd | CJ_7_11_win | полоса «24-7», shop_sign_247 |
| | cj_ss_1.txd, cj_ss_2.txd | CJ_7_11_edge | ценник-полоса на полках |

Не встали: стеллажи и холодильник 24/7 (в игре это текстуры коробок продуктов, развёртка не совпадает),
полки бара, дверь хранилища, стена больницы (больница строится объектами с сервера из gta3.img) —
их можно поставить только объектами с сервера.

## radar/ — значки фракций, бизнесов, работ и имущества на радаре и карте (ПК)

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
| 23 | radar_LocoSyndicate | СМИ |

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
| 44 | radar_triadsCasino | Водитель автобуса |
| 29 | radar_pizza | Развозчик пиццы |

Имущество (`src/prop_*.png`, рисует `tools/make_prop_radar_icons.py` из `ui/property`):

| Значок SA-MP | Текстура hud.txd | Наш значок |
|---|---|---|
| 31 | radar_propertyG | Дом |
| 15 | radar_CJ | Квартира |
| 32 | radar_propertyR | Особняк |
| 16 | radar_CRASH1 | Гараж |
| 43 | radar_triads | Личный транспорт |
| 9 | radar_boatyard | Лодки |
| 21 | radar_girlfriend | Вертолёты |

Рамка мини-карты, север, стрелка игрока и кулак (`src/radardisc.png` — левая верхняя четверть кольца,
`radar_north.png`, `radar_centre.png`, `fist.png`) — текстуры с теми же именами в hud.txd (`HUD_ART`).

Сборка: `python client-mods/tools/build_radar.py <оригинальный models/hud.txd> <папка для результата>`,
готовый `hud.txd` кладётся в `models/` игры. Значок на точку ставит админ в `/markers`.

## markers/ — свои маркеры (пикапы) в мире (ПК)

`src/*.png` нарезаны из картинки ChatGPT (`/mnt/project-files/textures/markers/chatgpt-prompt.md`).
`tools/build_markers.py` пишет плоскую вращающуюся «монетку» (dff) и txd для каждого пикапа, без оригинальных файлов GTA.
Готовые файлы в `build/`, они заменяют записи с теми же именами в `models/gta3.img`
(для лаунчера: `pack\models\gta3.img\`).

| Модель | dff / txd | Значок | Где на сервере |
|---|---|---|---|
| 1241 | adrenaline / icons8 | вход | двери, вход в бизнес и фракцию |
| 1247 | bribe / icons9 | выход | выходы из интерьеров |
| 1273 | property_fsale / icons3 | дом продаётся | дома |
| 1272 | property_locked / icons4 | дом занят | дома (1277, 1314 тоже в icons4) |
| 1239 | info / icons6 | бизнес | бизнесы, аренда, СТО, штрафстоянка, церковь |
| 1274 | bigdollar / icons2 | касса | автосалон, касса банка, хранилище, лотерея, владелец завода |
| 1275 | clothesp / icons1 | работа | работы, фракции, амбар, свалка, пост охраны |
| 1242 | bodyarmour / icons7 | склад | склады фракций и армии |
| 1212 | money / dyn_cash | банкомат | банкомат без аппарата |
| 1210 | briefcase / breifcase | АЗС | заправки |
| 1240 | health / icons5 | больница | стойка больницы |
| 1254 | killfrenzy / icons | автошкола | автошкола (1253, 1313 тоже в icons) |

## Наши текстуры на маппинге (`/mt`, SetObjectMaterial)

`tools/mapping_textures.py` — список из 31 текстуры `bst_*` (больница, банк, 24/7, бар, автосалон, банды,
базовые тайлы, таблички полиции). `build_markers.py` дописывает их в `icons5.txd` (пикап «больница», модель 1240),
который мы и так пишем целиком. На сервере: `SetObjectMaterial(obj, слот, 1240, "icons5", "bst_...", 0)`,
в `/mt` они есть в списке готовых. Уже стоят: стены больницы, флаг Grove, граффити Ballas, вывеска автосалона
(`interiors.inc`). Телефон: `build_mobile.py` кладёт их в APK, клиент ищет текстуру материала по имени
(SetObjectMaterial включён патчем 8b в `samp-mobile-patches/apply.py`; текст на объектах на телефоне пока не рисуется).

## Заготовки для будущих интерьеров (выбран лучший вариант из присланных, дубли удалены)

| Папка | Файл | Что | Куда планируется |
|---|---|---|---|
| bank/src | bank_marble_beige | бежевый мрамор плиткой | пол зала |
| bank/src | bank_marble_dark | тёмный мрамор | пол у касс / хранилища |
| bank/src | bank_teller_counter | стойка кассира | касса |
| bank/src | bank_vault_door | дверь хранилища | хранилище |
| bar/src | bar_counter_wood | фасад стойки | барная стойка |
| bar/src | bar_back_shelves | полки с бутылками | стена за стойкой |
| shop/src | shop_drinks_fridge | холодильник без брендов | 24/7 |
| shop/src | shop_grocery_shelves | металлический стеллаж | 24/7 |
| shop/src | shop_floor_grey_tile | серая плитка | пол 24/7 |
| shop/src | shop_sign_247 | вывеска 24/7 | вход (или текстом с сервера) |
| dealer/src | dealer_sign | вывеска «Автосалон Bastion» | автосалон |
| hospital/src | hospital_wall_green | зелёная стена с отбойником | коридоры больницы |
| gangs/src | ballas_graffiti | граффити Ballas, прозрачный фон | теги банд |
| gangs/src | grove_flag | флаг Grove | район Grove |
| gangs/src | yellow_brick_worn | жёлтый облезлый кирпич | стены в гетто |
| common/src | garage_concrete_tyres, concrete_tiles, asphalt, parquet, plaster_beige, carpet_blue, corrugated_metal, brick_red | базовые бесшовные тайлы | любые интерьеры |
| lspd/src | bpd4_sign_*, bpd4_height_chart | таблички и ростомер | текстом/объектом с сервера |
| lspd/src | bpd4_blue_fabric, bpd4_black_leather, bpd4_wood_planks | обивка, подушки, дерево | мебель полиции, нужен gta_int.img |
| lspd/src | bpd4_terrazzo_blue, bpd4_concrete, bpd4_subway_tile | запасные | если какой-то пол не понравится |

Удалены как худшие дубли: второй холодильник (реальные бренды), деревянный стеллаж, полки и стойка бара в перспективе,
тёмный мрамор 3:2, кирпич v1, лишняя серая плитка, второй ковролин и второй терраццо.

## weapons/ — значки оружия в HUD

`src/<модель>.png` (128×128, нарезаны из листов ChatGPT). Оригинальных txd оружия у нас нет, поэтому
`tools/build_weapons.py` кладёт в ПК-пак заплатки одной текстуры: `pack/models/gta3.img/<модель>.txd/<модель>icon.tex`.
Лаунчер находит `<модель>icon` в txd игрока внутри gta3.img и меняет только её (`texture` в client-pack.json).
На телефоне — `bastion-mods/tex/<модель>icon.btx` (`build_mobile.py`). Кулак — `fist` в hud.txd.
Те же значки (96 px) лежат в `ui/weapons/` для окон (выписка из тира).

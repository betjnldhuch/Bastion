ПАКЕТ FPS — КАК ДАТЬ ИГРОКАМ 120 / 144 / 165 FPS
=================================================

Почему SA-MP держит 90
  Клиент SA-MP 0.3.7 сам не даёт больше 90 FPS (/fpslimit 20..90).
  Большие проекты поднимают FPS ASI-плагинами. Просто «снять лимит» мало:
  GTA SA писалась под 30 FPS, и на высоком FPS ломается часть физики
  (подвеска машин, камера в прицеле, плавание, анимации). Поэтому в пакет
  кладём не только снятие лимита, но и фиксы физики.

Что положить в эту папку (fps\ рядом с BastionLauncher.exe)
  Все файлы — прямо в fps\, без подпапок. Скачивать только с официальных
  страниц авторов (GitHub), не с файлопомоек.

  1. Загрузчик ASI — Ultimate ASI Loader (ThirteenAG, лицензия MIT).
     Нужен файл dinput8.dll из архива для 32-бит (x86).
     https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases

  2. Снятие лимита FPS — одно из двух:
     • ModernFpsLimit.asi (xKumorio) — лимит 0..1000, без инпут-лага,
       настройки в ModernFpsLimit.ini (ключ fps). Под него уже настроен fps.ini.
       https://github.com/xKumorio/ModernFpsLimit
     • gtasa-precise-fps-limiter (ZantetsukenGT, GPL-3.0) — разрешает /fpslimit
       выше 90. Если берёшь его — поменяй limiter_ini / limiter_key в fps.ini.
       https://github.com/ZantetsukenGT/gtasa-precise-fps-limiter/releases

  3. Фиксы физики на высоком FPS:
     • SilentPatch для San Andreas (Silent) — SilentPatchSA.asi + SilentPatchSA.ini.
       Работает с SA-MP, чинит много багов, в том числе завязанных на FPS.
       https://github.com/CookiePLMonster/SilentPatch/releases
     • Framerate Vigilante (GTAmodding, лицензия MIT) — фиксы физики при снятом лимите.
       https://github.com/GTAmodding/FramerateVigilante

  Всё это требует gta_sa.exe версии 1.0 US (её ставят почти все сборки под SA-MP).

Что делает лаунчер
  • В настройках ползунок FPS от 30 до 165 (максимум — max_fps в fps.ini).
  • До 90 — пишет лимит в sa-mp.cfg, как обычно.
  • Выше 90 — копирует файлы пакета в папку игры и пишет лимит в настройки
    плагина (limiter_ini). Если папки fps нет — предупредит и запустит с 90.
  • «Проверять моды» не трогает файлы пакета, но только если они байт в байт
    такие же, как здесь. Переименованный чит с тем же именем всё равно найдётся.
    Обновил плагин — просто замени файл в этой папке.

Перед релизом обязательно
  Проверь пакет вживую: зайди на сервер с 165 FPS, покатайся, постреляй,
  поплавай. Если что-то вылетает — убирай плагины по одному и смотри, какой
  виноват. На сервере: /fps — твой FPS, /fpslist — FPS всех (админам).

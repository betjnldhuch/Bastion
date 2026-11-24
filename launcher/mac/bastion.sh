#!/bin/bash
# Bastion Online для macOS.
# Внутри .app: Wine (сборка CrossOver от Gcenx), wine-mono и ПК-лаунчер BastionLauncher.exe.
# Своё Windows-окружение (префикс) — в ~/Library/Application Support/Bastion Online/prefix,
# игру GTA SA игрок указывает свою. Логи — ~/Library/Logs/Bastion Online/bastion.log.
#
# Переменные для проверки в CI:
#   BASTION_TEST=1       — без окон-диалогов, ждать запуска лаунчера
#   BASTION_GTA_DIR=путь — папка игры без диалога выбора
#   BASTION_MENU=1       — служебное меню (ставит stub, если зажат Option)

set -u

APP="$(cd "$(dirname "$0")/../.." && pwd)"
RES="$APP/Contents/Resources"
WINEDIR="$RES/wine"
TITLE="Bastion Online"
BUILD="$(cat "$RES/build.txt" 2>/dev/null || echo dev)"
TEST="${BASTION_TEST:-0}"

# Меняется, когда меняется настройка префикса ниже: префикс донастроится сам
PREFIX_VER="1"

SUP="$HOME/Library/Application Support/Bastion Online"
LOGDIR="$HOME/Library/Logs/Bastion Online"
LOG="$LOGDIR/bastion.log"
export WINEPREFIX="$SUP/prefix"
DST="$WINEPREFIX/drive_c/BastionLauncher"

mkdir -p "$SUP" "$LOGDIR"
if [ -f "$LOG" ] && [ "$(stat -f %z "$LOG")" -gt 5000000 ]; then mv -f "$LOG" "$LOG.old"; fi
log() { echo "[$(date '+%F %T')] $*" >> "$LOG"; }
log "=== start build=$BUILD macOS=$(sw_vers -productVersion) arch=$(uname -m) app=$APP"

# ---------- окна macOS (текст передаётся аргументом, без подстановки в AppleScript) ----------
alert() {
    log "ALERT: $1"
    [ "$TEST" = 1 ] && return 0
    osascript -e 'on run a' -e 'display alert (item 2 of a) message (item 1 of a) as critical' -e 'end run' "$1" "$TITLE" >/dev/null 2>&1
}
notify() {
    log "NOTE: $1"
    [ "$TEST" = 1 ] && return 0
    osascript -e 'on run a' -e 'display notification (item 1 of a) with title (item 2 of a)' -e 'end run' "$1" "$TITLE" >/dev/null 2>&1
}
# ask "текст" "кнопка-нет" "кнопка-да" → печатает нажатую кнопку
ask() {
    osascript -e 'on run a' \
        -e 'button returned of (display dialog (item 1 of a) with title (item 4 of a) buttons {item 2 of a, item 3 of a} default button 2 with icon note)' \
        -e 'end run' "$1" "$2" "$3" "$TITLE" 2>/dev/null
}
choose_folder() {
    osascript -e 'on run a' -e 'POSIX path of (choose folder with prompt (item 1 of a))' -e 'end run' "$1" 2>/dev/null
}
choose_file() {
    osascript -e 'on run a' -e 'POSIX path of (choose file with prompt (item 1 of a))' -e 'end run' "$1" 2>/dev/null
}

# ---------- проверки системы ----------
case "$APP" in
    */AppTranslocation/*)
        alert "Перетащи Bastion Online в папку «Программы» и открой его оттуда. Из «Загрузок» или образа диска macOS не даёт игре работать."
        exit 1 ;;
esac

# Скачанное из интернета помечено карантином: без этого macOS не даст запустить Wine внутри
xattr -dr com.apple.quarantine "$APP" 2>/dev/null

if [ "$(uname -m)" = "arm64" ] && ! arch -x86_64 /usr/bin/true 2>/dev/null; then
    if [ "$TEST" = 1 ]; then log "Rosetta 2 не установлена"; exit 1; fi
    b="$(ask "Для игры на Mac с процессором Apple нужна Rosetta 2 (бесплатный компонент Apple). Установить сейчас? Попросит пароль от Mac." "Отмена" "Установить")"
    [ "$b" = "Установить" ] || exit 0
    osascript -e 'do shell script "/usr/sbin/softwareupdate --install-rosetta --agree-to-license" with administrator privileges' >>"$LOG" 2>&1
    if ! arch -x86_64 /usr/bin/true 2>/dev/null; then
        alert "Rosetta 2 не установилась. Попробуй в Терминале: softwareupdate --install-rosetta"
        exit 1
    fi
fi

WINE="$WINEDIR/bin/wine64"
[ -x "$WINE" ] || WINE="$WINEDIR/bin/wine"
WINESERVER="$WINEDIR/bin/wineserver"
if [ ! -x "$WINE" ]; then
    alert "Приложение повреждено: нет Wine. Скачай Bastion Online для Mac заново."
    exit 1
fi
export PATH="$WINEDIR/bin:$PATH"
export WINEDEBUG="${WINEDEBUG:--all,err+module}"
# esync быстрее в играх; лимит файлов поднимаем под него
ulimit -n 65536 2>/dev/null || ulimit -n 10240 2>/dev/null
export WINEESYNC="${WINEESYNC:-1}"

wine() { "$WINE" "$@" >>"$LOG" 2>&1; }
running() { pgrep -f "BastionLauncher.exe|gta_sa.exe|samp.exe" >/dev/null 2>&1; }

# ---------- Windows-окружение (префикс) ----------
user_dirs() {
    # Wine по умолчанию связывает «Документы», «Рабочий стол» и т.п. с папками Mac:
    # игра писала бы в ~/Documents, а macOS спрашивала бы доступ. Делаем их обычными папками.
    local u e
    for u in "$WINEPREFIX"/drive_c/users/*/; do
        [ "$(basename "$u")" = "Public" ] && continue
        for e in "$u"*; do
            if [ -L "$e" ]; then rm -f "$e"; mkdir -p "$e"; fi
        done
    done
}

mac_fonts() {
    # Arial, Tahoma и др. для чата SA-MP и окон лаунчера (с кириллицей) — из шрифтов macOS
    local fd="$WINEPREFIX/drive_c/windows/Fonts" f
    mkdir -p "$fd"
    for f in /System/Library/Fonts/Supplemental/{Arial,Tahoma,Verdana,"Times New Roman","Courier New",Trebuchet,Georgia,Impact,"Comic Sans"}*.ttf; do
        [ -f "$f" ] && cp -f "$f" "$fd/" 2>/dev/null
    done
}

setup_prefix() {
    notify "Первый запуск: готовлю окружение для игры. Это 1–2 минуты."
    log "prefix setup v$PREFIX_VER"
    mkdir -p "$WINEPREFIX"
    WINEDLLOVERRIDES="mscoree=d;mshtml=d;winemenubuilder.exe=d" wine wineboot -u
    "$WINESERVER" -w
    if [ -f "$RES/wine-mono.msi" ]; then
        log "install wine-mono"
        wine msiexec /i "$RES/wine-mono.msi" /qn
        "$WINESERVER" -w
    fi
    local reg="$SUP/bastion.reg"
    cat > "$reg" <<'REG'
REGEDIT4

[HKEY_CURRENT_USER\Software\Wine\DllOverrides]
"dinput8"="native,builtin"
"winemenubuilder.exe"=""
"mshtml"=""

[HKEY_CURRENT_USER\Software\Wine\Mac Driver]
"LeftOptionIsAlt"="y"
"RightOptionIsAlt"="y"

[HKEY_CURRENT_USER\Software\Wine\Direct3D]
"csmt"=dword:00000001
REG
    wine regedit /S "$reg"
    "$WINESERVER" -w
    mac_fonts
    echo "$PREFIX_VER" > "$WINEPREFIX/.bastion-prefix"
    log "prefix ready"
}

install_launcher() {
    # ПК-лаунчер из .app копируется в префикс при новой сборке .app.
    # Дальше он обновляется сам, как на Windows. launcher.ini игрока не трогаем.
    [ "$(cat "$DST/.mac-build" 2>/dev/null)" = "$BUILD" ] && return 0
    log "install launcher $BUILD"
    mkdir -p "$DST"
    local f
    for f in "$RES/BastionLauncher/"*; do
        [ "$(basename "$f")" = "launcher.ini" ] && [ -f "$DST/launcher.ini" ] && continue
        cp -Rf "$f" "$DST/"
    done
    echo "$BUILD" > "$DST/.mac-build"
}

# Папка Mac → путь Wine (диск Z: — это корень Mac)
winpath() { local p="${1%/}"; echo "Z:${p//\//\\}"; }

set_gta() {
    local dir="${1%/}" exe
    exe="$(find "$dir" -maxdepth 1 -iname gta_sa.exe 2>/dev/null | head -1)"
    if [ -z "$exe" ]; then
        alert "В папке «$dir» нет gta_sa.exe. Нужна папка с установленной GTA San Andreas (версия для Windows)."
        return 1
    fi
    wine reg add 'HKCU\Software\SAMP' /v gta_sa_exe /t REG_SZ /d "$(winpath "$exe")" /f
    log "gta folder: $dir"
    if ! find "$dir" -maxdepth 1 -iname samp.exe | grep -q .; then
        notify "В папке игры нет SA-MP 0.3.7. Поставь его: открой Bastion с зажатым Option → «Запустить установщик .exe»."
    fi
    return 0
}

ask_gta() {
    local d
    d="$(choose_folder "Выбери папку с GTA San Andreas (где лежит gta_sa.exe)")"
    [ -n "$d" ] && set_gta "$d"
}

first_gta() {
    [ -f "$SUP/.gta-asked" ] && return 0
    if [ -n "${BASTION_GTA_DIR:-}" ]; then
        set_gta "$BASTION_GTA_DIR"
    elif [ "$TEST" != 1 ]; then
        local b
        b="$(ask "Укажи папку со своей GTA San Andreas для Windows (где лежит gta_sa.exe). Если игры нет — нажми «Позже» и поставь её кнопкой в лаунчере." "Позже" "Указать папку")"
        [ "$b" = "Указать папку" ] && ask_gta
    fi
    touch "$SUP/.gta-asked"
}

run_exe() {
    local f="$1"
    log "run: $f"
    (cd "$(dirname "$f")" && nohup "$WINE" "$f" >>"$LOG" 2>&1 &)
}

menu() {
    local c
    c="$(osascript -e 'on run a' \
        -e 'set r to choose from list (items 2 thru -1 of a) with title (item 1 of a) with prompt "Что сделать?" default items {item 2 of a}' \
        -e 'if r is false then return ""' -e 'return item 1 of r' -e 'end run' \
        "$TITLE" "Играть" "Указать папку GTA" "Запустить установщик .exe (SA-MP и т.п.)" \
        "Открыть логи" "Открыть диск C: игры" "Сбросить окружение Windows" 2>/dev/null)"
    log "menu: $c"
    case "$c" in
        "Играть") return 0 ;;
        "Указать папку GTA") ask_gta; return 0 ;;
        "Запустить установщик .exe"*)
            local f; f="$(choose_file "Выбери .exe для Windows (например, установщик SA-MP 0.3.7)")"
            [ -n "$f" ] && run_exe "$f"
            exit 0 ;;
        "Открыть логи") open "$LOGDIR"; exit 0 ;;
        "Открыть диск C:"*) open "$WINEPREFIX/drive_c"; exit 0 ;;
        "Сбросить окружение"*)
            [ "$(ask "Окружение Windows создастся заново при следующем запуске. Игра не пропадёт (если лежит не на диске C:), ник и папку GTA укажешь заново." "Отмена" "Сбросить")" = "Сбросить" ] || exit 0
            "$WINESERVER" -k 2>/dev/null
            rm -rf "$WINEPREFIX" "$SUP/.gta-asked"
            notify "Окружение сброшено. Открой Bastion Online ещё раз."
            exit 0 ;;
        *) exit 0 ;;
    esac
}

# ---------- запуск ----------
if [ "${BASTION_MENU:-0}" != 1 ] && running; then
    log "already running"
    exit 0
fi

if [ "$(cat "$WINEPREFIX/.bastion-prefix" 2>/dev/null)" != "$PREFIX_VER" ]; then
    setup_prefix
fi
user_dirs
install_launcher

[ "${BASTION_MENU:-0}" = 1 ] && [ "$TEST" != 1 ] && menu
first_gta

if [ ! -f "$DST/BastionLauncher.exe" ]; then
    alert "Не найден BastionLauncher.exe. Открой Bastion с зажатым Option → «Сбросить окружение Windows»."
    exit 1
fi
log "launch BastionLauncher.exe"
cd "$DST" || exit 1
nohup "$WINE" BastionLauncher.exe >>"$LOG" 2>&1 &
exit 0

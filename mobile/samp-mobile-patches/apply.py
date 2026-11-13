#!/usr/bin/env python3
"""Prepare the pinned SAMP-Mobile (GTA-2.11) checkout for embedding into Bastion.

The upstream submodule stays unmodified in Git; this script edits only the
temporary CI checkout. Every edit is checked: if upstream code changes and a
pattern is no longer found, the script fails instead of silently skipping.

What it does:
  1. Native client: the hard-coded storage root /storage/emulated/0/GTA/ is
     replaced by BastionStoragePath(), which reads $BASTION_GTA_STORAGE (set by
     the Java side) and falls back to the upstream path.
  2. Java client: GameActivityBase sets $BASTION_GTA_STORAGE before the native
     libraries are loaded (from the "bastion.storage" extra, or from
     Android/data/<package>/files/game/ when launched without extras).
  3. Gradle: the :app application module becomes an Android library without
     Firebase / google-services (their config is bound to com.gta.game).
  4. Manifest: no launcher Activity, no <application> attributes that clash
     with Bastion, no all-files-access / install-packages permissions; the game
     Activities run in a separate ":game" process so System.exit() in the game
     does not kill the Bastion launcher.
  5. Resources: upstream launcher icons are removed so the Bastion icon wins
     (the upstream adaptive mipmap-anydpi-v26 icon would otherwise override it).
  6. libGlossHook.so is packaged, because libsamp.so is linked against it.

Usage: python3 apply.py <path-to-samp-mobile-checkout>
"""
from pathlib import Path
import re
import shutil
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: apply.py <samp-mobile checkout>")
root = Path(sys.argv[1]).resolve()
app = root / "app"
main = app / "src/main"
UPSTREAM_STORAGE = '"/storage/emulated/0/GTA/"'


# latin-1 keeps every byte as-is (some upstream sources are cp1251).
def read(path: Path) -> str:
    return path.read_text(encoding="latin-1")


def write(path: Path, text: str) -> None:
    path.write_text(text, encoding="latin-1")


def replace_exact(path: Path, old: str, new: str, count: int = 1) -> None:
    text = read(path)
    found = text.count(old)
    if found != count:
        raise SystemExit(f"{path}: expected {count} occurrence(s) of {old!r}, found {found}")
    write(path, text.replace(old, new))


def remove_block(source: str, block_name: str, required: bool = True) -> str:
    match = re.search(r"(?m)^[ \t]*" + re.escape(block_name) + r"\s*\{", source)
    if not match:
        if required:
            raise SystemExit("Gradle block not found: " + block_name)
        return source
    opening = source.find("{", match.start())
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                end = index + 1
                if end < len(source) and source[end] == "\n":
                    end += 1
                return source[:match.start()] + source[end:]
    raise SystemExit("Unbalanced Gradle block: " + block_name)


# ---------------------------------------------------------------------------
# 1. Native storage root
# ---------------------------------------------------------------------------
cpp = main / "cpp/samp"

replace_exact(cpp / "main.h", "extern char* g_pszStorage;",
              "extern char* g_pszStorage;\n"
              "// Bastion: game data root (with trailing '/'), from $BASTION_GTA_STORAGE.\n"
              "const char* BastionStoragePath();")

replace_exact(cpp / "main.cpp", f"char* g_pszStorage = {UPSTREAM_STORAGE};", """char* g_pszStorage = (char*)"/storage/emulated/0/GTA/";

// Bastion: the launcher installs game files into its own app storage
// (Android/data/online.bastion.launcher/files/game/) and exports that path in
// BASTION_GTA_STORAGE before libsamp.so is loaded. No storage permission needed.
const char* BastionStoragePath()
{
	static char s_storage[512] = "/storage/emulated/0/GTA/";
	static bool s_resolved = false;
	if (!s_resolved)
	{
		const char* env = getenv("BASTION_GTA_STORAGE");
		if (env != nullptr && env[0] != '\\0')
		{
			size_t len = strlen(env);
			snprintf(s_storage, sizeof(s_storage), "%s%s", env, env[len - 1] == '/' ? "" : "/");
			s_resolved = true;
		}
	}
	return s_storage;
}""")
replace_exact(cpp / "main.cpp", 'sprintf(buffer, "/storage/emulated/0/GTA/logcat.txt");',
              'snprintf(buffer, sizeof(buffer), "%slogcat.txt", BastionStoragePath());')

replace_exact(cpp / "settings.cpp", "char buff[0x7F];", "char buff[0x200];")
replace_exact(cpp / "settings.cpp", 'sprintf(buff, "/storage/emulated/0/GTA/SAMP/settings.ini");',
              'snprintf(buff, sizeof(buff), "%sSAMP/settings.ini", BastionStoragePath());')

replace_exact(cpp / "game/hooks.cpp", f"g_pszStorage = {UPSTREAM_STORAGE};",
              "g_pszStorage = (char*)BastionStoragePath();", count=2)

# A missing game file makes the engine call OS_FileRead(NULL, ...) and crash
# with SIGSEGV at 0x10 (seen on device). Record every file NvFOpen could not
# find, so the launcher can name it in the crash report.
replace_exact(cpp / "game/hooks.cpp", 'Log("NVFOpen hook | Error: file not found (%s)", path);',
              'Log("NVFOpen hook | Error: file not found (%s)", path);\n'
              '        { char bastionList[600]; snprintf(bastionList, sizeof(bastionList), "%sbastion_missing_files.txt", BastionStoragePath());\n'
              '          FILE* bastionMissing = fopen(bastionList, "a");\n'
              '          if (bastionMissing) { fprintf(bastionMissing, "%s\\n", path); fclose(bastionMissing); } }')

# Android/data on many devices is case-sensitive, while GTA requests names in
# its own case (e.g. "text/russian.gxt") and caches may ship "Text/RUSSIAN.GXT".
# If the exact path does not exist, open the real file whose path matches
# case-insensitively (the same file, not a substitute).
replace_exact(cpp / "game/hooks.cpp", "char lastFile[123];", r"""char lastFile[123];

#include <dirent.h>
#include <strings.h>
#include <unistd.h>
// Bastion: resolve an absolute path component by component, ignoring case.
static bool BastionResolveCase(const char* in, char* out, size_t outSize)
{
	if (in == nullptr || in[0] != '/') return false;
	char current[512] = "";
	const char* p = in + 1;
	while (*p)
	{
		const char* slash = strchr(p, '/');
		size_t len = slash ? (size_t)(slash - p) : strlen(p);
		char part[256];
		if (len == 0 || len >= sizeof(part)) { if (!slash) break; p = slash + 1; continue; }
		memcpy(part, p, len); part[len] = '\0';
		char candidate[512];
		snprintf(candidate, sizeof(candidate), "%s/%s", current, part);
		if (access(candidate, F_OK) != 0)
		{
			DIR* dir = opendir(current[0] ? current : "/");
			if (dir == nullptr) return false;
			bool found = false;
			struct dirent* e;
			while ((e = readdir(dir)) != nullptr)
			{
				if (strcasecmp(e->d_name, part) == 0)
				{
					snprintf(candidate, sizeof(candidate), "%s/%s", current, e->d_name);
					found = true;
					break;
				}
			}
			closedir(dir);
			if (!found) return false;
		}
		strncpy(current, candidate, sizeof(current) - 1);
		current[sizeof(current) - 1] = '\0';
		if (!slash) break;
		p = slash + 1;
	}
	if (strlen(current) + 1 > outSize) return false;
	strcpy(out, current);
	return true;
}""")
replace_exact(cpp / "game/hooks.cpp", 'FILE *f  = fopen(path, "rb");', r"""FILE *f  = fopen(path, "rb");
    if (!f)
    {
        char real[512];
        if (BastionResolveCase(path, real, sizeof(real)))
        {
            f = fopen(real, "rb");
            if (f) Log("NvFOpen: %s opened as %s (case differs)", path, real);
        }
    }""")

leftovers = []
for source in cpp.rglob("*"):
    if source.suffix not in {".cpp", ".h", ".c"} or "vendor" in source.parts:
        continue
    for number, line in enumerate(read(source).splitlines(), 1):
        if "/storage/emulated/0/GTA" in line and "BastionStoragePath" not in line \
                and "s_storage[512]" not in line and "g_pszStorage = (char*)" not in line \
                and not line.strip().startswith("//"):
            leftovers.append(f"{source.relative_to(root)}:{number}: {line.strip()}")
if leftovers:
    raise SystemExit("Hard-coded GTA storage path left in native code:\n" + "\n".join(leftovers))

# ---------------------------------------------------------------------------
# 1b. Upstream is developed on Windows: fix the letter case of #include paths
#     so the sources build on a case-sensitive file system (Linux CI).
# ---------------------------------------------------------------------------
cpp_root = main / "cpp"
include_dirs = [cpp, cpp / "util", cpp / "vendor/armhook", cpp / "vendor/imgui", cpp / "vendor",
                cpp / "game/RW", cpp / "game/Core", cpp_root / "opus/include"]
_listing_cache = {}


def _listing(directory: Path):
    if directory not in _listing_cache:
        try:
            _listing_cache[directory] = {p.name.lower(): p.name for p in directory.iterdir()}
        except OSError:
            _listing_cache[directory] = {}
    return _listing_cache[directory]


def _resolve_case(base: Path, rel: str):
    """Return rel with corrected case if base/rel exists case-insensitively."""
    current, fixed = base, []
    for part in rel.split("/"):
        if part in (".", ""):
            fixed.append(part)
            continue
        if part == "..":
            current = current.parent
            fixed.append(part)
            continue
        if (current / part).exists():
            real = part
        else:
            real = _listing(current).get(part.lower())
            if real is None:
                return None
        fixed.append(real)
        current = current / real
    return "/".join(fixed)


include_re = re.compile(r'^(\s*#\s*include\s*)([<"])([^>"]+)([>"])', re.M)
case_fixes = 0
for source in cpp_root.rglob("*"):
    if source.suffix.lower() not in {".c", ".cc", ".cpp", ".h", ".hpp", ".inl"} or not source.is_file():
        continue
    text = read(source)

    def fix(match):
        global case_fixes
        if match.group(2) == "<":
            return match.group(0)  # <...> may be a system header (e.g. <link.h>): never touch
        rel = match.group(3).replace("\\", "/")
        for base in [source.parent] + include_dirs:
            if (base / rel).is_file():
                if rel != match.group(3):
                    case_fixes += 1
                    return match.group(1) + match.group(2) + rel + match.group(4)
                return match.group(0)
        for base in [source.parent] + include_dirs:
            real = _resolve_case(base, rel)
            if real is not None and (base / real).is_file():
                case_fixes += 1
                return match.group(1) + match.group(2) + real + match.group(4)
        return match.group(0)  # system / NDK header

    new_text = include_re.sub(fix, text)
    if new_text != text:
        write(source, new_text)
print(f"fixed letter case / separators in {case_fixes} #include directive(s)")

# ---------------------------------------------------------------------------
# 1c. Keep symbols in libsamp.so (no "-s"), so on-device tombstones show
#     function names and CI can keep an unstripped copy for symbolization.
# ---------------------------------------------------------------------------
replace_exact(cpp / "CMakeLists.txt", '"${CMAKE_CXX_FLAGS} -w -s -g ', '"${CMAKE_CXX_FLAGS} -w -g ')

# ---------------------------------------------------------------------------
# 2. Java: export the storage path before native libraries load
# ---------------------------------------------------------------------------
base = main / "java/com/rockstargames/oswrapper/GameActivityBase.java"
replace_exact(base, """        instance = this;

        Log.i(TAG, "[!!] onCreate");""", """        instance = this;
        exportBastionStorage();

        Log.i(TAG, "[!!] onCreate");""")
replace_exact(base, "    public static GameActivityBase getInstance() {", """    /** Bastion: tell libsamp.so where the launcher installed the game files. */
    private void exportBastionStorage() {
        // Java exceptions of the game process -> files/last_crash.txt (shown by the launcher).
        final File crashFile = new File(getFilesDir(), "last_crash.txt");
        final Thread.UncaughtExceptionHandler previous = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler((thread, error) -> {
            try {
                java.io.StringWriter text = new java.io.StringWriter();
                java.io.PrintWriter out = new java.io.PrintWriter(text);
                out.println(new java.util.Date() + " [game] thread " + thread.getName());
                error.printStackTrace(out);
                out.flush();
                java.io.FileOutputStream f = new java.io.FileOutputStream(crashFile);
                try { f.write(text.toString().getBytes("UTF-8")); } finally { f.close(); }
            } catch (Throwable ignored) { }
            if (previous != null) previous.uncaughtException(thread, error);
        });
        try {
            String storage = getIntent() != null ? getIntent().getStringExtra("bastion.storage") : null;
            if (storage == null || storage.isEmpty()) {
                File ext = getExternalFilesDir(null);
                if (ext != null) storage = new File(ext, "game").getAbsolutePath();
            }
            if (storage != null) {
                if (!storage.endsWith("/")) storage = storage + "/";
                android.system.Os.setenv("BASTION_GTA_STORAGE", storage, true);
                Log.i(TAG, "[Bastion] BASTION_GTA_STORAGE = " + storage);
            }
        } catch (Exception e) {
            Log.e(TAG, "[Bastion] cannot export BASTION_GTA_STORAGE", e);
        }
    }

    public static GameActivityBase getInstance() {""")

# ---------------------------------------------------------------------------
# 2b. Game language. libGame.so maps the value of getDeviceLocale() through
#     OS_LanguageUserSelected (table at 0x271d24) and InitialiseLanguage:
#       0 AMERICAN, 1 FRENCH, 2 GERMAN, 3 ITALIAN, 4 SPANISH, 5 JAPANESE, 9 RUSSIAN
#     Upstream returns 5 for "ru", so Russian devices open text/japanese.gxt,
#     NvFOpen returns NULL and OS_FileRead(NULL) crashes (SIGSEGV at 0x10).
#     The launcher passes "bastion.language" = the language whose .gxt is installed.
# ---------------------------------------------------------------------------
lang = main / "java/com/rockstargames/oswrapper/services/LanguageServices.java"
lang_text = read(lang)
start = lang_text.find("    public int getDeviceLocale() {")
end = lang_text.rfind("}")
if start < 0 or end < 0:
    raise SystemExit("LanguageServices.getDeviceLocale not found")
end = lang_text.rfind("}", 0, end)  # end of getDeviceLocale
write(lang, lang_text[:start] + """    public int getDeviceLocale() {
        // Values are libGame.so codes (see apply.py): 0 en, 1 fr, 2 de, 3 it, 4 es, 5 ja, 9 ru.
        String language = null;
        try {
            if (activity.getIntent() != null) language = activity.getIntent().getStringExtra("bastion.language");
        } catch (Exception ignored) { }
        if (language == null || language.isEmpty()) language = java.util.Locale.getDefault().getLanguage();
        switch (language) {
            case "fr": return 1;
            case "de": return 2;
            case "it": return 3;
            case "es": return 4;
            case "ja": return 5;
            case "ru": return 9;
            default:   return 0;
        }
    }
""" + lang_text[end + 1:])
if 'return 9' not in read(lang) or 'if (language.equals("ru")) return 5' in read(lang):
    raise SystemExit("language mapping patch failed")

# ---------------------------------------------------------------------------
# 3. Gradle: application -> library, without Firebase
# ---------------------------------------------------------------------------
gradle = app / "build.gradle.kts"
g = read(gradle)
g = g.replace('id("com.android.application")', 'id("com.android.library")', 1)
for plugin in ('    id("com.google.gms.google-services")\n', '    id("com.google.firebase.crashlytics")\n'):
    if plugin not in g:
        raise SystemExit("plugin line not found: " + plugin.strip())
    g = g.replace(plugin, "")
for pattern in (r'(?m)^\s*applicationId\s*=\s*"[^"]+"\s*\n',
                r'(?m)^\s*versionCode\s*=\s*\d+\s*\n',
                r'(?m)^\s*versionName\s*=\s*"[^"]+"\s*\n',
                r'(?m)^\s*targetSdk\s*=\s*\d+\s*\n',
                r'(?m)^\s*isDebuggable\s*=\s*(true|false)\s*\n'):
    g, n = re.subn(pattern, "", g)
    if n == 0:
        raise SystemExit("Gradle line not found: " + pattern)
g = remove_block(g, "applicationVariants.all")
# debugSymbolLevel is an application-only NDK option.
g, n = re.subn(r'\s*ndk\s*\{\s*debugSymbolLevel\s*=\s*"FULL"\s*\}', "", g)
if n == 0:
    raise SystemExit("ndk debugSymbolLevel blocks not found")
g = remove_block(g, "firebaseCrashlytics")
g, n = re.subn(r'(?m)^\s*(//)?\s*implementation\((platform\()?"com\.google\.firebase:[^\n]*\n', "", g)
if n < 3:
    raise SystemExit("Firebase dependencies not found (got %d)" % n)
for bad in ("com.android.application", "applicationId", "google-services", "firebase", "applicationVariants"):
    if bad in g:
        raise SystemExit("Could not remove from SAMP-Mobile Gradle module: " + bad)
write(gradle, g)
(app / "google-services.json").unlink(missing_ok=True)

# ---------------------------------------------------------------------------
# 4. Manifest
# ---------------------------------------------------------------------------
manifest = main / "AndroidManifest.xml"
xml = read(manifest)
xml, n = re.subn(r'\s+package="com\.gta\.game"', "", xml, count=1)
if n != 1:
    raise SystemExit("manifest package attribute not found")
xml, n = re.subn(r'\s*<uses-sdk[^>]*/>', "", xml, count=1)
if n != 1:
    raise SystemExit("uses-sdk not found")
for perm in ("com.google.android.c2dm.permission.RECEIVE",
             "com.google.android.finsky.permission.BIND_GET_INSTALL_REFERRER_SERVICE",
             "android.permission.REQUEST_INSTALL_PACKAGES",
             "android.permission.MANAGE_EXTERNAL_STORAGE",
             "android.permission.READ_EXTERNAL_STORAGE",
             "android.permission.WRITE_EXTERNAL_STORAGE",
             "android.permission.READ_MEDIA_AUDIO",
             "android.permission.READ_MEDIA_VIDEO",
             "android.permission.READ_MEDIA_IMAGES",
             "android.permission.RECEIVE_BOOT_COMPLETED",
             "android.permission.FOREGROUND_SERVICE_DATA_SYNC",
             "com.rockstargames.gtasa.DYNAMIC_RECEIVER_NOT_EXPORTED_PERMISSION"):
    xml, n = re.subn(r'\s*<uses-permission(-sdk-23)?\s+android:name="' + re.escape(perm) + r'"[^>]*/>', "", xml)
    if n == 0:
        raise SystemExit("permission not found in manifest: " + perm)

# <application ...> attributes belong to Bastion (icon, label, theme, backup...).
xml, n = re.subn(r"<application\b[^>]*>", "<application>", xml, count=1, flags=re.S)
if n != 1:
    raise SystemExit("<application> not found")
xml, n = re.subn(r'\s*<meta-data\s+android:name="firebase_sessions_enabled"[^>]*/>', "", xml)
if n != 1:
    raise SystemExit("firebase meta-data not found")

# The upstream launcher screen (storage permission + START button) is replaced by Bastion.
xml, n = re.subn(r'\s*<activity\s+android:name="com\.gta\.launcher\.activity\.MainActivity".*?</activity>', "", xml,
                 count=1, flags=re.S)
if n != 1:
    raise SystemExit("upstream launcher Activity not found")
if "android.intent.category.LAUNCHER" in xml:
    raise SystemExit("a LAUNCHER intent-filter is still present in SAMP-Mobile manifest")

# Game Activities: private to the APK, own process.
def harden(match: "re.Match[str]") -> str:
    tag = match.group(0)
    tag = tag.replace('android:exported="true"', 'android:exported="false"')
    if "android:process=" not in tag:
        tag = tag.replace("<activity", '<activity\n            android:process=":game"', 1)
    return tag

xml, n = re.subn(r"<activity\b[^>]*>", harden, xml, flags=re.S)
if n != 3 or 'android:exported="true"' in xml:
    raise SystemExit("unexpected Activity set in SAMP-Mobile manifest (%d)" % n)
for activity in ("com.gta.game.SAMP", "com.rockstargames.gtasa.GameActivity", "com.rockstargames.gtasa.DownloaderActivity"):
    if f'android:name="{activity}"' not in xml:
        raise SystemExit("Activity missing after patch: " + activity)
write(manifest, xml)

# ---------------------------------------------------------------------------
# 5. Resources: let Bastion's icon/app name win
# ---------------------------------------------------------------------------
removed = 0
for res_dir in (main / "res").glob("mipmap*"):
    for icon in list(res_dir.glob("ic_launcher*")):
        icon.unlink()
        removed += 1
    if not any(res_dir.iterdir()):
        res_dir.rmdir()
if removed == 0:
    raise SystemExit("upstream launcher icons not found")
replace_exact(main / "res/values/strings.xml", '<string name="app_name">GTA 2.11</string>',
              '<string name="app_name">Bastion</string>')

# ---------------------------------------------------------------------------
# 6. Native dependencies of libsamp.so that are not in jniLibs upstream
# ---------------------------------------------------------------------------
# CMake copies libraries linked by path (bass, GlossHook) next to libsamp.so, and
# upstream also ships some of them in jniLibs. Use one identical, 16 KB-aligned
# copy of each: the vendor arm64 libbass.so is only 4 KB-aligned, the jniLibs one is 16 KB.
shutil.copy2(main / "jniLibs/arm64-v8a/libbass.so", cpp / "vendor/bass/libs/arm64-v8a/libbass.so")
shutil.copy2(main / "jniLibs/arm64-v8a/libbass_ssl.so", cpp / "vendor/bass/libs/arm64-v8a/libbass_ssl.so")
replace_exact(gradle, """        jniLibs {
            excludes.add("META-INF/*")
""", """        jniLibs {
            excludes.add("META-INF/*")
            // Same files from jniLibs and from CMake-linked prebuilts.
            pickFirsts.add("**/libbass.so")
            pickFirsts.add("**/libbass_ssl.so")
            pickFirsts.add("**/libGlossHook.so")
""")

gloss = cpp / "vendor/GlossHook/libs/ARM64/libGlossHook.so"
if not gloss.exists():
    raise SystemExit("libGlossHook.so (ARM64) not found")
shutil.copy2(gloss, main / "jniLibs/arm64-v8a/libGlossHook.so")

# ---------------------------------------------------------------------------
# 7. HTML windows (BastionWebUi): every dialog is drawn by assets/ui/index.html
#    (shipped by the launcher module) instead of the stock one. Caption "[ui:NAME]"
#    opens a custom window, any other dialog the generic "samp" window.
#    The answer goes back as a normal dialog response.
# ---------------------------------------------------------------------------
samp_java = main / "java/com/gta/game/SAMP.java"
shutil.copy2(Path(__file__).resolve().parent / "java/BastionWebUi.java", samp_java.parent / "BastionWebUi.java")
replace_exact(samp_java, """    private DialogManager mDialog;
""", """    private DialogManager mDialog;
    private BastionWebUi mWebUi;
""")
replace_exact(samp_java, """        runOnUiThread(() -> { this.mDialog.show(dialogId, dialogTypeId, caption, content, leftBtnText, rightBtnText); });""",
"""        runOnUiThread(() -> {
            if (BastionWebUi.wants(caption)) {
                this.mDialog.Hide();
                this.mWebUi.show(dialogId, caption, content, leftBtnText, rightBtnText);
                return;
            }
            this.mDialog.Hide();
            this.mWebUi.showDialog(dialogId, dialogTypeId, caption, content, leftBtnText, rightBtnText);
        });""")
replace_exact(samp_java, """        mDialog     = new DialogManager(this);
""", """        mDialog     = new DialogManager(this);
        mWebUi      = new BastionWebUi(this, mDialog);
""")
replace_exact(samp_java, """                if (pause) {
                    mDialog.hideWithoutReset();""", """                mWebUi.setPaused(pause);
                if (pause) {
                    mDialog.hideWithoutReset();""")

print("SAMP-Mobile prepared for Bastion: library module, Bastion storage, :game process, no Firebase.")

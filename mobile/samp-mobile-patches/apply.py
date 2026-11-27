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

# A server pickup whose model has no collision in this client (e.g. SA-MP 0.3.7
# objects 19197/19605 that the mobile cache lacks) crashes the game in
# CPickups::GenerateNewOne -> CWorld::Add -> CEntity::GetBoundCentre (SIGSEGV
# at 0x18, seen on device). Upstream only checks for a missing model info and
# then falls back to 18631, which can have the same problem. Use the standard
# "i" pickup (1239, generic collision, always loaded) instead.
replace_exact(cpp / "game/game.cpp",
              "    if(dwModelArray[iModel] == nullptr)\n        iModel = 18631;",
              "    // Bastion: no model info or no collision -> standard \"i\" pickup, not a crash.\n"
              "    if (iModel < 0 || iModel >= CModelInfo::NUM_MODEL_INFOS || dwModelArray[iModel] == nullptr\n"
              "        || dwModelArray[iModel]->GetColModel() == nullptr)\n"
              "    {\n"
              "        Log(\"[Bastion] pickup model %d has no collision in this client, using 1239\", iModel);\n"
              "        iModel = 1239;\n"
              "    }\n    // upstream fallback was 18631:")

# SetPlayerPosFindZ (admin map teleport) crashed the game on arm64: ScriptCommand
# 'v' refs are uintptr* and RetrieveRefs() writes 8 bytes, but get_ground_z got
# &fGroundZ (a 4-byte float on the stack) -> stack smash -> abort. Give it an
# 8-byte slot and take the low 32 bits as the float.
replace_exact(cpp / "game/game.cpp",
              "    float fGroundZ;\n    ScriptCommand(&get_ground_z, fX, fY, fZ, &fGroundZ);\n    return fGroundZ;",
              "    // Bastion: refs are written as uintptr (8 bytes) on arm64, a float is 4.\n"
              "    uintptr groundRef = 0;\n"
              "    ScriptCommand(&get_ground_z, fX, fY, fZ, &groundRef);\n"
              "    uint32_t groundBits = (uint32_t)groundRef;\n"
              "    float fGroundZ;\n"
              "    memcpy(&fGroundZ, &groundBits, sizeof(fGroundZ));\n"
              "    return fGroundZ;")

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

# ---------------------------------------------------------------------------
# 8. Server objects (CreateObject / mapping). Upstream GTA-2.11 stubbed CObject
#    (empty constructor/destructor/Process) and the object RPC handlers, so the
#    phone never showed any server object. Bodies come from cpp/*.cpp; a model the
#    client lacks is skipped, objects are visible in every interior like on PC.
#    Attachments are not supported yet; SetObjectMaterial: section 8b.
# ---------------------------------------------------------------------------
patch_dir = Path(__file__).resolve().parent / "cpp"
object_cpp = cpp / "game/object.cpp"
text = read(object_cpp)
start = text.find("CObject::CObject(int iModel")
end = text.find("// 0.3.7\nvoid CObject::SetRotation")
if start < 0 or end < 0 or "{/*" not in text[start:end]:
    raise SystemExit("object.cpp: stubbed CObject constructor/Process not found")
write(object_cpp, text[:start] + read(patch_dir / "object_core.cpp") + "\n" + text[end:])
replace_exact(object_cpp, """void CObject::MoveTo(float fX, float fY, float fZ, float fSpeed, float fRotX, float fRotY, float fRotZ)
{
""", """void CObject::MoveTo(float fX, float fY, float fZ, float fSpeed, float fRotX, float fRotY, float fRotZ)
{
	if (!m_pEntity) return;
""")
replace_exact(object_cpp, """void CObject::StopMoving()
{
""", """void CObject::StopMoving()
{
	if (!m_pEntity) { m_byteMoving &= ~1; return; }
""")
replace_exact(object_cpp, """void CObject::SetPos(float x, float y, float z)
{
    if (GamePool_Object_GetAt(m_dwGTAId))""", """void CObject::SetPos(float x, float y, float z)
{
    if (m_pEntity && GamePool_Object_GetAt(m_dwGTAId))""")
replace_exact(object_cpp, """void CObject::AttachToObject(CObject* pObject)
{
    if (GamePool_Object_GetAt(m_dwGTAId)) {""", """void CObject::AttachToObject(CObject* pObject)
{
    if (m_pEntity && pObject && pObject->m_pEntity && GamePool_Object_GetAt(m_dwGTAId)) {""")

replace_exact(cpp / "net/objectpool.h", "if (ObjectID <= MAX_OBJECTS && m_bObjectSlotState[ObjectID])",
              "if (ObjectID < MAX_OBJECTS && m_bObjectSlotState[ObjectID])")

script_rpc = cpp / "net/scriptrpc.cpp"
text = read(script_rpc)
rpc = read(patch_dir / "object_rpc.cpp")
start = text.find("void ScrCreateObject(RPCParameters* rpcParams)")
end = text.find("void ScrDestroyObject(RPCParameters *rpcParams)")
if start < 0 or end < 0 or "/*" not in text[start:end]:
    raise SystemExit("scriptrpc.cpp: commented ScrCreateObject not found")
create_end = rpc.find("void ScrSetObjectPos")
text = text[:start] + rpc[:create_end] + text[end:]
for name in ("ScrSetObjectPos", "ScrSetObjectRotation", "ScrMoveObject", "ScrStopObject"):
    stub = "void %s(RPCParameters* rpcParams)\n{\n\n}\n" % name
    if text.count(stub) != 1:
        raise SystemExit("scriptrpc.cpp: empty " + name + " not found")
    m = re.search(r"^void " + name + r"\(RPCParameters\* rpcParams\)\n\{.*?\n\}\n", rpc, re.S | re.M)
    text = text.replace(stub, m.group(0))
write(script_rpc, text)

# ---------------------------------------------------------------------------
# 8b. SetObjectMaterial (textures and colours; text is not drawn yet). Upstream
#     kept the RPC commented out, destroyed texdb textures in SetMaterial and its
#     render hook left the texture on the shared model geometry (every copy of the
#     model got it). Textures are found by name in texdb (GTA ones and our bst_*).
# ---------------------------------------------------------------------------
text = read(script_rpc)
start = text.find("void ScrSetObjectMaterial(RPCParameters* rpcParams)")
end = text.find("*/\n}\n", start)
if start < 0 or end < 0 or "/*" not in text[start:end]:
    raise SystemExit("scriptrpc.cpp: commented ScrSetObjectMaterial not found")
text = text[:start] + rpc[rpc.find("void ScrSetObjectMaterial"):] + text[end + len("*/\n}\n"):]
write(script_rpc, text)

text = read(object_cpp)
start = text.find("void CObject::SetMaterial(int iModel")
end = text.find("void CObject::SetMaterialText(")
if start < 0 or end < start:
    raise SystemExit("object.cpp: CObject::SetMaterial not found")
write(object_cpp, text[:start] + read(patch_dir / "object_material.cpp") + text[end:])

hooks_render = cpp / "game/hooks.cpp"
text = read(hooks_render)
start = text.find("void (*CObject_Render)(CObjectGta* thiz);")
end = text.find("/*((void (*)(void))(g_libGTASA + 0x5D1F48")
if start < 0 or end < start:
    raise SystemExit("hooks.cpp: CObject_Render_hook not found")
write(hooks_render, text[:start] + read(patch_dir / "object_render.cpp") + text[end:])

# ---------------------------------------------------------------------------
# 9. 3D text labels (text over markers, houses, doors). Upstream receives them
#    (C3DTextLabelPool) but never calls Render, so no label was ever drawn.
#    Draw them with the player tags; {RRGGBB} colours are kept, width is
#    measured without the colour codes so lines stay centred.
# ---------------------------------------------------------------------------
replace_exact(cpp / "gui/gui.cpp", """    if (pPlayerTags) pPlayerTags->Render(renderer());
""", """    if (pPlayerTags) pPlayerTags->Render(renderer());
    if (pNetGame && pNetGame->GetTextLabelPool()) pNetGame->GetTextLabelPool()->Render(renderer());
""")
label_cpp = cpp / "net/textlabelpool.cpp"
replace_exact(label_cpp, """                    ImVec2 sz = renderer->calculateTextSize(s_row, fontSize);""",
"""                    std::string plainRow;
                    for (size_t c = 0; c < s_row.size(); c++) {
                        if (s_row[c] == '{' && c + 7 < s_row.size() && s_row[c + 7] == '}') { c += 7; continue; }
                        plainRow += s_row[c];
                    }
                    if (plainRow.empty()) continue;
                    ImVec2 sz = renderer->calculateTextSize(plainRow, fontSize);""")

# ---------------------------------------------------------------------------
# 10. Russian keyboard. Upstream has only ENG lower/upper + NUM, and the
#     "lang" key just toggles letters/digits, so chat could not be typed in
#     Russian. Add RUS lower/upper (ЙЦУКЕН + Ё); the language key cycles
#     РУС -> 123 -> ENG -> РУС and shows where it goes. Input is cp1251 (what
#     the server expects): а-я = 0xE0-0xFF, А-Я = 0xC0-0xDF, ё 0xB8, Ё 0xA8.
#     Russian is the default layout.
# ---------------------------------------------------------------------------
kb_h = cpp / "gui/samp_widgets/keyboard.h"
replace_exact(kb_h, "\t\tENG_LOWER,\n\t\tENG_UPPER,\n\t\tNUM\n", "\t\tENG_LOWER,\n\t\tENG_UPPER,\n\t\tRUS_LOWER,\n\t\tRUS_UPPER,\n\t\tNUM\n")
replace_exact(kb_h, "class KeyboardLayoutNUM : public KeyboardLayout\n", """class KeyboardLayoutLowerRUS : public KeyboardLayout
{
public:
	KeyboardLayoutLowerRUS() : KeyboardLayout() {}
	virtual void performLayout() override;
};

class KeyboardLayoutUpperRUS : public KeyboardLayout
{
public:
	KeyboardLayoutUpperRUS() : KeyboardLayout() {}
	virtual void performLayout() override;
};

class KeyboardLayoutNUM : public KeyboardLayout
""")
replace_exact(kb_h, "\tKeyboardLayoutNUM* m_layoutNUM;\n", "\tKeyboardLayoutNUM* m_layoutNUM;\n\tKeyboardLayoutLowerRUS* m_layoutLowerRUS;\n\tKeyboardLayoutUpperRUS* m_layoutUpperRUS;\n")

kb = cpp / "gui/samp_widgets/keyboard.cpp"
replace_exact(kb, """    m_activeLayout = m_layoutLowerENG;
    m_activeLayoutType = KeyboardLayout::Type::ENG_LOWER;
    m_activeLayout->setVisible(true);
    m_isEngMode = true;
""", """    m_layoutLowerRUS = new KeyboardLayoutLowerRUS();
    this->addChild(m_layoutLowerRUS);
    m_layoutLowerRUS->setPadding(0.0f);
    m_layoutLowerRUS->setSpacing(0.0f);
    m_layoutLowerRUS->setVisible(false);

    m_layoutUpperRUS = new KeyboardLayoutUpperRUS();
    this->addChild(m_layoutUpperRUS);
    m_layoutUpperRUS->setPadding(0.0f);
    m_layoutUpperRUS->setSpacing(0.0f);
    m_layoutUpperRUS->setVisible(false);

    // Bastion: Russian by default
    m_activeLayout = m_layoutLowerRUS;
    m_activeLayoutType = KeyboardLayout::Type::RUS_LOWER;
    m_activeLayout->setVisible(true);
    m_isEngMode = false;
""")
replace_exact(kb, """    m_layoutNUM->setFixedSize(ImVec2(width(), height() - m_input->height()+50.0f));
    m_layoutNUM->setPosition(ImVec2(0.0f, UISettings::keyboardRowHeight()));

    Widget::performLayout();""", """    m_layoutNUM->setFixedSize(ImVec2(width(), height() - m_input->height()+50.0f));
    m_layoutNUM->setPosition(ImVec2(0.0f, UISettings::keyboardRowHeight()));
    m_layoutLowerRUS->setFixedSize(ImVec2(width(), height() - m_input->height()+50.0f));
    m_layoutLowerRUS->setPosition(ImVec2(0.0f, UISettings::keyboardRowHeight()));
    m_layoutUpperRUS->setFixedSize(ImVec2(width(), height() - m_input->height()+50.0f));
    m_layoutUpperRUS->setPosition(ImVec2(0.0f, UISettings::keyboardRowHeight()));

    Widget::performLayout();""")
replace_exact(kb, """        case KeyboardLayout::Type::NUM:
            m_layoutNUM->setVisible(true);
            m_activeLayout = m_layoutNUM;
            break;
    }
""", """        case KeyboardLayout::Type::NUM:
            m_layoutNUM->setVisible(true);
            m_activeLayout = m_layoutNUM;
            break;

        case KeyboardLayout::Type::RUS_LOWER:
            m_layoutLowerRUS->setVisible(true);
            m_activeLayout = m_layoutLowerRUS;
            break;

        case KeyboardLayout::Type::RUS_UPPER:
            m_layoutUpperRUS->setVisible(true);
            m_activeLayout = m_layoutUpperRUS;
            break;
    }
""")
replace_exact(kb, """            if (m_activeLayoutType == KeyboardLayout::Type::ENG_UPPER) setActiveLayout(KeyboardLayout::Type::ENG_LOWER);
            break;""", """            if (m_activeLayoutType == KeyboardLayout::Type::ENG_UPPER) setActiveLayout(KeyboardLayout::Type::ENG_LOWER);
            if (m_activeLayoutType == KeyboardLayout::Type::RUS_UPPER) setActiveLayout(KeyboardLayout::Type::RUS_LOWER);
            break;""")
replace_exact(kb, """            if (m_activeLayoutType == KeyboardLayout::Type::ENG_LOWER) setActiveLayout(KeyboardLayout::Type::ENG_UPPER);
            break;""", """            if (m_activeLayoutType == KeyboardLayout::Type::ENG_LOWER) setActiveLayout(KeyboardLayout::Type::ENG_UPPER);
            if (m_activeLayoutType == KeyboardLayout::Type::RUS_LOWER) setActiveLayout(KeyboardLayout::Type::RUS_UPPER);
            break;""")
kb_text = read(kb)
t_start = kb_text.find("void Keyboard::toggleLang()\n{")
t_end = kb_text.find("\n}\n", t_start)
if t_start < 0 or t_end < 0:
    raise SystemExit("keyboard.cpp: toggleLang not found")
kb_text = kb_text[:t_start] + """void Keyboard::toggleLang()
{
    // Bastion: RUS -> 123 -> ENG -> RUS
    switch (m_activeLayoutType)
    {
        case KeyboardLayout::Type::RUS_LOWER:
        case KeyboardLayout::Type::RUS_UPPER:
            setActiveLayout(KeyboardLayout::Type::NUM);
            break;
        case KeyboardLayout::Type::NUM:
            m_isEngMode = true;
            setActiveLayout(KeyboardLayout::Type::ENG_LOWER);
            break;
        default:
            m_isEngMode = false;
            setActiveLayout(KeyboardLayout::Type::RUS_LOWER);
            break;
    }""" + kb_text[t_end:]

# Captions of the language key: where it switches to.
def lang_caption(text, cls, caption):
    head = "void %s::performLayout()\n{" % cls
    i = text.find(head)
    j = text.find("m_rows[3]->addKey(ExtendedKey::LANG);", i)
    if i < 0 or j < 0:
        raise SystemExit("keyboard.cpp: LANG key of " + cls + " not found")
    return text[:j] + 'm_rows[3]->addKey(ExtendedKey::LANG)->setCaption(std::string("%s"));' % caption \
        + text[j + len("m_rows[3]->addKey(ExtendedKey::LANG);"):]
def c_utf8(s):
    return "".join("\\x%02x" % b for b in s.encode("utf-8"))
kb_text = lang_caption(kb_text, "KeyboardLayoutLowerENG", c_utf8("РУС"))
kb_text = lang_caption(kb_text, "KeyboardLayoutUpperENG", c_utf8("РУС"))
kb_text = lang_caption(kb_text, "KeyboardLayoutNUM", "ENG")

def rus_layout(cls, upper):
    rows = ["йцукенгшщзхъ", "фывапролджэё", "ячсмитьбю"]
    if upper:
        rows = [r.upper() for r in rows]
    out = ["void %s::performLayout()\n{" % cls,
           "    ImVec2 keySize = ImVec2(width() / 12, UISettings::keyboardRowHeight());"]
    for n, row in enumerate(rows):
        out.append("    m_rows[%d]->setPadding(0.0f);\n    m_rows[%d]->setSpacing(0.0f);" % (n, n))
        if n == 2:
            out.append("    m_rows[2]->addKey(ExtendedKey::%s)->setFixedSize(ImVec2(keySize.x * 1.5f, 0.0f));"
                       % ("SHIFT_OFF" if upper else "SHIFT_ON"))
        for ch in row:
            out.append('    m_rows[%d]->addKey(std::string("%s"), (char)0x%02X);' % (n, c_utf8(ch), ch.encode("cp1251")[0]))
        if n == 2:
            out.append("    m_rows[2]->addKey(ExtendedKey::BACKSPACE)->setFixedSize(ImVec2(keySize.x * 1.5f, 0.0f));")
    out.append("""    m_rows[3]->setPadding(0.0f);
    m_rows[3]->setSpacing(0.0f);
    m_rows[3]->addKey(std::string(","), ',');
    m_rows[3]->addKey(std::string("."), '.');
    m_rows[3]->addKey(ExtendedKey::LANG)->setCaption(std::string("123"));
    m_rows[3]->addKey(ExtendedKey::SPACE)->setFixedSize(ImVec2(keySize.x * 3.6f, 0.0f));
    m_rows[3]->addKey(std::string("?"), '?');
    m_rows[3]->addKey(std::string("!"), '!');
    m_rows[3]->addKey(ExtendedKey::SEND)->setFixedSize(ImVec2(keySize.x * 1.8f, 0.0f));

    KeyboardLayout::performLayout();
}
""")
    return "\n".join(out)
kb_text += "\n// Bastion: Russian layouts\n" + rus_layout("KeyboardLayoutLowerRUS", False) + "\n" + rus_layout("KeyboardLayoutUpperRUS", True)
write(kb, kb_text)
if "KeyboardLayoutLowerRUS::performLayout" not in read(kb) or "RUS_LOWER" not in read(kb_h):
    raise SystemExit("russian keyboard patch failed")

# ---------------------------------------------------------------------------
# 11. ApplyAnimation crashes. Upstream requests an unloaded IFP block and
#     plays the animation in the same frame (block not in memory yet), and
#     never checks the animation exists, e.g. the common preload trick
#     ApplyAnimation(.., "CRACK", "null", ..) used by the wounded/revive system.
#     Load the block synchronously and skip animations that do not exist.
# ---------------------------------------------------------------------------
replace_exact(cpp / "game/playerped.cpp", """    if(!pGame->IsAnimationLoaded(szAnimLib))
    {
        pGame->RequestAnimation(szAnimLib);
        ScriptCommand(&apply_animation, m_dwGTAId, szAnimName, szAnimLib, fT, opt1, opt2, opt3, opt4, iTime);
    }

    ScriptCommand(&apply_animation, m_dwGTAId, szAnimName, szAnimLib, fT, opt1, opt2, opt3, opt4, iTime);""",
"""    // Bastion: unknown block or animation -> skip instead of a native crash.
    CAnimBlock* bastionBlock = nullptr;
    for (int b = 0; b < CAnimManager::ms_numAnimBlocks; b++)
        if (!strcasecmp(CAnimManager::ms_aAnimBlocks[b].szName, szAnimLib)) { bastionBlock = &CAnimManager::ms_aAnimBlocks[b]; break; }
    if (!bastionBlock) { Log("[Bastion] ApplyAnimation: no anim block %s", szAnimLib); return; }

    if(!pGame->IsAnimationLoaded(szAnimLib))
    {
        pGame->RequestAnimation(szAnimLib);
        pGame->LoadRequestedModels();
        if (!pGame->IsAnimationLoaded(szAnimLib)) { Log("[Bastion] ApplyAnimation: block %s not loaded yet", szAnimLib); return; }
    }
    if (!CAnimManager::GetAnimation(szAnimName, bastionBlock)) { Log("[Bastion] ApplyAnimation: no anim %s:%s", szAnimLib, szAnimName); return; }

    ScriptCommand(&apply_animation, m_dwGTAId, szAnimName, szAnimLib, fT, opt1, opt2, opt3, opt4, iTime);""")
replace_exact(cpp / "game/playerped.cpp", '#include "World.h"\n', '#include "World.h"\n#include "Animation/AnimManager.h"\n')

# ---------------------------------------------------------------------------
# 12. Same arm64 bug as get_ground_z (section 1): ScriptCommand writes every
#     'v' (output) parameter back as 8 bytes, but these targets are 4-byte
#     ints, so the write overwrites the neighbouring field / stack slot.
#     Make the targets 8 bytes wide.
# ---------------------------------------------------------------------------
replace_exact(cpp / "game/playerped.cpp", "    static int iPlayerNum;", "    static uintptr iPlayerNum;")
replace_exact(cpp / "game/playerped.h", "    uint32_t\tm_dwArrow;", "    uintptr\tm_dwArrow;")
replace_exact(cpp / "game/playerped.h", "    uint32_t\tm_dwPissParticlesHandle;", "    uintptr\tm_dwPissParticlesHandle;")
replace_exact(cpp / "game/playerped.h", "        uint32_t dwObject;\n        uint32_t dwDrunkLevel;", "        uintptr dwObject;\n        uint32_t dwDrunkLevel;")
replace_exact(cpp / "game/vehicle.h", "\tuint32_t m_dwMarkerID;", "\tuintptr m_dwMarkerID;")
replace_exact(cpp / "game/vehicle.cpp", "\tuint32_t dwRet;", "\tuintptr dwRet = 0;")
replace_exact(cpp / "game/game.h", "\tuint32_t m_dwRaceCheckpointHandle;", "\tuintptr m_dwRaceCheckpointHandle;")

# ---------------------------------------------------------------------------
# 13. Keyboard in the Bastion style: dark panel (#0E1013) with an orange top
#     line, each key a rounded #1B1E23 tile (service keys #2B3440), the pressed
#     key and accents in Bastion orange #FF7A1A instead of the stock SA-MP look.
# ---------------------------------------------------------------------------
kb_text = read(kb)
if kb_text.count("ImColor(0xF5, 0x91, 0x32)") < 3:
    raise SystemExit("keyboard.cpp: stock accent colour not found")
kb_text = kb_text.replace("ImColor(0xF5, 0x91, 0x32)", "ImColor(0xFF, 0x7A, 0x1A)")
write(kb, kb_text)
replace_exact(kb, """    renderer->drawRect(absolutePosition(), absolutePosition() + size(), UISettings::keyboardBackgroundColor(), true);
    Widget::draw(renderer);""", """    // Bastion style
    renderer->drawRect(absolutePosition(), absolutePosition() + size(), ImColor(0x0E, 0x10, 0x13, 0xF0), true);
    renderer->drawRect(absolutePosition(), ImVec2(absolutePosition().x + size().x, absolutePosition().y + 3.0f),
                       ImColor(0xFF, 0x7A, 0x1A), true);
    Widget::draw(renderer);""")
replace_exact(kb, """    if (focused()) renderer->drawRect(absolutePosition(), absolutePosition() + size(), ImColor(0xFF, 0x7A, 0x1A), true);
""", """    {   // Bastion style: rounded tile per key
        const ImVec2 inset(3.0f, 3.0f);
        ImColor tile = m_extendedKey == ExtendedKey::NONE ? ImColor(0x1B, 0x1E, 0x23) : ImColor(0x2B, 0x34, 0x40);
        if (m_extendedKey == ExtendedKey::SEND) tile = ImColor(0xFF, 0x7A, 0x1A);
        if (focused()) tile = ImColor(0xFF, 0x7A, 0x1A);
        renderer->drawRect(absolutePosition() + inset, absolutePosition() + size() - inset, tile, true, 8.0f);
    }
""")

# ---------------------------------------------------------------------------
# 14. Phone keyboard in chat (launcher: "Клавиатура в чате: Телефона" ->
#     extra bastion.system_keyboard). Keyboard::show asks Java to open a bar
#     with an EditText (BastionSystemInput); the text comes back as cp1251 via
#     JNI, is queued and handed to the chat on the render thread.
# ---------------------------------------------------------------------------
shutil.copy2(Path(__file__).resolve().parent / "java/BastionSystemInput.java", samp_java.parent / "BastionSystemInput.java")
replace_exact(samp_java, """    private BastionWebUi mWebUi;
""", """    private BastionWebUi mWebUi;
    private BastionSystemInput mSystemInput;

    /** Bastion: called from native Keyboard::show. false -> use the built-in keyboard. */
    public boolean showSystemInput() {
        if (!BastionSystemInput.enabled(this)) return false;
        runOnUiThread(() -> mSystemInput.show());
        return true;
    }
""")
replace_exact(samp_java, """        mWebUi      = new BastionWebUi(this, mDialog);
""", """        mWebUi      = new BastionWebUi(this, mDialog);
        mSystemInput = new BastionSystemInput(this);
""")
replace_exact(samp_java, """                mWebUi.setPaused(pause);
""", """                mWebUi.setPaused(pause);
                mSystemInput.setPaused(pause);
""")
jni_h = cpp / "java/jniutil.h"
replace_exact(jni_h, "\tvoid ShowEditObject();\n", "\tvoid ShowEditObject();\n\tbool ShowSystemInput();\n\tjmethodID s_showSystemInput = nullptr;\n")
jni_cpp = cpp / "java/jniutil.cpp"
replace_exact(jni_cpp, """    s_hideEditObject = env->GetMethodID(clas, "hideEditObject", "()V");
""", """    s_hideEditObject = env->GetMethodID(clas, "hideEditObject", "()V");

    s_showSystemInput = env->GetMethodID(clas, "showSystemInput", "()Z");
    if (env->ExceptionCheck()) { env->ExceptionClear(); s_showSystemInput = nullptr; }
""")
write(jni_cpp, read(jni_cpp) + """
// Bastion: phone keyboard for the chat. false -> built-in keyboard.
bool CJavaWrapper::ShowSystemInput()
{
    if (!s_showSystemInput) return false;
    JNIEnv* env = GetEnv();
    if (!env) return false;
    jboolean ok = env->CallBooleanMethod(activity, s_showSystemInput);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }
    return ok == JNI_TRUE;
}
""")
replace_exact(kb_h, "\tvoid activateEvent(bool active) override;\n", "\tvoid activateEvent(bool active) override;\n\tvoid systemInputResult(const std::string& text, bool sent);\n\tstatic void processSystemInput();\n")
replace_exact(kb, """void Keyboard::show(Widget* caller)
{
""", """// Bastion: answers of the phone keyboard (JNI, UI thread) -> render thread.
#include <mutex>
#include <deque>
static std::mutex s_bastionInputMutex;
static std::deque<std::pair<std::string, bool>> s_bastionInputQueue;
static bool s_bastionSystemInputOpen = false;

extern "C" JNIEXPORT void JNICALL
Java_com_gta_game_BastionSystemInput_onInput(JNIEnv* env, jclass, jbyteArray text, jboolean sent)
{
    std::string s;
    if (text != nullptr)
    {
        jsize len = env->GetArrayLength(text);
        jbyte* bytes = env->GetByteArrayElements(text, nullptr);
        if (bytes) { s.assign((const char*)bytes, (size_t)len); env->ReleaseByteArrayElements(text, bytes, JNI_ABORT); }
    }
    std::lock_guard<std::mutex> lock(s_bastionInputMutex);
    s_bastionInputQueue.emplace_back(s, sent == JNI_TRUE);
}

void Keyboard::processSystemInput()
{
    std::deque<std::pair<std::string, bool>> items;
    {
        std::lock_guard<std::mutex> lock(s_bastionInputMutex);
        items.swap(s_bastionInputQueue);
    }
    for (auto& it : items)
        if (pUI && pUI->keyboard()) pUI->keyboard()->systemInputResult(it.first, it.second);
}

void Keyboard::systemInputResult(const std::string& text, bool sent)
{
    if (!s_bastionSystemInputOpen) return;
    s_bastionSystemInputOpen = false;
    pGame->EnableGameInput(true);
    Widget* caller = m_caller;
    m_caller = nullptr;
    if (sent && !text.empty())
    {
        m_history->add(text);
        if (caller) caller->keyboardEvent(text);
    }
}

void Keyboard::show(Widget* caller)
{
    if (pJavaWrapper && pJavaWrapper->ShowSystemInput())
    {
        m_caller = caller;
        s_bastionSystemInputOpen = true;
        pGame->EnableGameInput(false);
        return;
    }
""")
replace_exact(cpp / "gui/gui.cpp", """    if (pPlayerTags) pPlayerTags->Render(renderer());
""", """    Keyboard::processSystemInput();
    if (pPlayerTags) pPlayerTags->Render(renderer());
""")

# ---------------------------------------------------------------------------
# 15. /rec (reconnect). open.mp cannot make one client time out: the server kicks,
#     and the stock client stops at "Server closed the connection". Before the kick
#     the server sends a hidden dialog captioned "[bastion:rec]" (only to clients
#     whose version has "-bui2" or newer). The client remembers it and treats the
#     following disconnect like a lost connection, so it reconnects by itself.
# ---------------------------------------------------------------------------
replace_exact(cpp / "net/netgame.cpp", "int iNetModeSendMultiplier = 2;\n",
"int iNetModeSendMultiplier = 2;\nbool g_bBastionRec = false;   // Bastion: server asked for /rec, next kick = reconnect\n")
replace_exact(cpp / "net/netgame.cpp", """void CNetGame::Packet_DisconnectionNotification(Packet *pkt)
{
""", """void CNetGame::Packet_DisconnectionNotification(Packet *pkt)
{
	if (g_bBastionRec)
	{
		g_bBastionRec = false;
		Packet_ConnectionLost(pkt);
		return;
	}
""")
replace_exact(cpp / "net/netrpc.cpp", "	stringCompressor->DecodeString(szMessage, 4096, &bsData);\n",
"""	stringCompressor->DecodeString(szMessage, 4096, &bsData);
	if (!strcmp(szTitle, "[bastion:rec]"))
	{
		extern bool g_bBastionRec;
		g_bBastionRec = true;   // the server kicks right after this; reconnect instead of stopping
		return;
	}
""")

# ---------------------------------------------------------------------------
# 16. Own HUD panel (hud.inc -> assets/ui/index.html, BastionUI.hud). The server
#     sends a hidden dialog "[hud]" with JSON (health, armour, hunger, cash, bank)
#     only while no other dialog is open. Java passes it to the page without
#     touching the open window; native code stops drawing the stock health/armour
#     bars and money (cpp/bastion_hud.cpp). {"off":1} (/hud) brings them back.
# ---------------------------------------------------------------------------
hooks_cpp = cpp / "game/hooks.cpp"
replace_exact(hooks_cpp, "void InstallHooks()\n", read(patch_dir / "bastion_hud.cpp") + "\nvoid InstallHooks()\n")
replace_exact(hooks_cpp, """    CHook::InlineHook("_ZN4CHud14DrawCrossHairsEv", &DrawCrosshair_hook, &DrawCrosshair);
""", """    CHook::InlineHook("_ZN4CHud14DrawCrossHairsEv", &DrawCrosshair_hook, &DrawCrosshair);
    BastionInstallHudHooks();
""")
replace_exact(cpp / "net/netrpc.cpp", """	if (!strcmp(szTitle, "[bastion:rec]"))
""", """	if (!strncmp(szTitle, "[hud]", 5))
	{
		extern volatile bool g_bastionHud;
		g_bastionHud = strstr(szMessage, "\\"off\\"") == nullptr;   // the page draws health, armour and money
	}
	if (!strcmp(szTitle, "[bastion:rec]"))
""")
replace_exact(jni_h, "\tbool ShowSystemInput();\n", "\tbool ShowSystemInput();\n\tvoid SetBreath(int pct);\n\tjmethodID s_bastionBreath = nullptr;\n")
replace_exact(jni_cpp, """    if (env->ExceptionCheck()) { env->ExceptionClear(); s_showSystemInput = nullptr; }
""", """    if (env->ExceptionCheck()) { env->ExceptionClear(); s_showSystemInput = nullptr; }
    s_bastionBreath = env->GetMethodID(clas, "bastionBreath", "(I)V");
    if (env->ExceptionCheck()) { env->ExceptionClear(); s_bastionBreath = nullptr; }
""")
write(jni_cpp, read(jni_cpp) + """
// Bastion: air under water for the HUD page (BastionUI.breath), -1 hides it.
void CJavaWrapper::SetBreath(int pct)
{
    if (!s_bastionBreath) return;
    JNIEnv* env = GetEnv();
    if (!env) return;
    env->CallVoidMethod(activity, s_bastionBreath, pct);
    if (env->ExceptionCheck()) env->ExceptionClear();
}
""")
replace_exact(samp_java, """    /** Bastion: called from native Keyboard::show.""", """    /** Bastion: called from native CWidgetPlayerInfo (breath bar under water), -1 hides it. */
    public void bastionBreath(int pct) {
        runOnUiThread(() -> mWebUi.breath(pct));
    }

    /** Bastion: called from native Keyboard::show.""")
replace_exact(samp_java, """            if (BastionWebUi.wants(caption)) {""", """            if (BastionWebUi.isHud(caption)) {
                this.mWebUi.hud(content);   // HUD data, not a window: the open dialog stays
                return;
            }
            if (BastionWebUi.wants(caption)) {""")

# ---------------------------------------------------------------------------
# 17. Our textures on the phone (cpp/bastion_textures.cpp): radar icons, interiors
#     and markers from the PC client pack replace texdb textures by name. Files come
#     from the launcher (assets/bastion-mods -> <game>/SAMP/bastion/tex/), loaded
#     once right after RenderWare starts.
# ---------------------------------------------------------------------------
replace_exact(hooks_cpp, "void InstallHooks()\n", read(patch_dir / "bastion_textures.cpp") + "\nvoid InstallHooks()\n")
replace_exact(hooks_cpp, """    BastionInstallHudHooks();
""", """    BastionInstallHudHooks();
    BastionInstallTextureHooks();
""")
replace_exact(hooks_cpp, "bool (*CGame__InitialiseRenderWare)();\n",
              "static void BastionLoadTextures();\nbool (*CGame__InitialiseRenderWare)();\n")
replace_exact(hooks_cpp, """    TextureDatabaseRuntime::Load("samp", false, TextureDatabaseFormat::DF_Default);

    InitGui();""", """    TextureDatabaseRuntime::Load("samp", false, TextureDatabaseFormat::DF_Default);
    BastionLoadTextures();

    InitGui();""")
replace_exact(cpp / "CMakeLists.txt", """target_link_libraries(samp
        log
""", """target_link_libraries(samp
        log
        z
""")

# Spawning and changing uniform both replace the RenderWare object of a live ped.
# Delete it while its original model index still identifies its streaming reference.
playerped = cpp / "game/playerped.cpp"
replace_exact(playerped, """    if(!GamePool_Ped_GetAt(m_dwGTAId)) return;
    if(!IsPedModel(uiModel))""", """    if (!m_pPed || GamePool_Ped_GetAt(m_dwGTAId) != m_pPed) return;
    if(!IsPedModel(uiModel))""")
replace_exact(playerped, """        if (!CStreaming::TryLoadModel(uiModel))
            return;

        // CEntity::DeleteRwObject();
        m_pPed->m_nModelIndex = uiModel;

        m_pPed->SetModelIndex(uiModel);

        CStreaming::RemoveModelIfNoRefs(oldModelId);""", """        if (oldModelId == uiModel && m_pPed->m_pRwObject) return;
        if (!CStreaming::TryLoadModel(uiModel)) return;
        if (GamePool_Ped_GetAt(m_dwGTAId) != m_pPed) return;

        if (m_pPed->m_pRwObject) m_pPed->DeleteRwObject();
        m_pPed->SetModelIndex(uiModel);

        // Let streaming evict the previous model normally; immediate removal
        // during the spawn/render transition can invalidate pending references.
        if (oldModelId != uiModel && oldModelId < CModelInfo::NUM_MODEL_INFOS &&
            CModelInfo::GetModelInfo(oldModelId))
            CStreaming::SetModelIsDeletable(oldModelId);""")

# 18. Checkpoint handles must start at zero. Login/spawn can disable them
# before any checkpoint exists; UpdateCheckpoints must not delete a marker twice.
replace_exact(cpp / "game/game.cpp", "\tm_bRaceCheckpointsEnabled = false;\n\tm_dwRaceCheckpointHandle = 0;",
              "\tm_bRaceCheckpointsEnabled = false;\n\tm_dwCheckpointMarker = 0;\n\tm_dwRaceCheckpointMarker = 0;\n\tm_dwRaceCheckpointHandle = 0;")
replace_exact(cpp / "game/game.cpp", "\telse if (m_dwRaceCheckpointMarker)\n\t{\n\t\tDisableMarker(m_dwRaceCheckpointMarker);\n\t\tDisableRaceCheckpoint();\n\t\tm_dwRaceCheckpointMarker = 0;\n\t}",
              "\telse if (m_dwRaceCheckpointMarker || m_dwRaceCheckpointHandle)\n\t{\n\t\tDisableRaceCheckpoint();\n\t}")
replace_exact(cpp / "game/game.cpp", "    Log(\"DisableRaceCheckpoint\");\n    if (m_dwRaceCheckpointHandle != NULL)",
              "    Log(\"DisableRaceCheckpoint: handle=%u marker=%u\", (unsigned)m_dwRaceCheckpointHandle, (unsigned)m_dwRaceCheckpointMarker);\n    if (m_dwRaceCheckpointHandle != NULL)")

# 19. Spawn crash follow-up: preserve original Android crash context, initialize
# pickup output, and isolate the failing spawn operation in the client log.
replace_exact(cpp / "game/game.cpp", "    uintptr hnd;\n\n    auto dwModelArray",
              "    uintptr hnd = 0;\n\n    auto dwModelArray")
replace_exact(cpp / "main.cpp", "\tsigaction(SIGSEGV, &act, &act_old);",
              "\t// Preserve Android's native SIGSEGV handler and original fault context.\n\t// The upstream handler calls ART's previous handler directly, then unwinds\n\t// the signal handler itself; this can obscure the original crashing frame.\n\t// sigaction(SIGSEGV, &act, &act_old);")
replace_exact(cpp / "net/localplayer.cpp", "\tpGame->DisableTrainTraffic();\n\n\tm_pPlayerPed->m_pPed->SetPosn(m_SpawnInfo.vecPos.x,\n\t\tm_SpawnInfo.vecPos.y, m_SpawnInfo.vecPos.z + 0.5f);\n\n\tm_pPlayerPed->SetTargetRotation(m_SpawnInfo.fRotation);",
              "\tLog(\"[Bastion] spawn: disable train traffic\");\n\tpGame->DisableTrainTraffic();\n\tLog(\"[Bastion] spawn: teleport\");\n\tif (!m_pPlayerPed->m_pPed || GamePool_Ped_GetAt(m_pPlayerPed->m_dwGTAId) != m_pPlayerPed->m_pPed) {\n\t\tLog(\"[Bastion] spawn: ped no longer valid\");\n\t\treturn false;\n\t}\n\tm_pPlayerPed->m_pPed->SetPosn(m_SpawnInfo.vecPos.x,\n\t\tm_SpawnInfo.vecPos.y, m_SpawnInfo.vecPos.z + 0.5f);\n\n\tLog(\"[Bastion] spawn: rotation\");\n\tm_pPlayerPed->SetTargetRotation(m_SpawnInfo.fRotation);\n\tLog(\"[Bastion] spawn: rotation complete\");")
replace_exact(cpp / "net/localplayer.cpp", "\tm_iDisplayZoneTick = GetTickCount() + 1000;\n\n\treturn true;",
              "\tm_iDisplayZoneTick = GetTickCount() + 1000;\n\tLog(\"[Bastion] spawn: complete\");\n\n\treturn true;")

# 20. Retain first-active-tick diagnostic; V3 reproduced without skin changes.
replace_exact(cpp / "net/localplayer.cpp", "bool bFirstSpawn = true;",
              "bool bFirstSpawn = true;\nstatic bool bastionTraceFirstActive = false;")
replace_exact(cpp / "net/localplayer.cpp", "\tif (m_bIsActive && m_pPlayerPed != nullptr)\n\t{\n\t\t// local player is dead",
              "\tif (m_bIsActive && m_pPlayerPed != nullptr)\n\t{\n\t\tconst bool trace = bastionTraceFirstActive;\n\t\tbastionTraceFirstActive = false;\n\t\tif (trace) Log(\"[Bastion TEST] first active tick: start\");\n\t\t// local player is dead")
replace_exact(cpp / "net/localplayer.cpp", "\t\tpGame->UpdateCheckpoints();",
              "\t\tif (trace) Log(\"[Bastion TEST] first active tick: checkpoints\");\n\t\tpGame->UpdateCheckpoints();")
replace_exact(cpp / "net/localplayer.cpp", "\t\tUpdateSurfing();\n\t\tCheckWeapons();",
              "\t\tif (trace) Log(\"[Bastion TEST] first active tick: weapons\");\n\t\tUpdateSurfing();\n\t\tCheckWeapons();")
replace_exact(cpp / "net/localplayer.cpp", "\t\tUpdateCameraTarget();\n\t\t// PLAYER DATA UPDATES",
              "\t\tUpdateCameraTarget();\n\t\tif (trace) Log(\"[Bastion TEST] first active tick: sync\");\n\t\t// PLAYER DATA UPDATES")
replace_exact(cpp / "net/localplayer.cpp", "\tm_bIsWasted = false;\n\tm_bIsActive = true;",
              "\tm_bIsWasted = false;\n\tbastionTraceFirstActive = true;\n\tm_bIsActive = true;")

# 21. V4: texture DB lifetime and duplicate ARM64 hooks. The V3 test ruled out
# local skin switching as a necessary condition of this crash.
replace_exact(cpp / "util/CUtil.cpp", "    TextureDatabaseRuntime::Register(db_handle);\n\n    auto tex = CUtil::GetTexture(texture);\n    if(!tex)\n    {\n        Log(\"Error: Texture (%s) not found in database (%s)\", dbname, texture);\n        return nullptr;\n    }\n\n    TextureDatabaseRuntime::Unregister(db_handle);\n\n    return tex;",
              "    // Register is idempotent, not reference counted. Preserve registrations\n    // owned by GTA and undo only registrations made by this lookup, including misses.\n    // ARM64 GTA 2.11: the actual engine array is at 0xd13d40 (verified ELF symbol).\n    auto& registered = *reinterpret_cast<TDBArray<TextureDatabaseRuntime*>*>(g_libGTASA + 0xd13d40);\n    bool alreadyRegistered = false;\n    for (unsigned i = 0; i < registered.numEntries; ++i) {\n        if (registered.dataPtr[i] == db_handle) { alreadyRegistered = true; break; }\n    }\n    if (!alreadyRegistered) TextureDatabaseRuntime::Register(db_handle);\n    auto tex = CUtil::GetTexture(texture);\n    if (!alreadyRegistered) TextureDatabaseRuntime::Unregister(db_handle);\n    if (!tex) Log(\"Error: Texture (%s) not found in database (%s)\", dbname, texture);\n    return tex;")
replace_exact(cpp / "game/hooks.cpp", "    CHook::InlineHook(\"_ZN16CTaskSimpleGetUp10ProcessPedEP4CPed\", &CTaskSimpleGetUp__ProcessPed_hook, &CTaskSimpleGetUp__ProcessPed); // CTaskSimpleGetUp::ProcessPed\n    CHook::InlineHook(\"_ZN7CObject6RenderEv\", &CObject_Render_hook, & CObject_Render);",
              "    CHook::InlineHook(\"_ZN16CTaskSimpleGetUp10ProcessPedEP4CPed\", &CTaskSimpleGetUp__ProcessPed_hook, &CTaskSimpleGetUp__ProcessPed); // CTaskSimpleGetUp::ProcessPed\n    // CObject::Render is installed once below, not twice.")
replace_exact(cpp / "game/hooks.cpp", "#if  0\n    CHook::InlineHook(g_libGTASA + 0x005AA200 + 1, &CFont_PrintString_hook, &CFont_PrintString);\n#else\n    CHook::InlineHook(g_libGTASA + 0x5D6EC4, &CFont_PrintString_hook, &CFont_PrintString);\n#endif",
              "    // BastionInstallHudHooks installs the sole CFont::PrintString hook.")
replace_exact(cpp / "game/textdraw.cpp", "    RwTexture* tex;\n    if (strncmp(szSlot, \"none\", 5u))",
              "    RwTexture* tex = nullptr;\n    if (strncmp(szSlot, \"none\", 5u))")
replace_exact(cpp / "game/textdraw.cpp", "    if (m_TextDrawData.iTextureSlot != -1 && m_TextDrawData.iTextureSlot != 0x0 && TextDrawTexture[m_TextDrawData.iTextureSlot]) RwTextureDestroy((RwTexture*)TextDrawTexture[m_TextDrawData.iTextureSlot]);\n    DestroyTextDrawTexture(m_TextDrawData.iTextureSlot);",
              "    DestroyTextDrawTexture(m_TextDrawData.iTextureSlot);")
replace_exact(cpp / "game/textdraw.cpp", "void CTextDraw::DrawTextured()\n{",
              "void CTextDraw::DrawTextured()\n{\n    if (m_TextDrawData.iTextureSlot < 0 || m_TextDrawData.iTextureSlot >= 200) return;")
replace_exact(cpp / "game/textdraw.cpp", "        DestroyTextDrawTexture(m_TextDrawData.iTextureSlot);\n        LoadTexture();",
              "        DestroyTextDrawTexture(m_TextDrawData.iTextureSlot);\n        m_TextDrawData.iTextureSlot = GetFreeTextDrawTextureSlot();\n        LoadTexture();")
replace_exact(cpp / "game/font.cpp", "\tuint16_t* gxt_string = new uint16_t[0xFF];\n\tCFont::AsciiToGxtChar(string, gxt_string);\n\t((void (*)(float, float, uint16_t*))(g_libGTASA + 0x5D6EC4))(posX, posY, gxt_string);\n\tdelete gxt_string;\n\t((void (*)())(g_libGTASA + 0x5D6090))();//53411C ; _DWORD CFont::RenderFontBuffer(CFont *__hidden this)",
              "\tif (!string) return;\n\tuint16_t* gxt_string = new uint16_t[strlen(string) + 1]();\n\tCFont::AsciiToGxtChar(string, gxt_string);\n\t((void (*)(float, float, uint16_t*))(g_libGTASA + 0x5D6EC4))(posX, posY, gxt_string);\n\t((void (*)())(g_libGTASA + 0x5D6090))();//53411C ; _DWORD CFont::RenderFontBuffer(CFont *__hidden this)\n\tdelete[] gxt_string;")
# Keep the existing clock filter in the single HUD font hook.
replace_exact(hooks_cpp, "    BastionPrintString(x, y, text);",
              "    if (ShouldHideHudClock(x, y, text)) return;\n    BastionPrintString(x, y, text);")
# Diagnostic isolation: retain objects, but do not mutate their shared geometry
# during render. This temporarily removes per-object custom colours/textures.
replace_exact(hooks_cpp, "    CHook::InlineHook(\"_ZN7CObject6RenderEv\", &CObject_Render_hook, & CObject_Render);",
              "    Log(\"[Bastion TEST V4] object material render override disabled\");")

# 22. V5: isolate the native ImGui render after spawn (diagnostic only).
# V3/V4 register x24 points at libsamp's Debug##Default string; native UI
# is a new candidate, not a proven root cause. Keep GTA world and network live.
replace_exact(cpp / "game/hooks.cpp", "void MainLoop();\nvoid(*Render2dStuff)();\nvoid Render2dStuff_hook()\n{\n    Render2dStuff();\n    MainLoop();\n\n    if (pNetGame && pGame && !pGame->IsGamePaused())\n    {\n        CObjectPool* pObjectPool = pNetGame->GetObjectPool();\n        if (pObjectPool) {\n            pObjectPool->Process();\n            pObjectPool->ProcessMaterialText();\n        }\n\n        CTextDrawPool* pTextDrawPool = pNetGame->GetTextDrawPool();\n        if (pTextDrawPool && pTextDrawPool->GetState() == false) {\n            pTextDrawPool->SnapshotProcess();\n        }\n    }\n\n    if (pUI) pUI->render();\n}",
              "void MainLoop();\nvoid(*Render2dStuff)();\nextern bool g_BastionV5Spawned;\nvoid Render2dStuff_hook()\n{\n    static unsigned frameAfterSpawn = 0;\n    const bool trace = g_BastionV5Spawned && frameAfterSpawn < 120;\n    if (trace) Log(\"[Bastion V5] frame %u: GTA HUD begin\", frameAfterSpawn);\n    Render2dStuff();\n    if (trace) Log(\"[Bastion V5] frame %u: GTA HUD done, network begin\", frameAfterSpawn);\n    MainLoop();\n    if (trace) Log(\"[Bastion V5] frame %u: network done\", frameAfterSpawn);\n\n    if (pNetGame && pGame && !pGame->IsGamePaused())\n    {\n        CObjectPool* pObjectPool = pNetGame->GetObjectPool();\n        if (pObjectPool) {\n            pObjectPool->Process();\n            pObjectPool->ProcessMaterialText();\n        }\n        if (trace) Log(\"[Bastion V5] frame %u: objects done\", frameAfterSpawn);\n\n        CTextDrawPool* pTextDrawPool = pNetGame->GetTextDrawPool();\n        if (pTextDrawPool && pTextDrawPool->GetState() == false) {\n            pTextDrawPool->SnapshotProcess();\n        }\n    }\n\n    // Diagnostic: leave the normal login UI intact, but skip ImGui after spawn.\n    // Java/WebView UI, engine HUD and networking remain enabled.\n    if (g_BastionV5Spawned) {\n        if (frameAfterSpawn == 0) Log(\"[Bastion V5] native ImGui render disabled after spawn\");\n        if (trace) Log(\"[Bastion V5] frame %u: complete\", frameAfterSpawn);\n        if (frameAfterSpawn < 120) ++frameAfterSpawn;\n    } else if (pUI) {\n        pUI->render();\n    }\n}")
replace_exact(cpp / "net/localplayer.cpp", "static bool bastionTraceFirstActive = false;",
              "static bool bastionTraceFirstActive = false;\nbool g_BastionV5Spawned = false;")
replace_exact(cpp / "net/localplayer.cpp", "\tLog(\"[Bastion] spawn: complete\");",
              "\tg_BastionV5Spawned = true;\n\tLog(\"[Bastion] spawn: complete\");")
replace_exact(cpp / "gui/imguiwrapper.cpp", "\t\t\tdelete m_vertexBuffer;", "\t\t\tdelete[] m_vertexBuffer;")

print("SAMP-Mobile prepared for Bastion: library module, Bastion storage, :game process, no Firebase.")

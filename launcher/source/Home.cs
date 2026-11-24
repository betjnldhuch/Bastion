// =====================================================================
//  Своя папка лаунчера: %LocalAppData%\Bastion Online\Launcher
//  Лаунчер, запущенный из другого места (Загрузки, рабочий стол, Program Files,
//  прямо из zip), копирует себя туда, ставит ярлык на рабочий стол и в Пуск
//  и перезапускается уже оттуда. Шрифт и пакет FPS вшиты в exe и
//  выкладываются рядом, если их нет: хватает одного BastionLauncher.exe.
//  portable = true в launcher.ini рядом с exe — не переезжать (для своих сборок).
// =====================================================================
using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;

namespace BastionLauncher
{
    static class Home
    {
        const string ExeName = "BastionLauncher.exe";
        const string ShortcutName = "Bastion Online.lnk";

        // %LocalAppData%\Bastion Online — запись без прав администратора
        public static string Root
        {
            get
            {
                return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                                    "Bastion Online");
            }
        }
        public static string LauncherDir { get { return Path.Combine(Root, "Launcher"); } }
        // Папка для игры по умолчанию (кнопка УСТАНОВИТЬ)
        public static string GameDir { get { return Path.Combine(Root, "Game"); } }

        static string Norm(string p)
        {
            return Path.GetFullPath(p).TrimEnd('\\', '/').ToLowerInvariant();
        }

        public static bool IsHome
        {
            get
            {
                try { return Norm(AppDomain.CurrentDomain.BaseDirectory) == Norm(LauncherDir); }
                catch (Exception) { return false; }
            }
        }

        static bool Portable(string dir)
        {
            try
            {
                string ini = Path.Combine(dir, "launcher.ini");
                if (!File.Exists(ini)) return false;
                foreach (string raw in File.ReadAllLines(ini))
                {
                    string l = raw.Trim().ToLowerInvariant().Replace(" ", "");
                    if (l == "portable=true" || l == "portable=1") return true;
                }
            }
            catch (Exception) { }
            return false;
        }

        // true — запущена копия в своей папке, этот процесс надо закрыть
        public static bool Relocate()
        {
            string here = AppDomain.CurrentDomain.BaseDirectory;
            if (IsHome || Portable(here)) { Unpack(here); return false; }
            try
            {
                string dir = LauncherDir;
                Directory.CreateDirectory(dir);
                string self = Assembly.GetExecutingAssembly().Location;
                string target = Path.Combine(dir, ExeName);

                // Не откатывать: в своей папке может стоять уже обновлённая версия
                bool copyExe = !File.Exists(target);
                if (!copyExe)
                {
                    try { copyExe = AssemblyName.GetAssemblyName(target).Version.CompareTo(Updater.Current) < 0; }
                    catch (Exception) { copyExe = true; }
                }
                if (copyExe) File.Copy(self, target, true);

                // Настройки и свои файлы игрока переносим, если в новой папке их ещё нет
                foreach (string f in new string[] { "launcher.ini", "news.txt", "background.jpg", "servers.cache", "news.cache" })
                    CopyIfMissing(Path.Combine(here, f), Path.Combine(dir, f));
                foreach (string sub in new string[] { "fonts", "fps" })
                {
                    string src = Path.Combine(here, sub);
                    if (!Directory.Exists(src)) continue;
                    foreach (string f in Directory.GetFiles(src))
                        CopyIfMissing(f, Path.Combine(dir, sub, Path.GetFileName(f)));
                }
                Unpack(dir);
                MakeShortcuts(target);

                ProcessStartInfo psi = new ProcessStartInfo(target);
                psi.WorkingDirectory = dir;
                psi.UseShellExecute = false;
                Process.Start(psi);
                return true;
            }
            catch (Exception)
            {
                // Не вышло (нет места, антивирус) — работаем там, где запустили
                Unpack(here);
                return false;
            }
        }

        static void CopyIfMissing(string src, string dst)
        {
            try
            {
                if (!File.Exists(src) || File.Exists(dst)) return;
                Directory.CreateDirectory(Path.GetDirectoryName(dst));
                File.Copy(src, dst);
            }
            catch (Exception) { }
        }

        // Вшитые файлы (ресурсы home/fonts/…, home/fps/…) — на диск, если их нет.
        // Изменённый игроком fps.ini и т.п. не перезаписываются.
        public static void Unpack(string dir)
        {
            Assembly a = Assembly.GetExecutingAssembly();
            foreach (string name in a.GetManifestResourceNames())
            {
                if (!name.StartsWith("home/")) continue;
                try
                {
                    string rel = name.Substring(5).Replace('/', Path.DirectorySeparatorChar);
                    string dst = Path.Combine(dir, rel);
                    if (File.Exists(dst)) continue;
                    Directory.CreateDirectory(Path.GetDirectoryName(dst));
                    using (Stream s = a.GetManifestResourceStream(name))
                    using (FileStream o = File.Create(dst)) s.CopyTo(o);
                }
                catch (Exception) { }
            }
        }

        // Ярлыки «Bastion Online» на рабочем столе и в Пуске (через WScript.Shell)
        static void MakeShortcuts(string exe)
        {
            string[] dirs = {
                Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory),
                Environment.GetFolderPath(Environment.SpecialFolder.Programs)
            };
            foreach (string d in dirs)
            {
                try
                {
                    if (d.Length == 0) continue;
                    string lnk = Path.Combine(d, ShortcutName);
                    Type t = Type.GetTypeFromProgID("WScript.Shell");
                    if (t == null) return;
                    object shell = Activator.CreateInstance(t);
                    object sc = t.InvokeMember("CreateShortcut", BindingFlags.InvokeMethod, null, shell, new object[] { lnk });
                    Type st = sc.GetType();
                    st.InvokeMember("TargetPath", BindingFlags.SetProperty, null, sc, new object[] { exe });
                    st.InvokeMember("WorkingDirectory", BindingFlags.SetProperty, null, sc, new object[] { Path.GetDirectoryName(exe) });
                    st.InvokeMember("IconLocation", BindingFlags.SetProperty, null, sc, new object[] { exe + ",0" });
                    st.InvokeMember("Description", BindingFlags.SetProperty, null, sc, new object[] { "Bastion Online" });
                    st.InvokeMember("Save", BindingFlags.InvokeMethod, null, sc, null);
                }
                catch (Exception) { }
            }
        }
    }
}

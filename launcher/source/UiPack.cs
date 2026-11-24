// =====================================================================
//  Красивые окна Bastion на ПК (bastion_ui.asi + браузер CEF), как на телефоне.
//
//  Перед запуском игры лаунчер сверяет установленную версию с релизом
//  bastion-ui на GitHub (собирает .github/workflows/bastion-ui-pc.yml):
//    BastionUI-PC.sha256 — SHA-256 архива, BastionUI-PC.zip — сам архив.
//  Если версия новая или плагина нет — скачивает и распаковывает в папку игры:
//  bastion_ui.asi, папку bastion_ui\ и dinput8.dll (ASI Loader, только если
//  его ещё нет). Ошибка здесь не мешает запуску: окна будут обычными.
//  ui_url в launcher.ini: своя ссылка на архив или «-» — не ставить.
// =====================================================================
using System;
using System.IO;
using System.IO.Compression;
using System.Text;

namespace BastionLauncher
{
    static class UiPack
    {
        public const string DefaultUrl = "gh://betjnldhuch/bastion/bastion-ui/BastionUI-PC.zip";
        const string Mark = "bastion_ui\\installed.sha256";

        public static string Effective(string fromIni)
        {
            if (fromIni == "-") return "";
            string url = fromIni.Length > 0 ? fromIni : DefaultUrl;
            if (url.StartsWith("gh://") && Secrets.GitHubToken.Length == 0) return "";   // сборка без токена
            return url;
        }

        // Возвращает текст для строки статуса или null, если ставить нечего.
        public static string Sync(string gta, string url, Action<string> progress)
        {
            if (url.Length == 0 || !url.EndsWith(".zip", StringComparison.OrdinalIgnoreCase)) return null;
            string sumUrl = url.Substring(0, url.Length - 4) + ".sha256";

            progress("Проверка окон Bastion…");
            string tmp = Path.Combine(gta, "bastion_ui.sha256.tmp");
            string want;
            try
            {
                if (File.Exists(tmp)) File.Delete(tmp);
                GameInstaller.Download(sumUrl, tmp, delegate (string m) { }, 2);
                want = File.ReadAllText(tmp).Trim().Split(' ', '\t', '\r', '\n')[0].ToLowerInvariant();
            }
            finally { try { File.Delete(tmp); } catch (Exception) { } }
            if (want.Length != 64) throw new Exception("в релизе bastion-ui нет правильной контрольной суммы.");

            string mark = Path.Combine(gta, Mark);
            bool have = File.Exists(Path.Combine(gta, "bastion_ui.asi")) && File.Exists(mark) &&
                        File.ReadAllText(mark).Trim() == want;
            if (have) return null;

            string zip = Path.Combine(gta, "bastion_ui.download.zip");
            try
            {
                if (File.Exists(zip) && ClientPack.Sha256File(zip) != want) File.Delete(zip);
                if (!File.Exists(zip))
                    GameInstaller.Download(url, zip, delegate (string m) { progress("Окна Bastion. " + m); }, 4);
                progress("Окна Bastion: проверка архива…");
                if (ClientPack.Sha256File(zip) != want) throw new Exception("архив окон скачался с ошибкой, попробуй ещё раз.");
                progress("Окна Bastion: установка…");
                StopStaleCef(gta);
                Extract(zip, gta);
                File.WriteAllText(mark, want, Encoding.ASCII);
            }
            finally { try { File.Delete(zip); } catch (Exception) { } }
            return "Окна Bastion установлены: в игре они будут как на телефоне.";
        }

        // После вылета игры bastion_cef.exe иногда остаётся висеть и держит файлы окон:
        // тогда новая версия не ставится и в игре остаются старые окна. Закрываем только
        // процессы из папки этой игры.
        static void StopStaleCef(string gta)
        {
            string dir = Path.GetFullPath(Path.Combine(gta, "bastion_ui")).TrimEnd('\\') + "\\";
            foreach (System.Diagnostics.Process p in System.Diagnostics.Process.GetProcessesByName("bastion_cef"))
            {
                try
                {
                    string exe = p.MainModule.FileName;
                    if (!exe.StartsWith(dir, StringComparison.OrdinalIgnoreCase)) continue;
                    p.Kill();
                    p.WaitForExit(3000);
                }
                catch (Exception) { }
                finally { p.Dispose(); }
            }
        }

        static void Extract(string zip, string gta)
        {
            string root = Path.GetFullPath(gta).TrimEnd('\\') + "\\";
            bool loader = File.Exists(Path.Combine(gta, "dinput8.dll"));
            using (ZipArchive a = ZipFile.OpenRead(zip))
            {
                foreach (ZipArchiveEntry e in a.Entries)
                {
                    if (e.Name.Length == 0) continue;                         // папка
                    string rel = e.FullName.Replace('/', '\\');
                    int slash = rel.IndexOf('\\');
                    if (slash > 0 && rel.Substring(0, slash).Equals("BastionUI", StringComparison.OrdinalIgnoreCase))
                        rel = rel.Substring(slash + 1);                       // архив с папкой BastionUI\ внутри
                    if (rel.Equals("README.txt", StringComparison.OrdinalIgnoreCase)) continue;
                    if (rel.Equals("dinput8.dll", StringComparison.OrdinalIgnoreCase) && loader) continue;   // свой ASI Loader не трогаем
                    string dest = Path.GetFullPath(Path.Combine(gta, rel));
                    if (!dest.StartsWith(root, StringComparison.OrdinalIgnoreCase)) continue;   // чужие пути мимо папки игры
                    Directory.CreateDirectory(Path.GetDirectoryName(dest));
                    try { e.ExtractToFile(dest, true); }
                    catch (IOException) { throw new Exception("файл " + rel + " занят. Закрой игру и попробуй снова."); }
                    catch (UnauthorizedAccessException) { throw new Exception("нет доступа к папке игры. Запусти лаунчер от имени администратора."); }
                }
            }
        }
    }
}

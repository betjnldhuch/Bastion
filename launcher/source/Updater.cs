// =====================================================================
//  Данные из сети и обновление лаунчера.
//
//  Remote — папка лаунчеров на сервере раздачи (cloud/download-worker):
//  https://<адрес Worker>/launcher/. Оттуда берутся news.txt, servers.ini
//  и version.ini. Адрес выводится из адреса архива игры, вшитого при
//  сборке, поэтому отдельно настраивать его не нужно.
//
//  Updater — сверяет версию с version.ini и, если игрок согласен,
//  скачивает ZIP нового лаунчера и заменяет им файлы рядом с exe.
//  launcher.ini игрока не трогается. Запущенный exe нельзя перезаписать,
//  но можно переименовать: старый становится .old и удаляется при
//  следующем запуске.
//
//  version.ini:
//    pc_version = 2.1.0.0
//    pc_url     = BastionLauncher.zip      (имя в той же папке или полная https-ссылка)
//    pc_sha256  = ...                      (необязательно; certutil -hashfile файл SHA256)
//    pc_notes   = что нового, одной строкой
// =====================================================================
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Net;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;

namespace BastionLauncher
{
    static class Remote
    {
        // Папка лаунчеров на сервере раздачи. Пусто, если адрес архива не вшит.
        public static string BaseUrl
        {
            get
            {
                try
                {
                    if (Secrets.GameUrl.Length == 0) return "";
                    Uri u = new Uri(Secrets.GameUrl);
                    if (u.Scheme != "https") return "";
                    // Яндекс Диск и GitHub не раздают папку launcher/ — удалённых файлов нет
                    string h = u.Host.ToLowerInvariant();
                    if (h == "yadi.sk" || h.StartsWith("disk.") || h == "github.com") return "";
                    return "https://" + u.Authority + "/launcher/";
                }
                catch (Exception) { return ""; }
            }
        }

        // Значение из launcher.ini: пусто — адрес по умолчанию, «-» — выключено
        public static string Pick(string fromIni, string file)
        {
            if (fromIni == "-") return "";
            if (fromIni.Length > 0) return fromIni;
            string b = BaseUrl;
            return b.Length > 0 ? b + file : "";
        }

        // Небольшой текстовый файл (не больше 256 КБ)
        public static string FetchText(string url, int timeoutMs)
        {
            ServicePointManager.SecurityProtocol = (SecurityProtocolType)3072; // TLS 1.2
            HttpWebRequest rq = (HttpWebRequest)WebRequest.Create(url);
            rq.UserAgent = "Bastion-Launcher-PC";
            rq.Timeout = timeoutMs;
            rq.ReadWriteTimeout = timeoutMs;
            using (WebResponse rs = rq.GetResponse())
            using (StreamReader sr = new StreamReader(rs.GetResponseStream(), Encoding.UTF8))
            {
                char[] buf = new char[262144];
                int n = sr.ReadBlock(buf, 0, buf.Length);
                return new string(buf, 0, n);
            }
        }
    }

    class UpdateInfo
    {
        public Version Version;
        public string Url = "";
        public string Sha256 = "";
        public string Notes = "";
    }

    static class Updater
    {
        const long MaxArchive = 209715200L;     // 200 МБ
        const long MaxExtracted = 524288000L;   // 500 МБ
        const string ExeName = "BastionLauncher.exe";

        public static Version Current
        {
            get { return Assembly.GetExecutingAssembly().GetName().Version; }
        }

        static string BaseDir { get { return AppDomain.CurrentDomain.BaseDirectory; } }

        // Есть ли версия новее. null — нет, не удалось узнать или файл составлен неверно.
        public static UpdateInfo Check(string versionUrl)
        {
            if (versionUrl.Length == 0) return null;
            try
            {
                string txt = Remote.FetchText(versionUrl, 6000);
                UpdateInfo info = new UpdateInfo();
                foreach (string raw in txt.Replace("\r", "").Split('\n'))
                {
                    string line = raw.Trim().TrimStart('﻿');
                    if (line.Length == 0 || line.StartsWith(";") || line.StartsWith("#")) continue;
                    int eq = line.IndexOf('=');
                    if (eq <= 0) continue;
                    string key = line.Substring(0, eq).Trim().ToLowerInvariant();
                    string val = line.Substring(eq + 1).Trim();
                    if (key == "pc_version")
                    {
                        Version v;
                        if (Version.TryParse(val, out v)) info.Version = v;
                    }
                    else if (key == "pc_url") info.Url = val;
                    else if (key == "pc_sha256") info.Sha256 = val;
                    else if (key == "pc_notes") info.Notes = val;
                }
                if (info.Version == null || info.Url.Length == 0) return null;
                if (info.Version.CompareTo(Current) <= 0) return null;
                Uri full = new Uri(new Uri(versionUrl), info.Url);      // имя файла или полная ссылка
                if (full.Scheme != "https") return null;
                info.Url = full.AbsoluteUri;
                return info;
            }
            catch (Exception) { return null; }
        }

        // Убрать следы прошлого обновления (вызывается при запуске)
        public static void CleanOld()
        {
            try
            {
                foreach (string f in Directory.GetFiles(BaseDir, "*.old", SearchOption.AllDirectories))
                {
                    try { File.Delete(f); } catch (Exception) { }
                }
                string zip = Path.Combine(BaseDir, "update.download.zip");
                if (File.Exists(zip)) File.Delete(zip);
                string tmp = Path.Combine(BaseDir, "update.tmp");
                if (Directory.Exists(tmp)) Directory.Delete(tmp, true);
            }
            catch (Exception) { }
        }

        // Скачать и поставить. Возвращает путь к новому exe, его остаётся запустить.
        // progress вызывается из рабочего потока.
        public static string Apply(UpdateInfo info, Action<string> progress)
        {
            string zip = Path.Combine(BaseDir, "update.download.zip");
            string staging = Path.Combine(BaseDir, "update.tmp");
            try
            {
                Download(info.Url, zip, progress);
                if (info.Sha256.Length > 0)
                {
                    progress("Проверка обновления…");
                    if (!string.Equals(Sha256(zip), info.Sha256, StringComparison.OrdinalIgnoreCase))
                        throw new Exception("контрольная сумма не совпадает, файл повреждён.");
                }
                if (Directory.Exists(staging)) Directory.Delete(staging, true);
                Directory.CreateDirectory(staging);
                progress("Распаковка обновления…");
                string root = Extract(zip, staging);
                progress("Установка обновления…");
                Replace(root);
                return Path.Combine(BaseDir, ExeName);
            }
            finally
            {
                try { if (File.Exists(zip)) File.Delete(zip); } catch (Exception) { }
                try { if (Directory.Exists(staging)) Directory.Delete(staging, true); } catch (Exception) { }
            }
        }

        static void Download(string url, string target, Action<string> progress)
        {
            progress("Скачивание обновления…");
            ServicePointManager.SecurityProtocol = (SecurityProtocolType)3072; // TLS 1.2
            HttpWebRequest rq = (HttpWebRequest)WebRequest.Create(url);
            rq.UserAgent = "Bastion-Launcher-PC";
            rq.Timeout = 30000;
            rq.ReadWriteTimeout = 60000;
            using (WebResponse rs = rq.GetResponse())
            using (Stream input = rs.GetResponseStream())
            using (FileStream output = new FileStream(target, FileMode.Create, FileAccess.Write))
            {
                long expected = rs.ContentLength;
                if (expected > MaxArchive) throw new Exception("файл обновления слишком большой.");
                byte[] buf = new byte[1 << 16];
                long total = 0;
                int n, last = -1;
                while ((n = input.Read(buf, 0, buf.Length)) > 0)
                {
                    total += n;
                    if (total > MaxArchive) throw new Exception("файл обновления слишком большой.");
                    output.Write(buf, 0, n);
                    int pct = expected > 0 ? (int)(total * 100 / expected) : -1;
                    if (pct != last && pct >= 0) { last = pct; progress("Скачивание обновления: " + pct + "%"); }
                }
                if (expected > 0 && total != expected) throw new Exception("загрузка оборвалась.");
            }
        }

        static string Sha256(string path)
        {
            using (SHA256 sha = SHA256.Create())
            using (FileStream fs = File.OpenRead(path))
            {
                StringBuilder sb = new StringBuilder();
                foreach (byte b in sha.ComputeHash(fs)) sb.Append(b.ToString("x2"));
                return sb.ToString();
            }
        }

        // Распаковать архив и вернуть папку, в которой лежит BastionLauncher.exe
        static string Extract(string archive, string staging)
        {
            string guard = Path.GetFullPath(staging) + Path.DirectorySeparatorChar;
            string best = null;
            long size = 0;
            using (ZipArchive zip = ZipFile.OpenRead(archive))
            {
                foreach (ZipArchiveEntry e in zip.Entries)
                {
                    string name = e.FullName.Replace('\\', '/');
                    if (name.Length == 0) continue;
                    string path = Path.GetFullPath(Path.Combine(staging, name.Replace('/', Path.DirectorySeparatorChar)));
                    if (!path.StartsWith(guard, StringComparison.OrdinalIgnoreCase))
                        throw new Exception("небезопасный путь в архиве: " + e.FullName);
                    if (name.EndsWith("/")) { Directory.CreateDirectory(path); continue; }
                    size += e.Length;
                    if (size > MaxExtracted) throw new Exception("обновление слишком большое.");
                    Directory.CreateDirectory(Path.GetDirectoryName(path));
                    e.ExtractToFile(path, true);
                    if (string.Equals(Path.GetFileName(path), ExeName, StringComparison.OrdinalIgnoreCase))
                    {
                        string dir = Path.GetDirectoryName(path);
                        if (best == null || dir.Length < best.Length) best = dir;
                    }
                }
            }
            if (best == null) throw new Exception("в архиве нет " + ExeName + ".");
            return best;
        }

        static bool SameContent(string a, string b)
        {
            try
            {
                FileInfo fa = new FileInfo(a), fb = new FileInfo(b);
                if (!fb.Exists || fa.Length != fb.Length) return false;
                return Sha256(a) == Sha256(b);
            }
            catch (Exception) { return false; }
        }

        // Перенести файлы из распакованной папки на место. Настройки игрока не трогаем.
        static void Replace(string root)
        {
            string prefix = Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
            List<string> movedOld = new List<string>();     // что переименовали в .old — на случай отката
            List<string> written = new List<string>();
            try
            {
                foreach (string src in Directory.GetFiles(root, "*", SearchOption.AllDirectories))
                {
                    string rel = Path.GetFullPath(src).Substring(prefix.Length);
                    string lower = rel.ToLowerInvariant();
                    if (lower == "launcher.ini" || lower == "servers.cache" || lower == "news.cache") continue;
                    string target = Path.Combine(BaseDir, rel);
                    bool isExe = lower == ExeName.ToLowerInvariant();
                    if (File.Exists(target))
                    {
                        if (SameContent(src, target)) continue;
                        try
                        {
                            string old = target + ".old";
                            if (File.Exists(old)) File.Delete(old);
                            File.Move(target, old);
                            movedOld.Add(target);
                        }
                        catch (Exception)
                        {
                            if (isExe) throw;
                            continue;       // файл занят (например, шрифт) — оставляем прежний
                        }
                    }
                    Directory.CreateDirectory(Path.GetDirectoryName(target));
                    File.Copy(src, target, true);
                    written.Add(target);
                }
            }
            catch (Exception)
            {
                // откат: вернуть переименованные файлы на место
                foreach (string t in written) { try { File.Delete(t); } catch (Exception) { } }
                foreach (string t in movedOld) { try { if (!File.Exists(t)) File.Move(t + ".old", t); } catch (Exception) { } }
                throw;
            }
        }
    }
}

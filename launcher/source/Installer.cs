// =====================================================================
//  Установщик игры: скачивает ZIP с GTA SA + SA-MP, сверяет MD5,
//  распаковывает во временную папку и только потом переносит в папку игры.
//  Корень игры в архиве — самая верхняя папка, где лежит gta_sa.exe.
// =====================================================================
using System;
using System.IO;
using System.IO.Compression;
using System.Net;
using System.Security.Cryptography;
using System.Text;

namespace BastionLauncher
{
    static class GameInstaller
    {
        const long MaxArchive = 4294967296L;    // 4 ГБ
        const long MaxExtracted = 8589934592L;  // 8 ГБ

        // Ключ уходит только по https и только на хост официального архива
        static bool SendsKey(string url)
        {
            if (Secrets.DownloadKey.Length == 0 || Secrets.GameUrl.Length == 0) return false;
            try
            {
                Uri target = new Uri(url), official = new Uri(Secrets.GameUrl);
                return target.Scheme == "https" && official.Scheme == "https" &&
                       string.Equals(target.Host, official.Host, StringComparison.OrdinalIgnoreCase);
            }
            catch (Exception) { return false; }
        }

        // dest — папка, куда ляжет игра (её не должно быть или она пустая).
        // progress вызывается из рабочего потока.
        // url может содержать несколько ссылок через пробел или | — это части одного ZIP
        // (например, нарезка по 900 МБ для Яндекс Диска). Они скачиваются по порядку и
        // склеиваются, MD5 сверяется у склеенного архива.
        public static void Install(string url, string md5, string dest, Action<string> progress)
        {
            string[] parts = url.Split(new char[] { ' ', '|', '\n', '\r', '\t' }, StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length == 0) throw new Exception("Нужна прямая ссылка на ZIP (http:// или https://).");
            foreach (string p in parts)
                if (!p.StartsWith("http://") && !p.StartsWith("https://") && !p.StartsWith("b2://"))
                    throw new Exception("Нужна прямая ссылка на ZIP (http:// или https://).");
            dest = Path.GetFullPath(dest);
            if (Directory.Exists(dest) && Directory.GetFileSystemEntries(dest).Length > 0)
                throw new Exception("Папка " + dest + " не пустая. Выбери пустую папку для игры.");
            string parent = Path.GetDirectoryName(dest);
            Directory.CreateDirectory(parent);
            string archive = dest + ".download.zip";
            string staging = dest + ".installing";
            try
            {
                if (parts.Length == 1) Download(parts[0], archive, progress);
                else DownloadParts(parts, archive, progress);
                if (md5 != null && md5.Trim().Length > 0)
                {
                    progress("Проверка архива (MD5)…");
                    if (!string.Equals(Md5(archive), md5.Trim(), StringComparison.OrdinalIgnoreCase))
                        throw new Exception("MD5 не совпадает: архив повреждён или это другая версия.");
                }
                if (Directory.Exists(staging)) Directory.Delete(staging, true);
                Directory.CreateDirectory(staging);
                Extract(archive, staging, progress);

                progress("Проверка установки…");
                if (!File.Exists(Path.Combine(staging, "gta_sa.exe")))
                    throw new Exception("В архиве нет gta_sa.exe.");
                if (!File.Exists(Path.Combine(staging, "samp.exe")))
                    throw new Exception("В архиве нет samp.exe (клиент SA-MP 0.3.7).");

                if (Directory.Exists(dest)) Directory.Delete(dest, false);
                Directory.Move(staging, dest);
            }
            finally
            {
                try { if (File.Exists(archive)) File.Delete(archive); } catch (Exception) { }
                try { if (Directory.Exists(staging)) Directory.Delete(staging, true); } catch (Exception) { }
            }
        }

        // b2://бакет/путь → ссылка на файл в закрытом бакете Backblaze B2 и временный
        // токен (b2_authorize_account, ключ только на чтение). Возвращает {url, token}.
        static string[] ResolveB2(string address)
        {
            if (Secrets.B2KeyId.Length == 0 || Secrets.B2Key.Length == 0)
                throw new Exception("В этой сборке нет ключа для закрытого хранилища B2.");
            string rest = address.Substring("b2://".Length);
            int slash = rest.IndexOf('/');
            if (slash <= 0 || slash == rest.Length - 1)
                throw new Exception("Ссылка B2 должна быть вида b2://бакет/файл.zip");
            string bucket = rest.Substring(0, slash), file = rest.Substring(slash + 1);

            HttpWebRequest rq = (HttpWebRequest)WebRequest.Create("https://api.backblazeb2.com/b2api/v2/b2_authorize_account");
            rq.UserAgent = "Bastion-Launcher-PC";
            rq.Timeout = 30000;
            rq.Headers["Authorization"] = "Basic " +
                Convert.ToBase64String(Encoding.UTF8.GetBytes(Secrets.B2KeyId + ":" + Secrets.B2Key));
            string json;
            try
            {
                using (WebResponse rs = rq.GetResponse())
                using (StreamReader sr = new StreamReader(rs.GetResponseStream(), Encoding.UTF8))
                    json = sr.ReadToEnd();
            }
            catch (WebException ex)
            {
                HttpWebResponse er = ex.Response as HttpWebResponse;
                if (er != null && (int)er.StatusCode == 401)
                    throw new Exception("Хранилище B2 не приняло ключ лаунчера (401): ключ удалён или устарел.");
                if (er != null) throw new Exception("Хранилище B2 недоступно: HTTP " + (int)er.StatusCode);
                throw new Exception("Нет связи с хранилищем B2: " + ex.Message);
            }
            string downloadUrl = JsonString(json, "downloadUrl");
            string token = JsonString(json, "authorizationToken");
            if (downloadUrl.Length == 0 || token.Length == 0)
                throw new Exception("Хранилище B2 вернуло непонятный ответ.");
            StringBuilder path = new StringBuilder();
            foreach (string part in file.Split('/'))
            {
                if (path.Length > 0) path.Append('/');
                path.Append(Uri.EscapeDataString(part));
            }
            return new string[] { downloadUrl + "/file/" + Uri.EscapeDataString(bucket) + "/" + path, token };
        }

        // Строковое поле верхнего уровня из JSON (без экранированных кавычек внутри)
        static string JsonString(string json, string key)
        {
            System.Text.RegularExpressions.Match m = System.Text.RegularExpressions.Regex.Match(
                json, "\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
            return m.Success ? m.Groups[1].Value.Replace("\\/", "/") : "";
        }

        // Ссылка на Яндекс Диск (disk.yandex.ru/d/…, yadi.sk/…)?
        static bool IsYandexDisk(string url)
        {
            try
            {
                string h = new Uri(url).Host.ToLowerInvariant();
                return h == "yadi.sk" || h == "disk.yandex.ru" || h == "disk.yandex.com" || h == "disk.360.yandex.ru";
            }
            catch (Exception) { return false; }
        }

        // Публичная ссылка Яндекс Диска → временная прямая ссылка на файл (API Диска)
        static string ResolveYandexDisk(string url)
        {
            HttpWebRequest rq = (HttpWebRequest)WebRequest.Create(
                "https://cloud-api.yandex.net/v1/disk/public/resources/download?public_key=" + Uri.EscapeDataString(url));
            rq.UserAgent = "Bastion-Launcher-PC";
            rq.Timeout = 30000;
            string json;
            try
            {
                using (WebResponse rs = rq.GetResponse())
                using (StreamReader sr = new StreamReader(rs.GetResponseStream(), Encoding.UTF8))
                    json = sr.ReadToEnd();
            }
            catch (WebException ex)
            {
                HttpWebResponse er = ex.Response as HttpWebResponse;
                if (er != null && (int)er.StatusCode == 404)
                    throw new Exception("Яндекс Диск: файл по ссылке не найден или доступ по ссылке выключен.");
                if (er != null && ((int)er.StatusCode == 403 || (int)er.StatusCode == 429))
                    throw new Exception("Яндекс Диск временно ограничил скачивание этого файла. Попробуй позже.");
                if (er != null) throw new Exception("Яндекс Диск недоступен: HTTP " + (int)er.StatusCode);
                throw new Exception("Нет связи с Яндекс Диском: " + ex.Message);
            }
            string href = JsonString(json, "href");
            if (!href.StartsWith("https://")) throw new Exception("Яндекс Диск вернул непонятную ссылку.");
            return href;
        }

        static void DownloadParts(string[] parts, string target, Action<string> progress)
        {
            string piece = target + ".piece";
            try
            {
                using (FileStream output = new FileStream(target, FileMode.Create, FileAccess.Write))
                {
                    for (int i = 0; i < parts.Length; i++)
                    {
                        string prefix = "Часть " + (i + 1) + " из " + parts.Length + ". ";
                        Download(parts[i], piece, delegate (string msg) { progress(prefix + msg); });
                        using (FileStream input = File.OpenRead(piece)) input.CopyTo(output, 1 << 20);
                        File.Delete(piece);
                        if (output.Length > MaxArchive) throw new Exception("Архив больше 4 ГБ.");
                    }
                }
            }
            finally
            {
                try { if (File.Exists(piece)) File.Delete(piece); } catch (Exception) { }
            }
        }

        static void Download(string url, string target, Action<string> progress)
        {
            progress("Подключение к серверу…");
            ServicePointManager.SecurityProtocol = (SecurityProtocolType)3072; // TLS 1.2
            if (IsYandexDisk(url))
            {
                progress("Получение ссылки Яндекс Диска…");
                url = ResolveYandexDisk(url);
            }
            string b2Token = null;
            if (url.StartsWith("b2://"))
            {
                progress("Получение доступа к хранилищу…");
                string[] r = ResolveB2(url);
                url = r[0];
                b2Token = r[1];
            }
            HttpWebRequest rq = (HttpWebRequest)WebRequest.Create(url);
            rq.UserAgent = "Bastion-Launcher-PC";
            rq.Timeout = 30000;
            rq.ReadWriteTimeout = 60000;
            bool keyed = b2Token == null && SendsKey(url);
            if (b2Token != null) rq.Headers["Authorization"] = b2Token;
            else if (keyed) rq.Headers["X-Bastion-Key"] = Secrets.DownloadKey;
            HttpWebResponse rs;
            try { rs = (HttpWebResponse)rq.GetResponse(); }
            catch (WebException ex)
            {
                HttpWebResponse er = ex.Response as HttpWebResponse;
                if (er != null && b2Token != null && (int)er.StatusCode == 404)
                    throw new Exception("В хранилище B2 нет файла архива (404). Проверь имя бакета и файла.");
                if (er != null && b2Token != null && ((int)er.StatusCode == 401 || (int)er.StatusCode == 403))
                    throw new Exception("Хранилище B2 не отдало архив (" + (int)er.StatusCode + "): у ключа нет доступа к этому бакету.");
                if (er != null && (int)er.StatusCode == 404 && keyed)
                    throw new Exception("Сервер не отдал архив (404): файла нет в хранилище или ключ лаунчера устарел.");
                if (er != null) throw new Exception("Ошибка загрузки архива: HTTP " + (int)er.StatusCode);
                throw new Exception("Нет связи с сервером загрузки: " + ex.Message);
            }
            using (rs)
            using (Stream input = rs.GetResponseStream())
            using (FileStream output = new FileStream(target, FileMode.Create, FileAccess.Write))
            {
                long expected = rs.ContentLength;
                if (expected > MaxArchive) throw new Exception("Архив больше 4 ГБ.");
                byte[] buf = new byte[1 << 16];
                long total = 0;
                int n, last = -1;
                while ((n = input.Read(buf, 0, buf.Length)) > 0)
                {
                    total += n;
                    if (total > MaxArchive) throw new Exception("Архив больше 4 ГБ.");
                    output.Write(buf, 0, n);
                    int mb = (int)(total / 1048576);
                    int mark = expected > 0 ? (int)(total * 100 / expected) : mb;
                    if (mark != last)
                    {
                        last = mark;
                        progress(expected > 0
                            ? "Скачивание: " + mark + "%  (" + mb + " / " + (expected / 1048576) + " МБ)"
                            : "Скачано: " + mb + " МБ");
                    }
                }
                if (expected > 0 && total != expected) throw new Exception("Загрузка оборвалась, попробуй ещё раз.");
            }
        }

        static string Md5(string path)
        {
            using (MD5 md5 = MD5.Create())
            using (FileStream fs = File.OpenRead(path))
            {
                StringBuilder sb = new StringBuilder();
                foreach (byte b in md5.ComputeHash(fs)) sb.Append(b.ToString("x2"));
                return sb.ToString();
            }
        }

        // Корень игры — самая короткая папка внутри архива, где лежит gta_sa.exe
        static string FindRoot(ZipArchive zip)
        {
            string best = null;
            foreach (ZipArchiveEntry e in zip.Entries)
            {
                string name = e.FullName.Replace('\\', '/');
                if (!name.EndsWith("gta_sa.exe", StringComparison.OrdinalIgnoreCase)) continue;
                string dir = name.Substring(0, name.Length - "gta_sa.exe".Length);
                if (dir.Length > 0 && !dir.EndsWith("/")) continue;
                if (best == null || dir.Length < best.Length) best = dir;
            }
            if (best == null) throw new Exception("В архиве нет gta_sa.exe — это не архив игры.");
            return best;
        }

        static void Extract(string archive, string staging, Action<string> progress)
        {
            progress("Распаковка…");
            string root = Path.GetFullPath(staging) + Path.DirectorySeparatorChar;
            using (ZipArchive zip = ZipFile.OpenRead(archive))
            {
                string prefix = FindRoot(zip);
                int count = zip.Entries.Count, done = 0, last = -1;
                long size = 0;
                foreach (ZipArchiveEntry e in zip.Entries)
                {
                    done++;
                    string name = e.FullName.Replace('\\', '/');
                    if (!name.StartsWith(prefix, StringComparison.OrdinalIgnoreCase)) continue;
                    string rel = name.Substring(prefix.Length);
                    if (rel.Length == 0) continue;
                    string path = Path.GetFullPath(Path.Combine(staging, rel.Replace('/', Path.DirectorySeparatorChar)));
                    if (!path.StartsWith(root, StringComparison.OrdinalIgnoreCase))
                        throw new Exception("Небезопасный путь в архиве: " + e.FullName);
                    if (rel.EndsWith("/")) { Directory.CreateDirectory(path); continue; }
                    size += e.Length;
                    if (size > MaxExtracted) throw new Exception("Распакованная игра больше 8 ГБ.");
                    Directory.CreateDirectory(Path.GetDirectoryName(path));
                    e.ExtractToFile(path, true);
                    int pct = (int)((long)done * 100 / count);
                    if (pct != last) { last = pct; progress("Распаковка: " + pct + "%"); }
                }
            }
        }
    }
}

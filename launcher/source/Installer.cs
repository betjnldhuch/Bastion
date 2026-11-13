// =====================================================================
//  Установщик игры: скачивает ZIP с GTA SA + SA-MP, сверяет MD5,
//  распаковывает во временную папку и только потом переносит в папку игры.
//  Загрузка докачивается после обрыва (повторы + HTTP Range между запусками).
//  Корень игры в архиве — самая верхняя папка, где лежит gta_sa.exe.
// =====================================================================
using System;
using System.IO;
using System.IO.Compression;
using System.Net;
using System.Security.Cryptography;
using System.Text;
using System.Threading;

namespace BastionLauncher
{
    static class GameInstaller
    {
        const long MaxArchive = 17179869184L;   // 16 ГБ
        const long MaxExtracted = 34359738368L; // 32 ГБ

        // Ключ уходит только по https и только на хост официального архива
        public static bool SendsKey(string url)
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

        // Скачанное лежит рядом с папкой игры: «<папка>.download.zip» (или части
        // «.download.partN») и «.download.info» с адресом и MD5. Если загрузка оборвалась
        // или лаунчер закрыли, эти файлы остаются, и следующая установка в ту же папку
        // докачивает с места обрыва (HTTP Range).
        public static string ArchivePath(string dest) { return Path.GetFullPath(dest) + ".download.zip"; }

        // Есть ли недокачанный архив для этой папки (чтобы предложить продолжить)
        public static bool HasPartial(string dest)
        {
            string a = ArchivePath(dest);
            if (!File.Exists(InfoPath(a))) return false;
            if (File.Exists(a)) return true;
            for (int i = 0; i < 64; i++) if (File.Exists(PartPath(a, i))) return true;
            return false;
        }

        static string InfoPath(string archive) { return archive.Substring(0, archive.Length - 4) + ".info"; }
        static string PartPath(string archive, int i) { return archive.Substring(0, archive.Length - 4) + ".part" + i; }

        static void DeletePartial(string archive)
        {
            try { if (File.Exists(archive)) File.Delete(archive); } catch (Exception) { }
            try { if (File.Exists(InfoPath(archive))) File.Delete(InfoPath(archive)); } catch (Exception) { }
            for (int i = 0; i < 64; i++)
                try { if (File.Exists(PartPath(archive, i))) File.Delete(PartPath(archive, i)); } catch (Exception) { }
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
            if (parts.Length > 64) throw new Exception("Слишком много частей архива (больше 64).");
            foreach (string p in parts)
                if (!p.StartsWith("http://") && !p.StartsWith("https://") && !p.StartsWith("b2://"))
                    throw new Exception("Нужна прямая ссылка на ZIP (http:// или https://).");
            md5 = md5 == null ? "" : md5.Trim().ToLowerInvariant();
            dest = Path.GetFullPath(dest);
            if (Directory.Exists(dest) && Directory.GetFileSystemEntries(dest).Length > 0)
                throw new Exception("Папка " + dest + " не пустая. Выбери пустую папку для игры.");
            string parent = Path.GetDirectoryName(dest);
            Directory.CreateDirectory(parent);
            string archive = ArchivePath(dest);
            string staging = dest + ".installing";

            // Недокачанное от другого архива (сменилась ссылка или MD5) — начинаем заново
            string info = "url=" + url.Trim() + "\r\nmd5=" + md5 + "\r\n";
            string infoFile = InfoPath(archive);
            string old = null;
            try { if (File.Exists(infoFile)) old = File.ReadAllText(infoFile); } catch (Exception) { }
            if (old != info)
            {
                DeletePartial(archive);
                File.WriteAllText(infoFile, info);
            }

            bool done = false;
            try
            {
                if (parts.Length == 1) Download(parts[0], archive, progress);
                else DownloadParts(parts, archive, progress);
                if (md5.Length > 0)
                {
                    progress("Проверка архива (MD5)…");
                    if (Md5(archive, progress) != md5)
                    {
                        DeletePartial(archive); // битый архив докачивать бессмысленно
                        throw new Exception("MD5 не совпадает: архив повреждён или это другая версия. " +
                                            "Скачанное удалено, нажми УСТАНОВИТЬ ещё раз.");
                    }
                }
                if (Directory.Exists(staging)) Directory.Delete(staging, true);
                Directory.CreateDirectory(staging);
                try { Extract(archive, staging, progress); }
                catch (InvalidDataException)
                {
                    DeletePartial(archive);
                    throw new Exception("Архив повреждён (не открывается как ZIP). Скачанное удалено, нажми УСТАНОВИТЬ ещё раз.");
                }

                progress("Проверка установки…");
                if (!File.Exists(Path.Combine(staging, "gta_sa.exe")))
                    throw new Exception("В архиве нет gta_sa.exe.");
                if (!File.Exists(Path.Combine(staging, "samp.exe")))
                    throw new Exception("В архиве нет samp.exe (клиент SA-MP 0.3.7).");

                if (Directory.Exists(dest)) Directory.Delete(dest, false);
                Directory.Move(staging, dest);
                done = true;
            }
            finally
            {
                // Архив удаляется только после успешной установки — иначе остаётся для докачки
                if (done) DeletePartial(archive);
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

        // Части качаются каждая в свой файл (.partN) с докачкой и склеиваются в конце
        static void DownloadParts(string[] parts, string target, Action<string> progress)
        {
            for (int i = 0; i < parts.Length; i++)
            {
                string prefix = "Часть " + (i + 1) + " из " + parts.Length + ". ";
                Download(parts[i], PartPath(target, i), delegate (string msg) { progress(prefix + msg); });
            }
            progress("Склейка частей архива…");
            using (FileStream output = new FileStream(target, FileMode.Create, FileAccess.Write))
            {
                for (int i = 0; i < parts.Length; i++)
                {
                    using (FileStream input = File.OpenRead(PartPath(target, i))) input.CopyTo(output, 1 << 20);
                    if (output.Length > MaxArchive) throw new Exception("Архив больше 16 ГБ.");
                }
            }
            for (int i = 0; i < parts.Length; i++)
                try { File.Delete(PartPath(target, i)); } catch (Exception) { }
        }

        // Обрыв связи, таймаут, 5xx — можно повторить, скачанное не теряется
        class RetryException : Exception
        {
            public RetryException(string msg) : base(msg) { }
        }

        const int Attempts = 6;

        static void Download(string url, string target, Action<string> progress)
        {
            for (int attempt = 1; ; attempt++)
            {
                try { DownloadOnce(url, target, progress); return; }
                catch (RetryException ex)
                {
                    if (attempt >= Attempts)
                        throw new Exception(ex.Message + " Скачанное сохранено: нажми УСТАНОВИТЬ ещё раз " +
                                            "и выбери ту же папку — загрузка продолжится с места обрыва.");
                    int wait = Math.Min(30, attempt * 5);
                    for (int sec = wait; sec > 0; sec--)
                    {
                        progress("Связь оборвалась, повтор через " + sec + " с (попытка " + (attempt + 1) + " из " + Attempts + ")…");
                        Thread.Sleep(1000);
                    }
                }
            }
        }

        static void DownloadOnce(string url, string target, Action<string> progress)
        {
            progress("Подключение к серверу…");
            ServicePointManager.SecurityProtocol = (SecurityProtocolType)3072; // TLS 1.2
            // Временные ссылки Яндекс Диска и B2 получаем заново на каждую попытку
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
            long have = File.Exists(target) ? new FileInfo(target).Length : 0;
            HttpWebRequest rq = (HttpWebRequest)WebRequest.Create(url);
            rq.UserAgent = "Bastion-Launcher-PC";
            rq.Timeout = 30000;
            rq.ReadWriteTimeout = 60000;
            if (have > 0) rq.AddRange(have);
            bool keyed = b2Token == null && SendsKey(url);
            if (b2Token != null) rq.Headers["Authorization"] = b2Token;
            else if (keyed) rq.Headers["X-Bastion-Key"] = Secrets.DownloadKey;
            HttpWebResponse rs;
            try { rs = (HttpWebResponse)rq.GetResponse(); }
            catch (WebException ex)
            {
                HttpWebResponse er = ex.Response as HttpWebResponse;
                if (er == null) throw new RetryException("Нет связи с сервером загрузки: " + ex.Message + ".");
                int code = (int)er.StatusCode;
                er.Close();
                if (code == 416 && have > 0) return; // уже скачан целиком
                if (code >= 500 || code == 429) throw new RetryException("Сервер загрузки ответил HTTP " + code + ".");
                if (b2Token != null && code == 404)
                    throw new Exception("В хранилище B2 нет файла архива (404). Проверь имя бакета и файла.");
                if (b2Token != null && (code == 401 || code == 403))
                    throw new Exception("Хранилище B2 не отдало архив (" + code + "): у ключа нет доступа к этому бакету.");
                if (code == 404 && keyed)
                    throw new Exception("Сервер не отдал архив (404): файла нет в хранилище или ключ лаунчера устарел.");
                throw new Exception("Ошибка загрузки архива: HTTP " + code);
            }
            using (rs)
            {
                bool resume = have > 0 && rs.StatusCode == HttpStatusCode.PartialContent;
                if (!resume) have = 0; // сервер не умеет докачку — с начала
                long expected = rs.ContentLength >= 0 ? have + rs.ContentLength : -1;
                if (expected > MaxArchive) throw new Exception("Архив больше 16 ГБ.");
                CheckSpace(target, expected - have, expected);
                try
                {
                    using (Stream input = rs.GetResponseStream())
                    using (FileStream output = new FileStream(target, resume ? FileMode.Append : FileMode.Create, FileAccess.Write))
                    {
                        byte[] buf = new byte[1 << 16];
                        long total = have, mark = have;
                        int n;
                        DateTime start = DateTime.UtcNow, shown = DateTime.MinValue;
                        double speed = 0;
                        if (resume) progress("Докачка с " + (have / 1048576) + " МБ…");
                        while ((n = input.Read(buf, 0, buf.Length)) > 0)
                        {
                            total += n;
                            if (total > MaxArchive) throw new Exception("Архив больше 16 ГБ.");
                            output.Write(buf, 0, n);
                            DateTime now = DateTime.UtcNow;
                            if ((now - shown).TotalMilliseconds >= 500)
                            {
                                double sec = (now - start).TotalSeconds;
                                if (sec > 1) speed = (total - mark) / sec;
                                shown = now;
                                progress(Line(total, expected, speed));
                            }
                        }
                        if (expected > 0 && total < expected)
                            throw new RetryException("Загрузка оборвалась на " + (total / 1048576) + " МБ из " + (expected / 1048576) + ".");
                    }
                }
                catch (IOException ex) { throw new RetryException("Загрузка оборвалась: " + ex.Message); }
                catch (WebException ex) { throw new RetryException("Загрузка оборвалась: " + ex.Message); }
            }
        }

        // «Скачивание: 42%  (512 / 1220 МБ), 8.3 МБ/с, осталось 1 мин 25 с»
        static string Line(long total, long expected, double speed)
        {
            long mb = total / 1048576;
            string s = expected > 0
                ? "Скачивание: " + (int)(total * 100 / expected) + "%  (" + mb + " / " + (expected / 1048576) + " МБ)"
                : "Скачано: " + mb + " МБ";
            if (speed > 0)
            {
                s += ", " + (speed / 1048576).ToString("0.0", System.Globalization.CultureInfo.InvariantCulture) + " МБ/с";
                if (expected > 0)
                {
                    long left = (long)((expected - total) / speed);
                    s += ", осталось " + (left >= 3600 ? (left / 3600) + " ч " + (left % 3600 / 60) + " мин"
                                       : left >= 60 ? (left / 60) + " мин " + (left % 60) + " с" : left + " с");
                }
            }
            return s;
        }

        // Место на диске: докачать остаток + распаковать (берём с запасом размер архива)
        static void CheckSpace(string target, long remaining, long archiveSize)
        {
            if (archiveSize <= 0) return;
            try
            {
                DriveInfo d = new DriveInfo(Path.GetPathRoot(Path.GetFullPath(target)));
                long need = Math.Max(0, remaining) + archiveSize + archiveSize / 4;
                if (d.AvailableFreeSpace < need)
                    throw new Exception("Не хватает места на диске " + d.Name + ": нужно около " +
                                        (need / 1073741824.0).ToString("0.0", System.Globalization.CultureInfo.InvariantCulture) +
                                        " ГБ, свободно " +
                                        (d.AvailableFreeSpace / 1073741824.0).ToString("0.0", System.Globalization.CultureInfo.InvariantCulture) + " ГБ.");
            }
            catch (ArgumentException) { }
            catch (IOException) { }
        }

        static string Md5(string path, Action<string> progress)
        {
            using (MD5 md5 = MD5.Create())
            using (FileStream fs = File.OpenRead(path))
            {
                byte[] buf = new byte[1 << 20];
                long len = fs.Length, done = 0;
                int n, last = -1;
                while ((n = fs.Read(buf, 0, buf.Length)) > 0)
                {
                    md5.TransformBlock(buf, 0, n, null, 0);
                    done += n;
                    int pct = len > 0 ? (int)(done * 100 / len) : 100;
                    if (pct != last) { last = pct; progress("Проверка архива (MD5): " + pct + "%"); }
                }
                md5.TransformFinalBlock(buf, 0, 0);
                StringBuilder sb = new StringBuilder();
                foreach (byte b in md5.Hash) sb.Append(b.ToString("x2"));
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
                    if (size > MaxExtracted) throw new Exception("Распакованная игра больше 32 ГБ.");
                    Directory.CreateDirectory(Path.GetDirectoryName(path));
                    e.ExtractToFile(path, true);
                    int pct = (int)((long)done * 100 / count);
                    if (pct != last) { last = pct; progress("Распаковка: " + pct + "%"); }
                }
            }
        }
    }
}

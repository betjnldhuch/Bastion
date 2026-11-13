// =====================================================================
//  Клиентский пакет Bastion: проверка и докачка файлов игры.
//
//  Манифест client-pack.json лежит в папке launcher/ на сервере раздачи
//  (или по ссылке client_pack_url в launcher.ini). Перед запуском игры
//  лаунчер сверяет каждый файл из манифеста (размер + SHA-256) и скачивает
//  только изменившиеся. Формат описан в launcher/ROADMAP.md:
//
//  { "version": 3, "files": [
//      { "path": "models/hud.txd", "url": "pack/hud.txd", "size": 123, "sha256": "…" },
//      { "path": "models/gta3.img", "name": "radar00.txd", "url": "…", "size": 8192, "sha256": "…" } ] }
//
//  url — полная https-ссылка или имя рядом с манифестом.
//  name — запись внутри IMG-архива (только VER2), до 23 символов.
//  Оригиналы один раз сохраняются в _bastion_backup\ в папке игры.
// =====================================================================
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;

namespace BastionLauncher
{
    class PackFile
    {
        public string Path = "";    // относительно папки GTA, через /
        public string Name = "";    // запись внутри IMG или пусто
        public string Url = "";
        public long Size;
        public string Sha256 = "";
        public string Key { get { return Name.Length > 0 ? Path + ":" + Name : Path; } }
    }

    class PackManifest
    {
        public int Version;
        public List<PackFile> Files = new List<PackFile>();
        public long TotalSize { get { long t = 0; foreach (PackFile f in Files) t += f.Size; return t; } }
    }

    static class ClientPack
    {
        const long MaxFile = 1073741824L;       // 1 ГБ на файл
        const int Sector = 2048;
        const string BackupDir = "_bastion_backup";
        static readonly Regex SafePath = new Regex(@"^[A-Za-z0-9_.\-]+(/[A-Za-z0-9_.\-]+)*$");
        static readonly Regex SafeName = new Regex(@"^[A-Za-z0-9_.\-]{1,23}$");
        static readonly Regex Hex64 = new Regex(@"^[0-9a-f]{64}$");

        // ---------- манифест ----------
        public static PackManifest Fetch(string url)
        {
            string txt = Remote.FetchText(url, 8000);
            object root = new JavaScriptSerializer().DeserializeObject(txt.TrimStart('﻿'));
            IDictionary obj = root as IDictionary;
            if (obj == null) throw new Exception("манифест пакета составлен неверно.");
            PackManifest m = new PackManifest();
            if (obj.Contains("version")) m.Version = Convert.ToInt32(obj["version"]);
            IEnumerable files = obj.Contains("files") ? obj["files"] as IEnumerable : null;
            if (files == null) return m;
            Uri baseUri = new Uri(url);
            foreach (object o in files)
            {
                IDictionary d = o as IDictionary;
                if (d == null) continue;
                PackFile f = new PackFile();
                f.Path = Str(d, "path").Replace('\\', '/');
                f.Name = Str(d, "name");
                f.Sha256 = Str(d, "sha256").ToLowerInvariant();
                f.Size = d.Contains("size") ? Convert.ToInt64(d["size"]) : -1;
                string u = Str(d, "url");
                if (!SafePath.IsMatch(f.Path) || f.Path.Contains("..")) throw new Exception("плохой путь в пакете: " + f.Path);
                if (f.Name.Length > 0 && !SafeName.IsMatch(f.Name)) throw new Exception("плохое имя записи: " + f.Name);
                if (!Hex64.IsMatch(f.Sha256)) throw new Exception("нет SHA-256 у " + f.Key);
                if (f.Size < 0 || f.Size > MaxFile) throw new Exception("плохой размер у " + f.Key);
                if (f.Path.StartsWith(BackupDir, StringComparison.OrdinalIgnoreCase)) throw new Exception("плохой путь в пакете: " + f.Path);
                Uri full = new Uri(baseUri, u);
                if (full.Scheme != "https") throw new Exception("ссылка не https у " + f.Key);
                f.Url = full.AbsoluteUri;
                m.Files.Add(f);
            }
            return m;
        }

        static string Str(IDictionary d, string k)
        {
            return d.Contains(k) && d[k] != null ? Convert.ToString(d[k]).Trim() : "";
        }

        // ---------- синхронизация ----------
        // full = true — пересчитать SHA-256 всех файлов, не доверяя кэшу.
        // Возвращает число скачанных файлов. progress вызывается из рабочего потока.
        public static int Sync(string gta, PackManifest m, bool full, Action<string> progress)
        {
            gta = System.IO.Path.GetFullPath(gta);
            string backup = System.IO.Path.Combine(gta, BackupDir);
            Directory.CreateDirectory(backup);
            HashCache cache = new HashCache(System.IO.Path.Combine(backup, "pack.cache"));
            if (full) cache.Clear();

            List<PackFile> need = new List<PackFile>();
            int i = 0;
            foreach (PackFile f in m.Files)
            {
                i++;
                progress("Проверка файлов игры: " + i + " из " + m.Files.Count);
                if (!IsCurrent(gta, f, cache)) need.Add(f);
            }
            cache.Save();
            if (need.Count == 0) return 0;

            long total = 0, done = 0;
            foreach (PackFile f in need) total += f.Size;
            string tmp = System.IO.Path.Combine(backup, "download.tmp");
            try
            {
                for (int k = 0; k < need.Count; k++)
                {
                    PackFile f = need[k];
                    string head = "Обновление файлов " + (k + 1) + "/" + need.Count + ": ";
                    long before = done;
                    Download(f.Url, tmp, f.Size, delegate (long got)
                    {
                        long all = before + got;
                        progress(head + (total > 0 ? (int)(all * 100 / total) : 100) + "%  (" +
                                 (all / 1048576) + " / " + Math.Max(1, total / 1048576) + " МБ)");
                    });
                    if (Sha256File(tmp) != f.Sha256) throw new Exception("файл " + f.Key + " скачался с ошибкой (SHA-256). Попробуй ещё раз.");
                    progress(head + "установка " + f.Key);
                    if (f.Name.Length > 0) PutImgEntry(gta, f, tmp);
                    else PutFile(gta, f, tmp, cache);
                    done += f.Size;
                }
            }
            finally
            {
                try { if (File.Exists(tmp)) File.Delete(tmp); } catch (Exception) { }
                cache.Save();
            }
            return need.Count;
        }

        static string Target(string gta, PackFile f)
        {
            string p = System.IO.Path.GetFullPath(System.IO.Path.Combine(gta, f.Path.Replace('/', System.IO.Path.DirectorySeparatorChar)));
            if (!p.StartsWith(gta.TrimEnd('\\') + "\\", StringComparison.OrdinalIgnoreCase)) throw new Exception("плохой путь: " + f.Path);
            return p;
        }

        static bool IsCurrent(string gta, PackFile f, HashCache cache)
        {
            string target = Target(gta, f);
            if (!File.Exists(target)) return false;
            if (f.Name.Length > 0)
            {
                byte[] data = ReadImgEntry(target, f.Name, f.Size);
                return data != null && Sha256Bytes(data) == f.Sha256;
            }
            FileInfo fi = new FileInfo(target);
            if (fi.Length != f.Size) return false;
            string sha = cache.Get(f.Key, fi.Length, fi.LastWriteTimeUtc.Ticks);
            if (sha == null)
            {
                sha = Sha256File(target);
                cache.Put(f.Key, fi.Length, fi.LastWriteTimeUtc.Ticks, sha);
            }
            return sha == f.Sha256;
        }

        static void PutFile(string gta, PackFile f, string tmp, HashCache cache)
        {
            string target = Target(gta, f);
            string bak = System.IO.Path.Combine(gta, BackupDir, f.Path.Replace('/', '\\'));
            if (File.Exists(target) && !File.Exists(bak))
            {
                Directory.CreateDirectory(System.IO.Path.GetDirectoryName(bak));
                File.Copy(target, bak);
            }
            Directory.CreateDirectory(System.IO.Path.GetDirectoryName(target));
            File.Copy(tmp, target, true);
            FileInfo fi = new FileInfo(target);
            cache.Put(f.Key, fi.Length, fi.LastWriteTimeUtc.Ticks, f.Sha256);
        }

        // ---------- IMG v2 ----------
        // Каталог: "VER2", число записей, затем по 32 байта: offset (секторы), size (секторы, ushort),
        // archiveSize (ushort), имя (24 байта). Сектор — 2048 байт.
        class ImgEntry { public int Index; public uint Offset; public uint Sectors; public string Name; }

        static List<ImgEntry> ReadDir(FileStream fs, out int count)
        {
            BinaryReader br = new BinaryReader(fs);
            fs.Position = 0;
            if (Encoding.ASCII.GetString(br.ReadBytes(4)) != "VER2") throw new Exception("IMG-архив не версии 2 (VER2): " + fs.Name);
            count = br.ReadInt32();
            if (count < 0 || count > 200000) throw new Exception("IMG-архив повреждён: " + fs.Name);
            List<ImgEntry> list = new List<ImgEntry>(count);
            for (int i = 0; i < count; i++)
            {
                ImgEntry e = new ImgEntry();
                e.Index = i;
                e.Offset = br.ReadUInt32();
                ushort s1 = br.ReadUInt16(), s2 = br.ReadUInt16();
                e.Sectors = s1 != 0 ? s1 : s2;
                byte[] nb = br.ReadBytes(24);
                int z = Array.IndexOf(nb, (byte)0);
                e.Name = Encoding.ASCII.GetString(nb, 0, z < 0 ? 24 : z);
                list.Add(e);
            }
            return list;
        }

        static ImgEntry FindEntry(List<ImgEntry> dir, string name)
        {
            foreach (ImgEntry e in dir) if (string.Equals(e.Name, name, StringComparison.OrdinalIgnoreCase)) return e;
            return null;
        }

        static byte[] ReadImgEntry(string img, string name, long size)
        {
            using (FileStream fs = new FileStream(img, FileMode.Open, FileAccess.Read, FileShare.Read))
            {
                int count;
                ImgEntry e = FindEntry(ReadDir(fs, out count), name);
                if (e == null || (long)e.Sectors * Sector < size) return null;
                fs.Position = (long)e.Offset * Sector;
                byte[] data = new byte[size];
                int got = 0, n;
                while (got < size && (n = fs.Read(data, got, (int)size - got)) > 0) got += n;
                return got == size ? data : null;
            }
        }

        static void WriteDirEntry(FileStream fs, ImgEntry e)
        {
            BinaryWriter bw = new BinaryWriter(fs);
            fs.Position = 8 + (long)e.Index * 32;
            bw.Write(e.Offset);
            bw.Write((ushort)e.Sectors);
            bw.Write((ushort)0);
            byte[] nb = new byte[24];
            Encoding.ASCII.GetBytes(e.Name, 0, e.Name.Length, nb, 0);
            bw.Write(nb);
            bw.Flush();
        }

        // Записать данные в конец архива (по границе сектора), вернуть offset в секторах
        static uint Append(FileStream fs, byte[] data)
        {
            long end = (fs.Length + Sector - 1) / Sector * Sector;
            fs.SetLength(end);
            fs.Position = end;
            WritePadded(fs, data);
            return (uint)(end / Sector);
        }

        static void WritePadded(FileStream fs, byte[] data)
        {
            fs.Write(data, 0, data.Length);
            int pad = (int)((Sector - data.Length % Sector) % Sector);
            if (pad > 0) fs.Write(new byte[pad], 0, pad);
        }

        static uint SectorsFor(long len) { return (uint)((len + Sector - 1) / Sector); }

        static void PutImgEntry(string gta, PackFile f, string tmp)
        {
            string img = Target(gta, f);
            if (!File.Exists(img)) throw new Exception("нет файла " + f.Path + " — переустанови игру.");
            byte[] data = File.ReadAllBytes(tmp);
            uint need = SectorsFor(data.Length);
            if (need > 65535) throw new Exception("запись " + f.Name + " слишком большая для IMG.");
            using (FileStream fs = new FileStream(img, FileMode.Open, FileAccess.ReadWrite, FileShare.None))
            {
                int count;
                List<ImgEntry> dir = ReadDir(fs, out count);
                ImgEntry e = FindEntry(dir, f.Name);
                if (e != null)
                {
                    // оригинал записи — один раз в _bastion_backup\<path>\<name>
                    string bak = System.IO.Path.Combine(gta, BackupDir, f.Path.Replace('/', '\\'), f.Name);
                    if (!File.Exists(bak))
                    {
                        Directory.CreateDirectory(System.IO.Path.GetDirectoryName(bak));
                        byte[] orig = new byte[(long)e.Sectors * Sector];
                        fs.Position = (long)e.Offset * Sector;
                        int got = 0, n;
                        while (got < orig.Length && (n = fs.Read(orig, got, orig.Length - got)) > 0) got += n;
                        File.WriteAllBytes(bak, orig);
                    }
                    if (need <= e.Sectors)
                    {
                        fs.Position = (long)e.Offset * Sector;
                        WritePadded(fs, data);
                    }
                    else e.Offset = Append(fs, data);
                    e.Sectors = need;
                    WriteDirEntry(fs, e);
                    return;
                }

                // Новой записи нужно место в каталоге: записи, чьи данные лежат там,
                // переносятся в конец архива.
                long dirEnd = 8 + (long)(count + 1) * 32;
                uint firstFree = SectorsFor(dirEnd);
                foreach (ImgEntry o in dir)
                {
                    if (o.Offset >= firstFree || o.Sectors == 0) continue;
                    byte[] moved = new byte[(long)o.Sectors * Sector];
                    fs.Position = (long)o.Offset * Sector;
                    int got = 0, n;
                    while (got < moved.Length && (n = fs.Read(moved, got, moved.Length - got)) > 0) got += n;
                    o.Offset = Append(fs, moved);
                    WriteDirEntry(fs, o);
                }
                ImgEntry ne = new ImgEntry();
                ne.Index = count;
                ne.Name = f.Name;
                ne.Offset = Append(fs, data);
                ne.Sectors = need;
                WriteDirEntry(fs, ne);
                fs.Position = 4;
                new BinaryWriter(fs).Write(count + 1);
                fs.Flush();
            }
        }

        // ---------- загрузка и хэши ----------
        static void Download(string url, string target, long size, Action<long> progress)
        {
            ServicePointManager.SecurityProtocol = (SecurityProtocolType)3072; // TLS 1.2
            HttpWebRequest rq = (HttpWebRequest)WebRequest.Create(url);
            rq.UserAgent = "Bastion-Launcher-PC";
            rq.Timeout = 30000;
            rq.ReadWriteTimeout = 60000;
            if (GameInstaller.SendsKey(url)) rq.Headers["X-Bastion-Key"] = Secrets.DownloadKey;
            HttpWebResponse rs;
            try { rs = (HttpWebResponse)rq.GetResponse(); }
            catch (WebException ex)
            {
                HttpWebResponse er = ex.Response as HttpWebResponse;
                if (er != null) throw new Exception("сервер не отдал файл (HTTP " + (int)er.StatusCode + "): " + url);
                throw new Exception("нет связи с сервером загрузки: " + ex.Message);
            }
            using (rs)
            using (Stream input = rs.GetResponseStream())
            using (FileStream output = new FileStream(target, FileMode.Create, FileAccess.Write))
            {
                byte[] buf = new byte[1 << 16];
                long total = 0, lastMb = -1;
                int n;
                while ((n = input.Read(buf, 0, buf.Length)) > 0)
                {
                    total += n;
                    if (total > size) throw new Exception("файл больше, чем указано в пакете: " + url);
                    output.Write(buf, 0, n);
                    if (total / 524288 != lastMb) { lastMb = total / 524288; progress(total); }
                }
                if (total != size) throw new Exception("загрузка оборвалась, попробуй ещё раз.");
                progress(total);
            }
        }

        static string Hex(byte[] h)
        {
            StringBuilder sb = new StringBuilder(64);
            foreach (byte b in h) sb.Append(b.ToString("x2"));
            return sb.ToString();
        }

        public static string Sha256File(string path)
        {
            using (SHA256 sha = SHA256.Create())
            using (FileStream fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read, 1 << 20))
                return Hex(sha.ComputeHash(fs));
        }

        static string Sha256Bytes(byte[] data)
        {
            using (SHA256 sha = SHA256.Create()) return Hex(sha.ComputeHash(data));
        }

        // Кэш хэшей: чтобы не пересчитывать SHA-256 больших файлов при каждом запуске.
        // Строка: ключ|размер|время изменения|sha256. Файл изменился — хэш считается заново.
        class HashCache
        {
            readonly string path;
            readonly Dictionary<string, string[]> map = new Dictionary<string, string[]>(StringComparer.OrdinalIgnoreCase);
            bool dirty;

            public HashCache(string path)
            {
                this.path = path;
                try
                {
                    if (!File.Exists(path)) return;
                    foreach (string line in File.ReadAllLines(path, Encoding.UTF8))
                    {
                        string[] p = line.Split('|');
                        if (p.Length == 4) map[p[0]] = p;
                    }
                }
                catch (Exception) { map.Clear(); }
            }

            public string Get(string key, long size, long ticks)
            {
                string[] p;
                if (!map.TryGetValue(key, out p)) return null;
                return p[1] == size.ToString() && p[2] == ticks.ToString() ? p[3] : null;
            }

            public void Put(string key, long size, long ticks, string sha)
            {
                map[key] = new string[] { key, size.ToString(), ticks.ToString(), sha };
                dirty = true;
            }

            public void Clear() { map.Clear(); dirty = true; }

            public void Save()
            {
                if (!dirty) return;
                try
                {
                    List<string> lines = new List<string>();
                    foreach (string[] p in map.Values) lines.Add(string.Join("|", p));
                    File.WriteAllLines(path, lines.ToArray(), Encoding.UTF8);
                    dirty = false;
                }
                catch (Exception) { }
            }
        }
    }
}

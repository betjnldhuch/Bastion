// =====================================================================
//  Bastion Online Launcher
//  Лаунчер для SA-MP: ник, выбор сервера, онлайн, новости, запуск игры.
//  Core.cs — настройки, опрос серверов, реестр SA-MP. Окно — в Ui.cs.
//  Пишется в стиле C# 5, чтобы собирался встроенным в Windows csc.exe
//  (см. build.bat). Настройки серверов — в launcher.ini рядом с exe.
// =====================================================================
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Windows.Forms;
using Microsoft.Win32;

[assembly: System.Reflection.AssemblyTitle("Bastion Online Launcher")]
[assembly: System.Reflection.AssemblyProduct("Bastion Online")]
[assembly: System.Reflection.AssemblyVersion("2.5.1.0")]

namespace BastionLauncher
{
    class ServerEntry
    {
        public string Name;
        public string Host;
        public int Port;
        public string Description;

        // результат последнего опроса
        public bool Online;
        public bool HasPassword;
        public int Players;
        public int MaxPlayers;
        public int Ping;
        public string HostName;
        public string GameMode;
    }

    class Config
    {
        public string Title = "Bastion Online";
        public bool RequireRpName = true;
        public string Site = "";
        public string NewsUrl = "";
        public string ServersUrl = "";
        public string UpdateUrl = "";
        public string GameUrl = "";
        public string ClientPackUrl = "";
        public string UiUrl = "";      // красивые окна на ПК (UiPack.cs), «-» — не ставить
        // Кнопки вверху окна: link = НАЗВАНИЕ | https://… (из launcher.ini или servers.ini в сети)
        public List<string[]> Links = new List<string[]>();

        // Новости, список серверов и версия: адрес из launcher.ini, иначе папка launcher/
        // на сервере раздачи (Updater.cs). «-» в launcher.ini выключает.
        public string EffectiveNewsUrl { get { return Remote.Pick(NewsUrl, "news.txt"); } }
        public string EffectiveServersUrl { get { return Remote.Pick(ServersUrl, "servers.ini"); } }
        // Обновление лаунчера: по умолчанию релиз «builds» на GitHub (CI выкладывает туда
        // каждую сборку main вместе с version-pc.ini), поэтому номер версии менять руками не нужно.
        public string EffectiveUpdateUrl
        {
            get
            {
                if (UpdateUrl == "-") return "";
                return UpdateUrl.Length > 0 ? UpdateUrl : Updater.DefaultVersionUrl;
            }
        }
        public string EffectiveClientPackUrl { get { return Remote.Pick(ClientPackUrl, "client-pack.json"); } }
        public string GameMd5 = "";

        // Архив игры: game_url из launcher.ini, иначе адрес, вшитый при сборке
        public string EffectiveGameUrl { get { return GameUrl.Length > 0 ? GameUrl : Secrets.GameUrl; } }
        public string EffectiveGameMd5
        {
            get
            {
                if (GameMd5.Length > 0) return GameMd5;
                return EffectiveGameUrl == Secrets.GameUrl ? Secrets.GameMd5 : "";
            }
        }
        public List<ServerEntry> Servers = new List<ServerEntry>();

        public static string IniPath
        {
            get { return Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "launcher.ini"); }
        }

        public static Config Load()
        {
            Config c = new Config();
            if (!File.Exists(IniPath))
            {
                File.WriteAllText(IniPath, DefaultIni, new UTF8Encoding(true));
            }
            foreach (string raw in File.ReadAllLines(IniPath, Encoding.UTF8))
            {
                string line = raw.Trim();
                if (line.Length == 0 || line.StartsWith(";") || line.StartsWith("#")) continue;
                int eq = line.IndexOf('=');
                if (eq <= 0) continue;
                string key = line.Substring(0, eq).Trim().ToLowerInvariant();
                string val = line.Substring(eq + 1).Trim();

                if (key == "title") c.Title = val;
                else if (key == "site") c.Site = val;
                else if (key == "news_url") c.NewsUrl = val;
                else if (key == "servers_url") c.ServersUrl = val;
                else if (key == "update_url") c.UpdateUrl = val;
                else if (key == "game_url") c.GameUrl = val;
                else if (key == "game_md5") c.GameMd5 = val;
                else if (key == "client_pack_url") c.ClientPackUrl = val;
                else if (key == "ui_url") c.UiUrl = val;
                else if (key == "link") AddLink(c.Links, val);
                else if (key == "require_rp_name") c.RequireRpName = (val == "1" || val.ToLowerInvariant() == "true");
                else if (key == "server")
                {
                    // server = Имя | адрес | порт | описание
                    string[] p = val.Split('|');
                    if (p.Length < 2) continue;
                    ServerEntry s = new ServerEntry();
                    s.Name = p[0].Trim();
                    s.Host = p[1].Trim();
                    s.Port = 7777;
                    if (p.Length >= 3)
                    {
                        int port;
                        if (int.TryParse(p[2].Trim(), out port)) s.Port = port;
                    }
                    s.Description = p.Length >= 4 ? p[3].Trim() : "";
                    c.Servers.Add(s);
                }
            }
            c.LoadRemoteServers();
            return c;
        }

        // Разбор строк вида "server = Имя | адрес | порт | описание"
        static List<ServerEntry> ParseServers(string txt)
        {
            List<ServerEntry> list = new List<ServerEntry>();
            foreach (string raw in txt.Replace("\r", "").Split('\n'))
            {
                string line = raw.Trim();
                if (line.Length == 0 || line.StartsWith(";") || line.StartsWith("#")) continue;
                int eq = line.IndexOf('=');
                if (eq <= 0 || line.Substring(0, eq).Trim().ToLowerInvariant() != "server") continue;
                string[] p = line.Substring(eq + 1).Split('|');
                if (p.Length < 2 || p[1].Trim().Length == 0) continue;
                ServerEntry s = new ServerEntry();
                s.Name = p[0].Trim();
                s.Host = p[1].Trim();
                s.Port = 7777;
                int port;
                if (p.Length >= 3 && int.TryParse(p[2].Trim(), out port) && port > 0 && port < 65536) s.Port = port;
                s.Description = p.Length >= 4 ? p[3].Trim() : "";
                list.Add(s);
            }
            return list;
        }

        // link = НАЗВАНИЕ | https://…   (только http/https, не больше 4 кнопок)
        static void AddLink(List<string[]> list, string val)
        {
            string[] p = val.Split('|');
            if (p.Length < 2 || list.Count >= 4) return;
            string name = p[0].Trim().ToUpperInvariant(), url = p[1].Trim();
            if (name.Length == 0 || name.Length > 12) return;
            if (!url.StartsWith("https://") && !url.StartsWith("http://")) return;
            list.Add(new string[] { name, url });
        }

        static List<string[]> ParseLinks(string txt)
        {
            List<string[]> list = new List<string[]>();
            foreach (string raw in txt.Replace("\r", "").Split('\n'))
            {
                string line = raw.Trim();
                int eq = line.IndexOf('=');
                if (eq <= 0 || line.StartsWith(";") || line.Substring(0, eq).Trim().ToLowerInvariant() != "link") continue;
                AddLink(list, line.Substring(eq + 1));
            }
            return list;
        }

        static string ServersCachePath
        {
            get { return Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "servers.cache"); }
        }

        // Общий список серверов в интернете (servers_url): меняешь адрес хостинга
        // в одном файле — у всех игроков он обновится без новой версии лаунчера.
        // Нет сети — берём последний скачанный список (servers.cache),
        // нет и его — сервера из launcher.ini.
        void LoadRemoteServers()
        {
            string url = EffectiveServersUrl;
            if (url.Length == 0) return;
            string txt = null;
            try
            {
                txt = Remote.FetchText(url, 4000);
                if (ParseServers(txt).Count > 0)
                {
                    try { File.WriteAllText(ServersCachePath, txt, new UTF8Encoding(true)); } catch (Exception) { }
                }
                else txt = null;
            }
            catch (Exception) { txt = null; }

            if (txt == null)
            {
                try { if (File.Exists(ServersCachePath)) txt = File.ReadAllText(ServersCachePath, Encoding.UTF8); }
                catch (Exception) { txt = null; }
            }
            if (txt == null) return;
            List<ServerEntry> remote = ParseServers(txt);
            if (remote.Count > 0) Servers = remote;
            List<string[]> links = ParseLinks(txt);
            if (links.Count > 0) Links = links;
        }

        public const string DefaultIni =
"; ================================================================\r\n" +
";  Настройки лаунчера Bastion Online\r\n" +
";  Строки, начинающиеся с ; — комментарии.\r\n" +
"; ================================================================\r\n" +
"\r\n" +
"title = Bastion Online\r\n" +
"\r\n" +
"; Требовать ник вида Имя_Фамилия (как на сервере)\r\n" +
"require_rp_name = true\r\n" +
"\r\n" +
"; Сайт или Discord (кнопка в лаунчере). Пусто = кнопки нет.\r\n" +
"site =\r\n" +
"\r\n" +
"; Кнопки вверху окна (до 4): link = НАЗВАНИЕ | ссылка. Строки link в servers.ini\r\n" +
"; на сервере раздачи важнее этих.\r\n" +
"; link = ФОРУМ | https://forum.example.com\r\n" +
"; link = ДОНАТ | https://example.com/donate\r\n" +
"\r\n" +
"; Новости и список серверов лаунчер берёт из сети сам (папка launcher/ на сервере\r\n" +
"; раздачи игры), обновления — из релиза builds на GitHub. Если сети нет — из news.txt\r\n" +
"; и строк server ниже.\r\n" +
"; Сюда можно вписать свою ссылку, а знак - (минус) выключает загрузку из сети.\r\n" +
"news_url =\r\n" +
"servers_url =\r\n" +
"update_url =\r\n" +
"\r\n" +
"; Архив игры для кнопки УСТАНОВИТЬ (ZIP с gta_sa.exe и samp.exe) и его MD5.\r\n" +
"; Пусто = архив Bastion, вшитый в лаунчер при сборке.\r\n" +
"; GitHub Releases, Яндекс Диск или прямая ссылка. Части — через |, зеркала — через ;\r\n" +
"game_url =\r\n" +
"game_md5 =\r\n" +
"\r\n" +
"; Сервера:  server = Название | адрес | порт | описание\r\n" +
"; Адрес: 127.0.0.1 — свой компьютер; IP из Radmin VPN — для друзей;\r\n" +
"; потом сюда впишешь адрес хостинга.\r\n" +
"server = Bastion Peace | 192.168.31.182 | 7777 | Классический RP, как в старые добрые времена\r\n" +
"server = Bastion War   | 192.168.31.182 | 7778 | Сервер для сильнейших. Никакой администрации — всё решаешь ты\r\n";
    }

    // -----------------------------------------------------------------
    //  Опрос сервера по стандартному query-протоколу SA-MP (UDP, 'i')
    // -----------------------------------------------------------------
    static class SampQuery
    {
        static readonly Encoding Cp1251 = Encoding.GetEncoding(1251);

        public static void Query(ServerEntry s)
        {
            s.Online = false;
            try
            {
                IPAddress ip = Resolve(s.Host);
                if (ip == null) return;

                byte[] pkt = new byte[11];
                pkt[0] = (byte)'S'; pkt[1] = (byte)'A'; pkt[2] = (byte)'M'; pkt[3] = (byte)'P';
                byte[] ipb = ip.GetAddressBytes();
                Array.Copy(ipb, 0, pkt, 4, 4);
                pkt[8] = (byte)(s.Port & 0xFF);
                pkt[9] = (byte)((s.Port >> 8) & 0xFF);
                pkt[10] = (byte)'i';

                using (UdpClient udp = new UdpClient())
                {
                    udp.Client.ReceiveTimeout = 1500;
                    IPEndPoint ep = new IPEndPoint(ip, s.Port);
                    Stopwatch sw = Stopwatch.StartNew();
                    udp.Send(pkt, pkt.Length, ep);
                    IPEndPoint from = new IPEndPoint(IPAddress.Any, 0);
                    byte[] r = udp.Receive(ref from);
                    sw.Stop();
                    if (r.Length < 11 + 5) return;

                    int o = 11;
                    s.HasPassword = r[o] != 0; o += 1;
                    s.Players = BitConverter.ToUInt16(r, o); o += 2;
                    s.MaxPlayers = BitConverter.ToUInt16(r, o); o += 2;
                    s.HostName = ReadStr(r, ref o);
                    s.GameMode = ReadStr(r, ref o);
                    s.Ping = (int)sw.ElapsedMilliseconds;
                    s.Online = true;
                }
            }
            catch (Exception)
            {
                s.Online = false;
            }
        }

        static string ReadStr(byte[] r, ref int o)
        {
            if (o + 4 > r.Length) return "";
            int len = BitConverter.ToInt32(r, o); o += 4;
            if (len < 0 || o + len > r.Length) return "";
            string v = Cp1251.GetString(r, o, len); o += len;
            return v;
        }

        public static IPAddress Resolve(string host)
        {
            IPAddress ip;
            if (IPAddress.TryParse(host, out ip)) return ip;
            try
            {
                foreach (IPAddress a in Dns.GetHostAddresses(host))
                    if (a.AddressFamily == AddressFamily.InterNetwork) return a;
            }
            catch (Exception) { }
            return null;
        }
    }

    // -----------------------------------------------------------------
    //  Работа с реестром SA-MP и папкой GTA
    // -----------------------------------------------------------------
    static class Samp
    {
        const string RegPath = @"Software\SAMP";

        public static string GetValue(string name)
        {
            try
            {
                using (RegistryKey k = Registry.CurrentUser.OpenSubKey(RegPath))
                {
                    if (k == null) return "";
                    object v = k.GetValue(name);
                    return v == null ? "" : v.ToString();
                }
            }
            catch (Exception) { return ""; }
        }

        public static void SetValue(string name, string value)
        {
            using (RegistryKey k = Registry.CurrentUser.CreateSubKey(RegPath))
            {
                k.SetValue(name, value, RegistryValueKind.String);
            }
        }

        // В реестре SA-MP хранит путь к gta_sa.exe (ключ gta_sa_exe)
        public static string GtaFolder
        {
            get
            {
                string exe = GetValue("gta_sa_exe");
                if (exe.Length == 0) return "";
                try { return Path.GetDirectoryName(exe); } catch (Exception) { return ""; }
            }
        }

        // Файлы и папки модов, из-за которых SA-MP часто вылетает
        static readonly string[] ModFolders = { "cleo", "modloader", "moonloader", "scripts", "SAMPFUNCS" };
        static readonly string[] ModMasks = { "*.asi", "*.cleo", "d3d9.dll", "dinput8.dll" };
        // Файлы SA-MP / open.mp, которые трогать нельзя
        static readonly string[] Keep = { "samp.asi", "omp-client.asi", "samp.dll", "omp-client.dll", "vorbisfile.dll", "vorbis.dll", "ogg.dll" };

        public static List<string> FindMods(string gta)
        {
            List<string> list = new List<string>();
            if (gta.Length == 0 || !Directory.Exists(gta)) return list;
            foreach (string mask in ModMasks)
            {
                foreach (string f in Directory.GetFiles(gta, mask, SearchOption.TopDirectoryOnly))
                {
                    string n = Path.GetFileName(f).ToLowerInvariant();
                    if (Array.IndexOf(Keep, n) >= 0) continue;
                    if (FpsPack.IsPackFile(f)) continue;   // наш пакет FPS (сверяется по содержимому)
                    if (!list.Contains(f)) list.Add(f);
                }
            }
            foreach (string d in ModFolders)
            {
                string p = Path.Combine(gta, d);
                if (Directory.Exists(p)) list.Add(p);
            }
            return list;
        }

        // Переносит моды в папку _off (вернуть можно руками обратно)
        public static int DisableMods(string gta, List<string> mods)
        {
            string off = Path.Combine(gta, "_off");
            Directory.CreateDirectory(off);
            int moved = 0;
            foreach (string m in mods)
            {
                string target = Path.Combine(off, Path.GetFileName(m));
                if (File.Exists(target) || Directory.Exists(target))
                    target = target + "_" + DateTime.Now.ToString("yyyyMMddHHmmss");
                if (Directory.Exists(m)) Directory.Move(m, target);
                else File.Move(m, target);
                moved++;
            }
            return moved;
        }
    }

    // -----------------------------------------------------------------
    //  Пакет FPS: папка fps\ рядом с лаунчером.
    //  SA-MP сам не даёт больше 90 FPS (/fpslimit 20..90). Чтобы было
    //  120/144/165, в игру нужны ASI-плагины: загрузчик ASI, снятие лимита
    //  и фиксы физики. Команда кладёт их в папку fps\ (см. fps\README.txt),
    //  лаунчер копирует их в папку GTA и пишет туда выбранный лимит.
    //  Проверка модов пропускает эти файлы, только если они байт в байт
    //  совпадают с файлами пакета: переименованный чит так не пройдёт.
    // -----------------------------------------------------------------
    static class FpsPack
    {
        public const int SampMax = 90;          // больше SA-MP сам не умеет
        public const int MinFps = 30;
        public static readonly int[] Marks = { 60, 90, 120, 144 };   // засечки на ползунке

        public static string Dir
        {
            get { return Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "fps"); }
        }

        static string IniPath { get { return Path.Combine(Dir, "fps.ini"); } }

        // Настройки пакета из fps\fps.ini
        public static string LimiterIni = "";   // файл настроек плагина снятия лимита (в папке GTA)
        public static string LimiterKey = "fps";// строка в нём, где хранится лимит
        public static int MaxFps = 165;         // больше этого лаунчер не предложит

        static List<string> files;              // файлы пакета (полные пути)
        static Dictionary<string, string> hashes = new Dictionary<string, string>();

        public static void Load()
        {
            files = new List<string>();
            if (!Directory.Exists(Dir)) return;
            if (File.Exists(IniPath))
            {
                foreach (string raw in File.ReadAllLines(IniPath, Encoding.UTF8))
                {
                    string line = raw.Trim();
                    if (line.Length == 0 || line.StartsWith(";") || line.StartsWith("#")) continue;
                    int eq = line.IndexOf('=');
                    if (eq <= 0) continue;
                    string key = line.Substring(0, eq).Trim().ToLowerInvariant();
                    string val = line.Substring(eq + 1).Trim();
                    int n;
                    if (key == "limiter_ini") LimiterIni = val;
                    else if (key == "limiter_key" && val.Length > 0) LimiterKey = val;
                    else if (key == "max_fps" && int.TryParse(val, out n) && n > SampMax) MaxFps = n;
                }
            }
            foreach (string f in Directory.GetFiles(Dir))
            {
                string n = Path.GetFileName(f).ToLowerInvariant();
                if (n == "fps.ini" || n.StartsWith("readme") || n.StartsWith("license")) continue;
                files.Add(f);
            }
        }

        // Есть ли хоть один плагин (.asi) — без него выше 90 не поднять
        public static bool Available
        {
            get
            {
                if (files == null) Load();
                foreach (string f in files)
                    if (f.EndsWith(".asi", StringComparison.OrdinalIgnoreCase)) return true;
                return false;
            }
        }

        static string Hash(string path)
        {
            string h;
            if (hashes.TryGetValue(path, out h)) return h;
            using (SHA256 sha = SHA256.Create())
            using (FileStream fs = File.OpenRead(path))
                h = BitConverter.ToString(sha.ComputeHash(fs));
            hashes[path] = h;
            return h;
        }

        // Файл в папке GTA — это файл нашего пакета (то же имя и то же содержимое)?
        public static bool IsPackFile(string gtaFile)
        {
            if (files == null) Load();
            string name = Path.GetFileName(gtaFile);
            foreach (string f in files)
            {
                if (!string.Equals(Path.GetFileName(f), name, StringComparison.OrdinalIgnoreCase)) continue;
                try
                {
                    hashes.Remove(gtaFile);   // файл в игре мог поменяться
                    return Hash(f) == Hash(gtaFile);
                }
                catch (Exception) { return false; }
            }
            return false;
        }

        // Копирует пакет в папку GTA (только то, чего нет или что отличается).
        // Настройки плагинов (.ini), которые уже лежат в игре, не трогает.
        public static int Install(string gta)
        {
            if (files == null) Load();
            int copied = 0;
            foreach (string f in files)
            {
                string target = Path.Combine(gta, Path.GetFileName(f));
                bool isIni = f.EndsWith(".ini", StringComparison.OrdinalIgnoreCase);
                if (File.Exists(target) && (isIni || IsPackFile(target))) continue;
                File.Copy(f, target, true);
                hashes.Remove(target);
                copied++;
            }
            return copied;
        }

        // Есть ли в пакете FPS файл с таким именем (лаунчер копирует его в папку игры)
        public static bool Owns(string fileName)
        {
            if (files == null) Load();
            foreach (string f in files)
                if (string.Equals(Path.GetFileName(f), fileName, StringComparison.OrdinalIgnoreCase)) return true;
            return false;
        }

        // Установлен ли пакет в игре (все .asi/.dll на месте и совпадают)
        public static bool Installed(string gta)
        {
            if (files == null) Load();
            if (files.Count == 0) return false;
            foreach (string f in files)
            {
                if (f.EndsWith(".ini", StringComparison.OrdinalIgnoreCase)) continue;
                string target = Path.Combine(gta, Path.GetFileName(f));
                if (!File.Exists(target) || !IsPackFile(target)) return false;
            }
            return true;
        }

        // Записывает лимит в настройки плагина: меняет строку "fps = ..." или добавляет её
        public static void WriteLimit(string gta, int fps)
        {
            if (LimiterIni.Length == 0) return;
            string path = Path.Combine(gta, LimiterIni);
            List<string> lines = new List<string>();
            if (File.Exists(path)) lines.AddRange(File.ReadAllLines(path, Encoding.Default));
            bool done = false;
            for (int i = 0; i < lines.Count; i++)
            {
                string t = lines[i].Trim();
                int eq = t.IndexOf('=');
                if (eq > 0 && string.Equals(t.Substring(0, eq).Trim(), LimiterKey, StringComparison.OrdinalIgnoreCase))
                {
                    lines[i] = LimiterKey + "=" + fps;
                    done = true;
                }
            }
            if (!done)
            {
                // ключ кладём в первую секцию файла (или в начало, если секций нет)
                int at = -1;
                for (int i = 0; i < lines.Count; i++)
                    if (lines[i].Trim().StartsWith("[")) { at = i + 1; break; }
                if (at < 0)
                {
                    // файла ещё нет (плагин создаёт его при первом запуске): секция по имени файла, как у ModernFpsLimit
                    lines.Insert(0, "[" + Path.GetFileNameWithoutExtension(LimiterIni) + "]");
                    at = 1;
                }
                lines.Insert(at, LimiterKey + "=" + fps);
            }
            File.WriteAllLines(path, lines.ToArray(), Encoding.Default);
        }
    }

    // -----------------------------------------------------------------
    //  Настройки клиента SA-MP: Документы\GTA San Andreas User Files\SAMP\sa-mp.cfg
    // -----------------------------------------------------------------
    class SampCfg
    {
        List<string> keys = new List<string>();
        Dictionary<string, string> vals = new Dictionary<string, string>();

        public static string FilePath
        {
            get
            {
                return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),
                    @"GTA San Andreas User Files\SAMP\sa-mp.cfg");
            }
        }

        public static SampCfg Load()
        {
            SampCfg c = new SampCfg();
            try
            {
                if (File.Exists(FilePath))
                    foreach (string raw in File.ReadAllLines(FilePath, Encoding.Default))
                    {
                        int eq = raw.IndexOf('=');
                        if (eq <= 0) continue;
                        c.Set(raw.Substring(0, eq).Trim(), raw.Substring(eq + 1).Trim());
                    }
            }
            catch (Exception) { }
            return c;
        }

        public int GetInt(string k, int def)
        {
            string v; int r;
            if (vals.TryGetValue(k.ToLowerInvariant(), out v) && int.TryParse(v, out r)) return r;
            return def;
        }

        public void Set(string k, string v)
        {
            string lk = k.ToLowerInvariant();
            if (!vals.ContainsKey(lk)) keys.Add(lk);
            vals[lk] = v;
        }

        public void Save()
        {
            Directory.CreateDirectory(Path.GetDirectoryName(FilePath));
            StringBuilder sb = new StringBuilder();
            foreach (string k in keys) sb.Append(k).Append('=').Append(vals[k]).Append("\r\n");
            File.WriteAllText(FilePath, sb.ToString(), Encoding.Default);
        }
    }

    // -----------------------------------------------------------------
    //  Окно выбора разрешения GTA SA.
    //  При первом запуске (нет gta_sa.set) или на ПК с двумя видеокартами
    //  игра спрашивает устройство и разрешение экрана. Лаунчер после запуска
    //  сам выбирает разрешение рабочего стола и жмёт OK, игрок окна не видит.
    //  Трогаем только окна игры со списком (ComboBox/ListBox): сообщения об
    //  ошибках без списка остаются как есть.
    // -----------------------------------------------------------------
    static class VideoModeAuto
    {
        delegate bool EnumProc(IntPtr h, IntPtr l);
        [System.Runtime.InteropServices.DllImport("user32.dll")] static extern bool EnumWindows(EnumProc cb, IntPtr l);
        [System.Runtime.InteropServices.DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr parent, EnumProc cb, IntPtr l);
        [System.Runtime.InteropServices.DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
        [System.Runtime.InteropServices.DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
        [System.Runtime.InteropServices.DllImport("user32.dll")] static extern bool IsWindow(IntPtr h);
        [System.Runtime.InteropServices.DllImport("user32.dll", CharSet = System.Runtime.InteropServices.CharSet.Unicode)]
        static extern int GetClassName(IntPtr h, StringBuilder sb, int max);
        [System.Runtime.InteropServices.DllImport("user32.dll")] static extern int GetDlgCtrlID(IntPtr h);
        [System.Runtime.InteropServices.DllImport("user32.dll", CharSet = System.Runtime.InteropServices.CharSet.Unicode)]
        static extern IntPtr SendMessageTimeout(IntPtr h, int msg, IntPtr w, IntPtr l, int flags, int timeout, out IntPtr result);
        [System.Runtime.InteropServices.DllImport("user32.dll", CharSet = System.Runtime.InteropServices.CharSet.Unicode)]
        static extern IntPtr SendMessageTimeout(IntPtr h, int msg, IntPtr w, StringBuilder l, int flags, int timeout, out IntPtr result);
        [System.Runtime.InteropServices.DllImport("user32.dll")] static extern bool PostMessage(IntPtr h, int msg, IntPtr w, IntPtr l);
        [System.Runtime.InteropServices.DllImport("user32.dll")] static extern IntPtr GetDC(IntPtr h);
        [System.Runtime.InteropServices.DllImport("user32.dll")] static extern int ReleaseDC(IntPtr h, IntPtr dc);
        [System.Runtime.InteropServices.DllImport("gdi32.dll")] static extern int GetDeviceCaps(IntPtr dc, int index);

        const int WM_COMMAND = 0x0111, IDOK = 1, SMTO_ABORTIFHUNG = 2;
        const int CB_GETCOUNT = 0x0146, CB_GETLBTEXT = 0x0148, CB_GETLBTEXTLEN = 0x0149, CB_SETCURSEL = 0x014E;
        const int LB_SETCURSEL = 0x0186, LB_GETTEXT = 0x0189, LB_GETTEXTLEN = 0x018A, LB_GETCOUNT = 0x018B;
        const int SELCHANGE = 1;   // CBN_SELCHANGE и LBN_SELCHANGE
        static readonly Regex Mode = new Regex(@"(\d{3,5})\s*[xXхХ×*]\s*(\d{3,5})(?:\s*[xXхХ×*]\s*(\d{1,2}))?");

        static int deskW, deskH;

        // Следить за окнами игры 2 минуты после запуска (в своём потоке: лаунчер может закрыться раньше)
        public static void Watch()
        {
            Thread t = new Thread(Run);
            t.IsBackground = false;   // закрытие лаунчера не обрывает слежку
            t.Start();
        }

        static void Run()
        {
            try
            {
                IntPtr dc = GetDC(IntPtr.Zero);
                deskW = GetDeviceCaps(dc, 118);   // DESKTOPHORZRES: настоящий размер, без масштаба Windows
                deskH = GetDeviceCaps(dc, 117);   // DESKTOPVERTRES
                ReleaseDC(IntPtr.Zero, dc);
            }
            catch (Exception) { }
            if (deskW <= 0 || deskH <= 0) { deskW = Screen.PrimaryScreen.Bounds.Width; deskH = Screen.PrimaryScreen.Bounds.Height; }

            HashSet<IntPtr> done = new HashSet<IntPtr>();
            DateTime end = DateTime.Now.AddSeconds(120);
            bool seen = false;
            while (DateTime.Now < end)
            {
                try
                {
                    HashSet<uint> pids = new HashSet<uint>();
                    foreach (Process p in Process.GetProcessesByName("gta_sa")) { pids.Add((uint)p.Id); p.Dispose(); }
                    if (pids.Count > 0) seen = true;
                    else if (seen) return;   // игра закрылась
                    if (pids.Count > 0)
                    {
                        List<IntPtr> dialogs = new List<IntPtr>();
                        EnumWindows(delegate (IntPtr h, IntPtr l)
                        {
                            uint pid;
                            GetWindowThreadProcessId(h, out pid);
                            if (pids.Contains(pid) && IsWindowVisible(h) && ClassOf(h) == "#32770") dialogs.Add(h);
                            return true;
                        }, IntPtr.Zero);
                        foreach (IntPtr d in dialogs)
                            if (!done.Contains(d) && Handle(d)) done.Add(d);
                    }
                }
                catch (Exception) { }
                Thread.Sleep(250);
            }
        }

        static string ClassOf(IntPtr h)
        {
            StringBuilder sb = new StringBuilder(64);
            GetClassName(h, sb, sb.Capacity);
            return sb.ToString();
        }

        // Окно игры со списком: выбрать разрешение (если это список разрешений) и нажать OK
        static bool Handle(IntPtr dlg)
        {
            List<IntPtr> lists = new List<IntPtr>();
            IntPtr ok = IntPtr.Zero;
            EnumChildWindows(dlg, delegate (IntPtr h, IntPtr l)
            {
                string c = ClassOf(h).ToLowerInvariant();
                if (c == "combobox" || c == "listbox") lists.Add(h);
                else if (c == "button" && GetDlgCtrlID(h) == IDOK) ok = h;
                return true;
            }, IntPtr.Zero);
            if (lists.Count == 0) return true;   // не наше окно (ошибка и т.п.) — не трогаем

            foreach (IntPtr list in lists)
            {
                bool combo = ClassOf(list).ToLowerInvariant() == "combobox";
                int count = (int)Send(list, combo ? CB_GETCOUNT : LB_GETCOUNT, 0);
                int best = -1;
                long bestScore = -1;
                for (int i = 0; i < count && i < 1000; i++)
                {
                    long s = Score(Text(list, combo, i));
                    if (s > bestScore) { bestScore = s; best = i; }
                }
                if (best < 0 || bestScore <= 0) continue;   // не список разрешений (например, выбор устройства)
                Send(list, combo ? CB_SETCURSEL : LB_SETCURSEL, best);
                int id = GetDlgCtrlID(list);
                IntPtr r;
                SendMessageTimeout(dlg, WM_COMMAND, (IntPtr)((SELCHANGE << 16) | (id & 0xFFFF)), list, SMTO_ABORTIFHUNG, 2000, out r);
            }
            Thread.Sleep(150);
            if (!IsWindow(dlg)) return true;
            PostMessage(dlg, WM_COMMAND, (IntPtr)IDOK, ok);
            return true;
        }

        static IntPtr Send(IntPtr h, int msg, int w)
        {
            IntPtr r;
            SendMessageTimeout(h, msg, (IntPtr)w, IntPtr.Zero, SMTO_ABORTIFHUNG, 2000, out r);
            return r;
        }

        static string Text(IntPtr list, bool combo, int i)
        {
            int len = (int)Send(list, combo ? CB_GETLBTEXTLEN : LB_GETTEXTLEN, i);
            if (len <= 0 || len > 512) return "";
            StringBuilder sb = new StringBuilder(len + 2);
            IntPtr r;
            SendMessageTimeout(list, combo ? CB_GETLBTEXT : LB_GETTEXT, (IntPtr)i, sb, SMTO_ABORTIFHUNG, 2000, out r);
            return sb.ToString();
        }

        // Чем лучше разрешение, тем больше число. 0 — строка не похожа на разрешение.
        // Лучше всего — ровно как у рабочего стола в 32 бита, дальше — самое большое, что влезает в экран.
        static long Score(string s)
        {
            Match m = Mode.Match(s);
            if (!m.Success) return 0;
            long w = long.Parse(m.Groups[1].Value), h = long.Parse(m.Groups[2].Value);
            int bits = m.Groups[3].Success ? int.Parse(m.Groups[3].Value) : 32;
            long score = w * h;                                     // до ~33 млн
            if (bits == 32) score += 100000000L;
            if (w <= deskW && h <= deskH) score += 1000000000L;
            if (w == deskW && h == deskH) score += 10000000000L;
            return score;
        }
    }

    // -----------------------------------------------------------------
    //  Окно лаунчера
}

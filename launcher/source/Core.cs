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
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Windows.Forms;
using Microsoft.Win32;

[assembly: System.Reflection.AssemblyTitle("Bastion Online Launcher")]
[assembly: System.Reflection.AssemblyProduct("Bastion Online")]
[assembly: System.Reflection.AssemblyVersion("2.0.0.0")]

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
            return c;
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
"; Новости: берутся из news.txt рядом с лаунчером. Можно указать ссылку на txt в интернете.\r\n" +
"news_url =\r\n" +
"\r\n" +
"; Сервера:  server = Название | адрес | порт | описание\r\n" +
"; Адрес: 127.0.0.1 — свой компьютер; IP из Radmin VPN — для друзей;\r\n" +
"; потом сюда впишешь адрес хостинга.\r\n" +
"server = Bastion Peace | 127.0.0.1 | 7777 | Классический RP, как в старые добрые времена\r\n" +
"server = Bastion War   | 127.0.0.1 | 7778 | Сервер для сильнейших. Никакой администрации — всё решаешь ты\r\n";
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
    //  Окно лаунчера
}

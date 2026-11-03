// =====================================================================
//  Bastion RP Launcher
//  Простой лаунчер для SA-MP: ник, выбор сервера, онлайн, запуск игры.
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

[assembly: System.Reflection.AssemblyTitle("Bastion RP Launcher")]
[assembly: System.Reflection.AssemblyProduct("Bastion RP")]
[assembly: System.Reflection.AssemblyVersion("1.0.0.0")]

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
        public string Title = "Bastion RP";
        public bool RequireRpName = true;
        public string Site = "";
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
";  Настройки лаунчера Bastion RP\r\n" +
";  Строки, начинающиеся с ; — комментарии.\r\n" +
"; ================================================================\r\n" +
"\r\n" +
"title = Bastion RP\r\n" +
"\r\n" +
"; Требовать ник вида Имя_Фамилия (как на сервере)\r\n" +
"require_rp_name = true\r\n" +
"\r\n" +
"; Сайт или Discord (кнопка в лаунчере). Пусто = кнопки нет.\r\n" +
"site =\r\n" +
"\r\n" +
"; Сервера:  server = Название | адрес | порт | описание\r\n" +
"; Адрес: 127.0.0.1 — свой компьютер; IP из Radmin VPN — для друзей;\r\n" +
"; потом сюда впишешь адрес хостинга.\r\n" +
"server = Bastion Peace | 127.0.0.1 | 7777 | Классический RP, админы помогают\r\n" +
"server = Bastion War   | 127.0.0.1 | 7778 | Закон сильного, админы не вмешиваются\r\n";
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
    //  Окно лаунчера
    // -----------------------------------------------------------------
    class MainForm : Form
    {
        static readonly Color BgDark = Color.FromArgb(18, 20, 26);
        static readonly Color BgPanel = Color.FromArgb(30, 33, 42);
        static readonly Color Accent = Color.FromArgb(214, 158, 46);
        static readonly Color TextMain = Color.FromArgb(235, 235, 240);
        static readonly Color TextDim = Color.FromArgb(150, 155, 170);
        static readonly Color Good = Color.FromArgb(90, 200, 120);
        static readonly Color Bad = Color.FromArgb(220, 90, 90);

        Config cfg;
        ListView list;
        TextBox nick;
        TextBox gtaPath;
        Label status;
        Label descr;
        Button play;
        System.Windows.Forms.Timer refresh;
        int querying;

        public MainForm()
        {
            cfg = Config.Load();

            Text = cfg.Title + " — лаунчер";
            ClientSize = new Size(620, 520);
            FormBorderStyle = FormBorderStyle.FixedSingle;
            MaximizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            BackColor = BgDark;
            ForeColor = TextMain;
            Font = new Font("Segoe UI", 10f);
            try { Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch (Exception) { }

            // --- шапка ---
            Label logo = new Label();
            logo.Text = cfg.Title.ToUpperInvariant();
            logo.Font = new Font("Segoe UI", 26f, FontStyle.Bold);
            logo.ForeColor = Accent;
            logo.AutoSize = true;
            logo.Location = new Point(20, 14);
            Controls.Add(logo);

            Label sub = new Label();
            sub.Text = "San Andreas Multiplayer · Role Play";
            sub.ForeColor = TextDim;
            sub.AutoSize = true;
            sub.Location = new Point(24, 62);
            Controls.Add(sub);

            // --- список серверов ---
            Label ls = MakeCaption("Сервер", 20, 96);
            Controls.Add(ls);

            list = new ListView();
            list.View = View.Details;
            list.FullRowSelect = true;
            list.MultiSelect = false;
            list.HideSelection = false;
            list.HeaderStyle = ColumnHeaderStyle.Nonclickable;
            list.BackColor = BgPanel;
            list.ForeColor = TextMain;
            list.BorderStyle = BorderStyle.None;
            list.Location = new Point(20, 120);
            list.Size = new Size(580, 130);
            list.Columns.Add("Название", 170);
            list.Columns.Add("Адрес", 170);
            list.Columns.Add("Онлайн", 120);
            list.Columns.Add("Пинг", 100);
            foreach (ServerEntry s in cfg.Servers)
            {
                ListViewItem it = new ListViewItem(s.Name);
                it.SubItems.Add(s.Host + ":" + s.Port);
                it.SubItems.Add("…");
                it.SubItems.Add("");
                it.Tag = s;
                list.Items.Add(it);
            }
            list.SelectedIndexChanged += delegate { UpdateDescr(); };
            list.DoubleClick += delegate { Play(); };
            Controls.Add(list);

            descr = new Label();
            descr.ForeColor = TextDim;
            descr.Location = new Point(20, 256);
            descr.Size = new Size(580, 22);
            Controls.Add(descr);

            // --- ник ---
            Controls.Add(MakeCaption("Ник (Имя_Фамилия)", 20, 286));
            nick = new TextBox();
            nick.Location = new Point(20, 310);
            nick.Size = new Size(280, 28);
            nick.MaxLength = 24;
            nick.BackColor = BgPanel;
            nick.ForeColor = TextMain;
            nick.BorderStyle = BorderStyle.FixedSingle;
            nick.Text = Samp.GetValue("PlayerName");
            Controls.Add(nick);

            // --- папка GTA ---
            Controls.Add(MakeCaption("Папка GTA San Andreas (где gta_sa.exe и samp.exe)", 20, 346));
            gtaPath = new TextBox();
            gtaPath.Location = new Point(20, 370);
            gtaPath.Size = new Size(470, 28);
            gtaPath.BackColor = BgPanel;
            gtaPath.ForeColor = TextMain;
            gtaPath.BorderStyle = BorderStyle.FixedSingle;
            gtaPath.Text = Samp.GtaFolder;
            Controls.Add(gtaPath);

            Button browse = MakeButton("Обзор…", 500, 369, 100, 28, false);
            browse.Click += delegate { Browse(); };
            Controls.Add(browse);

            // --- кнопки снизу ---
            play = MakeButton("ИГРАТЬ", 420, 420, 180, 48, true);
            play.Font = new Font("Segoe UI", 14f, FontStyle.Bold);
            play.Click += delegate { Play(); };
            Controls.Add(play);

            Button mods = MakeButton("Проверить моды", 20, 420, 150, 48, false);
            mods.Click += delegate { CheckMods(true); };
            Controls.Add(mods);

            Button upd = MakeButton("Обновить", 180, 420, 110, 48, false);
            upd.Click += delegate { RefreshAll(); };
            Controls.Add(upd);

            if (cfg.Site.Length > 0)
            {
                Button site = MakeButton("Сайт", 300, 420, 110, 48, false);
                site.Click += delegate { try { Process.Start(cfg.Site); } catch (Exception) { } };
                Controls.Add(site);
            }

            status = new Label();
            status.ForeColor = TextDim;
            status.Location = new Point(20, 484);
            status.Size = new Size(580, 26);
            status.Text = "Готово.";
            Controls.Add(status);

            if (list.Items.Count > 0)
            {
                int last = 0;
                int.TryParse(Samp.GetValue("BastionLastServer"), out last);
                if (last < 0 || last >= list.Items.Count) last = 0;
                list.Items[last].Selected = true;
            }

            refresh = new System.Windows.Forms.Timer();
            refresh.Interval = 10000;
            refresh.Tick += delegate { RefreshAll(); };
            refresh.Start();

            Shown += delegate { RefreshAll(); };
        }

        Label MakeCaption(string text, int x, int y)
        {
            Label l = new Label();
            l.Text = text;
            l.ForeColor = TextDim;
            l.Font = new Font("Segoe UI", 9f, FontStyle.Bold);
            l.AutoSize = true;
            l.Location = new Point(x, y);
            return l;
        }

        Button MakeButton(string text, int x, int y, int w, int h, bool primary)
        {
            Button b = new Button();
            b.Text = text;
            b.Location = new Point(x, y);
            b.Size = new Size(w, h);
            b.FlatStyle = FlatStyle.Flat;
            b.FlatAppearance.BorderSize = primary ? 0 : 1;
            b.FlatAppearance.BorderColor = Color.FromArgb(70, 75, 90);
            b.BackColor = primary ? Accent : BgPanel;
            b.ForeColor = primary ? Color.FromArgb(20, 20, 20) : TextMain;
            b.Cursor = Cursors.Hand;
            return b;
        }

        ServerEntry Selected
        {
            get
            {
                if (list.SelectedItems.Count == 0) return null;
                return (ServerEntry)list.SelectedItems[0].Tag;
            }
        }

        void UpdateDescr()
        {
            ServerEntry s = Selected;
            if (s == null) { descr.Text = ""; return; }
            string t = s.Description;
            if (s.Online && !string.IsNullOrEmpty(s.GameMode)) t += (t.Length > 0 ? "  ·  " : "") + s.GameMode;
            descr.Text = t;
        }

        // --- опрос всех серверов в фоне ---
        void RefreshAll()
        {
            if (Interlocked.CompareExchange(ref querying, 1, 0) != 0) return;
            Thread th = new Thread(delegate ()
            {
                foreach (ServerEntry s in cfg.Servers) SampQuery.Query(s);
                try { BeginInvoke(new MethodInvoker(ShowResults)); } catch (Exception) { }
                Interlocked.Exchange(ref querying, 0);
            });
            th.IsBackground = true;
            th.Start();
        }

        void ShowResults()
        {
            foreach (ListViewItem it in list.Items)
            {
                ServerEntry s = (ServerEntry)it.Tag;
                it.UseItemStyleForSubItems = false;
                if (s.Online)
                {
                    it.SubItems[2].Text = s.Players + " / " + s.MaxPlayers + (s.HasPassword ? " (пароль)" : "");
                    it.SubItems[2].ForeColor = Good;
                    it.SubItems[3].Text = s.Ping + " мс";
                }
                else
                {
                    it.SubItems[2].Text = "выключен";
                    it.SubItems[2].ForeColor = Bad;
                    it.SubItems[3].Text = "—";
                }
                it.SubItems[2].BackColor = BgPanel;
                it.SubItems[3].BackColor = BgPanel;
                it.SubItems[3].ForeColor = TextMain;
                it.SubItems[1].BackColor = BgPanel;
                it.SubItems[1].ForeColor = TextMain;
            }
            UpdateDescr();
        }

        void Browse()
        {
            using (OpenFileDialog d = new OpenFileDialog())
            {
                d.Title = "Найди gta_sa.exe";
                d.Filter = "GTA San Andreas|gta_sa.exe|Все exe|*.exe";
                if (gtaPath.Text.Length > 0 && Directory.Exists(gtaPath.Text)) d.InitialDirectory = gtaPath.Text;
                if (d.ShowDialog(this) == DialogResult.OK)
                    gtaPath.Text = Path.GetDirectoryName(d.FileName);
            }
        }

        // Проверка модов. Возвращает true, если можно запускать.
        bool CheckMods(bool manual)
        {
            string gta = gtaPath.Text.Trim();
            if (gta.Length == 0 || !Directory.Exists(gta))
            {
                if (manual) Warn("Сначала укажи папку GTA San Andreas.");
                return true;
            }
            List<string> mods = Samp.FindMods(gta);
            if (mods.Count == 0)
            {
                if (manual) MessageBox.Show(this, "Лишних модов не найдено. Всё чисто!", cfg.Title, MessageBoxButtons.OK, MessageBoxIcon.Information);
                return true;
            }
            StringBuilder sb = new StringBuilder();
            sb.AppendLine("В папке игры найдены моды, из-за которых SA-MP может вылетать:");
            sb.AppendLine();
            int shown = 0;
            foreach (string m in mods)
            {
                if (shown++ >= 15) { sb.AppendLine("  … и ещё " + (mods.Count - 15)); break; }
                sb.AppendLine("  • " + Path.GetFileName(m));
            }
            sb.AppendLine();
            sb.AppendLine("Перенести их в папку _off? (Вернуть можно в любой момент, просто переложив обратно.)");
            if (!manual) sb.AppendLine("\r\n«Нет» — запустить игру как есть.");

            DialogResult r = MessageBox.Show(this, sb.ToString(), cfg.Title,
                manual ? MessageBoxButtons.YesNo : MessageBoxButtons.YesNoCancel, MessageBoxIcon.Warning);
            if (r == DialogResult.Cancel) return false;
            if (r == DialogResult.Yes)
            {
                try
                {
                    int n = Samp.DisableMods(gta, mods);
                    status.Text = "Перенесено в _off: " + n;
                }
                catch (Exception ex)
                {
                    Warn("Не получилось перенести моды: " + ex.Message +
                         "\r\nЕсли игра лежит в Program Files — запусти лаунчер от имени администратора.");
                }
            }
            return true;
        }

        void Play()
        {
            ServerEntry s = Selected;
            if (s == null) { Warn("Выбери сервер в списке."); return; }

            string name = nick.Text.Trim();
            if (name.Length < 3 || name.Length > 24 || !Regex.IsMatch(name, @"^[A-Za-z0-9_\[\]\.\$@=\(\)]+$"))
            {
                Warn("Ник: от 3 до 24 символов, только латиница, цифры и _.");
                return;
            }
            if (cfg.RequireRpName && !Regex.IsMatch(name, @"^[A-Z][a-z]+_[A-Z][a-z]+$"))
            {
                Warn("На сервере нужен РП-ник вида Имя_Фамилия, например Ivan_Petrov.");
                return;
            }

            string gta = gtaPath.Text.Trim();
            string gtaExe = Path.Combine(gta, "gta_sa.exe");
            string sampExe = Path.Combine(gta, "samp.exe");
            if (!File.Exists(gtaExe))
            {
                Warn("Не нашёл gta_sa.exe в этой папке. Нажми «Обзор…» и выбери gta_sa.exe.");
                return;
            }
            if (!File.Exists(sampExe))
            {
                Warn("В папке игры нет samp.exe. Установи клиент SA-MP 0.3.7 в эту папку.");
                return;
            }

            if (!CheckMods(false)) return;

            try
            {
                Samp.SetValue("PlayerName", name);
                Samp.SetValue("gta_sa_exe", gtaExe);
                Samp.SetValue("BastionLastServer", list.SelectedIndices[0].ToString());
            }
            catch (Exception ex)
            {
                Warn("Не удалось сохранить ник: " + ex.Message);
                return;
            }

            if (!s.Online)
            {
                DialogResult r = MessageBox.Show(this,
                    "Сервер сейчас не отвечает. Всё равно запустить игру?",
                    cfg.Title, MessageBoxButtons.YesNo, MessageBoxIcon.Question);
                if (r != DialogResult.Yes) return;
            }

            try
            {
                ProcessStartInfo psi = new ProcessStartInfo(sampExe, s.Host + ":" + s.Port);
                psi.WorkingDirectory = gta;
                psi.UseShellExecute = true;
                Process.Start(psi);
                status.Text = "Запускаю игру: " + s.Name + " (" + s.Host + ":" + s.Port + ")…";
            }
            catch (Exception ex)
            {
                Warn("Не удалось запустить игру: " + ex.Message);
            }
        }

        void Warn(string msg)
        {
            MessageBox.Show(this, msg, cfg.Title, MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }
    }

    static class Program
    {
        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            try
            {
                Application.Run(new MainForm());
            }
            catch (Exception ex)
            {
                MessageBox.Show("Ошибка лаунчера:\r\n" + ex, "Bastion RP", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
    }
}

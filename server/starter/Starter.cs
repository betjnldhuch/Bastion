// =====================================================================
//  Bastion Server — стартер ПК-сервера open.mp в стиле лаунчера Bastion.
//
//  Положи BastionServer.exe в папку сервера (рядом с omp-server.exe)
//  или укажи папку во вкладке «Настройки». Что умеет:
//    - старт / стоп / рестарт сервера, автоперезапуск при падении;
//    - консоль: живой log.txt сервера + ввод консольных команд;
//    - онлайн и список игроков (запрос к серверу по протоколу SA-MP);
//    - обновление мода: свежий rp.amx из последнего релиза GitHub,
//      компиляция rp.pwn компилятором Qawno, установка rp.amx из файла,
//      откат на прошлый rp.amx (копии в gamemodes\backup);
//    - правка главных полей config.json и бэкап базы scriptfiles\rp.db.
//
//  Свои настройки стартер хранит в BastionServer.ini рядом с exe.
//  Сборка: build.bat (компилятор .NET Framework 4, есть в каждой Windows).
// =====================================================================
using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Text;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Net;
using System.Net.Sockets;
using System.Reflection;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Web.Script.Serialization;
using System.Windows.Forms;

[assembly: AssemblyTitle("Bastion Server")]
[assembly: AssemblyProduct("Bastion Online")]
[assembly: AssemblyVersion("1.0.0.0")]

namespace BastionServer
{
    // -----------------------------------------------------------------
    //  Цвета и шрифты — те же, что у лаунчера
    // -----------------------------------------------------------------
    static class T
    {
        public static readonly Color Bg = Color.FromArgb(13, 14, 17);
        public static readonly Color Bar = Color.FromArgb(9, 10, 12);
        public static readonly Color Card = Color.FromArgb(20, 21, 25);
        public static readonly Color CardHover = Color.FromArgb(25, 27, 32);
        public static readonly Color Line = Color.FromArgb(34, 36, 43);
        public static readonly Color Orange = Color.FromArgb(255, 122, 26);
        public static readonly Color OrangeHot = Color.FromArgb(255, 150, 66);
        public static readonly Color Text = Color.FromArgb(238, 238, 242);
        public static readonly Color Dim = Color.FromArgb(128, 132, 145);
        public static readonly Color Green = Color.FromArgb(70, 200, 110);
        public static readonly Color Red = Color.FromArgb(235, 80, 70);
        public static readonly Color Yellow = Color.FromArgb(235, 200, 80);

        public static Font F(float size, FontStyle st) { return new Font("Segoe UI", size, st, GraphicsUnit.Point); }
        public static Font Mono(float size) { return new Font("Consolas", size, FontStyle.Regular, GraphicsUnit.Point); }

        public static void Hq(Graphics g)
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.TextRenderingHint = TextRenderingHint.AntiAliasGridFit;
        }
    }

    // -----------------------------------------------------------------
    //  Плоская кнопка: обычная, оранжевая (Primary) или красная (Danger)
    // -----------------------------------------------------------------
    class Btn : Control
    {
        public bool Primary, Danger;
        bool hover, down;

        public Btn(string text, int w)
        {
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer |
                     ControlStyles.UserPaint | ControlStyles.ResizeRedraw, true);
            Text = text;
            Size = new Size(w, 34);
            Cursor = Cursors.Hand;
            Font = T.F(9.5f, FontStyle.Bold);
        }

        protected override void OnMouseEnter(EventArgs e) { hover = true; Invalidate(); base.OnMouseEnter(e); }
        protected override void OnMouseLeave(EventArgs e) { hover = false; down = false; Invalidate(); base.OnMouseLeave(e); }
        protected override void OnMouseDown(MouseEventArgs e) { down = true; Invalidate(); base.OnMouseDown(e); }
        protected override void OnMouseUp(MouseEventArgs e) { down = false; Invalidate(); base.OnMouseUp(e); }
        protected override void OnEnabledChanged(EventArgs e) { Invalidate(); base.OnEnabledChanged(e); }

        protected override void OnPaint(PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            T.Hq(g);
            Color bg, fg, border = Color.Empty;
            if (!Enabled) { bg = T.Card; fg = Color.FromArgb(70, 73, 82); border = T.Line; }
            else if (Primary)
            {
                bg = down ? Color.FromArgb(230, 105, 15) : (hover ? T.OrangeHot : T.Orange);
                fg = Color.FromArgb(14, 14, 16);
            }
            else if (Danger)
            {
                bg = down ? Color.FromArgb(180, 45, 35) : (hover ? Color.FromArgb(220, 65, 55) : Color.FromArgb(200, 50, 40));
                fg = Color.White;
            }
            else
            {
                bg = hover ? T.CardHover : T.Card;
                fg = T.Text;
                border = hover ? T.Orange : T.Line;
            }
            g.Clear(bg);
            if (border != Color.Empty)
                using (Pen p = new Pen(border)) g.DrawRectangle(p, 0, 0, Width - 1, Height - 1);
            using (SolidBrush b = new SolidBrush(fg))
            using (StringFormat sf = new StringFormat())
            {
                sf.Alignment = StringAlignment.Center;
                sf.LineAlignment = StringAlignment.Center;
                g.DrawString(Text, Font, b, new RectangleF(0, 0, Width, Height), sf);
            }
        }
    }

    // -----------------------------------------------------------------
    //  Вкладки
    // -----------------------------------------------------------------
    class Tabs : Control
    {
        public string[] Items;
        public int Selected;
        public event EventHandler Changed;
        int hover = -1;

        public Tabs(params string[] items)
        {
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer |
                     ControlStyles.UserPaint | ControlStyles.ResizeRedraw, true);
            Items = items;
            Height = 38;
            Cursor = Cursors.Hand;
            Font = T.F(10f, FontStyle.Bold);
        }

        int ItemW { get { return 180; } }

        protected override void OnMouseMove(MouseEventArgs e)
        {
            int h = e.X / ItemW;
            if (h >= Items.Length) h = -1;
            if (h != hover) { hover = h; Invalidate(); }
            base.OnMouseMove(e);
        }
        protected override void OnMouseLeave(EventArgs e) { hover = -1; Invalidate(); base.OnMouseLeave(e); }
        protected override void OnMouseClick(MouseEventArgs e)
        {
            int i = e.X / ItemW;
            if (i >= 0 && i < Items.Length && i != Selected)
            {
                Selected = i;
                Invalidate();
                if (Changed != null) Changed(this, EventArgs.Empty);
            }
            base.OnMouseClick(e);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            T.Hq(g);
            g.Clear(T.Bar);
            using (Pen p = new Pen(T.Line)) g.DrawLine(p, 0, Height - 1, Width, Height - 1);
            for (int i = 0; i < Items.Length; i++)
            {
                Rectangle r = new Rectangle(i * ItemW, 0, ItemW, Height);
                Color fg = i == Selected ? T.Text : (i == hover ? T.Text : T.Dim);
                using (SolidBrush b = new SolidBrush(fg))
                using (StringFormat sf = new StringFormat())
                {
                    sf.Alignment = StringAlignment.Center;
                    sf.LineAlignment = StringAlignment.Center;
                    g.DrawString(Items[i].ToUpperInvariant(), Font, b, r, sf);
                }
                if (i == Selected)
                    using (SolidBrush b = new SolidBrush(T.Orange)) g.FillRectangle(b, r.X + 20, Height - 3, r.Width - 40, 3);
            }
        }
    }

    // -----------------------------------------------------------------
    //  Индикатор состояния сервера (точка + текст)
    // -----------------------------------------------------------------
    class StatusDot : Control
    {
        public Color Dot = T.Dim;
        public StatusDot()
        {
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer |
                     ControlStyles.UserPaint | ControlStyles.ResizeRedraw, true);
            Font = T.F(11f, FontStyle.Bold);
        }
        public void Set(string text, Color c) { Text = text; Dot = c; Invalidate(); }
        protected override void OnPaint(PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            T.Hq(g);
            g.Clear(Parent != null ? Parent.BackColor : T.Bg);
            int cy = Height / 2;
            using (SolidBrush b = new SolidBrush(Color.FromArgb(60, Dot))) g.FillEllipse(b, 0, cy - 8, 16, 16);
            using (SolidBrush b = new SolidBrush(Dot)) g.FillEllipse(b, 4, cy - 4, 8, 8);
            using (SolidBrush b = new SolidBrush(T.Text))
            using (StringFormat sf = new StringFormat())
            {
                sf.LineAlignment = StringAlignment.Center;
                g.DrawString(Text, Font, b, new RectangleF(24, 0, Width - 24, Height), sf);
            }
        }
    }

    // -----------------------------------------------------------------
    //  BastionServer.ini: ключ = значение
    // -----------------------------------------------------------------
    static class Cfg
    {
        static readonly Dictionary<string, string> v = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        public static string Path { get { return System.IO.Path.Combine(Program.ExeDir, "BastionServer.ini"); } }

        public static void Load()
        {
            v.Clear();
            try
            {
                if (!File.Exists(Path)) return;
                foreach (string raw in File.ReadAllLines(Path, Encoding.UTF8))
                {
                    string l = raw.Trim();
                    if (l.Length == 0 || l[0] == ';' || l[0] == '#') continue;
                    int eq = l.IndexOf('=');
                    if (eq > 0) v[l.Substring(0, eq).Trim()] = l.Substring(eq + 1).Trim();
                }
            }
            catch (Exception) { }
        }

        public static string Get(string key, string def)
        {
            string s;
            return v.TryGetValue(key, out s) && s.Length > 0 ? s : def;
        }
        public static bool On(string key, bool def) { string s = Get(key, def ? "1" : "0"); return s == "1" || s.ToLowerInvariant() == "true"; }

        public static void Set(string key, string value)
        {
            v[key] = value;
            try
            {
                StringBuilder sb = new StringBuilder();
                sb.Append("; Настройки стартера Bastion Server\r\n");
                foreach (KeyValuePair<string, string> kv in v) sb.Append(kv.Key).Append(" = ").Append(kv.Value).Append("\r\n");
                File.WriteAllText(Path, sb.ToString(), new UTF8Encoding(false));
            }
            catch (Exception) { }
        }
    }

    // -----------------------------------------------------------------
    //  Запрос онлайна по протоколу SA-MP (open.mp его поддерживает)
    // -----------------------------------------------------------------
    class QueryResult
    {
        public bool Ok;
        public int Players, MaxPlayers;
        public List<string> Names = new List<string>();
    }

    static class Query
    {
        static byte[] Packet(int port, char op)
        {
            byte[] p = new byte[11];
            p[0] = (byte)'S'; p[1] = (byte)'A'; p[2] = (byte)'M'; p[3] = (byte)'P';
            p[4] = 127; p[5] = 0; p[6] = 0; p[7] = 1;
            p[8] = (byte)(port & 0xFF); p[9] = (byte)(port >> 8);
            p[10] = (byte)op;
            return p;
        }

        static byte[] Ask(UdpClient u, IPEndPoint ep, int port, char op)
        {
            byte[] q = Packet(port, op);
            u.Send(q, q.Length, ep);
            IPEndPoint from = new IPEndPoint(IPAddress.Any, 0);
            byte[] r = u.Receive(ref from);
            if (r.Length < 11 || r[10] != (byte)op) return null;
            return r;
        }

        public static QueryResult Run(int port)
        {
            QueryResult res = new QueryResult();
            try
            {
                using (UdpClient u = new UdpClient())
                {
                    u.Client.ReceiveTimeout = 1200;
                    IPEndPoint ep = new IPEndPoint(IPAddress.Loopback, port);
                    byte[] r = Ask(u, ep, port, 'i');
                    if (r == null || r.Length < 16) return res;
                    res.Players = r[12] | (r[13] << 8);
                    res.MaxPlayers = r[14] | (r[15] << 8);
                    res.Ok = true;
                    if (res.Players > 0 && res.Players < 100)
                    {
                        byte[] c = Ask(u, ep, port, 'c');
                        if (c != null && c.Length >= 13)
                        {
                            Encoding enc = Encoding.GetEncoding(1251);
                            int n = c[11] | (c[12] << 8), pos = 13;
                            for (int i = 0; i < n && pos < c.Length; i++)
                            {
                                int len = c[pos++];
                                if (pos + len + 4 > c.Length) break;
                                string name = enc.GetString(c, pos, len);
                                pos += len;
                                int score = BitConverter.ToInt32(c, pos);
                                pos += 4;
                                res.Names.Add(name + "  (" + score + ")");
                            }
                        }
                    }
                }
            }
            catch (Exception) { }
            return res;
        }
    }

    // -----------------------------------------------------------------
    //  config.json: чтение, правка полей, запись с отступами
    // -----------------------------------------------------------------
    static class Json
    {
        public static Dictionary<string, object> Load(string path)
        {
            JavaScriptSerializer js = new JavaScriptSerializer();
            js.MaxJsonLength = int.MaxValue;
            object o = js.DeserializeObject(File.ReadAllText(path, Encoding.UTF8));
            Dictionary<string, object> d = o as Dictionary<string, object>;
            if (d == null) throw new Exception("config.json: ожидался объект { ... }");
            return d;
        }

        public static Dictionary<string, object> Sub(Dictionary<string, object> d, string key)
        {
            object o;
            Dictionary<string, object> s = d.TryGetValue(key, out o) ? o as Dictionary<string, object> : null;
            if (s == null) { s = new Dictionary<string, object>(); d[key] = s; }
            return s;
        }

        public static string Str(Dictionary<string, object> d, string key)
        {
            object o;
            if (!d.TryGetValue(key, out o) || o == null) return "";
            if (o is bool) return (bool)o ? "true" : "false";
            return Convert.ToString(o, CultureInfo.InvariantCulture);
        }

        public static string Write(object o)
        {
            StringBuilder sb = new StringBuilder();
            Emit(sb, o, 0);
            sb.Append('\n');
            return sb.ToString();
        }

        static void Emit(StringBuilder sb, object o, int ind)
        {
            string pad = new string(' ', (ind + 1) * 4), end = new string(' ', ind * 4);
            IDictionary<string, object> d = o as IDictionary<string, object>;
            if (d != null)
            {
                if (d.Count == 0) { sb.Append("{}"); return; }
                sb.Append("{\n");
                int i = 0;
                foreach (KeyValuePair<string, object> kv in d)
                {
                    sb.Append(pad); Str(sb, kv.Key); sb.Append(": ");
                    Emit(sb, kv.Value, ind + 1);
                    if (++i < d.Count) sb.Append(',');
                    sb.Append('\n');
                }
                sb.Append(end).Append('}');
                return;
            }
            if (o is string) { Str(sb, (string)o); return; }
            if (o == null) { sb.Append("null"); return; }
            if (o is bool) { sb.Append((bool)o ? "true" : "false"); return; }
            IEnumerable list = o as IEnumerable;
            if (list != null)
            {
                List<object> items = new List<object>();
                foreach (object x in list) items.Add(x);
                if (items.Count == 0) { sb.Append("[]"); return; }
                sb.Append("[\n");
                for (int i = 0; i < items.Count; i++)
                {
                    sb.Append(pad);
                    Emit(sb, items[i], ind + 1);
                    if (i + 1 < items.Count) sb.Append(',');
                    sb.Append('\n');
                }
                sb.Append(end).Append(']');
                return;
            }
            sb.Append(Convert.ToString(o, CultureInfo.InvariantCulture));
        }

        static void Str(StringBuilder sb, string s)
        {
            sb.Append('"');
            foreach (char c in s)
            {
                switch (c)
                {
                    case '"': sb.Append("\\\""); break;
                    case '\\': sb.Append("\\\\"); break;
                    case '\n': sb.Append("\\n"); break;
                    case '\r': sb.Append("\\r"); break;
                    case '\t': sb.Append("\\t"); break;
                    default:
                        if (c < 0x20) sb.Append("\\u").Append(((int)c).ToString("x4"));
                        else sb.Append(c);
                        break;
                }
            }
            sb.Append('"');
        }
    }

    // -----------------------------------------------------------------
    //  Главное окно
    // -----------------------------------------------------------------
    class MainForm : Form
    {
        // состояние сервера
        Process proc;
        StreamWriter stdin;
        bool stopping, restartAfterStop, closingApp, toTray;
        DateTime startedAt;
        int crashes;

        // хвост log.txt
        long logPos = -1;
        string logPartial = "";
        readonly Encoding enc1251 = Encoding.GetEncoding(1251);

        // элементы
        StatusDot status;
        Label lblOnline, lblUptime, lblDir;
        Btn btnStart, btnStop, btnRestart;
        Tabs tabs;
        Panel pConsole, pUpdate, pSettings;
        LogsPage pLogs;
        RichTextBox console;
        TextBox cmdBox;
        ListBox players;
        Label lblPlayers;
        Label lblModLocal, lblModRemote;
        CheckBox chkRestartAfterUpdate;
        Btn btnDownload, btnCompile, btnFromFile, btnRollback, btnBackupDb;
        TextBox fName, fMax, fPort, fRcon, fPass, fSite, fLang;
        CheckBox fRconOn, chkAutoRestart, chkAutoStart;
        NotifyIcon tray;
        System.Windows.Forms.Timer tick;
        volatile bool querying, busy;

        string ServerDir
        {
            get
            {
                string d = Cfg.Get("server_dir", "");
                if (d.Length > 0 && Directory.Exists(d)) return d;
                if (File.Exists(Path.Combine(Program.ExeDir, ExeName))) return Program.ExeDir;
                foreach (string c in new string[] { @"C:\openmp\Server", @"C:\openmp" })
                    if (File.Exists(Path.Combine(c, ExeName))) return c;
                return Program.ExeDir;
            }
        }
        string ExeName { get { return Cfg.Get("server_exe", "omp-server.exe"); } }
        string ExePath { get { return Path.Combine(ServerDir, ExeName); } }
        string GmDir { get { return Path.Combine(ServerDir, "gamemodes"); } }
        string ConfigPath { get { return Path.Combine(ServerDir, "config.json"); } }
        bool Running { get { try { return proc != null && !proc.HasExited; } catch (Exception) { return false; } } }

        public MainForm()
        {
            Text = "Bastion Server";
            BackColor = T.Bg;
            ForeColor = T.Text;
            Font = T.F(9.5f, FontStyle.Regular);
            ClientSize = new Size(1060, 660);
            MinimumSize = new Size(1000, 560);
            StartPosition = FormStartPosition.CenterScreen;
            try { Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch (Exception) { }

            BuildHeader();
            tabs = new Tabs("Консоль", "Логи", "Обновление мода", "Настройки");
            tabs.Dock = DockStyle.Top;
            tabs.Changed += delegate { ShowTab(); };

            pConsole = MakePage(); BuildConsole();
            pUpdate = MakePage(); BuildUpdate();
            pSettings = MakePage(); BuildSettings();
            pLogs = new LogsPage(delegate { return ServerDir; });
            Controls.Add(pConsole); Controls.Add(pLogs); Controls.Add(pUpdate); Controls.Add(pSettings);
            Controls.Add(tabs);
            Controls.Add(headerPanel);
            // BastionServer.exe --tab=2 откроет сразу «Настройки»
            foreach (string a in Environment.GetCommandLineArgs())
                if (a.StartsWith("--tab=")) { int t; if (int.TryParse(a.Substring(6), out t) && t >= 0 && t < 4) tabs.Selected = t; }
            ShowTab();

            tray = new NotifyIcon();
            tray.Icon = Icon ?? SystemIcons.Application;
            tray.Text = "Bastion Server";
            tray.DoubleClick += delegate { ShowFromTray(); };
            tray.MouseClick += delegate(object o, MouseEventArgs me) { if (me.Button == MouseButtons.Left) ShowFromTray(); };
            ContextMenu tm = new ContextMenu();
            tm.MenuItems.Add("Открыть стартер", delegate { ShowFromTray(); });
            tray.ContextMenu = tm;

            tick = new System.Windows.Forms.Timer();
            tick.Interval = 300;
            tick.Tick += delegate { OnTick(); };
            tick.Start();

            Shown += delegate
            {
                // окно всегда открывается нормально (даже если ярлык/автозапуск просит «свёрнутым»)
                if (WindowState == FormWindowState.Minimized) WindowState = FormWindowState.Normal;
                Activate();
                AttachExisting();
                ShowLogTail();
                LoadConfigFields();
                RefreshLocalMod();
                CheckRemote(false);
                if (!File.Exists(ExePath))
                    Say("Не найден " + ExeName + " в папке " + ServerDir + ". Укажи папку сервера во вкладке «Настройки».", T.Red);
                else if (!Running && Cfg.On("autostart", false)) StartServer();
                UpdateState();
            };
        }

        // ---------------- шапка ----------------
        Panel headerPanel;

        void BuildHeader()
        {
            headerPanel = new Panel();
            headerPanel.Dock = DockStyle.Top;
            headerPanel.Height = 76;
            headerPanel.BackColor = T.Bar;

            PictureBox logo = new PictureBox();
            logo.SizeMode = PictureBoxSizeMode.Zoom;
            logo.Bounds = new Rectangle(16, 14, 48, 48);
            try
            {
                Stream s = Assembly.GetExecutingAssembly().GetManifestResourceStream("logo-shield.png");
                if (s != null) logo.Image = Image.FromStream(s);
            }
            catch (Exception) { }
            headerPanel.Controls.Add(logo);

            Label title = new Label();
            title.Text = "BASTION SERVER";
            title.Font = T.F(15f, FontStyle.Bold);
            title.ForeColor = T.Text;
            title.AutoSize = true;
            title.Location = new Point(72, 12);
            headerPanel.Controls.Add(title);

            lblDir = new Label();
            lblDir.Font = T.F(8.5f, FontStyle.Regular);
            lblDir.ForeColor = T.Dim;
            lblDir.AutoSize = true;
            lblDir.Location = new Point(75, 44);
            headerPanel.Controls.Add(lblDir);

            status = new StatusDot();
            status.Bounds = new Rectangle(330, 10, 190, 30);
            headerPanel.Controls.Add(status);

            lblOnline = new Label();
            lblOnline.Font = T.F(9.5f, FontStyle.Regular);
            lblOnline.ForeColor = T.Dim;
            lblOnline.AutoSize = true;
            lblOnline.Location = new Point(332, 42);
            headerPanel.Controls.Add(lblOnline);

            lblUptime = new Label();
            lblUptime.Font = T.F(9.5f, FontStyle.Regular);
            lblUptime.ForeColor = T.Dim;
            lblUptime.AutoSize = true;
            lblUptime.Location = new Point(460, 42);
            headerPanel.Controls.Add(lblUptime);

            btnStart = new Btn("СТАРТ", 110); btnStart.Primary = true;
            btnRestart = new Btn("РЕСТАРТ", 110);
            btnStop = new Btn("СТОП", 110); btnStop.Danger = true;
            btnStart.Click += delegate { StartServer(); };
            btnStop.Click += delegate { StopServer(false); };
            btnRestart.Click += delegate { RestartServer(); };
            foreach (Btn b in new Btn[] { btnStart, btnRestart, btnStop })
            {
                b.Anchor = AnchorStyles.Top | AnchorStyles.Right;
                headerPanel.Controls.Add(b);
            }
            headerPanel.Resize += delegate { LayoutHeader(); };
            LayoutHeader();
        }

        void LayoutHeader()
        {
            int x = headerPanel.Width - 16;
            foreach (Btn b in new Btn[] { btnStop, btnRestart, btnStart })
            {
                x -= b.Width;
                b.Location = new Point(x, 21);
                x -= 8;
            }
        }

        Panel MakePage()
        {
            Panel p = new Panel();
            p.Dock = DockStyle.Fill;
            p.BackColor = T.Bg;
            p.Padding = new Padding(16);
            return p;
        }

        void ShowTab()
        {
            pConsole.Visible = tabs.Selected == 0;
            pLogs.Visible = tabs.Selected == 1;
            pUpdate.Visible = tabs.Selected == 2;
            pSettings.Visible = tabs.Selected == 3;
            if (tabs.Selected == 3) LoadConfigFields();
            if (tabs.Selected == 2) RefreshLocalMod();
        }

        void GoConsole() { tabs.Selected = 0; tabs.Invalidate(); ShowTab(); }

        // ---------------- консоль ----------------
        void BuildConsole()
        {
            Panel right = new Panel();
            right.Dock = DockStyle.Right;
            right.Width = 220;
            right.BackColor = T.Bg;
            right.Padding = new Padding(12, 0, 0, 0);

            lblPlayers = new Label();
            lblPlayers.Dock = DockStyle.Top;
            lblPlayers.Height = 26;
            lblPlayers.Text = "ИГРОКИ";
            lblPlayers.Font = T.F(9f, FontStyle.Bold);
            lblPlayers.ForeColor = T.Dim;

            players = new ListBox();
            players.Dock = DockStyle.Fill;
            players.BackColor = T.Card;
            players.ForeColor = T.Text;
            players.BorderStyle = BorderStyle.None;
            players.Font = T.F(9.5f, FontStyle.Regular);
            players.IntegralHeight = false;
            // двойной клик по игроку — его логи
            players.DoubleClick += delegate
            {
                string n = players.SelectedItem as string;
                if (n == null || n.StartsWith("(")) return;
                int sp = n.IndexOf("  (");
                tabs.Selected = 1; tabs.Invalidate(); ShowTab();
                pLogs.ShowPlayer(sp > 0 ? n.Substring(0, sp) : n);
            };

            FlowLayoutPanel tools = new FlowLayoutPanel();
            tools.Dock = DockStyle.Bottom;
            tools.Height = 140;
            tools.FlowDirection = FlowDirection.TopDown;
            tools.Padding = new Padding(0, 8, 0, 0);
            Btn bFolder = new Btn("Папка сервера", 208);
            Btn bLog = new Btn("Открыть log.txt", 208);
            Btn bClear = new Btn("Очистить экран", 208);
            bFolder.Click += delegate { OpenPath(ServerDir); };
            bLog.Click += delegate { OpenPath(Path.Combine(ServerDir, "log.txt")); };
            bClear.Click += delegate { console.Clear(); };
            tools.Controls.Add(bFolder); tools.Controls.Add(bLog); tools.Controls.Add(bClear);

            right.Controls.Add(players);
            right.Controls.Add(lblPlayers);
            right.Controls.Add(tools);

            console = new RichTextBox();
            console.Dock = DockStyle.Fill;
            console.BackColor = Color.FromArgb(8, 9, 11);
            console.ForeColor = Color.FromArgb(205, 208, 215);
            console.BorderStyle = BorderStyle.None;
            console.Font = T.Mono(9.5f);
            console.ReadOnly = true;
            console.WordWrap = true;
            console.DetectUrls = false;

            Panel input = new Panel();
            input.Dock = DockStyle.Bottom;
            input.Height = 42;
            input.Padding = new Padding(0, 8, 0, 0);
            input.BackColor = T.Bg;

            cmdBox = new TextBox();
            cmdBox.Dock = DockStyle.Fill;
            cmdBox.BackColor = T.Card;
            cmdBox.ForeColor = T.Text;
            cmdBox.BorderStyle = BorderStyle.FixedSingle;
            cmdBox.Font = T.Mono(11f);
            cmdBox.KeyDown += delegate(object s, KeyEventArgs e)
            {
                if (e.KeyCode == Keys.Enter) { e.SuppressKeyPress = true; SendCommand(); }
            };
            Btn bSend = new Btn("ОТПРАВИТЬ", 120);
            bSend.Dock = DockStyle.Right;
            bSend.Click += delegate { SendCommand(); };
            Label hint = new Label();
            hint.Text = "Команда:";
            hint.ForeColor = T.Dim;
            hint.Dock = DockStyle.Left;
            hint.Width = 70;
            hint.TextAlign = ContentAlignment.MiddleLeft;
            input.Controls.Add(cmdBox);
            input.Controls.Add(bSend);
            input.Controls.Add(hint);

            Panel left = new Panel();
            left.Dock = DockStyle.Fill;
            left.Controls.Add(console);
            left.Controls.Add(input);

            pConsole.Controls.Add(left);
            pConsole.Controls.Add(right);
        }

        void Append(string line, Color c)
        {
            if (console.IsDisposed) return;
            console.ReadOnly = false;
            if (console.TextLength > 400000)
            {
                console.Select(0, 150000);
                console.SelectedText = "";
            }
            console.SelectionStart = console.TextLength;
            console.SelectionLength = 0;
            console.SelectionColor = c;
            console.AppendText(line + "\n");
            console.ReadOnly = true;
            console.SelectionStart = console.TextLength;
            console.ScrollToCaret();
        }

        Color LineColor(string l)
        {
            string s = l.ToLowerInvariant();
            if (s.Contains("error") || s.Contains("ошибка") || s.Contains("run time") || s.Contains("crash")) return T.Red;
            if (s.Contains("warning") || s.Contains("предупрежд")) return T.Yellow;
            if (s.Contains("connect") || s.Contains("join")) return Color.FromArgb(140, 190, 255);
            return Color.FromArgb(205, 208, 215);
        }

        // сообщение самого стартера
        void Say(string text, Color c)
        {
            if (InvokeRequired) { BeginInvoke((MethodInvoker)delegate { Say(text, c); }); return; }
            Append("[" + DateTime.Now.ToString("HH:mm:ss") + "] » " + text, c);
        }
        void Say(string text) { Say(text, T.Orange); }

        void SendCommand()
        {
            string cmd = cmdBox.Text.Trim();
            if (cmd.Length == 0) return;
            cmdBox.Clear();
            if (!Running || stdin == null) { Say("Сервер не запущен из стартера, команду отправить некуда.", T.Red); return; }
            try
            {
                stdin.Write(cmd + "\n");
                stdin.Flush();
                Append("> " + cmd, T.OrangeHot);
            }
            catch (Exception ex) { Say("Команда не отправлена: " + ex.Message, T.Red); }
        }

        // ---------------- лог сервера ----------------
        string LogPath { get { return Path.Combine(ServerDir, "log.txt"); } }

        void ShowLogTail()
        {
            try
            {
                if (!File.Exists(LogPath)) { logPos = 0; return; }
                using (FileStream fs = new FileStream(LogPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete))
                {
                    long from = Math.Max(0, fs.Length - 12000);
                    fs.Position = from;
                    byte[] buf = new byte[fs.Length - from];
                    int n = fs.Read(buf, 0, buf.Length);
                    logPos = from + n;
                    string text = enc1251.GetString(buf, 0, n);
                    string[] lines = text.Replace("\r", "").Split('\n');
                    int skip = from > 0 ? 1 : 0; // первая строка обрезана
                    for (int i = skip; i < lines.Length; i++)
                        if (lines[i].Length > 0) Append(lines[i], LineColor(lines[i]));
                    Say("── выше: конец прошлого log.txt ──", T.Dim);
                }
            }
            catch (Exception) { logPos = 0; }
        }

        void PumpLog()
        {
            try
            {
                if (!File.Exists(LogPath)) { logPos = 0; return; }
                using (FileStream fs = new FileStream(LogPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete))
                {
                    if (logPos < 0) { logPos = fs.Length; return; } // ещё не показан хвост — начинаем с конца
                    if (fs.Length < logPos) { logPos = 0; logPartial = ""; }
                    if (fs.Length == logPos) return;
                    fs.Position = logPos;
                    int want = (int)Math.Min(fs.Length - logPos, 1 << 20);
                    byte[] buf = new byte[want];
                    int n = fs.Read(buf, 0, want);
                    logPos += n;
                    string text = logPartial + enc1251.GetString(buf, 0, n);
                    int last = text.LastIndexOf('\n');
                    if (last < 0) { logPartial = text; return; }
                    logPartial = text.Substring(last + 1);
                    foreach (string raw in text.Substring(0, last).Split('\n'))
                    {
                        string l = raw.TrimEnd('\r');
                        if (l.Length > 0) Append(l, LineColor(l));
                    }
                }
            }
            catch (Exception) { }
        }

        // ---------------- запуск / остановка ----------------
        void AttachExisting()
        {
            try
            {
                foreach (Process p in Process.GetProcessesByName(Path.GetFileNameWithoutExtension(ExeName)))
                {
                    string f = "";
                    try { f = p.MainModule.FileName; } catch (Exception) { }
                    if (string.Equals(f, ExePath, StringComparison.OrdinalIgnoreCase))
                    {
                        proc = p;
                        proc.EnableRaisingEvents = true;
                        proc.Exited += OnExited;
                        try { startedAt = p.StartTime; } catch (Exception) { startedAt = DateTime.Now; }
                        Say("Сервер уже был запущен не из стартера. Консольные команды ему не уйдут, СТОП закроет процесс.", T.Yellow);
                        return;
                    }
                }
            }
            catch (Exception) { }
        }

        bool CheckConfigBeforeStart()
        {
            try
            {
                if (!File.Exists(ConfigPath)) return true;
                Dictionary<string, object> d = Json.Load(ConfigPath);
                string pw = Json.Str(Json.Sub(d, "rcon"), "password");
                if (pw == "changeme")
                {
                    Say("В config.json пароль RCON всё ещё changeme — open.mp с ним не запустится. Поменяй во вкладке «Настройки».", T.Red);
                    return false;
                }
            }
            catch (Exception ex)
            {
                Say("config.json не читается: " + ex.Message + ". Сервер может не запуститься.", T.Yellow);
            }
            if (!File.Exists(Path.Combine(GmDir, "rp.amx")))
                Say("В gamemodes нет rp.amx. Поставь мод во вкладке «Обновление мода».", T.Yellow);
            return true;
        }

        void StartServer()
        {
            if (Running || busy) return;
            if (!File.Exists(ExePath))
            {
                Say("Не найден " + ExePath + ". Укажи папку сервера во вкладке «Настройки».", T.Red);
                return;
            }
            if (!CheckConfigBeforeStart()) return;
            try
            {
                ProcessStartInfo si = new ProcessStartInfo(ExePath);
                si.WorkingDirectory = ServerDir;
                si.UseShellExecute = false;
                si.CreateNoWindow = true;
                si.RedirectStandardInput = true;
                proc = Process.Start(si);
                stdin = new StreamWriter(proc.StandardInput.BaseStream, enc1251);
                stdin.AutoFlush = true;
                proc.EnableRaisingEvents = true;
                proc.Exited += OnExited;
                stopping = false;
                startedAt = DateTime.Now;
                // лог читаем с начала: open.mp при старте может его перезаписать
                if (File.Exists(LogPath)) { try { logPos = new FileInfo(LogPath).Length; } catch (Exception) { logPos = 0; } }
                else logPos = 0;
                logPartial = "";
                Say("Сервер запущен (PID " + proc.Id + ").", T.Green);
            }
            catch (Exception ex)
            {
                Say("Не удалось запустить сервер: " + ex.Message, T.Red);
                proc = null;
            }
            UpdateState();
        }

        void StopServer(bool thenStart)
        {
            if (!Running) { if (thenStart) StartServer(); return; }
            if (stopping) return;
            stopping = true;
            restartAfterStop = thenStart;
            Say(thenStart ? "Перезапуск сервера…" : "Останавливаю сервер…");
            Process p = proc;
            StreamWriter w = stdin;
            ThreadPool.QueueUserWorkItem(delegate
            {
                try
                {
                    if (w != null) { w.Write("exit\n"); w.Flush(); }
                }
                catch (Exception) { }
                bool done = false;
                try { done = w != null && p.WaitForExit(15000); } catch (Exception) { }
                if (!done)
                {
                    try { if (!p.HasExited) { p.Kill(); p.WaitForExit(5000); } } catch (Exception) { }
                }
            });
            UpdateState();
        }

        void RestartServer()
        {
            if (Running) StopServer(true);
            else StartServer();
        }

        void OnExited(object sender, EventArgs e)
        {
            if (IsDisposed) return;
            BeginInvoke((MethodInvoker)delegate
            {
                if (sender != proc) return;
                int code = 0;
                try { code = proc.ExitCode; } catch (Exception) { }
                bool wanted = stopping;
                proc = null;
                stdin = null;
                stopping = false;
                PumpLog();
                if (wanted)
                {
                    Say("Сервер остановлен.", T.Dim);
                    if (restartAfterStop) { restartAfterStop = false; StartServer(); }
                    else if (closingApp) Close();
                }
                else
                {
                    crashes++;
                    Say("Сервер закрылся сам (код " + code + ").", T.Red);
                    if (chkAutoRestart.Checked)
                    {
                        if ((DateTime.Now - startedAt).TotalSeconds < 20 && crashes >= 3)
                            Say("Падает сразу после запуска уже " + crashes + " раза подряд — автоперезапуск отключён до ручного старта. Смотри ошибки выше.", T.Red);
                        else
                        {
                            Say("Автоперезапуск через 5 секунд…", T.Yellow);
                            System.Windows.Forms.Timer t = new System.Windows.Forms.Timer();
                            t.Interval = 5000;
                            t.Tick += delegate { t.Stop(); t.Dispose(); if (!Running) StartServer(); };
                            t.Start();
                        }
                    }
                }
                UpdateState();
            });
        }

        void UpdateState()
        {
            bool r = Running;
            if (stopping) status.Set("ОСТАНОВКА…", T.Yellow);
            else if (r) status.Set("РАБОТАЕТ", T.Green);
            else status.Set("ОСТАНОВЛЕН", T.Red);
            btnStart.Enabled = !r && !busy;
            btnStop.Enabled = r && !stopping;
            btnRestart.Enabled = !stopping && !busy;
            lblDir.Text = ServerDir;
            if (!r) { lblOnline.Text = ""; lblUptime.Text = ""; players.Items.Clear(); lblPlayers.Text = "ИГРОКИ"; }
            tray.Text = "Bastion Server — " + (r ? "работает" : "остановлен");
        }

        int Port
        {
            get
            {
                try
                {
                    Dictionary<string, object> d = Json.Load(ConfigPath);
                    int p;
                    if (int.TryParse(Json.Str(Json.Sub(d, "network"), "port"), out p) && p > 0) return p;
                }
                catch (Exception) { }
                return 7777;
            }
        }

        int tickN;
        int cachedPort = 7777;

        void OnTick()
        {
            PumpLog();
            tickN++;
            if (tickN % 10 == 1) cachedPort = Port;
            if (Running)
            {
                // если сервер давно работает — счётчик падений подряд сбрасывается
                if (crashes > 0 && (DateTime.Now - startedAt).TotalMinutes > 5) crashes = 0;
                TimeSpan up = DateTime.Now - startedAt;
                lblUptime.Text = "аптайм " + (up.TotalHours >= 1 ? ((int)up.TotalHours) + " ч " : "") + up.Minutes + " мин";
                if (tickN % 10 == 0 && !querying)
                {
                    querying = true;
                    int port = cachedPort;
                    ThreadPool.QueueUserWorkItem(delegate
                    {
                        QueryResult q = Query.Run(port);
                        querying = false;
                        if (IsDisposed) return;
                        BeginInvoke((MethodInvoker)delegate { ShowQuery(q); });
                    });
                }
            }
            if (tickN % 3 == 0) UpdateState();
        }

        void ShowQuery(QueryResult q)
        {
            if (!Running) return;
            if (!q.Ok) { lblOnline.Text = "онлайн: нет ответа"; return; }
            lblOnline.Text = "онлайн " + q.Players + " / " + q.MaxPlayers;
            lblPlayers.Text = "ИГРОКИ (" + q.Players + ")";
            players.BeginUpdate();
            players.Items.Clear();
            foreach (string n in q.Names) players.Items.Add(n);
            if (q.Players >= 100) players.Items.Add("(список виден до 100 игроков)");
            players.EndUpdate();
        }

        // ---------------- обновление мода ----------------
        Label H(string text, Control parent, ref int y)
        {
            Label l = new Label();
            l.Text = text;
            l.Font = T.F(9f, FontStyle.Bold);
            l.ForeColor = T.Dim;
            l.AutoSize = true;
            l.Location = new Point(0, y);
            parent.Controls.Add(l);
            y += 26;
            return l;
        }

        Label Line(string text, Control parent, ref int y)
        {
            Label l = new Label();
            l.Text = text;
            l.ForeColor = T.Text;
            l.AutoSize = true;
            l.MaximumSize = new Size(900, 0);
            l.Location = new Point(0, y);
            parent.Controls.Add(l);
            y += 24;
            return l;
        }

        CheckBox Check(string text, Control parent, ref int y)
        {
            CheckBox c = new CheckBox();
            c.Text = text;
            c.ForeColor = T.Text;
            c.FlatStyle = FlatStyle.Flat;
            c.FlatAppearance.BorderColor = T.Line;
            c.FlatAppearance.CheckedBackColor = T.Orange;
            c.AutoSize = true;
            c.Location = new Point(0, y);
            parent.Controls.Add(c);
            y += 30;
            return c;
        }

        Panel ScrollPage(Panel page)
        {
            Panel s = new Panel();
            s.Dock = DockStyle.Fill;
            s.AutoScroll = true;
            s.BackColor = T.Bg;
            page.Controls.Add(s);
            return s;
        }

        void BuildUpdate()
        {
            Panel s = ScrollPage(pUpdate);
            int y = 0;
            H("МОД НА СЕРВЕРЕ", s, ref y);
            lblModLocal = Line("…", s, ref y);
            lblModRemote = Line("Свежая сборка на GitHub: проверяю…", s, ref y);
            y += 10;

            btnDownload = new Btn("СКАЧАТЬ СВЕЖИЙ МОД С GITHUB", 300); btnDownload.Primary = true;
            btnDownload.Location = new Point(0, y);
            btnDownload.Click += delegate { DownloadMod(); };
            Btn bCheck = new Btn("Проверить ещё раз", 180);
            bCheck.Location = new Point(312, y);
            bCheck.Click += delegate { CheckRemote(true); };
            s.Controls.Add(btnDownload); s.Controls.Add(bCheck);
            y += 46;
            Line("Берёт rp.amx и исходники (rp.pwn, bastion\\*.inc) из последнего релиза main. Старый rp.amx сохраняется в gamemodes\\backup.", s, ref y).ForeColor = T.Dim;
            y += 8;

            H("ДРУГИЕ СПОСОБЫ", s, ref y);
            btnCompile = new Btn("Скомпилировать rp.pwn (Qawno)", 300);
            btnCompile.Location = new Point(0, y);
            btnCompile.Click += delegate { CompileMod(); };
            btnFromFile = new Btn("Поставить rp.amx из файла…", 300);
            btnFromFile.Location = new Point(312, y);
            btnFromFile.Click += delegate { InstallFromFile(); };
            s.Controls.Add(btnCompile); s.Controls.Add(btnFromFile);
            y += 44;
            btnRollback = new Btn("Откатить на прошлый rp.amx", 300);
            btnRollback.Location = new Point(0, y);
            btnRollback.Click += delegate { Rollback(); };
            btnBackupDb = new Btn("Бэкап базы rp.db сейчас", 300);
            btnBackupDb.Location = new Point(312, y);
            btnBackupDb.Click += delegate { BackupDb(true); };
            s.Controls.Add(btnRollback); s.Controls.Add(btnBackupDb);
            y += 50;

            chkRestartAfterUpdate = Check("Перезапустить сервер после обновления мода", s, ref y);
            chkRestartAfterUpdate.Checked = Cfg.On("restart_after_update", true);
            chkRestartAfterUpdate.CheckedChanged += delegate { Cfg.Set("restart_after_update", chkRestartAfterUpdate.Checked ? "1" : "0"); };
            Line("Перед каждой заменой мода стартер сам копирует scriptfiles\\rp.db в scriptfiles\\backup.", s, ref y).ForeColor = T.Dim;
        }

        void RefreshLocalMod()
        {
            string amx = Path.Combine(GmDir, "rp.amx");
            string tag = Cfg.Get("mod_version", "");
            if (!File.Exists(amx)) { lblModLocal.Text = "rp.amx нет в " + GmDir; return; }
            FileInfo fi = new FileInfo(amx);
            lblModLocal.Text = "rp.amx от " + fi.LastWriteTime.ToString("dd.MM.yyyy HH:mm") + ", " + (fi.Length / 1024) + " КБ" +
                (tag.Length > 0 ? "   ·   версия " + tag : "");
        }

        string Repo { get { return Cfg.Get("repo", "betjnldhuch/bastion"); } }

        static string Http(string url)
        {
            ServicePointManager.SecurityProtocol = (SecurityProtocolType)3072; // TLS 1.2
            using (WebClient wc = new WebClient())
            {
                wc.Headers["User-Agent"] = "Bastion-Server-Starter";
                wc.Headers["Accept"] = "application/vnd.github+json";
                wc.Encoding = Encoding.UTF8;
                return wc.DownloadString(url);
            }
        }

        // tag, url архива gamemodes, дата
        string[] LatestRelease()
        {
            string json = Http("https://api.github.com/repos/" + Repo + "/releases/latest");
            Dictionary<string, object> d = new JavaScriptSerializer().DeserializeObject(json) as Dictionary<string, object>;
            if (d == null) throw new Exception("GitHub вернул непонятный ответ");
            string tag = Json.Str(d, "tag_name");
            object assets;
            if (d.TryGetValue("assets", out assets) && assets is IEnumerable)
                foreach (object a in (IEnumerable)assets)
                {
                    Dictionary<string, object> ad = a as Dictionary<string, object>;
                    if (ad == null) continue;
                    string name = Json.Str(ad, "name");
                    if (name.StartsWith("Bastion-gamemodes", StringComparison.OrdinalIgnoreCase) && name.EndsWith(".zip"))
                        return new string[] { tag, Json.Str(ad, "browser_download_url"), Json.Str(d, "published_at") };
                }
            throw new Exception("в релизе " + tag + " нет архива Bastion-gamemodes");
        }

        void CheckRemote(bool loud)
        {
            lblModRemote.Text = "Свежая сборка на GitHub: проверяю…";
            ThreadPool.QueueUserWorkItem(delegate
            {
                string text;
                try
                {
                    string[] r = LatestRelease();
                    string mine = Cfg.Get("mod_version", "");
                    DateTime when;
                    string date = DateTime.TryParse(r[2], CultureInfo.InvariantCulture, DateTimeStyles.AdjustToUniversal, out when)
                        ? when.ToLocalTime().ToString("dd.MM.yyyy HH:mm") : "";
                    text = "Свежая сборка на GitHub: " + r[0] + (date.Length > 0 ? " (" + date + ")" : "") +
                        (mine == r[0] ? "   ·   уже стоит" : (mine.Length > 0 ? "   ·   есть обновление" : ""));
                }
                catch (Exception ex) { text = "Свежая сборка на GitHub: не удалось проверить (" + ex.Message + ")"; }
                if (IsDisposed) return;
                BeginInvoke((MethodInvoker)delegate
                {
                    lblModRemote.Text = text;
                    if (loud) Say(text);
                });
            });
        }

        void SetBusy(bool b)
        {
            busy = b;
            foreach (Btn x in new Btn[] { btnDownload, btnCompile, btnFromFile, btnRollback })
                x.Enabled = !b;
            UpdateState();
        }

        // Остановить сервер (если нужно) → выполнить замену → запустить
        void SwapMod(string title, Func<string> swap)
        {
            bool wasRunning = Running;
            bool restart = wasRunning && chkRestartAfterUpdate.Checked;
            SetBusy(true);
            GoConsole();
            Action finish = delegate
            {
                string err = null;
                try
                {
                    BackupDb(false);
                    BackupAmx();
                    string msg = swap();
                    Say(msg, T.Green);
                }
                catch (Exception ex) { err = ex.Message; }
                BeginInvoke((MethodInvoker)delegate
                {
                    SetBusy(false);
                    RefreshLocalMod();
                    if (err != null) Say(title + ": ошибка — " + err, T.Red);
                    if (restart) StartServer();
                    else if (wasRunning && err == null) Say("Мод заменён. Чтобы он заработал, нажми РЕСТАРТ.", T.Yellow);
                });
            };
            if (restart)
            {
                Say(title + ": останавливаю сервер…");
                StopServer(false);
                ThreadPool.QueueUserWorkItem(delegate
                {
                    for (int i = 0; i < 250 && Running; i++) Thread.Sleep(100);
                    Thread.Sleep(500);
                    finish();
                });
            }
            else ThreadPool.QueueUserWorkItem(delegate { finish(); });
        }

        void DownloadMod()
        {
            Say("Ищу свежую сборку мода на GitHub…");
            SetBusy(true);
            ThreadPool.QueueUserWorkItem(delegate
            {
                string tmp = Path.Combine(Path.GetTempPath(), "bastion-gm-" + Guid.NewGuid().ToString("N").Substring(0, 8));
                string[] rel;
                string zip = tmp + ".zip";
                try
                {
                    rel = LatestRelease();
                    Say("Скачиваю " + rel[0] + "…");
                    ServicePointManager.SecurityProtocol = (SecurityProtocolType)3072;
                    using (WebClient wc = new WebClient())
                    {
                        wc.Headers["User-Agent"] = "Bastion-Server-Starter";
                        wc.DownloadFile(rel[1], zip);
                    }
                    ZipFile.ExtractToDirectory(zip, tmp);
                    if (!File.Exists(Path.Combine(tmp, @"gamemodes\rp.amx"))) throw new Exception("в архиве нет gamemodes\\rp.amx");
                }
                catch (Exception ex)
                {
                    Say("Не удалось скачать мод: " + ex.Message, T.Red);
                    try { File.Delete(zip); } catch (Exception) { }
                    try { Directory.Delete(tmp, true); } catch (Exception) { }
                    BeginInvoke((MethodInvoker)delegate { SetBusy(false); });
                    return;
                }
                string tag = rel[0];
                BeginInvoke((MethodInvoker)delegate
                {
                    SwapMod("Обновление мода", delegate
                    {
                        try
                        {
                            CopyTree(Path.Combine(tmp, "gamemodes"), GmDir);
                            Cfg.Set("mod_version", tag);
                            BeginInvoke((MethodInvoker)delegate { CheckRemote(false); });
                            return "Мод обновлён до " + tag + ".";
                        }
                        finally
                        {
                            try { File.Delete(zip); } catch (Exception) { }
                            try { Directory.Delete(tmp, true); } catch (Exception) { }
                        }
                    });
                });
            });
        }

        static void CopyTree(string from, string to)
        {
            Directory.CreateDirectory(to);
            foreach (string f in Directory.GetFiles(from))
                File.Copy(f, Path.Combine(to, Path.GetFileName(f)), true);
            foreach (string d in Directory.GetDirectories(from))
                CopyTree(d, Path.Combine(to, Path.GetFileName(d)));
        }

        void CompileMod()
        {
            string pawncc = "";
            foreach (string c in new string[] { @"qawno\pawncc.exe", @"pawno\pawncc.exe" })
                if (File.Exists(Path.Combine(ServerDir, c))) { pawncc = Path.Combine(ServerDir, c); break; }
            if (pawncc.Length == 0) { GoConsole(); Say("Не найден qawno\\pawncc.exe в папке сервера.", T.Red); return; }
            string src = Path.Combine(GmDir, "rp.pwn");
            if (!File.Exists(src)) { GoConsole(); Say("Нет gamemodes\\rp.pwn. Сначала скачай мод с GitHub.", T.Red); return; }
            string inc = Path.Combine(Path.GetDirectoryName(pawncc), "include");
            string outAmx = Path.Combine(GmDir, "rp.new.amx");
            GoConsole();
            Say("Компилирую rp.pwn…");
            SetBusy(true);
            ThreadPool.QueueUserWorkItem(delegate
            {
                bool ok = false;
                try
                {
                    try { File.Delete(outAmx); } catch (Exception) { }
                    ProcessStartInfo si = new ProcessStartInfo(pawncc,
                        "rp.pwn \"-i" + inc + "\" -i. -orp.new.amx -d3 \"-;+\" \"-(+\"");
                    si.WorkingDirectory = GmDir;
                    si.UseShellExecute = false;
                    si.CreateNoWindow = true;
                    si.RedirectStandardOutput = true;
                    si.RedirectStandardError = true;
                    si.StandardOutputEncoding = enc1251;
                    si.StandardErrorEncoding = enc1251;
                    using (Process p = Process.Start(si))
                    {
                        System.Threading.Tasks.Task<string> err = p.StandardError.ReadToEndAsync();
                        string o = p.StandardOutput.ReadToEnd();
                        p.WaitForExit();
                        int warnings = 0;
                        foreach (string l in (o + "\n" + err.Result).Replace("\r", "").Split('\n'))
                        {
                            if (l.Trim().Length == 0) continue;
                            if (l.Contains(" : warning ")) warnings++;
                            Say(l, LineColor(l));
                        }
                        ok = p.ExitCode == 0 && File.Exists(outAmx);
                        if (ok) Say("Компиляция без ошибок" + (warnings > 0 ? ", предупреждений: " + warnings : "") + ".", T.Green);
                        else Say("Компиляция не удалась, rp.amx не тронут.", T.Red);
                    }
                }
                catch (Exception ex) { Say("Компилятор не запустился: " + ex.Message, T.Red); }
                BeginInvoke((MethodInvoker)delegate
                {
                    SetBusy(false);
                    if (!ok) return;
                    SwapMod("Компиляция", delegate
                    {
                        string amx = Path.Combine(GmDir, "rp.amx");
                        File.Copy(outAmx, amx, true);
                        File.Delete(outAmx);
                        Cfg.Set("mod_version", "свой (скомпилирован " + DateTime.Now.ToString("dd.MM HH:mm") + ")");
                        return "Новый rp.amx поставлен.";
                    });
                });
            });
        }

        void InstallFromFile()
        {
            using (OpenFileDialog d = new OpenFileDialog())
            {
                d.Title = "Выбери rp.amx";
                d.Filter = "Мод (*.amx)|*.amx|Все файлы|*.*";
                if (d.ShowDialog(this) != DialogResult.OK) return;
                string file = d.FileName;
                if (string.Equals(Path.GetFullPath(file), Path.GetFullPath(Path.Combine(GmDir, "rp.amx")), StringComparison.OrdinalIgnoreCase))
                { Say("Это и есть текущий rp.amx.", T.Yellow); return; }
                SwapMod("Установка из файла", delegate
                {
                    File.Copy(file, Path.Combine(GmDir, "rp.amx"), true);
                    Cfg.Set("mod_version", "из файла " + Path.GetFileName(file));
                    return "rp.amx поставлен из " + file + ".";
                });
            }
        }

        string AmxBackupDir { get { return Path.Combine(GmDir, "backup"); } }

        void BackupAmx()
        {
            string amx = Path.Combine(GmDir, "rp.amx");
            if (!File.Exists(amx)) return;
            Directory.CreateDirectory(AmxBackupDir);
            string name = "rp-" + DateTime.Now.ToString("yyyyMMdd-HHmmss") + ".amx";
            File.Copy(amx, Path.Combine(AmxBackupDir, name), true);
            // в имени копии запоминаем, какая версия стояла
            File.WriteAllText(Path.Combine(AmxBackupDir, name + ".txt"), Cfg.Get("mod_version", ""), Encoding.UTF8);
            Prune(AmxBackupDir, "rp-*.amx", 10);
            Say("Старый rp.amx сохранён: gamemodes\\backup\\" + name, T.Dim);
        }

        static void Prune(string dir, string mask, int keep)
        {
            string[] files = Directory.GetFiles(dir, mask);
            Array.Sort(files, StringComparer.OrdinalIgnoreCase);
            for (int i = 0; i < files.Length - keep; i++)
            {
                try { File.Delete(files[i]); } catch (Exception) { }
                try { File.Delete(files[i] + ".txt"); } catch (Exception) { }
            }
        }

        void Rollback()
        {
            string[] files = Directory.Exists(AmxBackupDir) ? Directory.GetFiles(AmxBackupDir, "rp-*.amx") : new string[0];
            if (files.Length == 0) { GoConsole(); Say("Копий rp.amx ещё нет — откатывать не на что.", T.Yellow); return; }
            Array.Sort(files, StringComparer.OrdinalIgnoreCase);
            string last = files[files.Length - 1];
            string ver = "";
            try { ver = File.ReadAllText(last + ".txt", Encoding.UTF8).Trim(); } catch (Exception) { }
            if (MessageBox.Show(this, "Вернуть rp.amx из копии " + Path.GetFileName(last) + (ver.Length > 0 ? " (" + ver + ")" : "") + "?",
                    "Откат мода", MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes) return;
            string data = Path.Combine(Path.GetTempPath(), "bastion-rollback.amx");
            File.Copy(last, data, true);
            SwapMod("Откат", delegate
            {
                File.Copy(data, Path.Combine(GmDir, "rp.amx"), true);
                // копия, с которой откатились, больше не нужна (текущий мод уже сохранён свежей копией)
                try { File.Delete(last); File.Delete(last + ".txt"); } catch (Exception) { }
                Cfg.Set("mod_version", ver.Length > 0 ? ver : "откат " + Path.GetFileName(last));
                return "Откат выполнен: " + Path.GetFileName(last) + ".";
            });
        }

        void BackupDb(bool loud)
        {
            try
            {
                string db = Path.Combine(ServerDir, @"scriptfiles\rp.db");
                if (!File.Exists(db)) { if (loud) Say("Базы scriptfiles\\rp.db пока нет.", T.Yellow); return; }
                string dir = Path.Combine(ServerDir, @"scriptfiles\backup");
                Directory.CreateDirectory(dir);
                string name = "rp-" + DateTime.Now.ToString("yyyyMMdd-HHmmss") + ".db";
                // сервер держит файл открытым — копируем с общим доступом
                using (FileStream i = new FileStream(db, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete))
                using (FileStream o = new FileStream(Path.Combine(dir, name), FileMode.Create))
                    i.CopyTo(o);
                Prune(dir, "rp-*.db", 30);
                Say("База сохранена: scriptfiles\\backup\\" + name, loud ? T.Green : T.Dim);
            }
            catch (Exception ex) { Say("Бэкап базы не удался: " + ex.Message, T.Red); }
            if (loud && !InvokeRequired) GoConsole();
        }

        // ---------------- настройки ----------------
        TextBox Field(string label, Control parent, ref int y, int w)
        {
            Label l = new Label();
            l.Text = label;
            l.ForeColor = T.Dim;
            l.AutoSize = false;
            l.Size = new Size(200, 26);
            l.TextAlign = ContentAlignment.MiddleLeft;
            l.Location = new Point(0, y);
            TextBox t = new TextBox();
            t.BackColor = T.Card;
            t.ForeColor = T.Text;
            t.BorderStyle = BorderStyle.FixedSingle;
            t.Font = T.F(10f, FontStyle.Regular);
            t.Location = new Point(210, y);
            t.Width = w;
            parent.Controls.Add(l);
            parent.Controls.Add(t);
            y += 34;
            return t;
        }

        void BuildSettings()
        {
            Panel s = ScrollPage(pSettings);
            int y = 0;
            H("CONFIG.JSON", s, ref y);
            fName = Field("Название сервера", s, ref y, 420);
            fMax = Field("Макс. игроков (до 1000)", s, ref y, 100);
            fPort = Field("Порт", s, ref y, 100);
            fPass = Field("Пароль на вход (пусто = нет)", s, ref y, 220);
            fSite = Field("Сайт", s, ref y, 420);
            fLang = Field("Язык (в списке серверов)", s, ref y, 220);
            fRcon = Field("Пароль RCON", s, ref y, 220);
            fRcon.UseSystemPasswordChar = true;
            Btn eye = new Btn("показать", 90);
            eye.Height = 26;
            eye.Location = new Point(440, fRcon.Top);
            eye.Click += delegate { fRcon.UseSystemPasswordChar = !fRcon.UseSystemPasswordChar; };
            s.Controls.Add(eye);
            int cy = y;
            fRconOn = Check("RCON включён (вход /rcon login в игре)", s, ref cy);
            fRconOn.Left = 210;
            y = cy + 6;

            Btn save = new Btn("СОХРАНИТЬ CONFIG.JSON", 240); save.Primary = true;
            save.Location = new Point(210, y);
            save.Click += delegate { SaveConfigFields(); };
            Btn note = new Btn("Открыть в Блокноте", 200);
            note.Location = new Point(460, y);
            note.Click += delegate
            {
                try { Process.Start("notepad.exe", "\"" + ConfigPath + "\""); }
                catch (Exception ex) { Say("Не открылся Блокнот: " + ex.Message, T.Red); }
            };
            s.Controls.Add(save); s.Controls.Add(note);
            y += 54;

            H("СТАРТЕР", s, ref y);
            Label dl = new Label();
            dl.Text = "Папка сервера";
            dl.ForeColor = T.Dim;
            dl.Size = new Size(200, 26);
            dl.TextAlign = ContentAlignment.MiddleLeft;
            dl.Location = new Point(0, y);
            s.Controls.Add(dl);
            Btn pick = new Btn("Выбрать…", 120);
            pick.Location = new Point(210, y - 4);
            pick.Click += delegate { PickServerDir(); };
            s.Controls.Add(pick);
            Btn open = new Btn("Открыть", 120);
            open.Location = new Point(338, y - 4);
            open.Click += delegate { OpenPath(ServerDir); };
            s.Controls.Add(open);
            y += 42;

            chkAutoRestart = Check("Перезапускать сервер, если он упал", s, ref y);
            chkAutoRestart.Checked = Cfg.On("auto_restart", true);
            chkAutoRestart.CheckedChanged += delegate { Cfg.Set("auto_restart", chkAutoRestart.Checked ? "1" : "0"); };
            chkAutoStart = Check("Запускать сервер сразу при открытии стартера", s, ref y);
            chkAutoStart.Checked = Cfg.On("autostart", false);
            chkAutoStart.CheckedChanged += delegate { Cfg.Set("autostart", chkAutoStart.Checked ? "1" : "0"); };
            CheckBox chkTray = Check("Сворачивать стартер в трей (а не на панель задач)", s, ref y);
            chkTray.Checked = Cfg.On("minimize_to_tray", false);
            chkTray.CheckedChanged += delegate { Cfg.Set("minimize_to_tray", chkTray.Checked ? "1" : "0"); };
            CheckBox chkWin = Check("Открывать стартер при входе в Windows", s, ref y);
            chkWin.Checked = AutorunOn();
            chkWin.CheckedChanged += delegate { SetAutorun(chkWin.Checked); };
        }

        const string RunKey = @"Software\Microsoft\Windows\CurrentVersion\Run";

        bool AutorunOn()
        {
            try
            {
                using (Microsoft.Win32.RegistryKey k = Microsoft.Win32.Registry.CurrentUser.OpenSubKey(RunKey))
                    return k != null && k.GetValue("BastionServer") != null;
            }
            catch (Exception) { return false; }
        }

        void SetAutorun(bool on)
        {
            try
            {
                using (Microsoft.Win32.RegistryKey k = Microsoft.Win32.Registry.CurrentUser.CreateSubKey(RunKey))
                {
                    if (on) k.SetValue("BastionServer", "\"" + Application.ExecutablePath + "\"");
                    else k.DeleteValue("BastionServer", false);
                }
            }
            catch (Exception ex) { Say("Автозапуск не настроен: " + ex.Message, T.Red); }
        }

        void PickServerDir()
        {
            using (FolderBrowserDialog d = new FolderBrowserDialog())
            {
                d.Description = "Папка сервера open.mp (где лежит " + ExeName + ")";
                d.SelectedPath = ServerDir;
                if (d.ShowDialog(this) != DialogResult.OK) return;
                if (Running) { Say("Сначала останови сервер, потом меняй папку.", T.Yellow); return; }
                if (!File.Exists(Path.Combine(d.SelectedPath, ExeName)))
                    Say("В выбранной папке нет " + ExeName + ".", T.Yellow);
                Cfg.Set("server_dir", d.SelectedPath);
                Say("Папка сервера: " + d.SelectedPath);
                logPos = -1;
                logPartial = "";
                ShowLogTail();
                LoadConfigFields();
                RefreshLocalMod();
                AttachExisting();
                UpdateState();
            }
        }

        void LoadConfigFields()
        {
            foreach (TextBox t in new TextBox[] { fName, fMax, fPort, fRcon, fPass, fSite, fLang }) t.Text = "";
            if (!File.Exists(ConfigPath)) return;
            try
            {
                Dictionary<string, object> d = Json.Load(ConfigPath);
                fName.Text = Json.Str(d, "name");
                fMax.Text = Json.Str(d, "max_players");
                fPass.Text = Json.Str(d, "password");
                fSite.Text = Json.Str(d, "website");
                fLang.Text = Json.Str(d, "language");
                fPort.Text = Json.Str(Json.Sub(d, "network"), "port");
                Dictionary<string, object> r = Json.Sub(d, "rcon");
                fRcon.Text = Json.Str(r, "password");
                fRconOn.Checked = Json.Str(r, "enable") == "true";
            }
            catch (Exception ex) { Say("config.json не читается: " + ex.Message, T.Red); }
        }

        void SaveConfigFields()
        {
            int max, port;
            if (!int.TryParse(fMax.Text.Trim(), out max) || max < 1 || max > 1000) { Say("Макс. игроков: число от 1 до 1000.", T.Red); GoConsole(); return; }
            if (!int.TryParse(fPort.Text.Trim(), out port) || port < 1 || port > 65535) { Say("Порт: число от 1 до 65535.", T.Red); GoConsole(); return; }
            if (fRcon.Text.Trim().Length == 0 || fRcon.Text.Trim() == "changeme") { Say("Пароль RCON не может быть пустым или changeme.", T.Red); GoConsole(); return; }
            try
            {
                Dictionary<string, object> d = File.Exists(ConfigPath) ? Json.Load(ConfigPath) : new Dictionary<string, object>();
                d["name"] = fName.Text.Trim();
                d["max_players"] = max;
                d["password"] = fPass.Text.Trim();
                d["website"] = fSite.Text.Trim();
                d["language"] = fLang.Text.Trim();
                Json.Sub(d, "network")["port"] = port;
                Dictionary<string, object> r = Json.Sub(d, "rcon");
                r["password"] = fRcon.Text.Trim();
                r["enable"] = fRconOn.Checked;
                if (File.Exists(ConfigPath)) File.Copy(ConfigPath, ConfigPath + ".bak", true);
                File.WriteAllText(ConfigPath, Json.Write(d), new UTF8Encoding(false));
                cachedPort = port;
                Say("config.json сохранён (старый — config.json.bak).", T.Green);
                if (Running && MessageBox.Show(this, "Настройки применятся после перезапуска. Перезапустить сервер сейчас?",
                        "Bastion Server", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes)
                    RestartServer();
            }
            catch (Exception ex) { Say("config.json не сохранён: " + ex.Message, T.Red); }
            GoConsole();
        }

        void OpenPath(string p)
        {
            try { Process.Start(p); }
            catch (Exception ex) { Say("Не открылось: " + ex.Message, T.Red); }
        }

        // ---------------- закрытие ----------------
        protected override void OnResize(EventArgs e)
        {
            base.OnResize(e);
            if (WindowState == FormWindowState.Minimized && Running && (toTray || Cfg.On("minimize_to_tray", false)))
            {
                toTray = false;
                Hide();
                tray.Visible = true;
                tray.ShowBalloonTip(2000, "Bastion Server", "Сервер работает. Стартер свёрнут сюда.", ToolTipIcon.Info);
            }
        }

        public void ShowFromTray()
        {
            tray.Visible = false;
            Show();
            if (WindowState == FormWindowState.Minimized) WindowState = FormWindowState.Normal;
            Activate();
            TopMost = true; TopMost = false; // вытащить поверх остальных окон
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            if (Running && !closingApp)
            {
                DialogResult r = MessageBox.Show(this,
                    "Сервер работает.\n\nДа — остановить сервер и выйти.\nНет — свернуть стартер в трей (сервер продолжит работать).",
                    "Bastion Server", MessageBoxButtons.YesNoCancel, MessageBoxIcon.Question);
                e.Cancel = true;
                if (r == DialogResult.Yes) { closingApp = true; StopServer(false); }
                else if (r == DialogResult.No) { toTray = true; WindowState = FormWindowState.Minimized; }
                return;
            }
            if (Running) { e.Cancel = true; return; } // ждём остановки, OnExited закроет окно
            tray.Visible = false;
            base.OnFormClosing(e);
        }
    }

    static class Program
    {
        public static string ExeDir { get { return Path.GetDirectoryName(Application.ExecutablePath); } }

        [STAThread]
        static void Main()
        {
            bool created;
            using (Mutex m = new Mutex(true, "BastionServerStarter_" + ExeDir.ToLowerInvariant().GetHashCode().ToString("x"), out created))
            {
                string showName = "BastionServerStarterShow_" + ExeDir.ToLowerInvariant().GetHashCode().ToString("x");
                if (!created)
                {
                    // стартер уже открыт: просто показываем его окно, а не шлём в трей
                    try { using (EventWaitHandle h = EventWaitHandle.OpenExisting(showName)) h.Set(); return; }
                    catch (Exception) { }
                    MessageBox.Show("Стартер для этой папки уже открыт.", "Bastion Server");
                    return;
                }
                Application.EnableVisualStyles();
                Application.SetCompatibleTextRenderingDefault(false);
                Cfg.Load();
                MainForm f = new MainForm();
                EventWaitHandle show = new EventWaitHandle(false, EventResetMode.AutoReset, showName);
                Thread t = new Thread(delegate ()
                {
                    while (show.WaitOne())
                        try { f.BeginInvoke((MethodInvoker)delegate { f.ShowFromTray(); }); } catch (Exception) { }
                });
                t.IsBackground = true;
                t.Start();
                Application.Run(f);
            }
        }
    }
}

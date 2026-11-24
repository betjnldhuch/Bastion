// =====================================================================
//  Bastion Online Launcher — окно (дизайн v2)
//  Тёмный минимализм, оранжевый только на акцентах.
//  Фон: случайная сцена по времени суток (LoadScreens.cs).
//  Свой фон: положи background.jpg (или .png) рядом с exe, 1000x600.
// =====================================================================
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Text;
using System.IO;
using System.Net;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Windows.Forms;

namespace BastionLauncher
{
    static class Theme
    {
        public static readonly Color Bg = Color.FromArgb(13, 14, 17);
        public static readonly Color BgBar = Color.FromArgb(9, 10, 12);
        public static readonly Color Card = Color.FromArgb(20, 21, 25);
        public static readonly Color CardHover = Color.FromArgb(25, 27, 32);
        public static readonly Color CardSel = Color.FromArgb(29, 31, 37);
        public static readonly Color Line = Color.FromArgb(34, 36, 43);
        public static readonly Color Orange = Color.FromArgb(255, 122, 26);
        public static readonly Color OrangeHot = Color.FromArgb(255, 150, 66);
        public static readonly Color Text = Color.FromArgb(238, 238, 242);
        public static readonly Color Dim = Color.FromArgb(128, 132, 145);
        public static readonly Color Faint = Color.FromArgb(70, 73, 82);

        public static readonly string Display = Pick("Bahnschrift", "Segoe UI Black", "Segoe UI", "Arial");
        public static readonly string Body = Pick("Segoe UI", "Arial", "DejaVu Sans");

        // Фирменный тяжёлый шрифт: fonts\RussoOne-Regular.ttf рядом с exe (лицензия OFL)
        static System.Drawing.Text.PrivateFontCollection pfc;
        public static readonly FontFamily Heavy = LoadHeavy();
        public static readonly bool HeavyLoaded = pfc != null;

        static FontFamily LoadHeavy()
        {
            // если шрифт установлен в системе — берём его, иначе грузим из папки fonts
            foreach (FontFamily f in FontFamily.Families)
                if (f.Name == "Russo One") { pfc = new System.Drawing.Text.PrivateFontCollection(); return f; }
            try
            {
                string p = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, @"fonts\RussoOne-Regular.ttf");
                if (!File.Exists(p)) p = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "fonts/RussoOne-Regular.ttf");
                if (File.Exists(p))
                {
                    pfc = new System.Drawing.Text.PrivateFontCollection();
                    pfc.AddFontFile(p);
                    if (pfc.Families.Length > 0) return pfc.Families[0];
                }
            }
            catch (Exception) { }
            pfc = null;
            return new FontFamily(Display);
        }

        public static Font Hv(float size)
        {
            return new Font(Heavy, size, HeavyLoaded ? FontStyle.Regular : FontStyle.Bold, GraphicsUnit.Point);
        }

        static string Pick(params string[] names)
        {
            foreach (string n in names)
                foreach (FontFamily f in FontFamily.Families)
                    if (string.Equals(f.Name, n, StringComparison.OrdinalIgnoreCase)) return f.Name;
            return FontFamily.GenericSansSerif.Name;
        }

        public static Font F(float size, FontStyle st) { return new Font(Body, size, st, GraphicsUnit.Point); }
        public static Font D(float size, FontStyle st) { return new Font(Display, size, st, GraphicsUnit.Point); }

        public static void Hq(Graphics g)
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.TextRenderingHint = TextRenderingHint.AntiAliasGridFit;
            g.PixelOffsetMode = PixelOffsetMode.HighQuality;
        }

        // Текст с разрядкой (для подписей капсом)
        public static void Spaced(Graphics g, string s, Font f, Color c, float x, float y, float spacing)
        {
            using (SolidBrush b = new SolidBrush(c))
            {
                foreach (char ch in s)
                {
                    string t = ch.ToString();
                    g.DrawString(t, f, b, x, y, StringFormat.GenericTypographic);
                    float w = ch == ' ' ? f.Size * 0.45f : g.MeasureString(t, f, PointF.Empty, StringFormat.GenericTypographic).Width;
                    x += w + spacing;
                }
            }
        }
    }

    // -----------------------------------------------------------------
    //  Плоская кнопка
    // -----------------------------------------------------------------
    class FlatBtn : Control
    {
        public bool Primary;
        public bool Ghost;          // только текст, без фона
        public int Glyph;           // 0 текст, 1 свернуть, 2 закрыть
        bool hover, down;

        public FlatBtn()
        {
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer |
                     ControlStyles.UserPaint | ControlStyles.ResizeRedraw, true);
            Cursor = Cursors.Hand;
            Font = Theme.F(9.5f, FontStyle.Bold);
        }

        protected override void OnMouseEnter(EventArgs e) { hover = true; Invalidate(); base.OnMouseEnter(e); }
        protected override void OnMouseLeave(EventArgs e) { hover = false; down = false; Invalidate(); base.OnMouseLeave(e); }
        protected override void OnMouseDown(MouseEventArgs e) { down = true; Invalidate(); base.OnMouseDown(e); }
        protected override void OnMouseUp(MouseEventArgs e) { down = false; Invalidate(); base.OnMouseUp(e); }

        protected override void OnPaint(PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            Theme.Hq(g);
            Color parentBg = Parent != null ? Parent.BackColor : Theme.Bg;
            Color bg, fg, border = Color.Empty;

            if (Primary)
            {
                bg = down ? Color.FromArgb(230, 105, 15) : (hover ? Theme.OrangeHot : Theme.Orange);
                fg = Color.FromArgb(14, 14, 16);
            }
            else if (Ghost)
            {
                bg = hover ? Color.FromArgb(28, 30, 36) : parentBg;
                fg = hover ? Theme.Text : Theme.Dim;
                if (Glyph == 2 && hover) { bg = Color.FromArgb(200, 50, 40); fg = Color.White; }
            }
            else
            {
                bg = hover ? Theme.CardHover : Theme.Card;
                fg = Theme.Text;
                border = hover ? Theme.Orange : Theme.Line;
            }

            MainForm mf = Parent as MainForm;
            if (Ghost && !hover && mf != null && mf.Background != null)
                g.DrawImage(mf.Background, ClientRectangle, Bounds, GraphicsUnit.Pixel);
            else
                g.Clear(bg);
            if (border != Color.Empty)
                using (Pen p = new Pen(border)) g.DrawRectangle(p, 0, 0, Width - 1, Height - 1);

            if (Glyph == 1 || Glyph == 2)
            {
                using (Pen p = new Pen(fg, 1.4f))
                {
                    int cx = Width / 2, cy = Height / 2;
                    if (Glyph == 1) g.DrawLine(p, cx - 6, cy, cx + 6, cy);
                    else { g.DrawLine(p, cx - 5, cy - 5, cx + 5, cy + 5); g.DrawLine(p, cx + 5, cy - 5, cx - 5, cy + 5); }
                }
                return;
            }

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
    //  Карточка сервера
    // -----------------------------------------------------------------
    class ServerCard : Control
    {
        public ServerEntry Server;
        public bool Selected;
        bool hover;
        public bool Queried;

        public ServerCard(ServerEntry s)
        {
            Server = s;
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer |
                     ControlStyles.UserPaint | ControlStyles.ResizeRedraw, true);
            Cursor = Cursors.Hand;
        }

        protected override void OnMouseEnter(EventArgs e) { hover = true; Invalidate(); base.OnMouseEnter(e); }
        protected override void OnMouseLeave(EventArgs e) { hover = false; Invalidate(); base.OnMouseLeave(e); }

        protected override void OnPaint(PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            Theme.Hq(g);
            ServerEntry s = Server;
            g.Clear(Selected ? Theme.CardSel : (hover ? Theme.CardHover : Theme.Card));

            if (Selected)
                using (SolidBrush b = new SolidBrush(Theme.Orange)) g.FillRectangle(b, 0, 0, 3, Height);
            else
                using (Pen p = new Pen(Theme.Line)) g.DrawRectangle(p, 0, 0, Width - 1, Height - 1);

            int pad = 22;
            using (Font fName = Theme.Hv(15f))
            using (Font fDesc = Theme.F(9f, FontStyle.Regular))
            using (Font fNum = Theme.Hv(15f))
            using (Font fSmall = Theme.F(8f, FontStyle.Bold))
            using (SolidBrush text = new SolidBrush(Theme.Text))
            using (SolidBrush dim = new SolidBrush(Theme.Dim))
            using (SolidBrush orange = new SolidBrush(Theme.Orange))
            using (SolidBrush faint = new SolidBrush(Theme.Faint))
            {
                g.DrawString(s.Name.ToUpperInvariant(), fName, text, pad - 2, 16);
                RectangleF dr = new RectangleF(pad, 48, Width - pad - 118, 40);
                g.DrawString(s.Description, fDesc, dim, dr);

                // онлайн справа
                string num, sub;
                Brush numBrush;
                if (!Queried) { num = "…"; sub = "ПРОВЕРКА"; numBrush = dim; }
                else if (s.Online) { num = s.Players + " / " + s.MaxPlayers; sub = "ОНЛАЙН · " + s.Ping + " МС"; numBrush = text; }
                else { num = "OFF"; sub = "НЕ ОТВЕЧАЕТ"; numBrush = faint; }

                using (StringFormat right = new StringFormat())
                {
                    right.Alignment = StringAlignment.Far;
                    g.DrawString(num, fNum, numBrush, new RectangleF(Width - 160, 16, 160 - pad, 30), right);
                    g.DrawString(sub, fSmall, s.Online ? orange : faint, new RectangleF(Width - 200, 46, 200 - pad, 18), right);
                }

                // полоса заполненности
                int barY = Height - 14, barX = pad, barW = Width - pad * 2;
                using (SolidBrush track = new SolidBrush(Theme.Line)) g.FillRectangle(track, barX, barY, barW, 2);
                if (s.Online && s.MaxPlayers > 0)
                {
                    int w = (int)(barW * Math.Min(1.0, (double)s.Players / s.MaxPlayers));
                    if (s.Players > 0 && w < 4) w = 4;
                    g.FillRectangle(orange, barX, barY, w, 2);
                }
            }
        }
    }

    // -----------------------------------------------------------------
    //  Переключатель
    // -----------------------------------------------------------------
    class Toggle : Control
    {
        public bool Checked;
        bool hover;

        public Toggle(string text, bool on)
        {
            Text = text; Checked = on;
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer |
                     ControlStyles.UserPaint | ControlStyles.ResizeRedraw, true);
            Cursor = Cursors.Hand;
            Font = Theme.F(10f, FontStyle.Regular);
        }

        protected override void OnMouseEnter(EventArgs e) { hover = true; Invalidate(); base.OnMouseEnter(e); }
        protected override void OnMouseLeave(EventArgs e) { hover = false; Invalidate(); base.OnMouseLeave(e); }
        protected override void OnClick(EventArgs e) { Checked = !Checked; Invalidate(); base.OnClick(e); }

        protected override void OnPaint(PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            Theme.Hq(g);
            g.Clear(Parent != null ? Parent.BackColor : Theme.Card);
            using (SolidBrush t = new SolidBrush(hover ? Color.White : Theme.Text))
                g.DrawString(Text, Font, t, 0, (Height - Font.Height) / 2f);

            int w = 38, h = 18, x = Width - w - 1, y = (Height - h) / 2;
            using (GraphicsPath gp = Pill(x, y, w, h))
            using (SolidBrush b = new SolidBrush(Checked ? Theme.Orange : Theme.Line))
                g.FillPath(b, gp);
            int k = h - 6;
            int kx = Checked ? x + w - k - 3 : x + 3;
            using (SolidBrush b = new SolidBrush(Checked ? Color.FromArgb(14, 14, 16) : Theme.Dim))
                g.FillEllipse(b, kx, y + 3, k, k);
        }

        public static GraphicsPath Pill(int x, int y, int w, int h)
        {
            GraphicsPath gp = new GraphicsPath();
            gp.AddArc(x, y, h, h, 90, 180);
            gp.AddArc(x + w - h, y, h, h, 270, 180);
            gp.CloseFigure();
            return gp;
        }
    }

    // -----------------------------------------------------------------
    //  Выбор из нескольких значений (30 | 60 | 90)
    // -----------------------------------------------------------------
    class Segmented : Control
    {
        int[] opts;
        int val;
        public int Value { get { return val; } set { val = value; } }
        int hoverIdx = -1;

        public Segmented(int[] options, int value)
        {
            opts = options; Value = value;
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer |
                     ControlStyles.UserPaint | ControlStyles.ResizeRedraw, true);
            Cursor = Cursors.Hand;
            Font = Theme.F(9.5f, FontStyle.Bold);
        }

        int IndexAt(int x) { return Math.Max(0, Math.Min(opts.Length - 1, x * opts.Length / Math.Max(1, Width))); }

        protected override void OnMouseMove(MouseEventArgs e) { int i = IndexAt(e.X); if (i != hoverIdx) { hoverIdx = i; Invalidate(); } base.OnMouseMove(e); }
        protected override void OnMouseLeave(EventArgs e) { hoverIdx = -1; Invalidate(); base.OnMouseLeave(e); }
        protected override void OnMouseDown(MouseEventArgs e) { Value = opts[IndexAt(e.X)]; Invalidate(); base.OnMouseDown(e); }

        protected override void OnPaint(PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            Theme.Hq(g);
            g.Clear(Theme.BgBar);
            float cw = (float)Width / opts.Length;
            using (StringFormat sf = new StringFormat())
            {
                sf.Alignment = StringAlignment.Center;
                sf.LineAlignment = StringAlignment.Center;
                for (int i = 0; i < opts.Length; i++)
                {
                    RectangleF r = new RectangleF(i * cw, 0, cw, Height);
                    bool sel = opts[i] == Value;
                    if (sel) using (SolidBrush b = new SolidBrush(Theme.Orange)) g.FillRectangle(b, r);
                    else if (i == hoverIdx) using (SolidBrush b = new SolidBrush(Theme.CardHover)) g.FillRectangle(b, r);
                    using (SolidBrush t = new SolidBrush(sel ? Color.FromArgb(14, 14, 16) : Theme.Text))
                        g.DrawString(opts[i].ToString(), Font, t, r, sf);
                }
            }
            using (Pen p = new Pen(Theme.Line)) g.DrawRectangle(p, 0, 0, Width - 1, Height - 1);
        }
    }

    // -----------------------------------------------------------------
    //  Ползунок (лимит FPS): тащи мышью, колесо или стрелки — по 1
    // -----------------------------------------------------------------
    class Slider : Control
    {
        int min, max, val;
        public int[] Marks = new int[0];         // засечки под дорожкой
        public string Suffix = "";
        bool hover, drag;
        const int LabelW = 74;                  // справа — цифра

        public int Value
        {
            get { return val; }
            set { val = Math.Max(min, Math.Min(max, value)); Invalidate(); }
        }

        public Slider(int minimum, int maximum, int value)
        {
            min = minimum; max = Math.Max(minimum + 1, maximum); Value = value;
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer |
                     ControlStyles.UserPaint | ControlStyles.ResizeRedraw | ControlStyles.Selectable, true);
            Cursor = Cursors.Hand;
            Font = Theme.F(10.5f, FontStyle.Bold);
        }

        int TrackL { get { return 9; } }
        int TrackR { get { return Width - LabelW - 9; } }
        float XOf(int v) { return TrackL + (float)(v - min) * (TrackR - TrackL) / (max - min); }

        void SetFromX(int x)
        {
            float t = (float)(x - TrackL) / Math.Max(1, TrackR - TrackL);
            int v = min + (int)Math.Round(t * (max - min));
            // липнем к засечкам (60, 90, 120…), чтобы их было легко поймать
            foreach (int m in Marks) if (Math.Abs(m - v) <= 2) v = m;
            Value = v;
        }

        protected override void OnMouseDown(MouseEventArgs e) { Focus(); drag = true; SetFromX(e.X); base.OnMouseDown(e); }
        protected override void OnMouseUp(MouseEventArgs e) { drag = false; base.OnMouseUp(e); }
        protected override void OnMouseMove(MouseEventArgs e) { if (drag) SetFromX(e.X); base.OnMouseMove(e); }
        protected override void OnMouseEnter(EventArgs e) { hover = true; Invalidate(); base.OnMouseEnter(e); }
        protected override void OnMouseLeave(EventArgs e) { hover = false; Invalidate(); base.OnMouseLeave(e); }
        protected override void OnMouseWheel(MouseEventArgs e) { Value = val + (e.Delta > 0 ? 1 : -1); base.OnMouseWheel(e); }
        protected override bool IsInputKey(Keys k) { return k == Keys.Left || k == Keys.Right || base.IsInputKey(k); }
        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Left) Value = val - 1;
            else if (e.KeyCode == Keys.Right) Value = val + 1;
            base.OnKeyDown(e);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            Theme.Hq(g);
            g.Clear(Theme.Card);
            int cy = Height / 2 - 3;
            float x = XOf(val);
            using (SolidBrush b = new SolidBrush(Theme.BgBar)) g.FillRectangle(b, TrackL, cy - 2, TrackR - TrackL, 4);
            using (SolidBrush b = new SolidBrush(Theme.Orange)) g.FillRectangle(b, TrackL, cy - 2, x - TrackL, 4);
            using (Font mf = Theme.F(7f, FontStyle.Regular))
            using (SolidBrush dim = new SolidBrush(Theme.Faint))
            using (SolidBrush tick = new SolidBrush(Theme.Line))
            using (StringFormat sf = new StringFormat())
            {
                sf.Alignment = StringAlignment.Center;
                foreach (int m in Marks)
                {
                    if (m <= min || m >= max) continue;
                    float mx = XOf(m);
                    g.FillRectangle(tick, mx - 0.5f, cy + 5, 1, 4);
                    g.DrawString(m.ToString(), mf, dim, new RectangleF(mx - 20, cy + 8, 40, 14), sf);
                }
            }
            int r = (drag || hover) ? 8 : 7;
            using (SolidBrush b = new SolidBrush(Theme.Orange)) g.FillEllipse(b, x - r, cy - r, r * 2, r * 2);
            using (SolidBrush b = new SolidBrush(Theme.Card)) g.FillEllipse(b, x - 3, cy - 3, 6, 6);

            Rectangle lab = new Rectangle(Width - LabelW + 8, 0, LabelW - 8, Height);
            using (SolidBrush bg = new SolidBrush(Theme.BgBar)) g.FillRectangle(bg, lab);
            using (Pen p = new Pen(Focused ? Theme.Orange : Theme.Line)) g.DrawRectangle(p, lab.X, lab.Y, lab.Width - 1, lab.Height - 1);
            using (SolidBrush t = new SolidBrush(Theme.Text))
            using (StringFormat sf = new StringFormat())
            {
                sf.Alignment = StringAlignment.Center;
                sf.LineAlignment = StringAlignment.Center;
                g.DrawString(val + Suffix, Font, t, lab, sf);
            }
        }

        protected override void OnGotFocus(EventArgs e) { Invalidate(); base.OnGotFocus(e); }
        protected override void OnLostFocus(EventArgs e) { Invalidate(); base.OnLostFocus(e); }
    }

    // -----------------------------------------------------------------
    //  Главное окно
    // -----------------------------------------------------------------
    class MainForm : Form
    {
        [DllImport("user32.dll")] static extern bool ReleaseCapture();
        [DllImport("user32.dll")] static extern IntPtr SendMessage(IntPtr h, int msg, int w, int l);

        const int W = 1000, H = 600, BarY = 500;
        const int LeftX = 56, RightX = 576, RightW = 368;

        Config cfg;
        List<ServerCard> cards = new List<ServerCard>();
        ServerCard selected;
        TextBox nick;
        TextBox gtaPath;
        Panel settings;
        FlatBtn play;
        string statusText = "";
        List<string[]> news = new List<string[]>();
        Image bgImage;
        // Логотипы вшиты в exe (/resource в build.bat и pc-launcher.yml)
        static readonly Image LogoBanner = Res("logo-banner.png");
        static readonly Image LogoShield = Res("logo-shield.png");
        static Image Res(string name)
        {
            try
            {
                using (Stream st = typeof(MainForm).Assembly.GetManifestResourceStream(name))
                    if (st != null) using (Image tmp = Image.FromStream(st)) return new Bitmap(tmp);
            }
            catch (Exception) { }
            return null;
        }
        Bitmap bgCache;
        public Bitmap Background { get { if (bgCache == null) bgCache = BuildBackground(); return bgCache; } }
        System.Windows.Forms.Timer refresh;
        int querying;

        protected override CreateParams CreateParams
        {
            get
            {
                CreateParams cp = base.CreateParams;
                cp.ClassStyle |= 0x20000; // тень окна
                return cp;
            }
        }

        public MainForm()
        {
            cfg = Config.Load();
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.UserPaint, true);
            FormBorderStyle = FormBorderStyle.None;
            ClientSize = new Size(W, H);
            StartPosition = FormStartPosition.CenterScreen;
            BackColor = Theme.Bg;
            Text = cfg.Title;
            Font = Theme.F(10f, FontStyle.Regular);
            try { Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch (Exception) { }

            LoadBackground();
            LoadNews();

            // --- верхняя панель ---
            FlatBtn close = new FlatBtn(); close.Ghost = true; close.Glyph = 2;
            close.Bounds = new Rectangle(W - 46, 0, 46, 36);
            close.Click += delegate { Close(); };
            Controls.Add(close);

            FlatBtn min = new FlatBtn(); min.Ghost = true; min.Glyph = 1;
            min.Bounds = new Rectangle(W - 92, 0, 46, 36);
            min.Click += delegate { WindowState = FormWindowState.Minimized; };
            Controls.Add(min);

            FlatBtn setBtn = new FlatBtn(); setBtn.Ghost = true; setBtn.Text = "НАСТРОЙКИ";
            setBtn.Font = Theme.F(8.5f, FontStyle.Bold);
            setBtn.Bounds = new Rectangle(W - 200, 0, 108, 36);
            setBtn.Click += delegate { ToggleSettings(); };
            Controls.Add(setBtn);

            // кнопки-ссылки (ФОРУМ, ДОНАТ, …) справа налево от «Настройки»
            List<string[]> links = new List<string[]>(cfg.Links);
            if (links.Count == 0 && cfg.Site.Length > 0) links.Add(new string[] { "САЙТ", cfg.Site });
            int lx = W - 200;
            for (int li = links.Count - 1; li >= 0; li--)
            {
                string url = links[li][1];
                FlatBtn lb = new FlatBtn(); lb.Ghost = true; lb.Text = links[li][0];
                lb.Font = Theme.F(8.5f, FontStyle.Bold);
                int lw;
                using (Graphics mg = CreateGraphics()) lw = (int)mg.MeasureString(lb.Text, lb.Font).Width + 28;
                lx -= lw;
                lb.Bounds = new Rectangle(lx, 0, lw, 36);
                lb.Click += delegate { try { Process.Start(url); } catch (Exception) { } };
                Controls.Add(lb);
            }

            // --- карточки серверов ---
            int y = 128;
            int cardH = cfg.Servers.Count > 3 ? 84 : 104;
            foreach (ServerEntry s in cfg.Servers)
            {
                if (y + cardH > BarY - 16) break;
                ServerCard c = new ServerCard(s);
                c.Bounds = new Rectangle(RightX, y, RightW, cardH);
                c.Click += delegate (object o, EventArgs e) { Select((ServerCard)o); };
                c.DoubleClick += delegate { Play(); };
                Controls.Add(c);
                cards.Add(c);
                y += cardH + 12;
            }

            // --- нижняя панель: ник + играть ---
            nick = new TextBox();
            nick.BorderStyle = BorderStyle.None;
            nick.BackColor = Theme.BgBar;
            nick.ForeColor = Theme.Text;
            nick.Font = Theme.D(16f, FontStyle.Regular);
            nick.MaxLength = 24;
            nick.Bounds = new Rectangle(LeftX, BarY + 46, 300, 30);
            nick.Text = Samp.GetValue("PlayerName");
            nick.GotFocus += delegate { Invalidate(new Rectangle(LeftX, BarY + 78, 300, 4)); };
            nick.LostFocus += delegate { Invalidate(new Rectangle(LeftX, BarY + 78, 300, 4)); };
            nick.KeyDown += delegate (object o, KeyEventArgs e) { if (e.KeyCode == Keys.Enter) { e.SuppressKeyPress = true; Play(); } };
            Controls.Add(nick);

            play = new FlatBtn(); play.Primary = true; play.Text = "ИГРАТЬ";
            play.Font = Theme.Hv(17f);
            play.Bounds = new Rectangle(RightX + RightW - 240, BarY + 22, 240, 58);
            play.Click += delegate { Play(); };
            Controls.Add(play);

            BuildSettings();
            gtaPath.TextChanged += delegate { UpdatePlayLabel(); };
            UpdatePlayLabel();

            int last = 0;
            int.TryParse(Samp.GetValue("BastionLastServer"), out last);
            if (cards.Count > 0) Select(cards[last >= 0 && last < cards.Count ? last : 0]);

            refresh = new System.Windows.Forms.Timer();
            refresh.Interval = 10000;
            refresh.Tick += delegate { RefreshAll(); };
            refresh.Start();
            Shown += delegate { RefreshAll(); ActiveControl = play; CheckUpdate(); CheckGameUpdate(); };
            if (Environment.GetCommandLineArgs().Length > 1 && Environment.GetCommandLineArgs()[1] == "--settings") ToggleSettings();
        }

        // --- перетаскивание окна за фон ---
        protected override void OnMouseDown(MouseEventArgs e)
        {
            base.OnMouseDown(e);
            if (e.Button == MouseButtons.Left)
            {
                try { ReleaseCapture(); SendMessage(Handle, 0xA1, 2, 0); } catch (Exception) { }
            }
        }

        void Select(ServerCard c)
        {
            foreach (ServerCard x in cards) { x.Selected = (x == c); x.Invalidate(); }
            selected = c;
        }

        // ---------------- фон ----------------
        void LoadBackground()
        {
            string dir = AppDomain.CurrentDomain.BaseDirectory;
            foreach (string n in new string[] { "background.jpg", "background.png" })
            {
                string p = Path.Combine(dir, n);
                if (File.Exists(p))
                {
                    try { using (Image tmp = Image.FromFile(p)) bgImage = new Bitmap(tmp); } catch (Exception) { }
                    break;
                }
            }
            if (bgImage == null) bgImage = LoadScreens.Pick();
        }

        Bitmap BuildBackground()
        {
            Bitmap bmp = new Bitmap(W, H);
            using (Graphics g = Graphics.FromImage(bmp))
            {
                Theme.Hq(g);
                g.Clear(Theme.Bg);

                if (bgImage != null)
                {
                    // картинка «cover» + затемнение слева направо
                    float k = Math.Max((float)W / bgImage.Width, (float)H / bgImage.Height);
                    float iw = bgImage.Width * k, ih = bgImage.Height * k;
                    g.DrawImage(bgImage, (W - iw) / 2, (H - ih) / 2, iw, ih);
                    using (LinearGradientBrush lg = new LinearGradientBrush(new Rectangle(0, 0, W, H),
                        Color.FromArgb(120, Theme.Bg), Color.FromArgb(215, Theme.Bg), LinearGradientMode.Horizontal))
                        g.FillRectangle(lg, 0, 0, W, H);
                }
                else
                {
                    // оранжевое свечение снизу слева
                    using (GraphicsPath gp = new GraphicsPath())
                    {
                        gp.AddEllipse(-420, 180, 1000, 760);
                        using (PathGradientBrush pg = new PathGradientBrush(gp))
                        {
                            pg.CenterColor = Color.FromArgb(46, Theme.Orange);
                            pg.SurroundColors = new Color[] { Color.FromArgb(0, Theme.Orange) };
                            g.FillPath(pg, gp);
                        }
                    }
                    // тонкие диагональные линии
                    using (Pen p = new Pen(Color.FromArgb(9, 255, 255, 255), 1f))
                        for (int x = -H; x < W; x += 22) g.DrawLine(p, x, H, x + H, 0);

                    // огромная полупрозрачная «B» справа за карточками
                    using (Font huge = Theme.Hv(420f))
                    using (SolidBrush b = new SolidBrush(Color.FromArgb(7, 255, 255, 255)))
                        g.DrawString("B", huge, b, W - 520, -110);

                    // виньетка
                    using (GraphicsPath gp = new GraphicsPath())
                    {
                        gp.AddRectangle(new Rectangle(-200, -200, W + 400, H + 400));
                        using (PathGradientBrush pg = new PathGradientBrush(gp))
                        {
                            pg.CenterColor = Color.FromArgb(0, 0, 0, 0);
                            pg.SurroundColors = new Color[] { Color.FromArgb(160, 0, 0, 0) };
                            pg.FocusScales = new PointF(0.55f, 0.5f);
                            g.FillPath(pg, gp);
                        }
                    }
                }

                // нижняя панель
                using (SolidBrush b = new SolidBrush(Theme.BgBar)) g.FillRectangle(b, 0, BarY, W, H - BarY);
                using (Pen p = new Pen(Theme.Line)) g.DrawLine(p, 0, BarY, W, BarY);
                using (SolidBrush b = new SolidBrush(Theme.Orange)) g.FillRectangle(b, 0, BarY - 1, 120, 2);

                // рамка окна
                using (Pen p = new Pen(Theme.Line)) g.DrawRectangle(p, 0, 0, W - 1, H - 1);
            }
            return bmp;
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            g.DrawImageUnscaled(Background, 0, 0);
            Theme.Hq(g);

            using (SolidBrush text = new SolidBrush(Theme.Text))
            using (SolidBrush dim = new SolidBrush(Theme.Dim))
            using (SolidBrush orange = new SolidBrush(Theme.Orange))
            {
                // мини-логотип в шапке
                using (Font f = Theme.F(8.5f, FontStyle.Bold))
                {
                    if (LogoShield != null) g.DrawImage(LogoShield, 16, 6, 24, 24);
                    else g.FillRectangle(orange, 22, 14, 8, 8);
                    Theme.Spaced(g, cfg.Title.ToUpperInvariant(), f, Theme.Dim, LogoShield != null ? 46 : 40, 11, 2.5f);
                }

                // крупный заголовок: логотип-баннер, без него — нарисованный текст
                if (LogoBanner != null)
                {
                    float lw = 450, lh = lw * LogoBanner.Height / LogoBanner.Width;
                    g.DrawImage(LogoBanner, LeftX - 12, 236 - lh, lw, lh);
                }
                else
                {
                string title = cfg.Title.ToUpperInvariant();
                string first = title, rest = "";
                int sp = title.IndexOf(' ');
                if (sp > 0) { first = title.Substring(0, sp); rest = title.Substring(sp + 1); }
                DrawTitle(g, first, LeftX, 86, 440, 96);
                g.FillRectangle(orange, LeftX, 196, 44, 4);
                using (Font f = Theme.F(9f, FontStyle.Bold))
                    Theme.Spaced(g, (rest.Length > 0 ? rest + "  ·  " : "") + "SAN ANDREAS MULTIPLAYER", f, Theme.Dim, LeftX + 60, 190, 3f);
                }

                // новости
                if (!settings.Visible)
                {
                    int ny = 248;
                    using (Font cap = Theme.F(8.5f, FontStyle.Bold))
                        Theme.Spaced(g, "НОВОСТИ", cap, Theme.Orange, LeftX, ny, 3f);
                    ny += 30;
                    using (Font fDate = Theme.F(8f, FontStyle.Bold))
                    using (Font fHead = Theme.F(11.5f, FontStyle.Bold))
                    using (Font fBody = Theme.F(9f, FontStyle.Regular))
                    using (SolidBrush faint = new SolidBrush(Theme.Faint))
                    {
                        int shown = 0;
                        foreach (string[] n in news)
                        {
                            if (shown++ >= 3 || ny > BarY - 50) break;
                            g.DrawString(n[0], fDate, faint, LeftX, ny);
                            g.DrawString(n[1], fHead, text, LeftX + 86, ny - 3);
                            int hh = 22;
                            if (n[2].Length > 0)
                            {
                                RectangleF r = new RectangleF(LeftX + 86, ny + 20, 380, 40);
                                SizeF sz = g.MeasureString(n[2], fBody, 380);
                                g.DrawString(n[2], fBody, dim, r);
                                hh += (int)Math.Min(40, sz.Height) + 4;
                            }
                            ny += hh + 14;
                        }
                        if (news.Count == 0) g.DrawString("Пока новостей нет.", fBody, dim, LeftX, ny);
                    }
                }

                // заголовок колонки серверов
                using (Font cap = Theme.F(8.5f, FontStyle.Bold))
                    Theme.Spaced(g, "ВЫБЕРИ СЕРВЕР", cap, Theme.Dim, RightX, 100, 3f);

                // нижняя панель
                using (Font cap = Theme.F(8f, FontStyle.Bold))
                    Theme.Spaced(g, "ТВОЙ НИК", cap, Theme.Dim, LeftX, BarY + 24, 3f);
                using (SolidBrush ul = new SolidBrush(nick.Focused ? Theme.Orange : Theme.Line))
                    g.FillRectangle(ul, LeftX, BarY + 78, 300, nick.Focused ? 2 : 1);

                using (Font f = Theme.F(9f, FontStyle.Regular))
                    g.DrawString(statusText, f, dim, new RectangleF(LeftX + 330, BarY + 44, 300, 40));
            }
        }

        // Мощный заголовок: тяжёлые буквы, металл, тёмная обводка, тень
        static void DrawTitle(Graphics g, string s, float x, float y, float maxW, float maxH)
        {
            using (GraphicsPath path = new GraphicsPath())
            {
                path.AddString(s, Theme.Heavy, (int)(Theme.HeavyLoaded ? FontStyle.Regular : FontStyle.Bold),
                    100f, PointF.Empty, StringFormat.GenericTypographic);
                RectangleF b = path.GetBounds();
                if (b.Width <= 0 || b.Height <= 0) return;
                float k = Math.Min(maxW / b.Width, maxH / b.Height);
                using (Matrix m = new Matrix())
                {
                    m.Translate(x, y);
                    m.Scale(k, k);
                    m.Translate(-b.X, -b.Y);
                    path.Transform(m);
                }
                RectangleF r = path.GetBounds();

                using (GraphicsPath sh = (GraphicsPath)path.Clone())
                {
                    using (Matrix m = new Matrix()) { m.Translate(0, 6); sh.Transform(m); }
                    using (SolidBrush b2 = new SolidBrush(Color.FromArgb(170, 0, 0, 0))) g.FillPath(b2, sh);
                }
                using (Pen p = new Pen(Color.FromArgb(8, 8, 10), 5f))
                {
                    p.LineJoin = LineJoin.Round;
                    g.DrawPath(p, path);
                }
                using (LinearGradientBrush lg = new LinearGradientBrush(
                    new RectangleF(r.X, r.Y - 1, r.Width, r.Height + 2), Color.White, Color.Gray, LinearGradientMode.Vertical))
                {
                    ColorBlend cb = new ColorBlend();
                    cb.Colors = new Color[] {
                        Color.FromArgb(255, 255, 255), Color.FromArgb(222, 224, 230),
                        Color.FromArgb(150, 154, 164), Color.FromArgb(196, 199, 207), Color.FromArgb(104, 108, 118) };
                    cb.Positions = new float[] { 0f, 0.46f, 0.52f, 0.7f, 1f };
                    lg.InterpolationColors = cb;
                    g.FillPath(lg, path);
                }
                using (Pen p = new Pen(Color.FromArgb(60, 255, 255, 255), 1f)) g.DrawPath(p, path);
            }
        }

        void SetStatus(string s) { statusText = s; Invalidate(new Rectangle(LeftX + 330, BarY + 40, 310, 50)); }

        // ---------------- новости ----------------
        void LoadNews()
        {
            // Сразу показываем то, что есть на диске: последние скачанные новости, иначе news.txt
            string p = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "news.txt");
            string cache = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "news.cache");
            try { if (File.Exists(p)) ParseNews(File.ReadAllText(p, Encoding.UTF8)); } catch (Exception) { }
            string newsUrl = cfg.EffectiveNewsUrl;
            if (newsUrl.Length > 0)
            {
                try { if (File.Exists(cache)) ParseNews(File.ReadAllText(cache, Encoding.UTF8)); } catch (Exception) { }
                Thread th = new Thread(delegate ()
                {
                    try
                    {
                        string txt = Remote.FetchText(newsUrl, 6000);
                        if (!HasNews(txt)) return;
                        try { File.WriteAllText(cache, txt, new UTF8Encoding(true)); } catch (Exception) { }
                        BeginInvoke(new MethodInvoker(delegate { ParseNews(txt); Invalidate(); }));
                    }
                    catch (Exception) { }
                });
                th.IsBackground = true;
                th.Start();
            }
        }

        // Похоже ли это на файл новостей (а не на страницу ошибки)
        static bool HasNews(string txt)
        {
            foreach (string raw in txt.Replace("\r", "").Split('\n'))
            {
                string line = raw.Trim().TrimStart('\uFEFF');
                if (line.Length == 0 || line.StartsWith(";") || line.StartsWith("#")) continue;
                if (line.StartsWith("<") || line.Split('|').Length < 2) return false;
                return true;
            }
            return false;
        }

        // ---------------- обновление лаунчера ----------------
        void CheckUpdate()
        {
            string url = cfg.EffectiveUpdateUrl;
            if (url.Length == 0) return;
            Thread th = new Thread(delegate ()
            {
                UpdateInfo info = Updater.Check(url);
                if (info == null) return;
                try { BeginInvoke(new MethodInvoker(delegate { OfferUpdate(info); })); } catch (Exception) { }
            });
            th.IsBackground = true;
            th.Start();
        }

        void OfferUpdate(UpdateInfo info)
        {
            if (installing) return;
            string text = "Новая версия лаунчера: " + info.Version + "\r\nУ тебя: " + Updater.Current;
            if (info.Notes.Length > 0) text += "\r\n\r\n" + info.Notes;
            text += "\r\n\r\nЛаунчер скачает обновление и перезапустится сам. Игра и настройки останутся.";
            using (UpdateDialog d = new UpdateDialog(text))
                if (d.ShowDialog(this) != DialogResult.OK) { SetStatus("Доступно обновление лаунчера."); return; }

            installing = true;
            play.Enabled = false;
            Thread th = new Thread(delegate ()
            {
                string error = null, exe = null;
                try
                {
                    exe = Updater.Apply(info, delegate (string msg)
                    {
                        try { BeginInvoke(new MethodInvoker(delegate { SetStatus(msg); })); } catch (Exception) { }
                    });
                }
                catch (Exception ex) { error = ex.Message; }
                try
                {
                    BeginInvoke(new MethodInvoker(delegate
                    {
                        if (error == null)
                        {
                            try { Process.Start(exe); } catch (Exception) { }
                            Application.Exit();
                            return;
                        }
                        installing = false;
                        play.Enabled = true;
                        SetStatus("Обновление не удалось.");
                        Warn("Не получилось обновить лаунчер: " + error + "\r\nМожно играть на этой версии или скачать новую вручную.");
                    }));
                }
                catch (Exception) { }
            });
            th.IsBackground = true;
            th.Start();
        }

        void ParseNews(string txt)
        {
            List<string[]> list = new List<string[]>();
            foreach (string raw in txt.Replace("\r", "").Split('\n'))
            {
                string line = raw.Trim();
                if (line.Length == 0 || line.StartsWith(";") || line.StartsWith("#")) continue;
                string[] p = line.Split('|');
                if (p.Length < 2) continue;
                list.Add(new string[] { p[0].Trim(), p[1].Trim(), p.Length > 2 ? p[2].Trim() : "" });
            }
            news = list;
        }

        // ---------------- настройки ----------------
        Segmented segChat;
        Slider segFps;
        Toggle tStamp, tMulti, tMods, tClose;

        void BuildSettings()
        {
            settings = new Panel();
            settings.Bounds = new Rectangle(LeftX - 16, 52, 500, 436);
            settings.BackColor = Theme.Card;
            settings.Visible = false;
            settings.Paint += delegate (object o, PaintEventArgs e)
            {
                Graphics g = e.Graphics;
                Theme.Hq(g);
                using (Pen p = new Pen(Theme.Line)) g.DrawRectangle(p, 0, 0, settings.Width - 1, settings.Height - 1);
                using (SolidBrush b = new SolidBrush(Theme.Orange)) g.FillRectangle(b, 0, 0, 3, settings.Height);
                using (Font cap = Theme.F(8.5f, FontStyle.Bold))
                {
                    Theme.Spaced(g, "НАСТРОЙКИ", cap, Theme.Orange, 22, 16, 3f);
                    Theme.Spaced(g, "ПАПКА GTA SAN ANDREAS", cap, Theme.Dim, 22, 46, 2f);
                    Theme.Spaced(g, "ИГРА", cap, Theme.Dim, 22, 128, 2f);
                    Theme.Spaced(g, "ЛАУНЧЕР", cap, Theme.Dim, 22, 302, 2f);
                }
                using (Font f = Theme.F(10f, FontStyle.Regular))
                using (SolidBrush t = new SolidBrush(Theme.Text))
                {
                    g.DrawString("Лимит FPS", f, t, 22, 158);
                    g.DrawString("Строк в чате", f, t, 22, 198);
                }
                using (Font f = Theme.F(8.5f, FontStyle.Regular))
                using (SolidBrush dim = new SolidBrush(Theme.Faint))
                    g.DrawString("Там, где лежат gta_sa.exe и samp.exe", f, dim, 22, 100);
                using (SolidBrush ul = new SolidBrush(Theme.Line))
                {
                    g.FillRectangle(ul, 22, 94, 340, 1);
                    g.FillRectangle(ul, 22, 120, 458, 1);
                    g.FillRectangle(ul, 22, 294, 458, 1);
                }
            };

            gtaPath = new TextBox();
            gtaPath.BorderStyle = BorderStyle.None;
            gtaPath.BackColor = Theme.Card;
            gtaPath.ForeColor = Theme.Text;
            gtaPath.Font = Theme.F(10f, FontStyle.Regular);
            gtaPath.Bounds = new Rectangle(22, 70, 340, 22);
            gtaPath.Text = Samp.GtaFolder;
            settings.Controls.Add(gtaPath);

            FlatBtn browse = new FlatBtn(); browse.Text = "ОБЗОР";
            browse.Bounds = new Rectangle(376, 62, 104, 32);
            browse.Click += delegate { Browse(); };
            settings.Controls.Add(browse);

            SampCfg sc = SampCfg.Load();
            // выше 90 лаунчер хранит сам (в sa-mp.cfg SA-MP больше 90 не примет)
            FpsPack.Load();
            int fpsNow;
            // по умолчанию — максимум (165), если пакет FPS на месте; без пакета SA-MP выше 90 не даст
            if (!int.TryParse(Samp.GetValue("BastionFps"), out fpsNow)) fpsNow = FpsPack.Available ? FpsPack.MaxFps : FpsPack.SampMax;
            // раньше выше 90 было нельзя: тем, у кого стояло 90, один раз поднимаем до максимума
            if (fpsNow == FpsPack.SampMax && FpsPack.Available && Samp.GetValue("BastionFpsMax") != "1") fpsNow = FpsPack.MaxFps;
            segFps = new Slider(FpsPack.MinFps, FpsPack.MaxFps, fpsNow);
            segFps.Marks = FpsPack.Marks;
            segFps.Bounds = new Rectangle(200, 148, 280, 38);
            settings.Controls.Add(segFps);

            segChat = new Segmented(new int[] { 10, 15, 20 }, Snap(sc.GetInt("pagesize", 10), new int[] { 10, 15, 20 }));
            segChat.Bounds = new Rectangle(200, 192, 280, 30);
            settings.Controls.Add(segChat);

            tStamp = new Toggle("Время у сообщений в чате", sc.GetInt("timestamp", 0) == 1);
            tStamp.Bounds = new Rectangle(22, 230, 458, 26);
            settings.Controls.Add(tStamp);

            tMulti = new Toggle("Многоядерность (плавнее на современных ПК)", sc.GetInt("multicore", 1) == 1);
            tMulti.Bounds = new Rectangle(22, 260, 458, 26);
            settings.Controls.Add(tMulti);

            tMods = new Toggle("Проверять моды перед запуском", Samp.GetValue("BastionCheckMods") != "0");
            tMods.Bounds = new Rectangle(22, 324, 458, 26);
            settings.Controls.Add(tMods);

            tClose = new Toggle("Закрывать лаунчер после запуска игры", Samp.GetValue("BastionCloseOnPlay") == "1");
            tClose.Bounds = new Rectangle(22, 352, 458, 26);
            settings.Controls.Add(tClose);

            FlatBtn mods = new FlatBtn(); mods.Text = "ПРОВЕРИТЬ МОДЫ";
            mods.Bounds = new Rectangle(22, 388, 170, 36);
            mods.Click += delegate { CheckMods(true); };
            settings.Controls.Add(mods);

            FlatBtn verify = new FlatBtn(); verify.Text = "ПРОВЕРИТЬ ФАЙЛЫ";
            verify.Bounds = new Rectangle(200, 388, 130, 36);
            verify.Font = Theme.F(8f, FontStyle.Bold);
            verify.Click += delegate
            {
                if (!GameFound()) { Warn("Сначала укажи папку игры."); return; }
                ToggleSettings();
                if (cfg.EffectiveClientPackUrl.Length == 0) { OfferReinstall(false); return; }
                SyncPack(gtaPath.Text.Trim(), true, false);
            };
            settings.Controls.Add(verify);

            FlatBtn done = new FlatBtn(); done.Text = "СОХРАНИТЬ"; done.Primary = true;
            done.Font = Theme.Hv(11f);
            done.Bounds = new Rectangle(340, 388, 140, 36);
            done.Click += delegate { SaveSettings(true); ToggleSettings(); };
            settings.Controls.Add(done);

            Controls.Add(settings);
            settings.BringToFront();
        }

        // Лимит FPS перед запуском: до 90 — средствами SA-MP, выше — через пакет FPS.
        // Возвращает false, если запуск надо отменить.
        // Windows 11 24H2: оригинальная GTA SA падает при загрузке, потому что у Skimmer
        // в data\vehicles.ide не хватает полей колёс (игра читает мусор из памяти).
        // Дописываем недостающее, как в известном фиксе: -1, 0.7, 0.7, -1.
        static void FixSkimmer(string gta)
        {
            try
            {
                string path = Path.Combine(Path.Combine(gta, "data"), "vehicles.ide");
                if (!File.Exists(path)) return;
                Encoding enc = Encoding.GetEncoding(1252);
                string[] lines = File.ReadAllLines(path, enc);
                bool changed = false;
                for (int i = 0; i < lines.Length; i++)
                {
                    string t = lines[i].Trim();
                    if (!t.StartsWith("460")) continue;
                    string[] f = t.Split(',');
                    if (f.Length != 11 || f[0].Trim() != "460" ||
                        !string.Equals(f[1].Trim(), "skimmer", StringComparison.OrdinalIgnoreCase)) continue;
                    lines[i] = lines[i].TrimEnd() + ",\t-1,\t0.7,\t0.7,\t-1";
                    changed = true;
                }
                if (!changed) return;
                string bak = path + ".bak";
                if (!File.Exists(bak)) File.Copy(path, bak);
                File.WriteAllLines(path, lines, enc);
            }
            catch (Exception) { } // не критично: просто не трогаем файл
        }

        bool ApplyFps(string gta)
        {
            int want = segFps.Value;
            bool packIn = FpsPack.Available && FpsPack.Installed(gta);
            if (want > FpsPack.SampMax && !FpsPack.Available)
            {
                // пакета FPS нет (старая сборка лаунчера без папки fps) — без лишнего окна, просто 90
                want = FpsPack.SampMax;
                SetStatus("Нет папки fps рядом с лаунчером — игра пойдёт с 90 FPS. Скачай свежий лаунчер.");
            }
            else if (want > FpsPack.SampMax && !packIn)
            {
                try
                {
                    int n = FpsPack.Install(gta);
                    packIn = true;
                    SetStatus("Пакет FPS установлен в игру (файлов: " + n + ").");
                }
                catch (Exception ex)
                {
                    Warn("Не получилось поставить пакет FPS: " + ex.Message +
                         "\r\nЕсли игра лежит в Program Files — запусти лаунчер от имени администратора. Пока будет 90 FPS.");
                    want = FpsPack.SampMax;
                }
            }
            try
            {
                SampCfg sc = SampCfg.Load();
                sc.Set("fpslimit", Math.Min(want, FpsPack.SampMax).ToString());
                sc.Save();
                // плагин снятия лимита держит свой лимит: пишем ему то же значение (и 60, и 165)
                if (packIn) FpsPack.WriteLimit(gta, want);
            }
            catch (Exception ex)
            {
                Warn("Не удалось записать лимит FPS: " + ex.Message);
            }
            return true;
        }

        static int Snap(int v, int[] opts)
        {
            int best = opts[0];
            foreach (int o in opts) if (Math.Abs(o - v) < Math.Abs(best - v)) best = o;
            return best;
        }

        // Сохраняет настройки SA-MP (sa-mp.cfg) и лаунчера (реестр)
        bool SaveSettings(bool showStatus)
        {
            try
            {
                SampCfg sc = SampCfg.Load();
                sc.Set("fpslimit", Math.Min(segFps.Value, FpsPack.SampMax).ToString());
                sc.Set("pagesize", segChat.Value.ToString());
                sc.Set("timestamp", tStamp.Checked ? "1" : "0");
                sc.Set("multicore", tMulti.Checked ? "1" : "0");
                sc.Save();
                Samp.SetValue("BastionCheckMods", tMods.Checked ? "1" : "0");
                Samp.SetValue("BastionCloseOnPlay", tClose.Checked ? "1" : "0");
                Samp.SetValue("BastionFps", segFps.Value.ToString());
                Samp.SetValue("BastionFpsMax", "1");
                string exe = Path.Combine(gtaPath.Text.Trim(), "gta_sa.exe");
                if (gtaPath.Text.Trim().Length > 0 && File.Exists(exe)) Samp.SetValue("gta_sa_exe", exe);
                if (showStatus) SetStatus("Настройки сохранены.");
                return true;
            }
            catch (Exception ex)
            {
                Warn("Не удалось сохранить настройки: " + ex.Message);
                return false;
            }
        }

        void ToggleSettings()
        {
            settings.Visible = !settings.Visible;
            Invalidate();
        }

        // ---------------- опрос серверов ----------------
        void RefreshAll()
        {
            if (Interlocked.CompareExchange(ref querying, 1, 0) != 0) return;
            Thread th = new Thread(delegate ()
            {
                foreach (ServerEntry s in cfg.Servers) SampQuery.Query(s);
                try
                {
                    BeginInvoke(new MethodInvoker(delegate
                    {
                        foreach (ServerCard c in cards) { c.Queried = true; c.Invalidate(); }
                    }));
                }
                catch (Exception) { }
                Interlocked.Exchange(ref querying, 0);
            });
            th.IsBackground = true;
            th.Start();
        }

        // ---------------- действия ----------------
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
                    SetStatus("Моды перенесены в _off: " + n);
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
            if (selected == null) { Warn("Выбери сервер."); return; }
            ServerEntry s = selected.Server;

            string name = nick.Text.Trim();
            if (name.Length < 3 || name.Length > 24 || !Regex.IsMatch(name, @"^[A-Za-z0-9_\[\]\.\$@=\(\)]+$"))
            {
                Warn("Ник: от 3 до 24 символов, только латиница, цифры и _.");
                nick.Focus();
                return;
            }
            if (cfg.RequireRpName && !Regex.IsMatch(name, @"^[A-Z][a-z]+_[A-Z][a-z]+$"))
            {
                Warn("На сервере нужен РП-ник вида Имя_Фамилия, например Ivan_Petrov.");
                nick.Focus();
                return;
            }

            if (installing) return;
            string gta = gtaPath.Text.Trim();
            string gtaExe = Path.Combine(gta, "gta_sa.exe");
            string sampExe = Path.Combine(gta, "samp.exe");
            if ((gta.Length == 0 || !File.Exists(gtaExe)) && cfg.EffectiveGameUrl.Length > 0)
            {
                InstallGame();
                return;
            }
            if (gta.Length == 0 || !File.Exists(gtaExe))
            {
                Warn("Не нашёл игру. Открой «Настройки» и выбери папку, где лежит gta_sa.exe.");
                if (!settings.Visible) ToggleSettings();
                return;
            }
            if (!File.Exists(sampExe))
            {
                Warn("В папке игры нет samp.exe. Установи клиент SA-MP 0.3.7 в эту папку.");
                return;
            }

            if (!packChecked && cfg.EffectiveClientPackUrl.Length > 0)
            {
                SyncPack(gta, false, true);
                return;
            }
            if (!uiChecked && UiPack.Effective(cfg.UiUrl).Length > 0)
            {
                SyncUi(gta);
                return;
            }

            if (tMods.Checked && !CheckMods(false)) return;
            SaveSettings(false);
            if (!ApplyFps(gta)) return;
            FixSkimmer(gta);
            try { LoadScreens.Apply(gta); } catch (Exception) { }   // экран загрузки по времени суток

            try
            {
                Samp.SetValue("PlayerName", name);
                Samp.SetValue("gta_sa_exe", gtaExe);
                Samp.SetValue("BastionLastServer", cards.IndexOf(selected).ToString());
            }
            catch (Exception ex)
            {
                Warn("Не удалось сохранить ник: " + ex.Message);
                return;
            }

            if (!s.Online)
            {
                DialogResult r = MessageBox.Show(this, "Сервер сейчас не отвечает. Всё равно запустить игру?",
                    cfg.Title, MessageBoxButtons.YesNo, MessageBoxIcon.Question);
                if (r != DialogResult.Yes) return;
            }

            try
            {
                ProcessStartInfo psi = new ProcessStartInfo(sampExe, s.Host + ":" + s.Port);
                psi.WorkingDirectory = gta;
                psi.UseShellExecute = true;
                Process.Start(psi);
                VideoModeAuto.Watch();   // окно выбора разрешения при первом входе — выберем сами
                SetStatus("Запускаю " + s.Name + "…");
                if (tClose.Checked) { System.Windows.Forms.Timer t = new System.Windows.Forms.Timer(); t.Interval = 1500; t.Tick += delegate { t.Stop(); Close(); }; t.Start(); }
            }
            catch (Exception ex)
            {
                Warn("Не удалось запустить игру: " + ex.Message);
            }
        }

        // ---------------- установка игры ----------------
        bool installing;

        bool GameFound()
        {
            string gta = gtaPath.Text.Trim();
            return gta.Length > 0 && File.Exists(Path.Combine(gta, "gta_sa.exe"));
        }

        // Пока игры нет, а архив известен — вместо ИГРАТЬ кнопка УСТАНОВИТЬ
        void UpdatePlayLabel()
        {
            if (installing) return;
            string t = !GameFound() && cfg.EffectiveGameUrl.Length > 0 ? "УСТАНОВИТЬ" : "ИГРАТЬ";
            if (play.Text != t) { play.Text = t; play.Invalidate(); }
        }

        void InstallGame()
        {
            // Прошлая установка оборвалась — предложить докачать в ту же папку
            string pending = Samp.GetValue("BastionInstallDir");
            if (pending.Length > 0 && GameInstaller.HasPartial(pending) && !File.Exists(Path.Combine(pending, "gta_sa.exe")))
            {
                DialogResult r = MessageBox.Show(this, "Загрузка игры в папку\r\n" + pending +
                    "\r\nне закончилась. Продолжить с места обрыва?\r\n\r\n«Нет» — выбрать другую папку.",
                    cfg.Title, MessageBoxButtons.YesNoCancel, MessageBoxIcon.Question);
                if (r == DialogResult.Cancel) return;
                if (r == DialogResult.Yes) { StartInstall(pending); return; }
            }

            string baseDir;
            using (FolderBrowserDialog d = new FolderBrowserDialog())
            {
                d.Description = "Куда установить игру? Внутри появится папка «Bastion Online». " +
                                "Не выбирай Program Files — там игре не хватит прав.";
                d.ShowNewFolderButton = true;
                string games = Path.Combine(Path.GetPathRoot(Environment.SystemDirectory), "Games");
                d.SelectedPath = Directory.Exists(games) ? games : Path.GetPathRoot(Environment.SystemDirectory);
                if (d.ShowDialog(this) != DialogResult.OK) return;
                baseDir = d.SelectedPath;
            }
            string dest = Path.Combine(baseDir, "Bastion Online");
            if (File.Exists(Path.Combine(baseDir, "gta_sa.exe"))) dest = baseDir; // выбрали уже готовую игру
            if (File.Exists(Path.Combine(dest, "gta_sa.exe")))
            {
                gtaPath.Text = dest;
                SaveSettings(false);
                SetStatus("Игра уже есть в этой папке.");
                return;
            }
            string pf = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles);
            string pf86 = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86);
            if ((pf.Length > 0 && dest.StartsWith(pf, StringComparison.OrdinalIgnoreCase)) ||
                (pf86.Length > 0 && dest.StartsWith(pf86, StringComparison.OrdinalIgnoreCase)))
            {
                Warn("В Program Files игре не хватит прав на запись. Выбери другую папку, например C:\\Games.");
                return;
            }

            StartInstall(dest);
        }

        // ---------------- обновление игры целиком (как на телефоне) ----------------
        // Пока нет client-pack.json, обновление = новый архив игры (другой MD5 в сборке).
        // MD5 установленного архива хранится в реестре (BastionGameMd5).
        void CheckGameUpdate()
        {
            if (installing || !GameFound() || cfg.EffectiveClientPackUrl.Length > 0) return;
            string md5 = cfg.EffectiveGameMd5.Trim().ToLowerInvariant();
            if (md5.Length == 0) return;
            string have = Samp.GetValue("BastionGameMd5");
            if (have.Length == 0)
            {
                // Игра поставлена до этой версии лаунчера — считаем её текущей
                try { Samp.SetValue("BastionGameMd5", md5); } catch (Exception) { }
                return;
            }
            if (have != md5) OfferReinstall(true);
        }

        void OfferReinstall(bool isUpdate)
        {
            if (cfg.EffectiveGameUrl.Length == 0)
            {
                SetStatus("Файлы игры проверять не с чем: в лаунчере нет архива игры.");
                return;
            }
            string q = isUpdate
                ? "Вышло обновление игры Bastion. Скачать и установить его сейчас?"
                : "Скачать игру с Яндекс Диска заново и переустановить её?";
            q += "\r\n\r\nПапка игры заменится целиком: моды и свои файлы в ней пропадут. " +
                 "Ник и настройки лаунчера сохранятся.";
            if (MessageBox.Show(this, q, cfg.Title, MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes)
            {
                if (isUpdate) SetStatus("Есть обновление игры: «Настройки» → «Проверить файлы».");
                return;
            }
            StartInstall(gtaPath.Text.Trim(), true);
        }

        void StartInstall(string dest) { StartInstall(dest, false); }

        void StartInstall(string dest, bool reinstall)
        {
            if (!reinstall) try { Samp.SetValue("BastionInstallDir", dest); } catch (Exception) { }
            string url = cfg.EffectiveGameUrl, md5 = cfg.EffectiveGameMd5;
            installing = true;
            play.Text = reinstall ? "ОБНОВЛЕНИЕ…" : "УСТАНОВКА…"; play.Enabled = false; play.Invalidate();
            Thread th = new Thread(delegate ()
            {
                string error = null;
                try
                {
                    Action<string> report = delegate (string msg)
                    {
                        try { BeginInvoke(new MethodInvoker(delegate { SetStatus(msg); })); } catch (Exception) { }
                    };
                    if (reinstall) GameInstaller.Reinstall(url, md5, dest, report);
                    else GameInstaller.Install(url, md5, dest, report);
                }
                catch (Exception ex) { error = ex.Message; }
                try
                {
                    BeginInvoke(new MethodInvoker(delegate
                    {
                        installing = false;
                        play.Enabled = true;
                        if (error == null)
                        {
                            gtaPath.Text = dest;
                            SaveSettings(false);
                            try { Samp.SetValue("BastionInstallDir", ""); } catch (Exception) { }
                            try { Samp.SetValue("BastionGameMd5", md5.Trim().ToLowerInvariant()); } catch (Exception) { }
                            packChecked = false;
                            SetStatus((reinstall ? "Игра обновлена: " : "Игра установлена: ") + dest);
                            // Сразу докачать свежие файлы Bastion (client-pack.json)
                            if (cfg.EffectiveClientPackUrl.Length > 0)
                            {
                                UpdatePlayLabel();
                                SyncPack(dest, false, false);
                                return;
                            }
                        }
                        else
                        {
                            SetStatus(reinstall ? "Обновление не удалось." : "Установка не удалась.");
                            Warn((reinstall ? "Не получилось обновить игру: " : "Не получилось установить игру: ") + error);
                        }
                        UpdatePlayLabel();
                    }));
                }
                catch (Exception) { }
            });
            th.IsBackground = true;
            th.Start();
        }

        // ---------------- клиентский пакет (ClientPack.cs) ----------------
        bool packChecked;

        // Проверить файлы игры и скачать изменившиеся. full — пересчитать всё заново.
        // thenPlay — после проверки продолжить запуск игры.
        void SyncPack(string gta, bool full, bool thenPlay)
        {
            string url = cfg.EffectiveClientPackUrl;
            if (installing) return;
            if (url.Length == 0) { if (full) SetStatus("Пакет файлов Bastion не настроен."); return; }
            installing = true;
            string label = play.Text;
            play.Text = "ПРОВЕРКА…"; play.Enabled = false; play.Invalidate();
            Thread th = new Thread(delegate ()
            {
                string error = null, info = null;
                bool offline = false;
                try
                {
                    PackManifest m;
                    try { m = ClientPack.Fetch(url); }
                    catch (System.Net.WebException) { offline = true; m = null; }
                    if (m != null)
                    {
                        int n = ClientPack.Sync(gta, m, full, delegate (string msg)
                        {
                            try { BeginInvoke(new MethodInvoker(delegate { SetStatus(msg); })); } catch (Exception) { }
                        });
                        info = n > 0 ? "Файлы игры обновлены (" + n + "), версия пакета " + m.Version + "."
                                     : "Файлы игры в порядке, версия пакета " + m.Version + ".";
                    }
                }
                catch (Exception ex) { error = ex.Message; }
                try
                {
                    BeginInvoke(new MethodInvoker(delegate
                    {
                        installing = false;
                        play.Enabled = true;
                        play.Text = label; play.Invalidate();
                        UpdatePlayLabel();
                        if (offline) SetStatus("Нет связи с сервером обновлений, файлы не проверены.");
                        else if (error == null) SetStatus(info);
                        if (error != null)
                        {
                            SetStatus("Файлы игры не обновились.");
                            string q = "Не получилось обновить файлы игры: " + error +
                                       "\r\nЕсли игра в Program Files — запусти лаунчер от имени администратора.";
                            if (!thenPlay) { Warn(q); return; }
                            DialogResult r = MessageBox.Show(this, q + "\r\n\r\nЗапустить игру без обновления?",
                                cfg.Title, MessageBoxButtons.YesNo, MessageBoxIcon.Warning);
                            if (r != DialogResult.Yes) return;
                        }
                        packChecked = true;
                        if (thenPlay) Play();
                    }));
                }
                catch (Exception) { }
            });
            th.IsBackground = true;
            th.Start();
        }

        // ---------------- красивые окна на ПК (UiPack.cs) ----------------
        bool uiChecked;

        // Поставить или обновить bastion_ui и продолжить запуск. Ошибка запуск не останавливает.
        void SyncUi(string gta)
        {
            if (installing) return;
            installing = true;
            string label = play.Text;
            play.Text = "ПРОВЕРКА…"; play.Enabled = false; play.Invalidate();
            string url = UiPack.Effective(cfg.UiUrl);
            Thread th = new Thread(delegate ()
            {
                string error = null, info = null;
                try
                {
                    info = UiPack.Sync(gta, url, delegate (string msg)
                    {
                        try { BeginInvoke(new MethodInvoker(delegate { SetStatus(msg); })); } catch (Exception) { }
                    });
                }
                catch (Exception ex) { error = ex.Message; }
                try
                {
                    BeginInvoke(new MethodInvoker(delegate
                    {
                        installing = false;
                        play.Enabled = true;
                        play.Text = label; play.Invalidate();
                        UpdatePlayLabel();
                        uiChecked = true;
                        if (error != null) SetStatus("Окна Bastion не обновились (" + error + "), будут обычные.");
                        else if (info != null) SetStatus(info);
                        Play();
                    }));
                }
                catch (Exception) { }
            });
            th.IsBackground = true;
            th.Start();
        }

        void Warn(string msg)
        {
            MessageBox.Show(this, msg, cfg.Title, MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }
    }

    // -----------------------------------------------------------------
    //  Окно «Доступно обновление» в стиле лаунчера
    // -----------------------------------------------------------------
    class UpdateDialog : Form
    {
        public UpdateDialog(string message)
        {
            FormBorderStyle = FormBorderStyle.None;
            StartPosition = FormStartPosition.CenterParent;
            ShowInTaskbar = false;
            BackColor = Theme.Card;
            ClientSize = new Size(440, 250);
            Text = "Доступно обновление";

            Label title = new Label();
            title.Text = "ДОСТУПНО ОБНОВЛЕНИЕ";
            title.Font = Theme.Hv(14f);
            title.ForeColor = Theme.Text;
            title.BackColor = Theme.Card;
            title.SetBounds(24, 20, 392, 30);
            Controls.Add(title);

            Panel bar = new Panel();
            bar.BackColor = Theme.Orange;
            bar.SetBounds(24, 54, 40, 2);
            Controls.Add(bar);

            Label msg = new Label();
            msg.Text = message;
            msg.Font = Theme.F(9.5f, FontStyle.Regular);
            msg.ForeColor = Theme.Dim;
            msg.BackColor = Theme.Card;
            msg.SetBounds(24, 68, 392, 112);
            Controls.Add(msg);

            FlatBtn later = new FlatBtn();
            later.Text = "ПОЗЖЕ";
            later.SetBounds(196, 194, 100, 36);
            later.Click += delegate { DialogResult = DialogResult.Cancel; Close(); };
            Controls.Add(later);

            FlatBtn install = new FlatBtn();
            install.Primary = true;
            install.Text = "УСТАНОВИТЬ";
            install.SetBounds(306, 194, 110, 36);
            install.Click += delegate { DialogResult = DialogResult.OK; Close(); };
            Controls.Add(install);

            AcceptButton = null;
            KeyPreview = true;
            KeyDown += delegate (object s, KeyEventArgs e)
            {
                if (e.KeyCode == Keys.Escape) { DialogResult = DialogResult.Cancel; Close(); }
                else if (e.KeyCode == Keys.Enter) { DialogResult = DialogResult.OK; Close(); }
            };
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);
            using (Pen p = new Pen(Theme.Orange)) e.Graphics.DrawRectangle(p, 0, 0, Width - 1, Height - 1);
        }
    }

    static class Program
    {
        [STAThread]
        static void Main()
        {
            // Длинные пути (> 260 знаков) при распаковке игры: .NET 4.6.2+ по умолчанию
            // их блокирует для программ, собранных под 4.0. Включаем до любой работы с файлами.
            try
            {
                Type ac = Type.GetType("System.AppContext");
                if (ac != null)
                {
                    System.Reflection.MethodInfo set = ac.GetMethod("SetSwitch");
                    set.Invoke(null, new object[] { "Switch.System.IO.UseLegacyPathHandling", false });
                    set.Invoke(null, new object[] { "Switch.System.IO.BlockLongPaths", false });
                }
            }
            catch (Exception) { }
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Updater.CleanOld();     // следы прошлого обновления
            try
            {
                Application.Run(new MainForm());
            }
            catch (Exception ex)
            {
                MessageBox.Show("Ошибка лаунчера:\r\n" + ex, "Bastion Online", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
    }
}

// =====================================================================
//  Вкладка «Логи»: просмотр журнала событий мода (scriptfiles\logs.db,
//  таблица logs — её пишет bastion/logs.inc). Фильтр по нику (или IP),
//  виду события, периоду и тексту. Двойной клик по строке — все логи
//  этого игрока.
//
//  SQLite читается через sqlite3.dll: она вшита в exe и при первом
//  запуске распаковывается в %LOCALAPPDATA%\BastionServer.
// =====================================================================
using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;

namespace BastionServer
{
    // -----------------------------------------------------------------
    //  Минимальная обёртка над sqlite3.dll (только чтение)
    // -----------------------------------------------------------------
    static class Sqlite
    {
        const string Dll = "sqlite3";
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        static extern int sqlite3_open_v2(byte[] filename, out IntPtr db, int flags, IntPtr vfs);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        static extern int sqlite3_close_v2(IntPtr db);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        static extern int sqlite3_busy_timeout(IntPtr db, int ms);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        static extern int sqlite3_prepare_v2(IntPtr db, byte[] sql, int nbytes, out IntPtr stmt, IntPtr tail);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        static extern int sqlite3_bind_text(IntPtr stmt, int index, byte[] value, int nbytes, IntPtr destructor);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        static extern int sqlite3_bind_int64(IntPtr stmt, int index, long value);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        static extern int sqlite3_step(IntPtr stmt);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        static extern int sqlite3_finalize(IntPtr stmt);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        static extern long sqlite3_column_int64(IntPtr stmt, int col);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        static extern IntPtr sqlite3_column_blob(IntPtr stmt, int col);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        static extern int sqlite3_column_bytes(IntPtr stmt, int col);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        static extern IntPtr sqlite3_errmsg(IntPtr db);
        [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Unicode)]
        static extern IntPtr LoadLibrary(string path);
        [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Unicode)]
        static extern bool SetDllDirectory(string path);

        static bool loaded;

        // Распаковать вшитую sqlite3.dll и загрузить её до первого вызова
        public static void Ensure()
        {
            if (loaded) return;
            loaded = true;
            if (Environment.OSVersion.Platform != PlatformID.Win32NT) return;
            // рядом с exe лежит своя — Windows возьмёт её сама
            if (File.Exists(Path.Combine(Program.ExeDir, "sqlite3.dll"))) return;
            Stream s = Assembly.GetExecutingAssembly().GetManifestResourceStream("sqlite3.dll");
            if (s == null) return;
            string dir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "BastionServer");
            Directory.CreateDirectory(dir);
            // Имя файла обязательно sqlite3.dll: DllImport("sqlite3") ищет модуль
            // именно с таким именем, а под другим именем (sqlite3-NNN.dll) уже
            // загруженную библиотеку не узнаёт. Версию различаем папкой.
            dir = Path.Combine(dir, "sqlite3-" + s.Length);
            Directory.CreateDirectory(dir);
            string path = Path.Combine(dir, "sqlite3.dll");
            if (!File.Exists(path) || new FileInfo(path).Length != s.Length)
            {
                string tmp = path + ".tmp";
                using (FileStream o = File.Create(tmp)) s.CopyTo(o);
                if (File.Exists(path)) File.Delete(path);
                File.Move(tmp, path);
            }
            s.Dispose();
            SetDllDirectory(dir);
            if (LoadLibrary(path) == IntPtr.Zero)
                throw new Exception("не загрузилась sqlite3.dll (" + Marshal.GetLastWin32Error() + ")");
        }

        static byte[] Z(string s) { byte[] b = Encoding.UTF8.GetBytes(s + "\0"); return b; }

        static string Err(IntPtr db)
        {
            IntPtr p = sqlite3_errmsg(db);
            if (p == IntPtr.Zero) return "?";
            int n = 0;
            while (Marshal.ReadByte(p, n) != 0) n++;
            byte[] b = new byte[n];
            Marshal.Copy(p, b, 0, n);
            return Encoding.UTF8.GetString(b);
        }

        // Выполняет SELECT, параметры: string (как байты cp1251) или long
        public static List<object[]> Query(string file, string sql, object[] args, int cols, Encoding enc)
        {
            Ensure();
            IntPtr db;
            // READWRITE без CREATE: база в режиме WAL, читателю нужен доступ к -shm
            int rc = sqlite3_open_v2(Z(file), out db, 0x2, IntPtr.Zero);
            if (rc != 0)
            {
                string e = db != IntPtr.Zero ? Err(db) : ("код " + rc);
                if (db != IntPtr.Zero) sqlite3_close_v2(db);
                throw new Exception(e);
            }
            List<object[]> rows = new List<object[]>();
            IntPtr st = IntPtr.Zero;
            try
            {
                sqlite3_busy_timeout(db, 3000);
                byte[] q = Encoding.UTF8.GetBytes(sql);
                if (sqlite3_prepare_v2(db, q, q.Length, out st, IntPtr.Zero) != 0) throw new Exception(Err(db));
                for (int i = 0; i < args.Length; i++)
                {
                    if (args[i] is string)
                    {
                        byte[] b = enc.GetBytes((string)args[i]);
                        sqlite3_bind_text(st, i + 1, b, b.Length, new IntPtr(-1)); // SQLITE_TRANSIENT
                    }
                    else sqlite3_bind_int64(st, i + 1, Convert.ToInt64(args[i]));
                }
                while (true)
                {
                    rc = sqlite3_step(st);
                    if (rc == 101) break;          // DONE
                    if (rc != 100) throw new Exception(Err(db)); // не ROW
                    object[] r = new object[cols];
                    for (int c = 0; c < cols; c++)
                    {
                        int n = sqlite3_column_bytes(st, c);
                        IntPtr p = sqlite3_column_blob(st, c);
                        if (p == IntPtr.Zero || n == 0) { r[c] = ""; continue; }
                        byte[] b = new byte[n];
                        Marshal.Copy(p, b, 0, n);
                        r[c] = enc.GetString(b);
                    }
                    rows.Add(r);
                }
            }
            finally
            {
                if (st != IntPtr.Zero) sqlite3_finalize(st);
                sqlite3_close_v2(db);
            }
            return rows;
        }
    }

    // -----------------------------------------------------------------
    //  Страница «Логи»
    // -----------------------------------------------------------------
    class LogsPage : Panel
    {
        static readonly string[] TypeKeys = { "", "money", "bank", "kill", "dmg", "admin", "conn", "chat", "cmd", "item", "veh", "faction", "family", "ac" };
        static readonly string[] Titles = { "Все", "Деньги", "Банк", "Убийства", "Урон", "Админы", "Вход / выход", "Чат", "Команды", "Предметы", "Транспорт", "Фракции", "Семьи", "Античит" };
        static readonly string[] Periods = { "За час", "За сутки", "За неделю", "За месяц", "За всё время" };
        static readonly int[] PeriodSec = { 3600, 86400, 7 * 86400, 30 * 86400, 0 };
        const int Limit = 2000;

        readonly Func<string> serverDir;
        readonly Encoding enc = Encoding.GetEncoding(1251);
        TextBox fNick, fText;
        ComboBox fType, fPeriod;
        ListView list;
        Label info;

        public LogsPage(Func<string> serverDir)
        {
            this.serverDir = serverDir;
            Dock = DockStyle.Fill;
            BackColor = T.Bg;
            Padding = new Padding(16);

            FlowLayoutPanel bar = new FlowLayoutPanel();
            bar.Dock = DockStyle.Top;
            bar.Height = 44;
            bar.WrapContents = false;
            bar.BackColor = T.Bg;

            fNick = Box(140);
            fType = Combo(Titles, 125);
            fPeriod = Combo(Periods, 115);
            fPeriod.SelectedIndex = 1;
            fText = Box(130);
            Btn find = new Btn("НАЙТИ", 90); find.Primary = true;
            find.Margin = new Padding(6, 0, 0, 0);
            find.Click += delegate { Search(); };
            Btn reset = new Btn("Сброс", 70);
            reset.Margin = new Padding(6, 0, 0, 0);
            reset.Click += delegate { fNick.Text = ""; fText.Text = ""; fType.SelectedIndex = 0; fPeriod.SelectedIndex = 1; Search(); };
            Btn csv = new Btn("В файл", 80);
            csv.Margin = new Padding(6, 0, 0, 0);
            csv.Click += delegate { Export(); };

            bar.Controls.Add(Cap("Ник / IP"));
            bar.Controls.Add(fNick);
            bar.Controls.Add(Cap("Вид"));
            bar.Controls.Add(fType);
            bar.Controls.Add(Cap("Период"));
            bar.Controls.Add(fPeriod);
            bar.Controls.Add(Cap("Текст"));
            bar.Controls.Add(fText);
            bar.Controls.Add(find);
            bar.Controls.Add(reset);
            bar.Controls.Add(csv);
            foreach (Control c in new Control[] { fNick, fText })
                c.KeyDown += delegate(object s, KeyEventArgs e) { if (e.KeyCode == Keys.Enter) { e.SuppressKeyPress = true; Search(); } };
            fType.SelectedIndexChanged += delegate { if (Visible) Search(); };
            fPeriod.SelectedIndexChanged += delegate { if (Visible) Search(); };

            info = new Label();
            info.Dock = DockStyle.Bottom;
            info.Height = 28;
            info.ForeColor = T.Dim;
            info.TextAlign = ContentAlignment.MiddleLeft;
            info.Text = "Двойной клик по строке — все логи этого игрока. Ник можно писать частью: Ivan*";

            list = new ListView();
            list.Dock = DockStyle.Fill;
            list.View = View.Details;
            list.FullRowSelect = true;
            list.HideSelection = false;
            list.BackColor = Color.FromArgb(8, 9, 11);
            list.ForeColor = T.Text;
            list.BorderStyle = BorderStyle.None;
            list.Font = T.F(9.5f, FontStyle.Regular);
            list.OwnerDraw = true;
            list.DrawColumnHeader += DrawHeader;
            list.DrawItem += delegate(object s, DrawListViewItemEventArgs e) { e.DrawDefault = true; };
            list.DrawSubItem += delegate(object s, DrawListViewSubItemEventArgs e) { e.DrawDefault = true; };
            list.Columns.Add("Время", 130);
            list.Columns.Add("Вид", 95);
            list.Columns.Add("Ник", 150);
            list.Columns.Add("Кому / кого", 150);
            list.Columns.Add("Сумма", 80, HorizontalAlignment.Right);
            list.Columns.Add("IP", 110);
            list.Columns.Add("Что", 520);
            list.DoubleClick += delegate { DrillDown(); };

            Panel gap = new Panel();
            gap.Dock = DockStyle.Top;
            gap.Height = 8;

            Controls.Add(list);
            Controls.Add(gap);
            Controls.Add(info);
            Controls.Add(bar);
        }

        Label Cap(string t)
        {
            Label l = new Label();
            l.Text = t;
            l.ForeColor = T.Dim;
            l.AutoSize = true;
            l.Margin = new Padding(8, 9, 2, 0);
            return l;
        }

        TextBox Box(int w)
        {
            TextBox t = new TextBox();
            t.Width = w;
            t.BackColor = T.Card;
            t.ForeColor = T.Text;
            t.BorderStyle = BorderStyle.FixedSingle;
            t.Font = T.F(10f, FontStyle.Regular);
            t.Margin = new Padding(0, 5, 0, 0);
            return t;
        }

        ComboBox Combo(string[] items, int w)
        {
            ComboBox c = new ComboBox();
            c.DropDownStyle = ComboBoxStyle.DropDownList;
            c.FlatStyle = FlatStyle.Flat;
            c.BackColor = T.Card;
            c.ForeColor = T.Text;
            c.Width = w;
            c.Font = T.F(10f, FontStyle.Regular);
            c.Margin = new Padding(0, 5, 0, 0);
            c.Items.AddRange(items);
            c.SelectedIndex = 0;
            return c;
        }

        void DrawHeader(object sender, DrawListViewColumnHeaderEventArgs e)
        {
            using (SolidBrush b = new SolidBrush(T.Bar)) e.Graphics.FillRectangle(b, e.Bounds);
            using (Pen p = new Pen(T.Line)) e.Graphics.DrawLine(p, e.Bounds.Left, e.Bounds.Bottom - 1, e.Bounds.Right, e.Bounds.Bottom - 1);
            TextRenderer.DrawText(e.Graphics, e.Header.Text.ToUpperInvariant(), T.F(8.5f, FontStyle.Bold),
                new Rectangle(e.Bounds.X + 6, e.Bounds.Y, e.Bounds.Width - 8, e.Bounds.Height), T.Dim,
                TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis |
                (e.Header.TextAlign == HorizontalAlignment.Right ? TextFormatFlags.Right : TextFormatFlags.Left));
        }

        string DbPath { get { return Path.Combine(serverDir(), "scriptfiles", "logs.db"); } }

        static string TypeTitle(string key)
        {
            int i = Array.IndexOf(TypeKeys, key);
            return i > 0 ? Titles[i] : key;
        }

        Color RowColor(string type, long amount)
        {
            switch (type)
            {
                case "kill": return T.Red;
                case "dmg": return Color.FromArgb(220, 140, 120);
                case "admin": return T.OrangeHot;
                case "ac": return T.Yellow;
                case "money":
                case "bank": return amount < 0 ? Color.FromArgb(230, 120, 110) : T.Green;
                case "conn": return Color.FromArgb(140, 190, 255);
                case "chat":
                case "cmd": return Color.FromArgb(190, 193, 202);
            }
            return T.Text;
        }

        bool searched;
        protected override void OnVisibleChanged(EventArgs e)
        {
            base.OnVisibleChanged(e);
            if (Visible && !searched) Search();
        }

        public void ShowPlayer(string nick)
        {
            fNick.Text = nick;
            fType.SelectedIndex = 0;
            fPeriod.SelectedIndex = 4;
            Search();
        }

        void DrillDown()
        {
            if (list.SelectedItems.Count == 0) return;
            ListViewItem it = list.SelectedItems[0];
            string nick = it.SubItems[2].Text;
            if (nick.Length == 0) nick = it.SubItems[3].Text;
            if (nick.Length > 0) ShowPlayer(nick);
        }

        void Search()
        {
            searched = true;
            string db = DbPath;
            if (!File.Exists(db))
            {
                list.Items.Clear();
                info.Text = "Нет файла scriptfiles\\logs.db — он появится после первого запуска сервера со свежим модом.";
                return;
            }
            StringBuilder where = new StringBuilder("1=1");
            List<object> args = new List<object>();
            string nick = fNick.Text.Trim();
            if (nick.Length > 0)
            {
                if (System.Text.RegularExpressions.Regex.IsMatch(nick, @"^\d{1,3}(\.\d{1,3}){1,3}\.?\*?$"))
                {
                    where.Append(" AND ip LIKE ?");
                    args.Add(nick.TrimEnd('*') + "%");
                }
                else if (nick.Contains("*"))
                {
                    where.Append(" AND (name LIKE ? OR tname LIKE ?)");
                    string like = nick.Replace("%", "").Replace("_", "\\_").Replace('*', '%');
                    where.Replace("LIKE ?", "LIKE ? ESCAPE '\\'");
                    args.Add(like); args.Add(like);
                }
                else
                {
                    where.Append(" AND (name = ? COLLATE NOCASE OR tname = ? COLLATE NOCASE)");
                    args.Add(nick); args.Add(nick);
                }
            }
            string type = TypeKeys[Math.Max(0, fType.SelectedIndex)];
            if (type.Length > 0) { where.Append(" AND type = ?"); args.Add(type); }
            int per = PeriodSec[Math.Max(0, fPeriod.SelectedIndex)];
            if (per > 0)
            {
                where.Append(" AND time >= ?");
                args.Add((long)(DateTime.UtcNow - new DateTime(1970, 1, 1)).TotalSeconds - per);
            }
            string text = fText.Text.Trim();
            if (text.Length > 0) { where.Append(" AND text LIKE ?"); args.Add("%" + text + "%"); }

            string sql = "SELECT time, type, name, tname, amount, ip, text FROM logs WHERE " + where +
                         " ORDER BY id DESC LIMIT " + Limit;
            List<object[]> rows;
            Cursor = Cursors.WaitCursor;
            try { rows = Sqlite.Query(db, sql, args.ToArray(), 7, enc); }
            catch (Exception ex)
            {
                Cursor = Cursors.Default;
                info.Text = "Логи не читаются: " + ex.Message;
                return;
            }
            Cursor = Cursors.Default;

            long plus = 0, minus = 0;
            DateTime epoch = new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Utc);
            list.BeginUpdate();
            list.Items.Clear();
            foreach (object[] r in rows)
            {
                long t, amount;
                long.TryParse((string)r[0], out t);
                long.TryParse((string)r[4], out amount);
                string k = (string)r[1];
                if (k == "money" || k == "bank") { if (amount > 0) plus += amount; else minus += amount; }
                bool showAmount = k == "money" || k == "bank" || k == "admin" || k == "item" || k == "dmg";
                ListViewItem it = new ListViewItem(new string[] {
                    epoch.AddSeconds(t).ToLocalTime().ToString("dd.MM.yy HH:mm:ss"),
                    TypeTitle(k), (string)r[2], (string)r[3],
                    showAmount && amount != 0 ? amount.ToString("N0") : "",
                    (string)r[5], (string)r[6] });
                it.ForeColor = RowColor(k, amount);
                list.Items.Add(it);
            }
            list.EndUpdate();
            string s = "Найдено " + rows.Count + (rows.Count >= Limit ? "+ (показаны последние " + Limit + ", сузь поиск)" : "");
            if (plus != 0 || minus != 0) s += "   ·   деньги: +" + plus.ToString("N0") + " / " + minus.ToString("N0");
            s += "   ·   двойной клик — все логи игрока";
            info.Text = s;
        }

        void Export()
        {
            if (list.Items.Count == 0) return;
            using (SaveFileDialog d = new SaveFileDialog())
            {
                d.Filter = "Таблица CSV (*.csv)|*.csv";
                d.FileName = "logs-" + (fNick.Text.Trim().Length > 0 ? fNick.Text.Trim().Replace("*", "") + "-" : "") + DateTime.Now.ToString("yyyyMMdd-HHmm") + ".csv";
                if (d.ShowDialog(this) != DialogResult.OK) return;
                StringBuilder sb = new StringBuilder();
                List<string> head = new List<string>();
                foreach (ColumnHeader c in list.Columns) head.Add(c.Text);
                sb.Append(string.Join(";", head.ToArray())).Append("\r\n");
                foreach (ListViewItem it in list.Items)
                {
                    List<string> cells = new List<string>();
                    foreach (ListViewItem.ListViewSubItem si in it.SubItems) cells.Add("\"" + si.Text.Replace("\"", "\"\"") + "\"");
                    sb.Append(string.Join(";", cells.ToArray())).Append("\r\n");
                }
                // UTF-8 с BOM — Excel откроет кириллицу правильно
                File.WriteAllText(d.FileName, sb.ToString(), new UTF8Encoding(true));
            }
        }
    }
}

// =====================================================================
//  Telegram-бот стартера: управление сервером с телефона.
//
//  Работает через long polling (стартер сам ходит в api.telegram.org),
//  поэтому не нужно открывать порты ни дома, ни на арендованном сервере.
//  Токен бота и ID владельцев хранятся только в BastionServer.ini.
//  Команды принимаются только от ID из списка «tg_admins».
//
//  Код совместим с компилятором C# 5 (.NET Framework 4 csc.exe).
// =====================================================================
using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Net;
using System.Text;
using System.Threading;
using System.Web.Script.Serialization;

namespace BastionServer
{
    class TgClient : WebClient
    {
        public int TimeoutMs = 30000;
        protected override WebRequest GetWebRequest(Uri address)
        {
            WebRequest r = base.GetWebRequest(address);
            if (r != null) r.Timeout = TimeoutMs;
            HttpWebRequest h = r as HttpWebRequest;
            if (h != null) h.ReadWriteTimeout = TimeoutMs;
            return r;
        }
    }

    class TgBot
    {
        // Что бот просит у стартера (всё вызывается в потоке окна)
        public Func<string> GetStatus;          // текст статуса без онлайна
        public Func<int> GetPort;
        public Func<string> GetLogPath;
        public Func<bool> IsRunning;
        public Func<string, long, string> DoAction; // stop / restart / run / update / backup
        public Func<string, string> SendConsole;
        public Action<string> OnState;          // строка состояния для вкладки «Настройки»
        public Func<Func<object>, object> Ui;    // выполнить в потоке окна

        readonly string token, api;
        readonly HashSet<long> admins;
        volatile bool stopped;
        long offset;
        long startedUnix;
        static readonly JavaScriptSerializer js = new JavaScriptSerializer();

        public TgBot(string token, IEnumerable<long> admins, string api)
        {
            this.token = token;
            this.admins = new HashSet<long>(admins);
            this.api = (api ?? "").TrimEnd('/');
            if (this.api.Length == 0) this.api = "https://api.telegram.org";
        }

        public static List<long> ParseIds(string s)
        {
            List<long> r = new List<long>();
            foreach (string p in (s ?? "").Split(new char[] { ',', ';', ' ' }, StringSplitOptions.RemoveEmptyEntries))
            {
                long id;
                if (long.TryParse(p.Trim(), NumberStyles.Integer, CultureInfo.InvariantCulture, out id)) r.Add(id);
            }
            return r;
        }

        public void Start()
        {
            startedUnix = (long)(DateTime.UtcNow - new DateTime(1970, 1, 1)).TotalSeconds;
            Thread t = new Thread(Loop);
            t.IsBackground = true;
            t.Name = "Telegram";
            t.Start();
        }

        public void Stop() { stopped = true; }

        void State(string s) { try { if (OnState != null && !stopped) OnState(s); } catch (Exception) { } }

        // ---------------- HTTP ----------------
        object Call(string method, Dictionary<string, object> args, int timeoutMs)
        {
            ServicePointManager.SecurityProtocol = (SecurityProtocolType)3072; // TLS 1.2
            using (TgClient wc = new TgClient())
            {
                wc.TimeoutMs = timeoutMs;
                wc.Encoding = Encoding.UTF8;
                wc.Headers[HttpRequestHeader.ContentType] = "application/json; charset=utf-8";
                string body = js.Serialize(args ?? new Dictionary<string, object>());
                string resp;
                try { resp = wc.UploadString(api + "/bot" + token + "/" + method, "POST", body); }
                catch (WebException ex)
                {
                    HttpWebResponse hr = ex.Response as HttpWebResponse;
                    if (hr == null) throw;
                    int code = (int)hr.StatusCode;
                    string desc = "";
                    try
                    {
                        using (StreamReader sr = new StreamReader(hr.GetResponseStream(), Encoding.UTF8))
                        {
                            Dictionary<string, object> e = js.DeserializeObject(sr.ReadToEnd()) as Dictionary<string, object>;
                            if (e != null && e.ContainsKey("description")) desc = Convert.ToString(e["description"]);
                        }
                    }
                    catch (Exception) { }
                    throw new TgError(code, desc.Length > 0 ? desc : ex.Message);
                }
                Dictionary<string, object> d = js.DeserializeObject(resp) as Dictionary<string, object>;
                if (d == null) throw new Exception("Telegram вернул непонятный ответ");
                object res;
                d.TryGetValue("result", out res);
                return res;
            }
        }

        class TgError : Exception
        {
            public int Code;
            public TgError(int code, string msg) : base(msg) { Code = code; }
        }

        static Dictionary<string, object> D(object o) { return o as Dictionary<string, object>; }
        static object G(Dictionary<string, object> d, string k) { object v; return d != null && d.TryGetValue(k, out v) ? v : null; }
        static long L(object o) { try { return o == null ? 0 : Convert.ToInt64(o, CultureInfo.InvariantCulture); } catch (Exception) { return 0; } }

        // ---------------- главный цикл ----------------
        void Loop()
        {
            string me = "";
            while (!stopped)
            {
                try
                {
                    Dictionary<string, object> u = D(Call("getMe", null, 20000));
                    me = Convert.ToString(G(u, "username"));
                    break;
                }
                catch (TgError ex)
                {
                    if (ex.Code == 401 || ex.Code == 404) { State("неверный токен бота — проверь его у @BotFather"); return; }
                    State("ошибка Telegram: " + ex.Message + ", повторю через 30 с");
                }
                catch (Exception ex) { State("нет связи с Telegram (" + ex.Message + "), повторю через 30 с"); }
                for (int i = 0; i < 300 && !stopped; i++) Thread.Sleep(100);
            }
            if (stopped) return;
            SetCommands();
            State("подключён как @" + me + (admins.Count == 0 ? " · впиши свой ID (напиши боту /start)" : " · владельцев: " + admins.Count));
            if (admins.Count > 0)
                try { Notify("🟢 Стартер на связи. " + Ui1(GetStatus).Split('\n')[0]); } catch (Exception) { }

            int fails = 0;
            while (!stopped)
            {
                object res;
                try
                {
                    Dictionary<string, object> a = new Dictionary<string, object>();
                    a["offset"] = offset;
                    a["timeout"] = 50;
                    a["allowed_updates"] = new string[] { "message", "callback_query" };
                    res = Call("getUpdates", a, 75000);
                    if (fails > 0) State("подключён как @" + me + " · владельцев: " + admins.Count);
                    fails = 0;
                }
                catch (TgError ex)
                {
                    if (ex.Code == 401) { State("токен отозван — введи новый"); return; }
                    if (ex.Code == 409) State("этот бот уже запущен в другом стартере — у каждого сервера должен быть свой бот");
                    else State("ошибка Telegram: " + ex.Message);
                    fails++;
                    Pause(fails);
                    continue;
                }
                catch (Exception ex)
                {
                    fails++;
                    if (fails >= 2) State("нет связи с Telegram (" + ex.Message + "), переподключаюсь…");
                    Pause(fails);
                    continue;
                }
                if (stopped) return;
                IEnumerable list = res as IEnumerable;
                if (list == null) continue;
                foreach (object o in list)
                {
                    Dictionary<string, object> up = D(o);
                    if (up == null) continue;
                    offset = Math.Max(offset, L(G(up, "update_id")) + 1);
                    try { Handle(up); }
                    catch (Exception ex) { State("ошибка обработки команды: " + ex.Message); }
                }
            }
        }

        void Pause(int fails)
        {
            int s = Math.Min(60, 3 * fails);
            for (int i = 0; i < s * 10 && !stopped; i++) Thread.Sleep(100);
        }

        void SetCommands()
        {
            try
            {
                List<object> cmds = new List<object>();
                string[,] c = {
                    { "status", "Состояние и онлайн" }, { "players", "Список игроков" }, { "log", "Последние строки log.txt" },
                    { "run", "Запустить сервер" }, { "restart", "Перезапустить" }, { "stop", "Остановить" },
                    { "update", "Свежий мод с GitHub" }, { "backup", "Бэкап базы" }, { "cmd", "Консольная команда: /cmd текст" },
                    { "help", "Кнопки и помощь" } };
                for (int i = 0; i < c.GetLength(0); i++)
                {
                    Dictionary<string, object> x = new Dictionary<string, object>();
                    x["command"] = c[i, 0]; x["description"] = c[i, 1];
                    cmds.Add(x);
                }
                Dictionary<string, object> a = new Dictionary<string, object>();
                a["commands"] = cmds;
                Call("setMyCommands", a, 20000);
            }
            catch (Exception) { }
        }

        // ---------------- отправка ----------------
        public void Send(long chat, string html, object markup)
        {
            if (stopped) return;
            Dictionary<string, object> a = new Dictionary<string, object>();
            a["chat_id"] = chat;
            a["text"] = html.Length > 4000 ? html.Substring(0, 4000) : html;
            a["parse_mode"] = "HTML";
            a["disable_web_page_preview"] = true;
            if (markup != null) a["reply_markup"] = markup;
            try { Call("sendMessage", a, 20000); }
            catch (Exception) { }
        }

        public void SendAsync(long chat, string html)
        {
            if (stopped) return;
            ThreadPool.QueueUserWorkItem(delegate { Send(chat, html, null); });
        }

        // сообщение всем владельцам (падение сервера и т.п.)
        public void Notify(string html)
        {
            if (stopped) return;
            long[] ids = new long[admins.Count];
            admins.CopyTo(ids);
            ThreadPool.QueueUserWorkItem(delegate { foreach (long id in ids) Send(id, html, null); });
        }

        public static string Esc(string s)
        {
            return (s ?? "").Replace("&", "&amp;").Replace("<", "&lt;").Replace(">", "&gt;");
        }

        static object Keyboard()
        {
            Dictionary<string, object> k = new Dictionary<string, object>();
            k["keyboard"] = new object[] {
                new object[] { "📊 Статус", "👥 Игроки", "📜 Лог" },
                new object[] { "▶️ Старт", "🔄 Рестарт", "⏹ Стоп" },
                new object[] { "⬆️ Обновить мод", "💾 Бэкап базы" } };
            k["resize_keyboard"] = true;
            k["is_persistent"] = true;
            return k;
        }

        static object Confirm(string action, string label)
        {
            Dictionary<string, object> yes = new Dictionary<string, object>();
            yes["text"] = "✅ " + label; yes["callback_data"] = "do:" + action;
            Dictionary<string, object> no = new Dictionary<string, object>();
            no["text"] = "Отмена"; no["callback_data"] = "no";
            Dictionary<string, object> k = new Dictionary<string, object>();
            k["inline_keyboard"] = new object[] { new object[] { yes, no } };
            return k;
        }

        R Ui1<R>(Func<R> f) { return (R)Ui(delegate { return (object)f(); }); }

        // ---------------- входящие ----------------
        void Handle(Dictionary<string, object> up)
        {
            Dictionary<string, object> cb = D(G(up, "callback_query"));
            if (cb != null) { HandleCallback(cb); return; }

            Dictionary<string, object> msg = D(G(up, "message"));
            if (msg == null) return;
            long chat = L(G(D(G(msg, "chat")), "id"));
            long from = L(G(D(G(msg, "from")), "id"));
            string text = Convert.ToString(G(msg, "text") ?? "").Trim();
            if (chat == 0 || text.Length == 0) return;

            if (!admins.Contains(from) && !admins.Contains(chat))
            {
                Send(chat, "⛔ Нет доступа.\nТвой ID: <code>" + from + "</code>\n" +
                    "Если это твой сервер — впиши этот ID в стартере: «Настройки» → «Telegram-бот» → «ID владельцев».", null);
                return;
            }
            // команды, отправленные пока стартер был выключен, не выполняем
            if (L(G(msg, "date")) < startedUnix - 60)
            {
                Send(chat, "Пропустил старую команду «" + Esc(text) + "» — она пришла, пока стартер был выключен.", null);
                return;
            }
            Command(chat, text);
        }

        void HandleCallback(Dictionary<string, object> cb)
        {
            string id = Convert.ToString(G(cb, "id"));
            long from = L(G(D(G(cb, "from")), "id"));
            Dictionary<string, object> m = D(G(cb, "message"));
            long chat = L(G(D(G(m, "chat")), "id"));
            long mid = L(G(m, "message_id"));
            string data = Convert.ToString(G(cb, "data") ?? "");
            Dictionary<string, object> ans = new Dictionary<string, object>();
            ans["callback_query_id"] = id;
            bool ok = admins.Contains(from) || admins.Contains(chat);
            if (!ok) ans["text"] = "Нет доступа";
            try { Call("answerCallbackQuery", ans, 15000); } catch (Exception) { }
            if (!ok || chat == 0) return;

            string done = data == "no" ? "Отменено." : "Выполняю…";
            try
            {
                Dictionary<string, object> e = new Dictionary<string, object>();
                e["chat_id"] = chat; e["message_id"] = mid; e["text"] = done;
                Call("editMessageText", e, 15000);
            }
            catch (Exception) { }
            if (data.StartsWith("do:")) Act(chat, data.Substring(3));
        }

        void Act(long chat, string action)
        {
            string r = Ui1<string>(delegate { return DoAction(action, chat); });
            if (!string.IsNullOrEmpty(r)) Send(chat, r, null);
        }

        static string Norm(string s)
        {
            StringBuilder b = new StringBuilder();
            foreach (char c in s.ToLowerInvariant()) if (char.IsLetterOrDigit(c) || c == '/' || c == ' ') b.Append(c);
            string r = b.ToString().Trim();
            if (r.StartsWith("/"))
            {
                int sp = r.IndexOf(' ');
                string head = sp < 0 ? r : r.Substring(0, sp);
                int at = s.IndexOf('@');
                if (at > 0 && at < (sp < 0 ? s.Length : sp)) head = head.Substring(0, Math.Min(head.Length, at)); // /status@MyBot
                r = head.Substring(1);
            }
            return r.Replace('ё', 'е');
        }

        void Command(long chat, string text)
        {
            // консольная команда: /cmd текст  или  > текст
            string raw = null;
            if (text.StartsWith(">")) raw = text.Substring(1).Trim();
            else if (text.StartsWith("/cmd", StringComparison.OrdinalIgnoreCase))
            {
                int sp = text.IndexOf(' ');
                raw = sp < 0 ? "" : text.Substring(sp + 1).Trim();
                if (raw.Length == 0) { Send(chat, "Напиши так: <code>/cmd gmx</code> или <code>&gt; say Привет</code>", null); return; }
            }
            if (raw != null)
            {
                if (raw.Length == 0) return;
                Send(chat, Esc(Ui1<string>(delegate { return SendConsole(raw); })), null);
                return;
            }

            string k = Norm(text);
            string arg = "";
            int argSp = text.Trim().IndexOf(' ');
            if (argSp > 0) arg = text.Trim().Substring(argSp + 1).Trim();

            if (k == "start" || k == "help" || k == "меню" || k == "помощь")
                Send(chat, Help(), Keyboard());
            else if (k == "status" || k.StartsWith("статус"))
                Send(chat, Status(), null);
            else if (k == "players" || k.StartsWith("игрок") || k == "онлайн")
                Send(chat, Players(), null);
            else if (k == "log" || k.StartsWith("лог"))
            {
                int n; if (!int.TryParse(arg, out n)) n = 25;
                Send(chat, Log(Math.Max(5, Math.Min(100, n))), null);
            }
            else if (k == "restart" || k.StartsWith("рестарт") || k.StartsWith("перезапу"))
                Send(chat, Ui1(IsRunning) ? "Перезапустить сервер? Игроков выкинет на минуту." : "Сервер не запущен. Запустить?", Confirm("restart", "Перезапустить"));
            else if (k == "stop" || k.StartsWith("стоп") || k.StartsWith("останов"))
            {
                if (!Ui1(IsRunning)) Send(chat, "Сервер и так остановлен.", null);
                else Send(chat, "Остановить сервер? Игроки не смогут зайти, пока не нажмёшь «Старт».", Confirm("stop", "Остановить"));
            }
            else if (k == "run" || k.StartsWith("старт") || k.StartsWith("запуст"))
                Act(chat, "run");
            else if (k == "update" || k.StartsWith("обнов"))
                Send(chat, "Скачать свежий rp.amx с GitHub и поставить? Перед заменой стартер сохранит прошлый мод и базу.", Confirm("update", "Обновить"));
            else if (k == "backup" || k.StartsWith("бэкап") || k.StartsWith("бекап"))
                Act(chat, "backup");
            else
                Send(chat, "Не понял. Жми кнопки внизу или /help.", Keyboard());
        }

        static string Help()
        {
            return "<b>Bastion Server — пульт</b>\n" +
                "📊 Статус — работает ли сервер, онлайн, аптайм, версия мода\n" +
                "👥 Игроки — кто сейчас в игре\n" +
                "📜 Лог — последние строки log.txt (<code>/log 50</code> — 50 строк)\n" +
                "▶️ Старт · 🔄 Рестарт · ⏹ Стоп\n" +
                "⬆️ Обновить мод — свежий rp.amx с GitHub\n" +
                "💾 Бэкап базы — копия scriptfiles\\rp.db\n" +
                "<code>/cmd текст</code> — консольная команда серверу (например <code>/cmd say Рестарт через 5 минут</code>)\n\n" +
                "Если сервер упадёт, я сразу напишу.";
        }

        string Status()
        {
            string s = Ui1(GetStatus);
            if (Ui1(IsRunning))
            {
                QueryResult q = Query.Run(Ui1(GetPort));
                s += q.Ok ? "\n👥 Онлайн: <b>" + q.Players + " / " + q.MaxPlayers + "</b>" : "\n👥 Онлайн: сервер не отвечает на запрос (ещё грузится?)";
            }
            return s;
        }

        string Players()
        {
            if (!Ui1(IsRunning)) return "Сервер остановлен.";
            QueryResult q = Query.Run(Ui1(GetPort));
            if (!q.Ok) return "Сервер не отвечает на запрос.";
            if (q.Players == 0) return "Онлайн 0 / " + q.MaxPlayers + ". Никого нет.";
            StringBuilder b = new StringBuilder();
            b.Append("👥 <b>Онлайн " + q.Players + " / " + q.MaxPlayers + "</b>\n");
            foreach (string n in q.Names) b.Append(Esc(n)).Append('\n');
            if (q.Players >= 100) b.Append("(список виден до 100 игроков)");
            return b.ToString();
        }

        string Log(int lines)
        {
            string path = Ui1(GetLogPath);
            if (!File.Exists(path)) return "Лога пока нет: " + Esc(path);
            string text;
            using (FileStream fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete))
            {
                long from = Math.Max(0, fs.Length - 64000);
                fs.Position = from;
                byte[] buf = new byte[fs.Length - from];
                int n = fs.Read(buf, 0, buf.Length);
                text = Encoding.GetEncoding(1251).GetString(buf, 0, n);
            }
            List<string> all = new List<string>(text.Replace("\r", "").Split('\n'));
            if (all.Count > 0 && text.Length >= 64000) all.RemoveAt(0); // первая строка обрезана
            all.RemoveAll(delegate(string x) { return x.Trim().Length == 0; });
            int start = Math.Max(0, all.Count - lines);
            StringBuilder b = new StringBuilder();
            for (int i = all.Count - 1; i >= start; i--)
            {
                string l = all[i].Length > 300 ? all[i].Substring(0, 300) + "…" : all[i];
                string e = Esc(l) + "\n";
                if (b.Length + e.Length > 3700) break;
                b.Insert(0, e);
            }
            if (b.Length == 0) return "Лог пустой.";
            return "📜 <b>" + Esc(Path.GetFileName(path)) + "</b>\n<pre>" + b.ToString().TrimEnd() + "</pre>";
        }
    }
}

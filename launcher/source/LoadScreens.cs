// =====================================================================
//  Загрузочные экраны Bastion по времени суток.
//
//  5 сцен × день / закат / ночь вшиты в exe (loadscreens\*.jpg, /resource).
//  Время берём локальное у игрока: день 7–17, закат 17–20, ночь 20–7.
//  - Фон лаунчера: случайная сцена текущего времени суток.
//  - Экран загрузки игры: перед запуском в models\txd\loadscs.txd
//    текстуры loadsc0..loadsc14 заменяются нашими (DXT1 1024×512), игра
//    сама перемешивает их — сцена каждый раз случайная. Остальные текстуры
//    файла (заставки, логотипы) не трогаем. Оригинал один раз сохраняется
//    в _bastion_backup\models\txd\loadscs.txd, собранные файлы кэшируются
//    в _bastion_backup\loadscreens\<время>.txd.
// =====================================================================
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

namespace BastionLauncher
{
    static class LoadScreens
    {
        public static readonly string[] Scenes = { "pier", "chase", "grove", "lv", "vinewood" };
        const int Version = 1;               // поменять при замене картинок, чтобы кэш пересобрался
        const int TexW = 1024, TexH = 512;
        static readonly Random rnd = new Random();

        public static string Period(DateTime t)
        {
            int h = t.Hour;
            if (h >= 7 && h < 17) return "day";
            if (h >= 17 && h < 20) return "sunset";
            return "night";
        }

        public static Bitmap Load(string scene, string period)
        {
            try
            {
                using (Stream st = typeof(LoadScreens).Assembly.GetManifestResourceStream("ls_" + scene + "_" + period + ".jpg"))
                    if (st != null) using (Image tmp = Image.FromStream(st)) return new Bitmap(tmp);
            }
            catch (Exception) { }
            return null;
        }

        // Случайная сцена текущего времени суток (или null, если картинок в exe нет)
        public static Bitmap Pick()
        {
            return Load(Scenes[rnd.Next(Scenes.Length)], Period(DateTime.Now));
        }

        // ---------------- экран загрузки игры ----------------

        // Ставит в игру картинки текущего времени суток. Ошибки не мешают запуску.
        public static void Apply(string gta)
        {
            string target = Path.Combine(gta, @"models\txd\loadscs.txd");
            string backup = Path.Combine(gta, @"_bastion_backup\models\txd\loadscs.txd");
            if (!File.Exists(backup))
            {
                if (!File.Exists(target)) return;
                Directory.CreateDirectory(Path.GetDirectoryName(backup));
                File.Copy(target, backup);
            }

            string period = Period(DateTime.Now);
            byte[] orig = File.ReadAllBytes(backup);
            string cache = Path.Combine(gta, @"_bastion_backup\loadscreens\" + period + ".txd");
            string stamp = cache + ".ver";
            string want = Version + ":" + orig.Length;
            byte[] data = null;
            if (File.Exists(cache) && File.Exists(stamp) && File.ReadAllText(stamp) == want)
                data = File.ReadAllBytes(cache);
            if (data == null)
            {
                data = Build(orig, period);
                if (data == null) return;
                Directory.CreateDirectory(Path.GetDirectoryName(cache));
                File.WriteAllBytes(cache, data);
                File.WriteAllText(stamp, want);
            }
            if (File.Exists(target) && new FileInfo(target).Length == data.Length && Same(File.ReadAllBytes(target), data)) return;
            File.WriteAllBytes(target, data);
        }

        static bool Same(byte[] a, byte[] b)
        {
            if (a.Length != b.Length) return false;
            for (int i = 0; i < a.Length; i++) if (a[i] != b[i]) return false;
            return true;
        }

        // Пересобирает TXD: текстуры loadscN заменяются нашими по кругу 5 сцен.
        public static byte[] Build(byte[] b, string period)
        {
            List<byte[]> tex = new List<byte[]>();
            foreach (string s in Scenes)
            {
                using (Bitmap img = Load(s, period))
                {
                    if (img == null) return null;
                    tex.Add(Dxt1(img));
                }
            }

            MemoryStream outp = new MemoryStream();
            int replaced = 0;
            int off = 0;
            while (off + 12 <= b.Length)
            {
                int type = BitConverter.ToInt32(b, off), size = BitConverter.ToInt32(b, off + 4);
                uint ver = BitConverter.ToUInt32(b, off + 8);
                int s = off + 12, e = Math.Min(s + size, b.Length);
                if (type != 0x16) { outp.Write(b, off, e - off); off = e; continue; }

                MemoryStream body = new MemoryStream();
                int c = s;
                while (c + 12 <= e)
                {
                    int ct = BitConverter.ToInt32(b, c), cs = BitConverter.ToInt32(b, c + 4);
                    uint cver = BitConverter.ToUInt32(b, c + 8);
                    int ce = Math.Min(c + 12 + cs, e);
                    if (ct == 0x15 && c + 12 + 12 + 88 <= ce && BitConverter.ToInt32(b, c + 12) == 0x01)
                    {
                        int ss = c + 24;
                        uint filt = BitConverter.ToUInt32(b, ss + 4);
                        string name = Str(b, ss + 8);
                        string mask = Str(b, ss + 40);
                        int n;
                        if (name.StartsWith("loadsc", StringComparison.OrdinalIgnoreCase) &&
                            int.TryParse(name.Substring(6), out n) && n >= 0)
                        {
                            byte[] nat = Native(name, mask, filt, tex[n % tex.Count], cver);
                            body.Write(nat, 0, nat.Length);
                            replaced++;
                            c = ce;
                            continue;
                        }
                    }
                    body.Write(b, c, ce - c);
                    c = ce;
                }
                Chunk(outp, 0x16, body.ToArray(), ver);
                off = e;
            }
            return replaced > 0 ? outp.ToArray() : null;
        }

        static string Str(byte[] b, int o)
        {
            int n = 0;
            while (n < 32 && b[o + n] != 0) n++;
            return Encoding.ASCII.GetString(b, o, n);
        }

        static void Chunk(Stream s, int type, byte[] data, uint ver)
        {
            s.Write(BitConverter.GetBytes(type), 0, 4);
            s.Write(BitConverter.GetBytes(data.Length), 0, 4);
            s.Write(BitConverter.GetBytes(ver), 0, 4);
            s.Write(data, 0, data.Length);
        }

        // Текстура D3D9 (платформа 9): DXT1 без альфы, один уровень
        static byte[] Native(string name, string mask, uint filt, byte[] dxt, uint ver)
        {
            MemoryStream st = new MemoryStream();
            BinaryWriter w = new BinaryWriter(st);
            w.Write(9u); w.Write(filt);
            w.Write(Fixed(name)); w.Write(Fixed(mask));
            w.Write(0x200u);                         // FORMAT_565
            w.Write(0x31545844u);                    // 'DXT1'
            w.Write((ushort)TexW); w.Write((ushort)TexH);
            w.Write((byte)16); w.Write((byte)1); w.Write((byte)4); w.Write((byte)8); // depth, levels, rasterType, compressed
            w.Write(dxt.Length); w.Write(dxt);
            w.Flush();
            MemoryStream tex = new MemoryStream();
            Chunk(tex, 0x01, st.ToArray(), ver);
            Chunk(tex, 0x03, new byte[0], ver);
            MemoryStream res = new MemoryStream();
            Chunk(res, 0x15, tex.ToArray(), ver);
            return res.ToArray();
        }

        static byte[] Fixed(string s)
        {
            byte[] r = new byte[32];
            byte[] a = Encoding.ASCII.GetBytes(s);
            Array.Copy(a, r, Math.Min(a.Length, 31));
            return r;
        }

        // ---------------- DXT1 ----------------

        static byte[] Dxt1(Bitmap src)
        {
            byte[] px = new byte[TexW * TexH * 4];
            using (Bitmap bmp = new Bitmap(TexW, TexH, PixelFormat.Format32bppArgb))
            {
                using (Graphics g = Graphics.FromImage(bmp))
                {
                    g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                    g.PixelOffsetMode = PixelOffsetMode.HighQuality;
                    using (ImageAttributes ia = new ImageAttributes())
                    {
                        ia.SetWrapMode(WrapMode.TileFlipXY);   // без тёмной каймы по краям
                        g.DrawImage(src, new Rectangle(0, 0, TexW, TexH), 0, 0, src.Width, src.Height, GraphicsUnit.Pixel, ia);
                    }
                }
                BitmapData bd = bmp.LockBits(new Rectangle(0, 0, TexW, TexH), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
                for (int y = 0; y < TexH; y++)
                    Marshal.Copy(new IntPtr(bd.Scan0.ToInt64() + (long)y * bd.Stride), px, y * TexW * 4, TexW * 4);
                bmp.UnlockBits(bd);
            }

            byte[] outp = new byte[TexW * TexH / 2];
            int o = 0;
            int[] r = new int[16], gg = new int[16], bb = new int[16];
            for (int by = 0; by < TexH; by += 4)
                for (int bx = 0; bx < TexW; bx += 4)
                {
                    for (int i = 0; i < 16; i++)
                    {
                        int p = ((by + i / 4) * TexW + bx + i % 4) * 4;   // BGRA
                        bb[i] = px[p]; gg[i] = px[p + 1]; r[i] = px[p + 2];
                    }
                    Block(r, gg, bb, outp, o);
                    o += 8;
                }
            return outp;
        }

        // Концы — крайние точки блока по главной оси (по наибольшему разбросу канала)
        static void Block(int[] r, int[] g, int[] b, byte[] outp, int o)
        {
            double mr = 0, mg = 0, mb = 0;
            for (int i = 0; i < 16; i++) { mr += r[i]; mg += g[i]; mb += b[i]; }
            mr /= 16; mg /= 16; mb /= 16;
            // ковариация и степенной метод для главной оси
            double xx = 0, xy = 0, xz = 0, yy = 0, yz = 0, zz = 0;
            for (int i = 0; i < 16; i++)
            {
                double dr = r[i] - mr, dg = g[i] - mg, db = b[i] - mb;
                xx += dr * dr; xy += dr * dg; xz += dr * db; yy += dg * dg; yz += dg * db; zz += db * db;
            }
            double ax = 1, ay = 1, az = 1;
            for (int k = 0; k < 6; k++)
            {
                double nx = xx * ax + xy * ay + xz * az, ny = xy * ax + yy * ay + yz * az, nz = xz * ax + yz * ay + zz * az;
                double len = Math.Max(Math.Abs(nx), Math.Max(Math.Abs(ny), Math.Abs(nz)));
                if (len < 1e-9) { ax = ay = az = 0; break; }
                ax = nx / len; ay = ny / len; az = nz / len;
            }
            int lo = 0, hi = 0;
            double dmin = double.MaxValue, dmax = double.MinValue;
            for (int i = 0; i < 16; i++)
            {
                double d = (r[i] - mr) * ax + (g[i] - mg) * ay + (b[i] - mb) * az;
                if (d < dmin) { dmin = d; lo = i; }
                if (d > dmax) { dmax = d; hi = i; }
            }
            ushort c0 = To565(r[hi], g[hi], b[hi]), c1 = To565(r[lo], g[lo], b[lo]);
            if (c0 < c1) { ushort t = c0; c0 = c1; c1 = t; }
            uint bits = 0;
            if (c0 != c1)
            {
                int[] pr = new int[4], pg = new int[4], pb = new int[4];
                From565(c0, out pr[0], out pg[0], out pb[0]);
                From565(c1, out pr[1], out pg[1], out pb[1]);
                pr[2] = (2 * pr[0] + pr[1]) / 3; pg[2] = (2 * pg[0] + pg[1]) / 3; pb[2] = (2 * pb[0] + pb[1]) / 3;
                pr[3] = (pr[0] + 2 * pr[1]) / 3; pg[3] = (pg[0] + 2 * pg[1]) / 3; pb[3] = (pb[0] + 2 * pb[1]) / 3;
                for (int i = 0; i < 16; i++)
                {
                    int best = 0, bd = int.MaxValue;
                    for (int k = 0; k < 4; k++)
                    {
                        int dr = r[i] - pr[k], dg = g[i] - pg[k], db = b[i] - pb[k];
                        int d = dr * dr * 3 + dg * dg * 4 + db * db * 2;
                        if (d < bd) { bd = d; best = k; }
                    }
                    bits |= (uint)best << (2 * i);
                }
            }
            outp[o] = (byte)c0; outp[o + 1] = (byte)(c0 >> 8);
            outp[o + 2] = (byte)c1; outp[o + 3] = (byte)(c1 >> 8);
            outp[o + 4] = (byte)bits; outp[o + 5] = (byte)(bits >> 8);
            outp[o + 6] = (byte)(bits >> 16); outp[o + 7] = (byte)(bits >> 24);
        }

        static ushort To565(int r, int g, int b)
        {
            return (ushort)(((r * 31 + 127) / 255) << 11 | ((g * 63 + 127) / 255) << 5 | ((b * 31 + 127) / 255));
        }

        static void From565(ushort c, out int r, out int g, out int b)
        {
            r = ((c >> 11) & 31) * 255 / 31; g = ((c >> 5) & 63) * 255 / 63; b = (c & 31) * 255 / 31;
        }
    }
}

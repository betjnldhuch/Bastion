package online.bastion.launcher;

import java.io.File;
import java.io.RandomAccessFile;
import java.util.ArrayList;
import java.util.List;

/**
 * Где внутри APK лежат несжатые lib/arm64-v8a/*.so (смещение данных в файле).
 * Нужно, чтобы кадр стека вида «base.apk (offset 0x16ee000) pc 0x5a4cc»
 * превратить в «libGame.so + 0x7d04cc»: так бывает, когда хук меняет права
 * страниц и Android не может сопоставить участок памяти с ELF.
 */
final class ApkLibs {
    static final class Lib {
        final String name; final long start, size;
        Lib(String n, long s, long z) { name = n; start = s; size = z; }
    }

    private final List<Lib> libs = new ArrayList<Lib>();
    private final File apk;

    ApkLibs(File apk) {
        this.apk = apk;
        try {
            RandomAccessFile f = new RandomAccessFile(apk, "r");
            try { read(f); } finally { f.close(); }
        } catch (Exception ignored) { }
    }

    Lib byName(String name) {
        for (Lib l : libs) if (l.name.equals(name)) return l;
        return null;
    }

    /** Библиотека, в файловых данных которой лежит смещение offset внутри APK. */
    Lib find(long offset) {
        for (Lib l : libs) if (offset >= l.start && offset < l.start + l.size) return l;
        return null;
    }

    /**
     * Имя функции по .dynsym библиотеки (ELF64 внутри APK): «имя+0xсмещение» или null.
     * Работает и для stripped-библиотек — экспортированные символы остаются.
     */
    String symbol(Lib lib, long pc) {
        try {
            RandomAccessFile f = new RandomAccessFile(apk, "r");
            try {
                byte[] eh = new byte[64];
                f.seek(lib.start);
                f.readFully(eh);
                if (eh[0] != 0x7f || eh[1] != 'E' || eh[4] != 2) return null;
                long shoff = u64(eh, 0x28);
                int shentsize = u16(eh, 0x3a), shnum = u16(eh, 0x3c);
                if (shoff == 0 || shnum == 0 || shentsize < 64) return null;
                byte[] sh = new byte[shentsize * shnum];
                f.seek(lib.start + shoff);
                f.readFully(sh);
                for (int i = 0; i < shnum; i++) {
                    int o = i * shentsize;
                    if (u32(sh, o + 4) != 11) continue;               // SHT_DYNSYM
                    long symOff = u64(sh, o + 0x18), symSize = u64(sh, o + 0x20);
                    int link = (int) u32(sh, o + 0x28);
                    int lo = link * shentsize;
                    long strOff = u64(sh, lo + 0x18), strSize = u64(sh, lo + 0x20);
                    if (symSize > 32L << 20 || strSize > 32L << 20) return null;
                    byte[] syms = new byte[(int) symSize];
                    f.seek(lib.start + symOff);
                    f.readFully(syms);
                    long bestValue = -1; int bestName = -1;
                    for (int s = 0; s + 24 <= syms.length; s += 24) {
                        int type = syms[s + 4] & 0xf;
                        if (type != 2) continue;                          // STT_FUNC
                        long value = u64(syms, s + 8), size = u64(syms, s + 16);
                        if (value <= pc && pc < value + Math.max(size, 1) && value > bestValue) {
                            bestValue = value; bestName = (int) u32(syms, s);
                        }
                    }
                    if (bestName < 0) return null;
                    byte[] name = new byte[256];
                    f.seek(lib.start + strOff + bestName);
                    int n = f.read(name);
                    int z = 0;
                    while (z < n && name[z] != 0) z++;
                    return new String(name, 0, z, "UTF-8") + "+0x" + Long.toHexString(pc - bestValue);
                }
            } finally { f.close(); }
        } catch (Exception ignored) { }
        return null;
    }

    private void read(RandomAccessFile f) throws Exception {
        long len = f.length();
        int tail = (int) Math.min(len, 65557);
        byte[] end = new byte[tail];
        f.seek(len - tail);
        f.readFully(end);
        int eocd = -1;
        for (int i = tail - 22; i >= 0; i--)
            if (u32(end, i) == 0x06054b50L) { eocd = i; break; }
        if (eocd < 0) return;
        int count = u16(end, eocd + 10);
        long cdOffset = u32(end, eocd + 16);
        long cdSize = u32(end, eocd + 12);
        byte[] cd = new byte[(int) cdSize];
        f.seek(cdOffset);
        f.readFully(cd);
        int p = 0;
        byte[] local = new byte[30];
        for (int i = 0; i < count && p + 46 <= cd.length; i++) {
            if (u32(cd, p) != 0x02014b50L) break;
            int method = u16(cd, p + 10);
            long compSize = u32(cd, p + 20);
            int nameLen = u16(cd, p + 28), extraLen = u16(cd, p + 30), commentLen = u16(cd, p + 32);
            long lho = u32(cd, p + 42);
            String name = new String(cd, p + 46, nameLen, "UTF-8");
            p += 46 + nameLen + extraLen + commentLen;
            if (method != 0 || !name.startsWith("lib/") || !name.endsWith(".so")) continue;
            f.seek(lho);
            f.readFully(local);
            long data = lho + 30 + u16(local, 26) + u16(local, 28);
            libs.add(new Lib(name.substring(name.lastIndexOf('/') + 1), data, compSize));
        }
    }

    private static int u16(byte[] b, int o) { return (b[o] & 0xff) | (b[o + 1] & 0xff) << 8; }
    private static long u32(byte[] b, int o) { return (u16(b, o) | (long) u16(b, o + 2) << 16) & 0xffffffffL; }
    private static long u64(byte[] b, int o) { return u32(b, o) | u32(b, o + 4) << 32; }
}

package online.bastion.launcher;

import android.content.Context;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.RandomAccessFile;
import java.util.HashMap;
import java.util.Map;

/**
 * Наша графика для телефона (собирает client-mods/tools/build_mobile.py из ПК-пака):
 * assets/bastion-mods/tex/*.btx кладутся в SAMP/bastion/tex/ (клиент подменяет ими
 * текстуры texdb по имени), assets/bastion-mods/img/gta3/*.dff дописываются в gta3.img
 * игры (маркеры). Делается перед запуском, только когда version.txt в APK новее.
 */
final class BastionMods {
    private static final String ASSETS = "bastion-mods";
    private static final String STAMP = "SAMP/bastion/installed.txt";
    private static final String[] GTA3_IMG = {"texdb/gta3.img", "models/gta3.img"};

    private BastionMods() { }

    /** Ничего не бросает: без наших файлов игра всё равно запустится со стандартной графикой. */
    static void apply(Context ctx, File root) {
        try {
            String version = new String(read(ctx.getAssets().open(ASSETS + "/version.txt")), "UTF-8").trim();
            File stamp = new File(root, STAMP);
            if (stamp.isFile() && new String(read(new java.io.FileInputStream(stamp)), "UTF-8").trim().equals(version)) return;

            File texDir = new File(root, "SAMP/bastion/tex");
            deleteTree(texDir);
            texDir.mkdirs();
            String[] tex = ctx.getAssets().list(ASSETS + "/tex");
            if (tex != null) for (String name : tex) {
                byte[] data = read(ctx.getAssets().open(ASSETS + "/tex/" + name));
                FileOutputStream out = new FileOutputStream(new File(texDir, name));
                try { out.write(data); } finally { out.close(); }
            }

            Map<String, byte[]> dff = new HashMap<String, byte[]>();
            String[] models = ctx.getAssets().list(ASSETS + "/img/gta3");
            if (models != null) for (String name : models)
                dff.put(name.toLowerCase(java.util.Locale.ROOT), read(ctx.getAssets().open(ASSETS + "/img/gta3/" + name)));
            for (String rel : GTA3_IMG) {
                File img = ClientInstaller.resolveIgnoreCase(root, rel);
                if (img != null && img.isFile()) patchImg(img, dff);
            }

            FileOutputStream out = new FileOutputStream(stamp);
            try { out.write((version + "\n").getBytes("UTF-8")); } finally { out.close(); }
        } catch (Exception ignored) { }
    }

    /**
     * Заменяет записи IMG v2 ("VER2"): данные дописываются в конец архива, затем
     * в каталоге меняются смещение и размер. Каталог пишется последним, так что
     * оборванная запись оставляет рабочий архив со старыми моделями.
     */
    static int patchImg(File img, Map<String, byte[]> files) throws Exception {
        RandomAccessFile f = new RandomAccessFile(img, "rw");
        try {
            byte[] head = new byte[8];
            f.readFully(head);
            if (head[0] != 'V' || head[1] != 'E' || head[2] != 'R' || head[3] != '2') return 0;
            int count = le32(head, 4);
            if (count <= 0 || count > 200000) return 0;
            byte[] dir = new byte[count * 32];
            f.readFully(dir);
            long pos = (f.length() + 2047) / 2048 * 2048;
            int done = 0;
            for (int i = 0; i < count; i++) {
                int o = i * 32;
                int len = 0;
                while (len < 24 && dir[o + 8 + len] != 0) len++;
                String name = new String(dir, o + 8, len, "ISO-8859-1").toLowerCase(java.util.Locale.ROOT);
                byte[] data = files.get(name);
                if (data == null) continue;
                int sectors = (data.length + 2047) / 2048;
                f.seek(pos);
                f.write(data);
                f.write(new byte[sectors * 2048 - data.length]);
                put32(dir, o, (int) (pos / 2048));
                dir[o + 4] = (byte) sectors; dir[o + 5] = (byte) (sectors >> 8);
                dir[o + 6] = 0; dir[o + 7] = 0;
                pos += (long) sectors * 2048;
                done++;
            }
            if (done > 0) {
                f.getFD().sync();
                f.seek(8);
                f.write(dir);
            }
            return done;
        } finally {
            f.close();
        }
    }

    private static int le32(byte[] b, int o) {
        return (b[o] & 0xFF) | (b[o + 1] & 0xFF) << 8 | (b[o + 2] & 0xFF) << 16 | (b[o + 3] & 0xFF) << 24;
    }

    private static void put32(byte[] b, int o, int v) {
        b[o] = (byte) v; b[o + 1] = (byte) (v >> 8); b[o + 2] = (byte) (v >> 16); b[o + 3] = (byte) (v >> 24);
    }

    private static byte[] read(InputStream in) throws Exception {
        try {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buf = new byte[65536];
            int n;
            while ((n = in.read(buf)) != -1) out.write(buf, 0, n);
            return out.toByteArray();
        } finally {
            in.close();
        }
    }

    private static void deleteTree(File f) {
        File[] kids = f.listFiles();
        if (kids != null) for (File k : kids) deleteTree(k);
        f.delete();
    }
}

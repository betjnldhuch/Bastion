package online.bastion.launcher;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

/**
 * Минимальный разбор tombstone.proto (Android 12+), который возвращает
 * ApplicationExitInfo.getTraceInputStream() для нативного падения.
 * Достаёт сигнал, адрес, abort message, стек упавшего потока и хвост logcat.
 */
final class Tombstone {
    private Tombstone() { }

    /** Читает поток; если это не protobuf (старый текстовый tombstone) — отдаёт текст как есть. */
    static String format(InputStream in, int maxLogLines) throws Exception {
        ByteArrayOutputStream buf = new ByteArrayOutputStream();
        byte[] chunk = new byte[16384];
        int n;
        while ((n = in.read(chunk)) != -1 && buf.size() < 8 * 1024 * 1024) buf.write(chunk, 0, n);
        byte[] data = buf.toByteArray();
        if (data.length > 0 && (data[0] == '*' || data[0] == 'B' || data[0] == '-')) {
            String text = new String(data, "UTF-8");
            return text.length() > 12000 ? text.substring(0, 12000) : text;
        }
        try {
            return parse(data, maxLogLines);
        } catch (RuntimeException e) {
            return "не удалось разобрать tombstone: " + e;
        }
    }

    // ---------------------------------------------------------------- protobuf
    private static final class Field {
        int number, wire;
        long varint;
        byte[] bytes;
        String str() { try { return new String(bytes, "UTF-8"); } catch (Exception e) { return ""; } }
    }

    private static List<Field> fields(byte[] b) {
        List<Field> out = new ArrayList<Field>();
        int[] pos = {0};
        while (pos[0] < b.length) {
            long key = varint(b, pos);
            Field f = new Field();
            f.number = (int) (key >>> 3);
            f.wire = (int) (key & 7);
            switch (f.wire) {
                case 0: f.varint = varint(b, pos); break;
                case 1: pos[0] += 8; break;
                case 5: pos[0] += 4; break;
                case 2: {
                    int len = (int) varint(b, pos);
                    if (len < 0 || pos[0] + len > b.length) throw new IllegalStateException("bad length");
                    f.bytes = new byte[len];
                    System.arraycopy(b, pos[0], f.bytes, 0, len);
                    pos[0] += len;
                    break;
                }
                default: throw new IllegalStateException("wire type " + f.wire);
            }
            out.add(f);
        }
        return out;
    }

    private static long varint(byte[] b, int[] pos) {
        long result = 0;
        int shift = 0;
        while (true) {
            if (pos[0] >= b.length) throw new IllegalStateException("truncated");
            int x = b[pos[0]++] & 0xff;
            result |= (long) (x & 0x7f) << shift;
            if ((x & 0x80) == 0) return result;
            shift += 7;
            if (shift > 63) throw new IllegalStateException("varint");
        }
    }

    private static String parse(byte[] data, int maxLogLines) {
        StringBuilder r = new StringBuilder();
        long crashTid = -1;
        String abort = null;
        List<Field> top = fields(data);
        byte[] crashThread = null;
        List<byte[]> threads = new ArrayList<byte[]>();
        List<byte[]> mappings = new ArrayList<byte[]>();
        for (Field f : top) {
            if (f.number == 2 && f.wire == 2) r.append("Сборка Android: ").append(f.str()).append('\n');
            else if (f.number == 17 && f.wire == 2) mappings.add(f.bytes);
            else if (f.number == 6 && f.wire == 0) crashTid = f.varint;
            else if (f.number == 14 && f.wire == 2) abort = f.str();
            else if (f.number == 10 && f.wire == 2) {               // Signal
                String name = "", code = "";
                long addr = 0;
                boolean hasAddr = false;
                for (Field s : fields(f.bytes)) {
                    if (s.number == 2 && s.wire == 2) name = s.str();
                    else if (s.number == 4 && s.wire == 2) code = s.str();
                    else if (s.number == 8 && s.wire == 0) hasAddr = s.varint != 0;
                    else if (s.number == 9 && s.wire == 0) addr = s.varint;
                }
                r.append("Сигнал: ").append(name).append(' ').append(code);
                if (hasAddr) r.append(", адрес 0x").append(Long.toHexString(addr));
                r.append('\n');
            } else if (f.number == 15 && f.wire == 2) {             // Cause
                for (Field c : fields(f.bytes))
                    if (c.number == 1 && c.wire == 2) r.append("Причина: ").append(c.str()).append('\n');
            } else if (f.number == 16 && f.wire == 2) {             // map<uint32, Thread>
                long key = -1;
                byte[] value = null;
                for (Field e : fields(f.bytes)) {
                    if (e.number == 1 && e.wire == 0) key = e.varint;
                    else if (e.number == 2 && e.wire == 2) value = e.bytes;
                }
                if (value != null) {
                    threads.add(value);
                    if (key == crashTid) crashThread = value;
                }
            }
        }
        if (abort != null && abort.length() > 0) r.append("Abort: ").append(abort).append('\n');
        if (crashThread == null && !threads.isEmpty()) crashThread = threads.get(0);
        if (crashThread != null) appendThread(r, crashThread, 64, true);

        r.append("\n[Загруженные библиотеки приложения: base, offset, build-id]\n");
        for (byte[] m : mappings) {
            long begin = 0, end = 0, offset = 0, bias = 0;
            boolean exec = false;
            String name = "", build = "";
            for (Field f : fields(m)) {
                if (f.number == 1 && f.wire == 0) begin = f.varint;
                else if (f.number == 2 && f.wire == 0) end = f.varint;
                else if (f.number == 3 && f.wire == 0) offset = f.varint;
                else if (f.number == 6 && f.wire == 0) exec = f.varint != 0;
                else if (f.number == 7 && f.wire == 2) name = f.str();
                else if (f.number == 8 && f.wire == 2) build = f.str();
                else if (f.number == 9 && f.wire == 0) bias = f.varint;
            }
            if (!exec || !(name.contains("online.bastion.launcher") || name.endsWith(".so") && name.contains("/data/"))) continue;
            r.append(String.format("%012x-%012x off %08x  %s", begin, end, offset, shortName(name)));
            if (build.length() > 0) r.append("  bid ").append(build);
            r.append('\n');
        }

        r.append("\n[Другие потоки: верх стека]\n");
        int shown = 0;
        for (byte[] t : threads) {
            if (t == crashThread) continue;
            appendThread(r, t, 3, false);
            if (++shown >= 40) break;
        }

        if (maxLogLines > 0) {
            List<String> logs = new ArrayList<String>();
            for (Field f : top) {
                if (f.number != 18 || f.wire != 2) continue;          // LogBuffer
                for (Field lb : fields(f.bytes)) {
                    if (lb.number != 2 || lb.wire != 2) continue;     // LogMessage
                    String tag = "", msg = "";
                    long prio = 0;
                    for (Field m : fields(lb.bytes)) {
                        if (m.number == 4 && m.wire == 0) prio = m.varint;
                        else if (m.number == 5 && m.wire == 2) tag = m.str();
                        else if (m.number == 6 && m.wire == 2) msg = m.str().trim();
                    }
                    logs.add("VDIWEF".charAt((int) Math.max(0, Math.min(5, prio - 2))) + "/" + tag + ": " + msg);
                }
            }
            if (!logs.isEmpty()) {
                r.append("\n[logcat процесса игры, последние строки]\n");
                for (int i = Math.max(0, logs.size() - maxLogLines); i < logs.size(); i++)
                    r.append(logs.get(i)).append('\n');
            }
        }
        return r.toString();
    }

    private static String shortName(String file) {
        int apk = file.indexOf(".apk!");
        if (apk >= 0) return file.substring(apk + 5);
        int slash = file.lastIndexOf('/');
        return slash >= 0 ? file.substring(slash + 1) : file;
    }

    private static void appendThread(StringBuilder r, byte[] thread, int maxFrames, boolean header) {
        String name = "";
        long tid = 0;
        List<byte[]> frames = new ArrayList<byte[]>();
        for (Field t : fields(thread)) {
            if (t.number == 1 && t.wire == 0) tid = t.varint;
            else if (t.number == 2 && t.wire == 2) name = t.str();
            else if (t.number == 4 && t.wire == 2) frames.add(t.bytes);
        }
        if (header) r.append("Упавший поток: ").append(name).append(" (tid ").append(tid).append(")\nСтек:\n");
        else r.append("-- ").append(name).append(" (tid ").append(tid).append(")\n");
        int i = 0;
        for (byte[] frame : frames) {
            long relPc = 0, fnOff = 0, mapOff = 0;
            String fn = "", file = "", build = "";
            for (Field f : fields(frame)) {
                if (f.number == 1 && f.wire == 0) relPc = f.varint;
                else if (f.number == 4 && f.wire == 2) fn = f.str();
                else if (f.number == 5 && f.wire == 0) fnOff = f.varint;
                else if (f.number == 6 && f.wire == 2) file = f.str();
                else if (f.number == 7 && f.wire == 0) mapOff = f.varint;
                else if (f.number == 8 && f.wire == 2) build = f.str();
            }
            r.append(String.format("#%02d pc %08x  %s", i, relPc, shortName(file)));
            if (mapOff != 0) r.append(String.format(" (offset 0x%x)", mapOff));
            if (fn.length() > 0) r.append(" (").append(fn).append('+').append(fnOff).append(')');
            if (header && build.length() > 0) r.append(" (BuildId: ").append(build).append(')');
            r.append('\n');
            if (++i >= maxFrames) break;
        }
    }
}

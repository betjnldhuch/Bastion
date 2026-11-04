package online.bastion.launcher;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.nio.charset.Charset;

/** Опрос сервера по стандартному query-протоколу SA-MP / open.mp (UDP, пакет 'i'). */
public final class SampQuery {
    private static final Charset CP1251 = Charset.forName("windows-1251");

    private SampQuery() { }

    /** Блокирующий вызов: только из фонового потока. */
    public static void query(Config.Server s) {
        boolean ok = false;
        DatagramSocket sock = null;
        try {
            InetAddress ip = resolve(s.host);
            if (ip == null) return;
            byte[] ipb = ip.getAddress();
            byte[] pkt = new byte[11];
            pkt[0] = 'S'; pkt[1] = 'A'; pkt[2] = 'M'; pkt[3] = 'P';
            System.arraycopy(ipb, 0, pkt, 4, 4);
            pkt[8] = (byte) (s.port & 0xFF);
            pkt[9] = (byte) ((s.port >> 8) & 0xFF);
            pkt[10] = 'i';

            sock = new DatagramSocket();
            sock.setSoTimeout(1500);
            long t0 = System.nanoTime();
            sock.send(new DatagramPacket(pkt, pkt.length, ip, s.port));
            byte[] buf = new byte[2048];
            DatagramPacket in = new DatagramPacket(buf, buf.length);
            sock.receive(in);
            long t1 = System.nanoTime();
            int n = in.getLength();
            if (n < 11 + 5) return;

            int[] o = {11};
            o[0] += 1; // password flag
            s.players = u16(buf, o);
            s.maxPlayers = u16(buf, o);
            s.hostName = str(buf, n, o);
            s.ping = (int) ((t1 - t0) / 1000000L);
            ok = true;
        } catch (Exception ignored) {
        } finally {
            if (sock != null) sock.close();
            s.online = ok;
            s.queried = true;
        }
    }

    private static int u16(byte[] b, int[] o) {
        int v = (b[o[0]] & 0xFF) | ((b[o[0] + 1] & 0xFF) << 8);
        o[0] += 2;
        return v;
    }

    private static String str(byte[] b, int n, int[] o) {
        if (o[0] + 4 > n) return "";
        int len = (b[o[0]] & 0xFF) | ((b[o[0] + 1] & 0xFF) << 8) | ((b[o[0] + 2] & 0xFF) << 16) | ((b[o[0] + 3] & 0xFF) << 24);
        o[0] += 4;
        if (len < 0 || o[0] + len > n) return "";
        String v = new String(b, o[0], len, CP1251);
        o[0] += len;
        return v;
    }

    private static InetAddress resolve(String host) {
        try {
            for (InetAddress a : InetAddress.getAllByName(host))
                if (a instanceof Inet4Address) return a;
        } catch (Exception ignored) { }
        return null;
    }
}

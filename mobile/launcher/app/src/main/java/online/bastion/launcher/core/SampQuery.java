package online.bastion.launcher.core;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.SocketTimeoutException;
import java.nio.charset.Charset;
import java.security.SecureRandom;

/**
 * Query-протокол SA-MP / open.mp (UDP, тот же порт, что и у игры).
 *
 * Запрос: "SAMP" + 4 байта IPv4 + 2 байта порта (little-endian) + код операции.
 *   'i' — информация: пароль, игроки, слоты, название, режим, язык;
 *   'p' — пинг: к запросу добавляются 4 случайных байта, сервер возвращает их обратно.
 * Сервер повторяет 11-байтный заголовок запроса в начале ответа.
 *
 * Все методы блокирующие — вызывать только из фонового потока.
 */
public final class SampQuery {

    public static final int DEFAULT_TIMEOUT_MS = 1200;
    public static final int DEFAULT_ATTEMPTS = 2;

    static final int HEADER_SIZE = 11;
    private static final int MAX_PACKET = 2048;
    /** Защита от мусорных длин строк в ответе. */
    private static final int MAX_STRING = 512;
    private static final Charset CP1251 = Charset.forName("windows-1251");
    private static final SecureRandom RANDOM = new SecureRandom();

    private SampQuery() { }

    /** Ответ пришёл, но это не корректный ответ SA-MP. */
    static final class MalformedException extends Exception {
        private static final long serialVersionUID = 1L;
        MalformedException(String m) { super(m); }
    }

    public static QueryResult query(ServerAddress address) {
        return query(address, DEFAULT_TIMEOUT_MS, DEFAULT_ATTEMPTS);
    }

    public static QueryResult query(ServerAddress address, int timeoutMs, int attempts) {
        Inet4Address ip = resolveIpv4(address.host);
        if (ip == null) return QueryResult.unresolved();

        DatagramSocket socket = null;
        try {
            socket = new DatagramSocket();
            // connect() отбрасывает пакеты от чужих адресов на уровне ОС.
            socket.connect(ip, address.port);

            byte[] info = null;
            int infoLength = 0;
            int infoRtt = -1;
            byte[] buffer = new byte[MAX_PACKET];
            for (int i = 0; i < attempts && info == null; i++) {
                byte[] request = request(ip, address.port, 'i', null);
                long started = System.nanoTime();
                socket.send(new DatagramPacket(request, request.length));
                int n = receiveMatching(socket, buffer, request, HEADER_SIZE, timeoutMs);
                if (n > 0) {
                    infoRtt = elapsedMs(started);
                    info = buffer;
                    infoLength = n;
                }
            }
            if (info == null) return QueryResult.offline();

            byte[] infoCopy = new byte[infoLength];
            System.arraycopy(info, 0, infoCopy, 0, infoLength);
            int ping = ping(socket, ip, address.port, timeoutMs, attempts);
            return parseInfo(infoCopy, infoLength, ping >= 0 ? ping : infoRtt);
        } catch (MalformedException e) {
            // На порту отвечает что-то, но не SA-MP-сервер.
            return QueryResult.offline();
        } catch (IOException e) {
            // Нет сети, ICMP «порт недоступен» и т.п. — для игрока это «сервер не отвечает».
            return QueryResult.offline();
        } finally {
            if (socket != null) socket.close();
        }
    }

    /** Пинг по пакету 'p'. Возвращает лучший результат из попыток или -1. */
    private static int ping(DatagramSocket socket, Inet4Address ip, int port, int timeoutMs, int attempts)
            throws IOException {
        int best = -1;
        byte[] buffer = new byte[MAX_PACKET];
        for (int i = 0; i < attempts; i++) {
            byte[] token = new byte[4];
            RANDOM.nextBytes(token);
            byte[] request = request(ip, port, 'p', token);
            long started = System.nanoTime();
            socket.send(new DatagramPacket(request, request.length));
            int n = receiveMatching(socket, buffer, request, request.length, timeoutMs);
            if (n > 0) {
                int ms = elapsedMs(started);
                if (best < 0 || ms < best) best = ms;
            }
        }
        return best;
    }

    /**
     * Ждёт пакет, чьи первые {@code matchLength} байт (кроме IP и порта) совпадают с запросом.
     * Посторонние и запоздавшие ответы пропускаются. @return длина пакета или -1 по таймауту.
     */
    private static int receiveMatching(DatagramSocket socket, byte[] buffer, byte[] request,
                                       int matchLength, int timeoutMs) throws IOException {
        long deadline = System.nanoTime() + timeoutMs * 1000000L;
        DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
        while (true) {
            int left = (int) ((deadline - System.nanoTime()) / 1000000L);
            if (left <= 0) return -1;
            socket.setSoTimeout(left);
            packet.setLength(buffer.length);
            try {
                socket.receive(packet);
            } catch (SocketTimeoutException e) {
                return -1;
            }
            if (matches(buffer, packet.getLength(), request, matchLength)) return packet.getLength();
        }
    }

    /**
     * Сравниваем сигнатуру, код операции и (для 'p') токен. IP/порт в ответе не сравниваем:
     * за NAT сервер видит другой адрес, а источник уже проверен connect().
     */
    static boolean matches(byte[] data, int length, byte[] request, int matchLength) {
        if (length < matchLength) return false;
        for (int i = 0; i < 4; i++) if (data[i] != request[i]) return false;
        for (int i = 10; i < matchLength; i++) if (data[i] != request[i]) return false;
        return true;
    }

    static byte[] request(Inet4Address ip, int port, char opcode, byte[] extra) {
        int extraLength = extra == null ? 0 : extra.length;
        byte[] p = new byte[HEADER_SIZE + extraLength];
        p[0] = 'S'; p[1] = 'A'; p[2] = 'M'; p[3] = 'P';
        System.arraycopy(ip.getAddress(), 0, p, 4, 4);
        p[8] = (byte) (port & 0xFF);
        p[9] = (byte) ((port >> 8) & 0xFF);
        p[10] = (byte) opcode;
        if (extraLength > 0) System.arraycopy(extra, 0, p, HEADER_SIZE, extraLength);
        return p;
    }

    /** Разбор ответа 'i'. Пакетно-приватный, чтобы его можно было проверить тестом. */
    static QueryResult parseInfo(byte[] b, int length, int pingMs) throws MalformedException {
        Cursor c = new Cursor(b, length, HEADER_SIZE);
        boolean passworded = c.u8() != 0;
        int players = c.u16();
        int maxPlayers = c.u16();
        String hostName = c.string();
        String gameMode = c.string();
        String language = c.string();
        if (players > maxPlayers || maxPlayers > 1000) throw new MalformedException("bad player count");
        return QueryResult.online(passworded, players, maxPlayers, hostName, gameMode, language, pingMs);
    }

    private static final class Cursor {
        private final byte[] b;
        private final int length;
        private int pos;

        Cursor(byte[] b, int length, int pos) { this.b = b; this.length = length; this.pos = pos; }

        private void need(int n) throws MalformedException {
            if (pos + n > length) throw new MalformedException("packet too short");
        }

        int u8() throws MalformedException { need(1); return b[pos++] & 0xFF; }

        int u16() throws MalformedException {
            need(2);
            int v = (b[pos] & 0xFF) | ((b[pos + 1] & 0xFF) << 8);
            pos += 2;
            return v;
        }

        String string() throws MalformedException {
            need(4);
            long len = (b[pos] & 0xFFL) | ((b[pos + 1] & 0xFFL) << 8)
                    | ((b[pos + 2] & 0xFFL) << 16) | ((b[pos + 3] & 0xFFL) << 24);
            pos += 4;
            if (len > MAX_STRING) throw new MalformedException("string too long");
            need((int) len);
            String s = new String(b, pos, (int) len, CP1251);
            pos += (int) len;
            return s;
        }
    }

    private static int elapsedMs(long startedNanos) {
        return (int) Math.max(0, (System.nanoTime() - startedNanos) / 1000000L);
    }

    /** SA-MP работает только по IPv4: берём первый IPv4-адрес домена. */
    static Inet4Address resolveIpv4(String host) {
        try {
            for (InetAddress a : InetAddress.getAllByName(host)) {
                if (a instanceof Inet4Address) return (Inet4Address) a;
            }
        } catch (IOException ignored) {
            // UnknownHostException и подобные: адрес не найден.
        } catch (SecurityException ignored) {
            // Нет разрешения INTERNET.
        }
        return null;
    }
}

package online.bastion.launcher.core;

import java.io.ByteArrayOutputStream;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.nio.charset.Charset;

/** Маленький UDP-сервер для тестов: отвечает на 'i' и 'p' как настоящий SA-MP. */
final class FakeSampServer {
    private final DatagramSocket socket;
    private final Thread thread;

    FakeSampServer(final boolean answer) throws Exception {
        socket = new DatagramSocket(0, InetAddress.getByName("127.0.0.1"));
        thread = new Thread(new Runnable() {
            @Override public void run() {
                byte[] buf = new byte[512];
                while (!socket.isClosed()) {
                    try {
                        DatagramPacket in = new DatagramPacket(buf, buf.length);
                        socket.receive(in);
                        if (!answer || in.getLength() < 11) continue;
                        byte[] header = new byte[in.getLength()];
                        System.arraycopy(buf, 0, header, 0, header.length);
                        byte[] reply;
                        if (header[10] == 'i') {
                            byte[] h11 = new byte[11];
                            System.arraycopy(header, 0, h11, 0, 11);
                            reply = infoPacket(h11, false, 7, 50, "Fake Server", "Test", "RU");
                        } else if (header[10] == 'p') {
                            reply = header;  // эхо заголовка и токена
                        } else {
                            continue;
                        }
                        socket.send(new DatagramPacket(reply, reply.length, in.getSocketAddress()));
                    } catch (Exception e) {
                        return;
                    }
                }
            }
        });
        thread.setDaemon(true);
        thread.start();
    }

    int port() { return socket.getLocalPort(); }

    void close() { socket.close(); }

    static byte[] infoPacket(byte[] header, boolean password, int players, int max,
                             String host, String mode, String lang) {
        Charset cp = Charset.forName("windows-1251");
        ByteArrayOutputStream o = new ByteArrayOutputStream();
        o.write(header, 0, header.length);
        o.write(password ? 1 : 0);
        o.write(players & 0xFF); o.write(players >> 8);
        o.write(max & 0xFF); o.write(max >> 8);
        for (String s : new String[]{host, mode, lang}) {
            byte[] b = s.getBytes(cp);
            o.write(b.length & 0xFF); o.write((b.length >> 8) & 0xFF); o.write(0); o.write(0);
            o.write(b, 0, b.length);
        }
        return o.toByteArray();
    }
}

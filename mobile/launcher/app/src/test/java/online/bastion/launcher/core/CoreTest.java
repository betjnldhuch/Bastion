package online.bastion.launcher.core;

import org.junit.Test;

import java.io.StringReader;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

public class CoreTest {

    // ---------- ServerAddress ----------

    @Test public void acceptsIpv4AndDomain() throws Exception {
        assertEquals("192.168.1.10:7777", ServerAddress.parse(" 192.168.1.10 ", "7777").toString());
        assertEquals("play.bastion.online:7778", ServerAddress.parse("Play.Bastion.Online", 7778).toString());
    }

    @Test public void rejectsBadAddresses() {
        String[][] bad = {
                {"", "7777"}, {"1.2.3", "7777"}, {"256.1.1.1", "7777"}, {"host name", "7777"},
                {"::1", "7777"}, {"a..b", "7777"}, {"1.2.3.4", "0"}, {"1.2.3.4", "65536"},
                {"1.2.3.4", "77a7"}, {"1.2.3.4", ""}, {"1.2.3.4", "-1"},
        };
        for (String[] b : bad) {
            try {
                ServerAddress.parse(b[0], b[1]);
                fail("принят плохой адрес " + b[0] + ":" + b[1]);
            } catch (ServerAddress.InvalidException expected) { }
        }
    }

    // ---------- Nickname ----------

    @Test public void nickname() {
        assertNull(Nickname.validate("Ivan_Petrov", true));
        assertNull(Nickname.validate("[BST]player.1", false));
        assertNotNull(Nickname.validate("ivan_petrov", true));
        assertNotNull(Nickname.validate("Иван_Петров", false));
        assertNotNull(Nickname.validate("ab", false));
        assertNotNull(Nickname.validate("Abcdefghij_Klmnopqrstu", false)); // 22 символа
        assertNotNull(Nickname.validate("a;b=c", false));
    }

    // ---------- LauncherConfig ----------

    @Test public void parsesLauncherIni() throws Exception {
        String ini = "﻿; comment\n"
                + "title = BASTION\n"
                + "lock_servers = true\n"
                + "server = Bastion Peace | 192.168.1.10 | 7777 | Классический RP\n"
                + "server = Bastion War | 192.168.1.10 | 7778 | | secret\n"
                + "server = Broken | 1.2.3.4 | 99999\n"
                + "game_activity = com.rockstargames.gtasa.GTASA\n"
                + "require_file = /SAMP/main.scm\n"
                + "require_file = ../etc/passwd\n";
        LauncherConfig c = LauncherConfig.parse(new StringReader(ini));
        assertTrue(c.serversLocked);
        assertEquals(2, c.servers.size());
        assertEquals("bastion_peace", c.servers.get(0).id);
        assertEquals(7778, c.servers.get(1).defaultAddress.port);
        assertEquals("secret", c.servers.get(1).password);
        assertEquals(1, c.requiredFiles.size());
        assertEquals("SAMP/main.scm", c.requiredFiles.get(0));
        assertEquals(2, c.warnings.size()); // плохой порт + плохой путь
    }

    // ---------- SampSettingsIni ----------

    @Test public void createsSettingsFromScratch() {
        String out = new SampSettingsIni()
                .set("client", "name", "Ivan_Petrov")
                .set("client", "host", "192.168.1.10")
                .set("client", "port", "7777")
                .setIfMissing("debug", "debug", "false")
                .apply(null);
        assertEquals("[client]\nname = Ivan_Petrov\nhost = 192.168.1.10\nport = 7777\n\n[debug]\ndebug = false\n", out);
    }

    @Test public void keepsUserLinesAndReplacesOwnKeys() {
        String existing = "; мой конфиг\r\n[Client]\r\nName=Old_Name\r\nhost = 1.1.1.1\r\nname = dup\r\n\r\n"
                + "[debug]\r\ndebug = true\r\n\r\n[gui]\r\nFont = Arial.ttf\r\n";
        String out = new SampSettingsIni()
                .set("client", "name", "Ivan_Petrov")
                .set("client", "host", "192.168.1.10")
                .set("client", "port", "7778")
                .set("debug", "online", "true")
                .setIfMissing("debug", "debug", "false")
                .set("gui", "ChatMaxMessages", "10")
                .apply(existing);
        assertEquals("; мой конфиг\n[Client]\nName = Ivan_Petrov\nhost = 192.168.1.10\nport = 7778\n\n"
                + "[debug]\ndebug = true\nonline = true\n\n[gui]\nFont = Arial.ttf\nChatMaxMessages = 10\n", out);
    }

    @Test(expected = IllegalArgumentException.class)
    public void rejectsValueThatBreaksIni() {
        new SampSettingsIni().set("client", "password", "a;b");
    }

    // ---------- SampQuery (разбор пакета) ----------

    @Test public void parsesInfoPacket() throws Exception {
        byte[] p = FakeSampServer.infoPacket(new byte[11], true, 12, 100, "Bastion Peace", "RP", "Русский");
        QueryResult r = SampQuery.parseInfo(p, p.length, 42);
        assertTrue(r.isOnline());
        assertTrue(r.passworded);
        assertEquals(12, r.players);
        assertEquals(100, r.maxPlayers);
        assertEquals("Bastion Peace", r.hostName);
        assertEquals("Русский", r.language);
        assertEquals(42, r.pingMs);
    }

    @Test public void rejectsTruncatedInfoPacket() {
        byte[] p = FakeSampServer.infoPacket(new byte[11], false, 1, 50, "Name", "Mode", "Lang");
        try {
            SampQuery.parseInfo(p, p.length - 3, 0);
            fail("обрезанный пакет принят");
        } catch (SampQuery.MalformedException expected) { }
    }

    // ---------- SampQuery (по сети) ----------

    @Test public void queriesLiveServer() throws Exception {
        FakeSampServer server = new FakeSampServer(true);
        try {
            QueryResult r = SampQuery.query(ServerAddress.parse("127.0.0.1", server.port()), 1000, 2);
            assertTrue(r.isOnline());
            assertEquals(7, r.players);
            assertEquals("Fake Server", r.hostName);
            assertTrue(r.pingMs >= 0);
        } finally {
            server.close();
        }
    }

    @Test public void silentServerIsOffline() throws Exception {
        FakeSampServer server = new FakeSampServer(false);
        try {
            long t0 = System.currentTimeMillis();
            QueryResult r = SampQuery.query(ServerAddress.parse("127.0.0.1", server.port()), 300, 2);
            assertEquals(QueryResult.State.OFFLINE, r.state);
            assertTrue("таймаут не соблюдён", System.currentTimeMillis() - t0 < 2000);
        } finally {
            server.close();
        }
    }

    @Test public void unknownHostIsUnresolved() throws Exception {
        QueryResult r = SampQuery.query(ServerAddress.parse("nonexistent.invalid", 7777), 300, 1);
        assertEquals(QueryResult.State.UNRESOLVED, r.state);
        assertFalse(r.isOnline());
    }
}

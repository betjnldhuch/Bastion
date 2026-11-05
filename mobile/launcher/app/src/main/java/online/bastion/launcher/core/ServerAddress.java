package online.bastion.launcher.core;

/**
 * Адрес сервера: хост (IP или домен) + UDP-порт.
 * Неизменяемый. Создаётся только через {@link #parse}, поэтому всегда корректный.
 */
public final class ServerAddress {

    /** Мод хранит хост в буфере на 127 байт (MAX_SETTINGS_STRING в settings.h). */
    public static final int MAX_HOST_LENGTH = 127;
    public static final int MIN_PORT = 1;
    public static final int MAX_PORT = 65535;

    public final String host;
    public final int port;

    private ServerAddress(String host, int port) {
        this.host = host;
        this.port = port;
    }

    /** Ошибка разбора адреса с понятным игроку текстом. */
    public static final class InvalidException extends Exception {
        private static final long serialVersionUID = 1L;
        public InvalidException(String message) { super(message); }
    }

    public static ServerAddress parse(String host, String port) throws InvalidException {
        return parse(host, parsePort(port));
    }

    public static ServerAddress parse(String host, int port) throws InvalidException {
        String h = host == null ? "" : host.trim();
        if (h.length() == 0) throw new InvalidException("Адрес не указан");
        if (h.length() > MAX_HOST_LENGTH) throw new InvalidException("Адрес слишком длинный");
        if (!isValidHost(h)) throw new InvalidException("Адрес содержит недопустимые символы");
        if (port < MIN_PORT || port > MAX_PORT) throw new InvalidException("Порт должен быть от 1 до 65535");
        return new ServerAddress(h.toLowerCase(java.util.Locale.ROOT), port);
    }

    private static int parsePort(String port) throws InvalidException {
        String p = port == null ? "" : port.trim();
        if (p.length() == 0) throw new InvalidException("Порт не указан");
        if (p.length() > 5) throw new InvalidException("Порт должен быть от 1 до 65535");
        for (int i = 0; i < p.length(); i++) {
            if (p.charAt(i) < '0' || p.charAt(i) > '9') throw new InvalidException("Порт — только цифры");
        }
        return Integer.parseInt(p);
    }

    /**
     * IPv4 или имя хоста (буквы, цифры, точки, дефисы). SA-MP работает только по IPv4,
     * поэтому IPv6-адреса не принимаются.
     */
    static boolean isValidHost(String h) {
        if (h.startsWith(".") || h.endsWith(".") || h.startsWith("-") || h.contains("..")) return false;
        boolean allDigitsAndDots = true;
        for (int i = 0; i < h.length(); i++) {
            char c = h.charAt(i);
            boolean ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-';
            if (!ok) return false;
            if (!(c == '.' || (c >= '0' && c <= '9'))) allDigitsAndDots = false;
        }
        return !allDigitsAndDots || isIpv4(h);
    }

    private static boolean isIpv4(String h) {
        String[] parts = h.split("\\.", -1);
        if (parts.length != 4) return false;
        for (String part : parts) {
            if (part.length() == 0 || part.length() > 3) return false;
            if (Integer.parseInt(part) > 255) return false;
        }
        return true;
    }

    @Override public String toString() { return host + ":" + port; }

    @Override public boolean equals(Object o) {
        if (!(o instanceof ServerAddress)) return false;
        ServerAddress a = (ServerAddress) o;
        return port == a.port && host.equals(a.host);
    }

    @Override public int hashCode() { return host.hashCode() * 31 + port; }
}

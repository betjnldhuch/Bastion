package online.bastion.launcher.core;

/** Сервер из launcher.ini: как он называется и куда подключаться по умолчанию. */
public final class ServerEntry {
    /** Стабильный ключ для сохранённых настроек (не зависит от порядка строк в ini). */
    public final String id;
    public final String name;
    public final String description;
    public final ServerAddress defaultAddress;
    /** Пароль сервера (пусто, если сервер открытый). Пишется в [client] password. */
    public final String password;

    public ServerEntry(String name, String description, ServerAddress defaultAddress, String password) {
        this.name = name;
        this.description = description == null ? "" : description;
        this.defaultAddress = defaultAddress;
        this.password = password == null ? "" : password;
        this.id = makeId(name);
    }

    static String makeId(String name) {
        StringBuilder b = new StringBuilder();
        for (char c : name.toLowerCase(java.util.Locale.ROOT).toCharArray()) {
            if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) b.append(c);
            else if (b.length() > 0 && b.charAt(b.length() - 1) != '_') b.append('_');
        }
        while (b.length() > 0 && b.charAt(b.length() - 1) == '_') b.setLength(b.length() - 1);
        return b.length() == 0 ? "server" : b.toString();
    }
}

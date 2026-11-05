package online.bastion.launcher.core;

/** Проверка ника по правилам SA-MP / open.mp. */
public final class Nickname {
    public static final int MIN_LENGTH = 3;
    /** Сервер SA-MP отклоняет ники длиннее 20 символов. */
    public static final int MAX_LENGTH = 20;

    private Nickname() { }

    /** @return null, если ник подходит, иначе текст ошибки для игрока. */
    public static String validate(String nick, boolean requireRpName) {
        String n = nick == null ? "" : nick;
        if (n.length() == 0) return "Введи ник. Формат: Имя_Фамилия, например Ivan_Petrov.";
        if (n.length() < MIN_LENGTH || n.length() > MAX_LENGTH)
            return "Ник должен быть от " + MIN_LENGTH + " до " + MAX_LENGTH + " символов.";
        for (int i = 0; i < n.length(); i++) {
            if (!isAllowedChar(n.charAt(i)))
                return "В нике можно использовать только латиницу, цифры и символы _ [ ] ( ) $ @ . =";
        }
        if (requireRpName && !isRpName(n))
            return "На сервере нужен РП-ник: Имя_Фамилия латиницей, каждое слово с большой буквы. Например: Ivan_Petrov.";
        return null;
    }

    /** Набор символов, который принимает сервер SA-MP. */
    static boolean isAllowedChar(char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                || "_[]()$@.=".indexOf(c) >= 0;
    }

    public static boolean isRpName(String n) {
        return n != null && n.matches("^[A-Z][a-z]+_[A-Z][a-z]+$");
    }
}

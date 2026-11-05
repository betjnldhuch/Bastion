package online.bastion.launcher.core;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * Обновляет SAMP/settings.ini, который читает клиентский мод (jni/settings.cpp, библиотека inih).
 *
 * Меняются только ключи, которые принадлежат лаунчеру. Всё остальное — комментарии,
 * ключи [gui], которые ты правишь руками, порядок строк — остаётся как было.
 * inih не различает регистр секций и ключей, поэтому и мы сравниваем без учёта регистра.
 */
public final class SampSettingsIni {

    /** секция -> (ключ -> значение), в порядке добавления. */
    private final Map<String, Map<String, String>> updates = new LinkedHashMap<String, Map<String, String>>();
    /** Ключи, которые записываются, только если их ещё нет в файле. */
    private final Map<String, Map<String, String>> defaults = new LinkedHashMap<String, Map<String, String>>();

    /** Значение пишется всегда. */
    public SampSettingsIni set(String section, String key, String value) {
        put(updates, section, key, value);
        return this;
    }

    /** Значение пишется, только если в файле такого ключа нет (не перетираем ручные правки). */
    public SampSettingsIni setIfMissing(String section, String key, String value) {
        put(defaults, section, key, value);
        return this;
    }

    private static void put(Map<String, Map<String, String>> target, String section, String key, String value) {
        if (!isSafeValue(value)) throw new IllegalArgumentException("Недопустимое значение для " + key);
        String s = section.toLowerCase(Locale.ROOT);
        Map<String, String> m = target.get(s);
        if (m == null) { m = new LinkedHashMap<String, String>(); target.put(s, m); }
        m.put(key, value);
    }

    /**
     * inih считает «;» после пробела началом комментария, а перевод строки
     * разорвал бы файл. Такие значения записать корректно нельзя.
     */
    public static boolean isSafeValue(String v) {
        return v != null && v.indexOf('\n') < 0 && v.indexOf('\r') < 0 && v.indexOf(';') < 0
                && v.indexOf('\0') < 0;
    }

    /**
     * @param existing текущее содержимое файла или null, если файла нет.
     * @return новое содержимое (строки разделены \n).
     */
    public String apply(String existing) {
        List<String> lines = new ArrayList<String>();
        if (existing != null && existing.length() > 0) {
            String text = existing.replace("\r\n", "\n").replace('\r', '\n');
            if (text.endsWith("\n")) text = text.substring(0, text.length() - 1);
            for (String l : text.split("\n", -1)) lines.add(l);
        }

        // Что ещё осталось дописать: копии карт, из которых убираем найденные ключи.
        Map<String, Map<String, String>> pendingUpdates = deepCopy(updates);
        Map<String, Map<String, String>> pendingDefaults = deepCopy(defaults);

        List<String> out = new ArrayList<String>();
        // Ключи, которые мы уже записали: повторы ниже удаляем, иначе inih склеит значения.
        java.util.Set<String> written = new java.util.HashSet<String>();
        String section = "";
        for (int i = 0; i < lines.size(); i++) {
            String line = lines.get(i);
            String trimmed = line.trim();
            if (trimmed.startsWith("[") && trimmed.indexOf(']') > 0) {
                appendMissing(out, section, pendingUpdates, pendingDefaults);
                section = trimmed.substring(1, trimmed.indexOf(']')).trim().toLowerCase(Locale.ROOT);
                out.add(line);
                continue;
            }
            String key = keyOf(trimmed);
            if (key != null) {
                if (written.contains(section + '\u0000' + key)) continue;
                String newValue = take(pendingUpdates, section, key);
                // Ключ уже есть в файле — значение по умолчанию не нужно.
                take(pendingDefaults, section, key);
                if (newValue != null) {
                    written.add(section + '\u0000' + key);
                    out.add(originalKey(trimmed) + " = " + newValue);
                    continue;
                }
            }
            out.add(line);
        }
        appendMissing(out, section, pendingUpdates, pendingDefaults);

        // Секций, которых не было в файле, — дописываем в конец.
        for (String s : allSections(pendingUpdates, pendingDefaults)) {
            if (out.size() > 0 && out.get(out.size() - 1).trim().length() > 0) out.add("");
            out.add("[" + s + "]");
            appendMissing(out, s, pendingUpdates, pendingDefaults);
        }

        StringBuilder b = new StringBuilder();
        for (String l : out) b.append(l).append('\n');
        return b.toString();
    }

    /** Дописывает в конец текущей секции ключи, которых в ней не нашлось. */
    private static void appendMissing(List<String> out, String section,
                                      Map<String, Map<String, String>> pendingUpdates,
                                      Map<String, Map<String, String>> pendingDefaults) {
        List<String> add = new ArrayList<String>();
        Map<String, String> u = pendingUpdates.remove(section);
        if (u != null) for (Map.Entry<String, String> e : u.entrySet()) add.add(e.getKey() + " = " + e.getValue());
        Map<String, String> d = pendingDefaults.remove(section);
        if (d != null) for (Map.Entry<String, String> e : d.entrySet()) add.add(e.getKey() + " = " + e.getValue());
        if (add.isEmpty()) return;
        // Вставляем перед пустыми строками в конце секции, чтобы не ломать оформление.
        int at = out.size();
        while (at > 0 && out.get(at - 1).trim().length() == 0) at--;
        out.addAll(at, add);
    }

    private static List<String> allSections(Map<String, Map<String, String>> a, Map<String, Map<String, String>> b) {
        List<String> s = new ArrayList<String>(a.keySet());
        for (String k : b.keySet()) if (!s.contains(k)) s.add(k);
        return s;
    }

    private static String take(Map<String, Map<String, String>> pending, String section, String key) {
        Map<String, String> m = pending.get(section);
        if (m == null) return null;
        for (String k : new ArrayList<String>(m.keySet())) {
            if (k.equalsIgnoreCase(key)) return m.remove(k);
        }
        return null;
    }

    /** Ключ строки «key = value» или null для комментариев и мусора. */
    private static String keyOf(String trimmed) {
        if (trimmed.length() == 0 || trimmed.startsWith(";") || trimmed.startsWith("#")) return null;
        int eq = trimmed.indexOf('=');
        int colon = trimmed.indexOf(':');  // inih разрешает и «key: value»
        int sep = eq < 0 ? colon : (colon < 0 ? eq : Math.min(eq, colon));
        if (sep <= 0) return null;
        return trimmed.substring(0, sep).trim().toLowerCase(Locale.ROOT);
    }

    private static String originalKey(String trimmed) {
        int eq = trimmed.indexOf('=');
        int colon = trimmed.indexOf(':');
        int sep = eq < 0 ? colon : (colon < 0 ? eq : Math.min(eq, colon));
        return trimmed.substring(0, sep).trim();
    }

    private static Map<String, Map<String, String>> deepCopy(Map<String, Map<String, String>> src) {
        Map<String, Map<String, String>> copy = new LinkedHashMap<String, Map<String, String>>();
        for (Map.Entry<String, Map<String, String>> e : src.entrySet())
            copy.put(e.getKey(), new LinkedHashMap<String, String>(e.getValue()));
        return copy;
    }
}

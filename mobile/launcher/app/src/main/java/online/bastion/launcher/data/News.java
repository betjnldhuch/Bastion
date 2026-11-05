package online.bastion.launcher.data;

import android.content.Context;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Новости из assets/news.txt. Формат строки: дата | заголовок | текст. */
public final class News {

    public static final class Item {
        public final String date, title, text;
        Item(String date, String title, String text) {
            this.date = date;
            this.title = title;
            this.text = text;
        }
    }

    private News() { }

    public static List<Item> load(Context context) {
        List<Item> items = new ArrayList<Item>();
        BufferedReader r = null;
        try {
            r = new BufferedReader(new InputStreamReader(context.getAssets().open("news.txt"), "UTF-8"));
            String line;
            while ((line = r.readLine()) != null) {
                line = line.replace("﻿", "").trim();
                if (line.length() == 0 || line.startsWith(";")) continue;
                String[] p = line.split("\\|", 3);
                if (p.length == 3) items.add(new Item(p[0].trim(), p[1].trim(), p[2].trim()));
            }
        } catch (IOException e) {
            return Collections.emptyList();  // нет файла — просто нет новостей
        } finally {
            if (r != null) try { r.close(); } catch (IOException ignored) { }
        }
        return items;
    }
}

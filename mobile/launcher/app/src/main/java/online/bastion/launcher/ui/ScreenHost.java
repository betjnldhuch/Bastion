package online.bastion.launcher.ui;

import android.app.Activity;

import online.bastion.launcher.core.LauncherConfig;
import online.bastion.launcher.data.LauncherPrefs;
import online.bastion.launcher.net.ServerMonitor;

/** Что экранам нужно от Activity. Экраны не знают про MainActivity напрямую. */
public interface ScreenHost {
    Activity activity();
    LauncherConfig config();
    LauncherPrefs prefs();
    ServerMonitor monitor();

    void openMain();
    void openSettings();
    /** Нажата кнопка «Играть». */
    void play();
    void showDiagnostics();
}

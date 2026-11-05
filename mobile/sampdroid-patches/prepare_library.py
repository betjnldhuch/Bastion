#!/usr/bin/env python3
"""Convert the pinned SampDroid Android app module into an embeddable library."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1] / "sampdroid"
gradle = root / "app/build.gradle.kts"
text = gradle.read_text(encoding="utf-8")

text = text.replace('id("com.android.application")', 'id("com.android.library")', 1)
text = re.sub(r'(?m)^\s*applicationId\s*=\s*"[^"]+"\s*\n', '', text)
text = re.sub(r'(?m)^\s*versionCode\s*=\s*\d+\s*\n', '', text)
text = re.sub(r'(?m)^\s*versionName\s*=\s*"[^"]+"\s*\n', '', text)
# Library modules cannot own APK signing configuration. The launcher is the
# only installable application and controls signing for the combined APK.
text = re.sub(r'(?ms)^val officialKeystorePath.*?^}\n\n(?=android \{)', '', text)
text = re.sub(r'(?ms)^\s*signingConfigs \{.*?^\s*}\n', '', text)
text = re.sub(r'(?m)^\s*signingConfig\s*=.*\n', '', text)
if 'id("com.android.library")' not in text or 'applicationId' in text:
    raise SystemExit("Could not safely convert SampDroid Gradle module to library")
gradle.write_text(text, encoding="utf-8")

manifest = root / "app/src/main/AndroidManifest.xml"
xml = manifest.read_text(encoding="utf-8")
xml, count = re.subn(r'\s*<intent-filter>\s*<action android:name="android.intent.action.MAIN"\s*/>\s*<category android:name="android.intent.category.LAUNCHER"\s*/>\s*</intent-filter>', '', xml, count=1, flags=re.S)
if count != 1:
    raise SystemExit("Expected SampDroid launcher intent-filter was not found")
xml = xml.replace('android:exported="true"', 'android:exported="false"', 1)
manifest.write_text(xml, encoding="utf-8")
print("SampDroid converted to an Android library; its Activity is embedded, not a launcher.")

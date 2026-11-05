#!/usr/bin/env python3
"""Convert the pinned SampDroid Android app module into an embeddable library."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1] / "sampdroid"
gradle = root / "app/build.gradle.kts"
text = gradle.read_text(encoding="utf-8")

def remove_gradle_block(source: str, block_name: str) -> str:
    match = re.search(r"(?m)^\s*" + re.escape(block_name) + r"\s*\{", source)
    if not match:
        return source
    opening = source.find("{", match.start())
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[:match.start()] + source[index + 1:]
    raise SystemExit("Unbalanced Gradle block: " + block_name)

text = text.replace('id("com.android.application")', 'id("com.android.library")', 1)
text = re.sub(r'(?m)^\s*applicationId\s*=\s*"[^"]+"\s*\n', '', text)
text = re.sub(r'(?m)^\s*versionCode\s*=\s*\d+\s*\n', '', text)
text = re.sub(r'(?m)^\s*versionName\s*=\s*"[^"]+"\s*\n', '', text)
# Library modules cannot own APK signing configuration. The launcher is the
# only installable application and controls signing for the combined APK.
text = re.sub(r'(?ms)^val officialKeystorePath.*?^}\n\n(?=android \{)', '', text)
text = remove_gradle_block(text, "signingConfigs")
text = re.sub(r'(?ms)^\s*if \(hasOfficialReleaseSigning\) \{.*?^\s*\}\s*', '', text)
text = re.sub(r'(?m)^\s*signingConfig\s*=.*\n', '', text)
text = re.sub(r'(?m)^\s*isDebuggable\s*=\s*true\s*\n', '', text)
if 'id("com.android.library")' not in text or 'applicationId' in text or 'signingConfigs' in text:
    raise SystemExit("Could not safely convert SampDroid Gradle module to library")
gradle.write_text(text, encoding="utf-8")

manifest = root / "app/src/main/AndroidManifest.xml"
xml = manifest.read_text(encoding="utf-8")
xml, count = re.subn(r'\s*<intent-filter>\s*<action android:name="android.intent.action.MAIN"\s*/>\s*<category android:name="android.intent.category.LAUNCHER"\s*/>\s*</intent-filter>', '', xml, count=1, flags=re.S)
if count != 1:
    raise SystemExit("Expected SampDroid launcher intent-filter was not found")
xml = xml.replace('android:exported="true"', 'android:exported="false"', 1)
xml = xml.replace('android:name=".ui.MainActivity" android:exported="false"', 'android:name=".ui.MainActivity" android:exported="false" android:theme="@style/Theme.SampDroid"', 1)
manifest.write_text(xml, encoding="utf-8")
print("SampDroid converted to an Android library; its Activity is embedded, not a launcher.")

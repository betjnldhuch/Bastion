#!/usr/bin/env bash
# Сборка APK без Android Studio (Linux, пакеты Ubuntu: android-sdk,
# android-sdk-platform-23, dalvik-exchange, openjdk). В Android Studio
# можно просто открыть эту папку — там сборка идёт через Gradle.
set -euo pipefail
cd "$(dirname "$0")"

SDK=${ANDROID_SDK:-/usr/lib/android-sdk}
BT=$(ls -d "$SDK"/build-tools/* | grep -v debian | sort -V | tail -1)
JAR="$SDK/platforms/android-23/android.jar"
SRC=app/src/main
OUT=build
KS=${BASTION_KEYSTORE:-tools/debug.keystore}
VERSION_CODE=$(grep -o 'versionCode [0-9]*' app/build.gradle | grep -o '[0-9]*')
VERSION_NAME=$(grep -o 'versionName "[^"]*"' app/build.gradle | cut -d'"' -f2)

rm -rf "$OUT" && mkdir -p "$OUT/gen" "$OUT/classes"

if [ ! -f "$KS" ]; then
  keytool -genkeypair -keystore "$KS" -storepass android -keypass android \
    -alias bastiondebug -keyalg RSA -keysize 2048 -validity 10000 \
    -dname "CN=Bastion Online Debug" >/dev/null 2>&1
fi

# В исходном манифесте нет package (так требует Gradle), aapt он нужен.
sed 's|<manifest xmlns:android="http://schemas.android.com/apk/res/android">|<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="online.bastion.launcher">|' \
  "$SRC/AndroidManifest.xml" > "$OUT/AndroidManifest.xml"

"$BT/aapt" package -f -m -J "$OUT/gen" -M "$OUT/AndroidManifest.xml" \
  --min-sdk-version 21 --target-sdk-version 23 \
  --version-code "$VERSION_CODE" --version-name "$VERSION_NAME" \
  -S "$SRC/res" -A "$SRC/assets" -I "$JAR" -F "$OUT/unsigned.apk"

javac -nowarn -Xlint:-options -source 8 -target 8 -encoding UTF-8 \
  -bootclasspath "$JAR" -d "$OUT/classes" \
  $(find "$SRC/java" "$OUT/gen" -name '*.java')

dalvik-exchange --dex --min-sdk-version=21 --output="$OUT/classes.dex" "$OUT/classes"
(cd "$OUT" && "$BT/aapt" add unsigned.apk classes.dex >/dev/null)

"$BT/zipalign" -f -p 4 "$OUT/unsigned.apk" "$OUT/aligned.apk"
"$BT/apksigner" sign --ks "$KS" --ks-pass pass:android --key-pass pass:android \
  --out "$OUT/BastionOnline.apk" "$OUT/aligned.apk"
"$BT/apksigner" verify "$OUT/BastionOnline.apk"
echo "Готово: $OUT/BastionOnline.apk"

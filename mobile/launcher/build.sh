#!/usr/bin/env bash
# Build the single Bastion Online APK (launcher + embedded native multiplayer client).
set -euo pipefail
cd "$(dirname "$0")"

ROOT="$(cd ../.. && pwd)"
CLIENT="$ROOT/mobile/client/sampdroid"

if [ ! -f "$CLIENT/app/src/main/cpp/CMakeLists.txt" ]; then
  if [ -d "$ROOT/.git" ]; then
    git -C "$ROOT" submodule update --init --recursive mobile/client/sampdroid
  fi
fi

if [ ! -f "$CLIENT/app/src/main/cpp/CMakeLists.txt" ]; then
  echo "Embedded client source is missing. Clone with --recurse-submodules or run:"
  echo "  git submodule update --init --recursive"
  exit 1
fi

if [ -x ./gradlew ]; then
  GRADLE=./gradlew
elif command -v gradle >/dev/null 2>&1; then
  GRADLE=gradle
else
  echo "Gradle is required. Open mobile/launcher in Android Studio or install Gradle."
  exit 1
fi

"$GRADLE" --no-daemon --project-dir . assembleDebug

APK="app/build/outputs/apk/debug/app-debug.apk"
if [ ! -f "$APK" ]; then
  echo "Gradle finished but APK was not found at $APK"
  exit 1
fi

mkdir -p build
cp "$APK" build/BastionOnline.apk
echo "Bastion APK: $(pwd)/build/BastionOnline.apk"

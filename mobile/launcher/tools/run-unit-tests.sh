#!/usr/bin/env bash
# Юнит-тесты ядра (пакет core: адреса, ники, launcher.ini, settings.ini, query-протокол)
# без Android SDK и Gradle. Нужны JDK и JUnit 4 (Ubuntu: apt install junit4).
# В Android Studio те же тесты запускаются через Gradle: ./gradlew :app:testDebugUnitTest
set -euo pipefail
cd "$(dirname "$0")/.."

CP=${JUNIT_CP:-/usr/share/java/junit4.jar:/usr/share/java/hamcrest-core.jar}
OUT=build/unit-tests
rm -rf "$OUT" && mkdir -p "$OUT"

javac -nowarn -encoding UTF-8 -cp "$CP" -d "$OUT" \
  $(find app/src/main/java/online/bastion/launcher/core app/src/test/java -name '*.java')
java -cp "$OUT:$CP" org.junit.runner.JUnitCore online.bastion.launcher.core.CoreTest

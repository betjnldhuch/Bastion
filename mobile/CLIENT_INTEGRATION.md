# Bastion Android single-app integration

## Release target

The player installs and launches one application: **Bastion Online**.
The launcher, file installer, server/profile setup, and multiplayer screen live in
one APK and one Android application ID. No second launcher icon or separately
installed client is part of the intended player flow.

## Embedded client source

The Android multiplayer module is pinned as a Git submodule at
`mobile/client/sampdroid`. The pin currently references upstream commit
`f9daca1f4271dfd34c4aaeeaa4aaf21ae0e31d86`.

The Android app Gradle module includes the embedded Kotlin sources, resources,
assets, and CMake native library. It declares the client's main screen as a
non-exported internal Activity. The build generates a Bastion-branded copy of
that screen's title/subtitle without modifying the upstream checkout.

The embedded module currently targets:
- Android API 24+;
- `armeabi-v7a` and `arm64-v8a`;
- Kotlin 1.9.24 / Android Gradle Plugin 8.5.2;
- CMake 3.22.1 and NDK 26.1.10909125.

## Launcher-to-game handoff

Before opening the internal game Activity, Bastion writes the chosen nickname
and selected server host/port to the embedded client's own local preference
stores. This is an in-process handoff; it does not use cross-app intents,
another app's private storage, or undocumented exported APIs.

## License and branding

The embedded source is MIT-licensed. Preserve its LICENSE and NOTICE files and
the copyright/license notices in redistributed source and binary distributions.
The app icon, package ID, launcher screen, and in-game client screen are branded
as Bastion Online; license attribution remains available in the open-source
notices. Branding does not imply that Bastion authored the upstream code.

## Remaining verification and work

- The integrated Gradle/CMake build has not yet been run in this environment.
  Do not distribute an APK until CI and a device build pass.
- Test both ARM ABIs on Android 7+ and verify the native library loads.
- Test the full connection and gameplay path against the actual Bastion server;
  the upstream client is experimental and compatibility is not assumed.
- The installer currently stages Bastion-owned files in the app's private
  directory. It does not yet mount those files into the client's native game
  data runtime. Add a typed resource adapter only after supported resource
  formats and runtime APIs are verified.
- Players must provide their own legally obtained GTA: San Andreas game data.
  Do not bundle or download proprietary GTA game files.

## Build

Clone with submodules:

```bash
git clone --recurse-submodules https://github.com/the-gta-man/Bastion.git
cd Bastion/mobile/launcher
./build.sh
```

Or, from an existing clone:

```bash
git submodule update --init --recursive
```

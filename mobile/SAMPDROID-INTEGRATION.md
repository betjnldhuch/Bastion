# Bastion native Android client — embedded single APK

The native SA-MP/open.mp client is pinned as a Git submodule at
`mobile/sampdroid`:

- Upstream: https://github.com/xykal/sampdroid
- Pinned commit: `f9daca1f4271dfd34c4aaeeaa4aaf21ae0e31d86`
- Upstream license: MIT (see the submodule's `LICENSE` and `NOTICE.md`).

## One installable application

The only installable package is `online.bastion.launcher`. The Bastion launcher
remains the launcher Activity and visual shell. The SampDroid source module is
converted from an Android application to an Android library in the CI checkout,
then linked into the launcher APK. Its launcher intent-filter and application ID
are removed during conversion. The game Activity is launched explicitly inside
the same package; there is no `online.bastion.client` APK.

The client is built for `armeabi-v7a` and `arm64-v8a`. GTA game assets and
proprietary GTA binaries are not redistributed; players select game data they
are entitled to use.

## Build

Run **Build single Bastion Android APK** from GitHub Actions, or push a change
to the integration branch. Download the `Bastion-Launcher-Embedded-debug`
artifact. The workflow checks the resulting APK package name is exactly
`online.bastion.launcher` and rejects the former separate client package.

## Bastion patches

The upstream submodule remains pinned and unmodified in Git. CI applies patches
to its temporary checkout:

- **Autoconnect:** the launcher passes `bastion.host`, `bastion.port`,
  `bastion.name`, and `bastion.nick`; the client saves the nickname and opens
  the matching server connection screen.
- **Russian UI:** `ru_strings.py` translates player-facing strings found in
  the source and resources. Any missing dictionary keys are reported in CI logs;
  this is an ongoing localization pass, not a claim that every diagnostic/native
  string has already been translated.

A successful APK build proves packaging and compilation only. Actual gameplay,
device installation, and server admission still require phone testing.

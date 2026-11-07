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

Run **Build single Bastion Android APK** (`.github/workflows/bastion-single-apk.yml`) from GitHub Actions, or push a change
to the integration branch. Download the `Bastion-Launcher-Embedded-debug`
artifact. The workflow checks the resulting APK package name is exactly
`online.bastion.launcher` and rejects the former separate client package.

## Bastion patches

The upstream submodule remains pinned and unmodified in Git. CI applies patches
to its temporary checkout:

- **Autoconnect:** the launcher passes `bastion.host`, `bastion.port`,
  `bastion.name`, and `bastion.nick`; the client saves the nickname and opens
  the matching server connection screen.
- **Game data from the Bastion installer:** the launcher's ZIP installer unpacks
  the game into `files/game`; the launcher passes the folder with `data/gta.dat`
  as `bastion.game_dir`, and the client mounts it as a `file://` tree (no SAF
  folder picker). `GtaDataTreeResolver.fromTreeUri` accepts `file://` URIs.
- **Russian UI:** `ru_strings.py` translates player-facing strings found in
  the source and resources. Any missing dictionary keys are reported in CI logs;
  this is an ongoing localization pass, not a claim that every diagnostic/native
  string has already been translated.

- **open.mp handshake, login and spawn** (`native-openmp-rpc-spawn-cp1251.patch`,
  applied first by `apply.py`):
  - RPCs use the real RakPeer::RPC framing (`ID_RPC, id, WriteCompressed(bits),
    data`). Upstream SampDroid omitted the bit count, so open.mp dropped
    ClientJoin and never sent PlayerInit ("Menunggu PlayerInit").
  - `TogglePlayerSpectating(false)` after spectating and `SpawnPlayer()`
    (bodyless RPC 129) make the client send PlayerSpawn, like the PC client.
    The Bastion gamemode spawns players this way after login.
  - Windows-1251 server text (dialogs, chat) is shown as UTF-8, player input
    (chat, commands, dialog text/passwords) is sent as Windows-1251, and the JNI
    boundary never passes invalid UTF-8 to `NewStringUTF`.
- **Server dialogs:** `{RRGGBB}` colour codes are rendered in the dialog body and
  stripped from titles, buttons, list rows and chat; tapping outside a dialog no
  longer cancels it (Bastion kicks on a cancelled login dialog).
- **handling.cfg:** boat/bike/plane sections and records the Android vehicle
  model cannot use (e.g. REEFER's negative traction on line 281) are skipped
  instead of rejecting the whole game folder; physics values are not changed.

## Verification

- `Build single Bastion Android APK` runs the SampDroid native test suite with
  the Bastion patches, builds both ABIs, checks the APK, and on
  `integrate/openmp-*` branches publishes it as a GitHub pre-release.
- `Bastion e2e` builds the same patched native client for the host and runs it
  against a real open.mp server with `server/gamemodes/rp.pwn`: register ->
  gender -> spawn, then login -> spawn, with Russian dialogs and chat.

A successful APK build proves packaging and compilation only. Actual gameplay,
device installation, and server admission still require phone testing.

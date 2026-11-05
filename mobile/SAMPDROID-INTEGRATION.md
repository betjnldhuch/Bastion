# Bastion native Android client

The native SA-MP/open.mp client is pinned as a Git submodule at
`mobile/sampdroid`:

- Upstream: https://github.com/xykal/sampdroid
- Pinned commit: `f9daca1f4271dfd34c4aaeeaa4aaf21ae0e31d86`
- Upstream license: MIT (see the submodule's `LICENSE` and `NOTICE.md`).

The upstream source is kept intact in Git. The CI build applies only local
branding at build time: Android application ID `online.bastion.client` and
display name `Bastion Online`. It builds both `armeabi-v7a` and
`arm64-v8a` variants from the upstream NDK configuration.

## Build

Run the **Build Bastion native SA-MP client** workflow manually from GitHub
Actions, or push a change to this integration branch. Download the
`Bastion-native-client-debug` artifact from the completed workflow run.

The existing Bastion launcher remains in `mobile/launcher`. During this
integration stage the native client is a separate APK/package; the launcher
will open it when installed. Passing server/nickname settings into the native
session still needs an explicit integration with the client's connection UI.

## Important status

This is an experimental native client, not a bundled GTA:SA distribution.
It does not contain GTA game assets or proprietary GTA binaries. Players must
select/provide game data they are entitled to use. The upstream README documents
which gameplay and live-server compatibility areas remain experimental.

## Bastion patches

`mobile/sampdroid-patches/apply.py` runs in CI after branding and before the
Gradle build. It only edits the build checkout, the submodule stays pinned:

- **Autoconnect:** the launcher starts `online.bastion.client` with extras
  `bastion.host`, `bastion.port`, `bastion.name`, `bastion.nick`. The client saves
  the nickname, queries the server and opens its connect screen directly.
- **Russian UI:** `ru_strings.py` maps player-facing SampDroid strings to Russian.
  Debug/developer screens stay in the original language. The script prints any
  string it could not find, so an upstream update shows what to re-check.

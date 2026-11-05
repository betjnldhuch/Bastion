# Bastion Android native client build

The GitHub Actions workflow `.github/workflows/bastion-native-client.yml` checks out the SA-MP native source from `the-gta-man/-`, applies compatibility fixes, and attempts to compile the ARMv7 native library.

## Run the build

Open **Actions → Build Bastion native SA-MP client → Run workflow**.

The workflow uploads an artifact named `Bastion-SAMP-native-armeabi-v7a` containing `libsamp.so`.

## Important limitation

The artifact is only a native shared library. It is **not an installable APK** and does not include an Android Activity/app shell, launcher integration, game cache, or GTA: San Andreas assets. A compatible Android client shell and legally obtained game assets are still required before the launcher can start a complete game.

If the workflow fails, inspect the Actions job log; this legacy source may need further build fixes.

# Bastion Android client installer

The launcher reads this repository's manifest:

`https://raw.githubusercontent.com/the-gta-man/Bastion/main/mobile/installer/manifest.json`

Files are installed under the launcher's app-specific directory:

`Android/data/online.bastion.launcher/files/client/`

Each manifest entry must describe a file that the Bastion project is authorized to redistribute:

- `path`: relative destination path (letters, digits, dots, underscores, hyphens and slashes).
- `url`: HTTPS download URL.
- `size`: exact byte size.
- `sha256`: lowercase SHA-256 checksum.

Example:

```json
{
  "version": 1,
  "product": "Bastion Android Client Assets",
  "files": [
    {
      "path": "bastion/config/client.ini",
      "url": "https://downloads.example.org/bastion/client.ini",
      "size": 12345,
      "sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    }
  ]
}
```

The installer validates the manifest, rejects unsafe paths and non-HTTPS URLs, downloads each file to a temporary file, verifies its exact size and SHA-256, and only then replaces the destination. Existing files are kept if a download or verification fails.

The manifest is intentionally empty until redistributable client assets are published. The server-side Pawn gamemode archive is not an Android client package and must not be listed here. Original GTA: San Andreas game data and third-party app APKs are not bundled or redistributed by this installer.

## Direct ZIP install from the launcher

The Settings screen also has a direct ZIP installer. Paste a direct HTTPS URL to a ZIP containing the client files; the launcher downloads it, shows download progress, safely extracts it, and installs the extracted contents to the same `client/` directory. The ZIP's internal folder structure is preserved.

The ZIP is first extracted into a staging directory. The current client folder is only replaced after extraction succeeds. Archive entries that attempt path traversal or write outside the destination are rejected. Download size is limited to 2 GiB and extracted data to 4 GiB.

This mode accepts a URL supplied by the launcher user; it does not assume that a third-party launcher APK, GTA game data, or the server-side Pawn gamemode is a usable Android client. The ZIP must contain files the project is authorized to use.

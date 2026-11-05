# Bastion Android client installer

The Android launcher downloads only the files listed in `manifest.json`.
Downloads are saved under the launcher's private app storage:

`Android/data/online.bastion.launcher/files/client/`

Each item in `files` must contain:

- `path`: relative destination path, using letters, digits, dots, underscores, hyphens and slashes.
- `url`: HTTPS URL to the file.
- `size`: exact file size in bytes.
- `sha256`: lowercase SHA-256 checksum (64 hexadecimal characters).

Example entry (replace with a real, authorized file URL and its actual size/hash):

```json
{
  "path": "models/example.dff",
  "url": "https://downloads.example.org/bastion/models/example.dff",
  "size": 12345,
  "sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
}
```

The example is illustrative only; it is not a real downloadable asset. The repository's manifest intentionally starts with an empty `files` array until redistributable client assets and their checksums are supplied.

The installer verifies exact size and SHA-256 before writing each file. It does not download or install the GTA: San Andreas APK, does not install another launcher, and does not write into another app's private Android/data directory.

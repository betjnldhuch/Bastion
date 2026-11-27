"""Build the Unity Station street patches for the PC client pack."""
import hashlib
import io
import json
import struct
from pathlib import Path

from PIL import Image

from txdpatch import hdr

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "client-mods/streets/src"
PACK = ROOT / "launcher/client-pack"
RW_VERSION = 0x1803FFFF

# The same material occurs in multiple dictionaries near the station.
DICTIONARIES = {
    "lanroad": ("crossing_law", "easykerb", "sidelatino1_LAe", "sl_pavebutt1",
                "snpedtest1", "tarmacplain_bank"),
    "union_las": ("crazypave", "easykerb", "snpedtest1", "tarmacplain_bank"),
    "railway_las": ("concretenewb256",),
    "civic06_lan": ("tarmacplain_bank",),
}
NAMES = sorted({name for names in DICTIONARIES.values() for name in names})


def mobile_images():
    for name in NAMES:
        with Image.open(SOURCE / (name + ".png")) as image:
            image.thumbnail((512, 512), Image.Resampling.LANCZOS)
            yield name.lower(), image.convert("RGBA")


def street_native(name):
    with Image.open(SOURCE / (name + ".png")) as source:
        image = source.convert("RGB")
    size = image.size
    levels = []
    while True:
        dds = io.BytesIO()
        image.save(dds, format="DDS", pixel_format="DXT1")
        block = dds.getvalue()[128:]
        expected = ((image.width + 3) // 4) * ((image.height + 3) // 4) * 8
        if len(block) != expected:
            raise ValueError(f"Invalid DXT1 mip size for {name}")
        levels.append(struct.pack("<I", len(block)) + block)
        if image.size == (1, 1):
            break
        image = image.resize((max(1, image.width // 2), max(1, image.height // 2)),
                             Image.Resampling.LANCZOS)
    header = struct.pack("<II32s32sI4sHH4B", 9, 0x1106, name.encode("ascii"), b"",
                         0x8200, b"DXT1", *size, 16, len(levels), 4, 8)
    body = hdr(1, header + b"".join(levels), RW_VERSION) + hdr(3, b"", RW_VERSION)
    return hdr(21, body, RW_VERSION)


def main():
    from build_mobile import OUT, btx, write_version

    rasters = {name: street_native(name) for name in NAMES}
    manifest = PACK / "client-pack.json"
    data = json.loads(manifest.read_bytes())
    entries = {entry["url"]: entry for entry in data["files"]}
    changed = False
    for dictionary, names in DICTIONARIES.items():
        for name in names:
            url = f"pack/models/gta3.img/{dictionary}.txd/{name}.tex"
            payload = rasters[name]
            dest = PACK / url
            dest.parent.mkdir(parents=True, exist_ok=True)
            if not dest.exists() or dest.read_bytes() != payload:
                dest.write_bytes(payload)
            entry = dict(path="models/gta3.img", name=dictionary + ".txd", texture=name,
                         url=url, size=len(payload), sha256=hashlib.sha256(payload).hexdigest())
            if entries.get(url) != entry:
                changed = True
                entries[url] = entry
    if changed:
        data["version"] += 1
        data["files"] = list(entries.values())
        pending = manifest.with_suffix(".json.tmp")
        pending.write_bytes((json.dumps(data, ensure_ascii=False, indent=2) + "\n").encode())
        pending.replace(manifest)

    for key, image in mobile_images():
        (Path(OUT) / "tex" / (key + ".btx")).write_bytes(btx(image, key))
    write_version()


if __name__ == "__main__":
    main()

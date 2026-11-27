"""Read the HD vehicle rasters selected for the mobile texture database."""
import json
from pathlib import Path
import struct
from PIL import Image
import rw


def mobile_transport(pack):
    pack = Path(pack)
    manifest = pack.parent.parent / 'transport-textures.json'
    if not manifest.is_file():
        return
    rules = json.loads(manifest.read_bytes())
    for path, textures in rules.items():
        txd = (pack / path).read_bytes()
        root, begin, end = next(rw.chunks(txd, 0, len(txd)))
        if root != 0x16:
            raise ValueError(f'{path} is not a texture dictionary')
        remaining = set(textures)
        for kind, start, stop in rw.chunks(txd, begin, end):
            if kind != 0x15:
                continue
            _, at, limit = next(rw.chunks(txd, start, stop))
            name = txd[at+8:at+40].split(b'\0')[0].decode('ascii').lower()
            if name not in textures:
                continue
            alias, edge = textures[name]
            fmt, w, h = struct.unpack_from('<4sHH', txd, at+76)
            decoder = {b'DXT1': 1, b'DXT5': 3}[fmt]
            length, = struct.unpack_from('<I', txd, at+88)
            if at+92+length > limit:
                raise ValueError(f'Truncated vehicle texture {path}/{name}')
            image = Image.frombytes('RGBA', (w,h), txd[at+92:at+92+length], 'bcn', decoder)
            if decoder == 1 and not txd[at+87] & 1:
                image.putalpha(255)
            image.thumbnail((edge,edge), Image.Resampling.LANCZOS, reducing_gap=None)
            remaining.remove(name)
            yield alias, image
        if remaining:
            raise ValueError(f'Missing textures in {path}: {sorted(remaining)}')

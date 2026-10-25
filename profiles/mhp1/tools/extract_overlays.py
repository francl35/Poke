"""Extract plain MWo3 overlays from MHP1 DATA.BIN."""
import argparse
import collections
from pathlib import Path
import re
import struct

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("archive", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()

size = args.archive.stat().st_size
if size % 2048:
    raise SystemExit("Archive size is not sector-aligned")

with args.archive.open("rb") as source:
    first = source.read(4)
    if len(first) != 4:
        raise SystemExit("Truncated archive")
    directory_size = struct.unpack("<I", first)[0] * 2048
    if not 4 <= directory_size <= min(size, 1024 * 1024):
        raise SystemExit("Invalid directory size")
    source.seek(0)
    directory = source.read(directory_size)
    words = struct.unpack(f"<{len(directory)//4}I", directory)
    stop = next(
        (i for i, value in enumerate(words) if value == size // 2048),
        None,
    )
    if stop is None:
        raise SystemExit("Archive-end marker missing")
    offsets = words[:stop + 1]
    if any(a > b for a, b in zip(offsets, offsets[1:])):
        raise SystemExit("Non-monotonic directory")

    images = {}
    counts = collections.Counter()
    for index, (start, end) in enumerate(zip(offsets, offsets[1:])):
        padded_size = (end - start) * 2048
        if padded_size < 64:
            continue
        source.seek(start * 2048)
        header = source.read(64)
        if header[:4] != b"MWo3":
            continue
        base, code_size, data_size = struct.unpack_from("<III", header, 8)
        name = header[32:64].split(b"\0", 1)[0].decode("ascii")
        if not re.fullmatch(r"[A-Za-z0-9_]+\.ovl", name):
            raise SystemExit(f"Entry {index}: unexpected name {name!r}")
        image_size = 64 + code_size + data_size
        if code_size % 4 or image_size > padded_size:
            raise SystemExit(f"Entry {index}: invalid section sizes")
        source.seek(start * 2048)
        image = source.read(image_size)
        filename = f"overlay_{base:08X}_{name[:-4]}.bin"
        if filename in images:
            raise SystemExit(f"Duplicate overlay: {filename}")
        images[filename] = image
        counts[base] += 1

args.output.mkdir(parents=True, exist_ok=True)
for filename, image in images.items():
    destination = args.output / filename
    if destination.exists() and destination.read_bytes() != image:
        raise SystemExit(f"Refusing to overwrite different file: {destination}")
for filename, image in images.items():
    (args.output / filename).write_bytes(image)

print(f"Extracted {len(images)} overlays into {args.output}")
for base, count in sorted(counts.items()):
    print(f"  0x{base:08X}: {count} overlays")

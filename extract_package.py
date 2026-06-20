#!/usr/bin/env python3
"""Extract all resources from a Sims 4 DBPF v2.1 package file."""

import struct
import sys
import zlib
import os
import json
from pathlib import Path

HEADER_SIZE = 96
INDEX_ENTRY_SIZE = 32
COMPRESSED_FLAG = 0x80000000

# Extension by resource type GUID (Sims 4).
EXTENSION_BY_TYPE = {
    0x00B2D882: ".dds",       # DDS texture
    0x2BC04EDF: ".lrle",      # LRLE image
    0x3453CF95: ".dds",       # DXT5 RLE (decodes to DDS-like)
    0xBA856C78: ".dds",       # DXT5 RLE (variant magic "DXT5RLES")
    0x3C2A8647: ".jpg",       # JPEG
    0x220557DA: ".stbl",      # string table
    0x545AC67A: ".data",      # compiled data
    0x034AEECB: ".data",
    0x6B20C4F3: ".rig",       # RIG skeleton
    0xBC4A5044: ".rig",       # RIG skeleton (alt type id)
    0x376840D7: ".clip",      # animation clip ("MVhd")
    0xBDD82221: ".auev",      # audio event ("AUEV")
    0x81CA1A10: ".mtbl",      # modulation table ("MTBL")
    0xC0DB5AE7: ".sculpt",    # sculpt table
}


def detect_extension(rtype: int, data: bytes) -> str:
    ext = EXTENSION_BY_TYPE.get(rtype)
    if ext:
        return ext
    if data[:5] == b"<?xml":
        return ".xml"
    if data[:4] == b"DDS ":
        return ".dds"
    if data[:3] == b"\xff\xd8\xff":
        return ".jpg"
    if data[:4] == b"STBL":
        return ".stbl"
    if data[:4] == b"LRLE":
        return ".lrle"
    if data[:5] == b"DATA\x00" or data[:4] == b"DATA":
        return ".data"
    if data[:4] == b"MVhd":
        return ".clip"
    if data[:4] == b"AUEV":
        return ".auev"
    if data[:4] == b"MTBL":
        return ".mtbl"
    if data[:4] == b"DXT5":
        return ".dds"
    return ".bin"


def parse_header(data: bytes):
    if data[:4] != b"DBPF":
        raise ValueError("Not a DBPF package file (bad magic)")
    major, minor = struct.unpack_from("<II", data, 4)
    entry_count = struct.unpack_from("<I", data, 0x24)[0]
    index_size = struct.unpack_from("<I", data, 0x2C)[0]
    index_offset = struct.unpack_from("<Q", data, 0x40)[0]
    return {
        "major": major,
        "minor": minor,
        "entry_count": entry_count,
        "index_size": index_size,
        "index_offset": index_offset,
    }


def parse_index_entry(buf: bytes):
    (rtype, group, inst_ex, inst, offset,
     size_flag, uncompressed_size, extra) = struct.unpack_from(
        "<IIIIIIII", buf, 0)
    compressed = bool(size_flag & COMPRESSED_FLAG)
    stored_size = size_flag & 0x7FFFFFFF
    return {
        "type": rtype,
        "group": group,
        "instance_ex": inst_ex,
        "instance": inst,
        "instance_full": (inst_ex << 32) | inst,
        "offset": offset,
        "size_flag": size_flag,
        "compressed": compressed,
        "stored_size": stored_size,
        "uncompressed_size": uncompressed_size,
        "extra": extra,
    }


def tgi_name(e: dict) -> str:
    return f"{e['type']:08X}-{e['group']:08X}-{e['instance_full']:016X}"


def dbpf_decompress(raw: bytes, expected: int | None = None) -> bytes:
    """Decompress a DBPF compressed resource.

    Sims 4 stores compressed resources either as a raw zlib stream or using
    the DBPF block compression (chunked zlib with a small header). Try both.
    """
    # Plain zlib (commonly seen in mod packages).
    try:
        out = zlib.decompress(raw)
        if expected is None or len(out) == expected:
            return out
    except zlib.error:
        pass

    # DBPF block compression: a series of chunks, each prefixed with a
    # 4-byte little-endian uncompressed size and 4-byte compressed size,
    # followed by raw DEFLATE data. Preceded by a single header byte.
    try:
        return _dbpf_block_decompress(raw, expected)
    except Exception:
        pass

    raise ValueError("could not decompress resource")


def _dbpf_block_decompress(raw: bytes, expected: int | None) -> bytes:
    out = bytearray()
    pos = 1  # skip leading compression-type byte
    n = len(raw)
    while pos < n:
        if pos + 8 > n:
            break
        usize, csize = struct.unpack_from("<II", raw, pos)
        pos += 8
        if csize == 0 and usize == 0:
            break
        chunk = raw[pos:pos + csize]
        pos += csize
        out += zlib.decompress(chunk)
    if expected is not None and len(out) != expected:
        raise ValueError(f"decompressed size mismatch: {len(out)} != {expected}")
    return bytes(out)


def extract(package_path: str, out_dir: str) -> None:
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    raw_dir = out / "raw"
    dec_dir = out / "decompressed"
    raw_dir.mkdir(exist_ok=True)
    dec_dir.mkdir(exist_ok=True)

    with open(package_path, "rb") as f:
        header_bytes = f.read(HEADER_SIZE)
        header = parse_header(header_bytes)
        print(f"DBPF v{header['major']}.{header['minor']}  "
              f"entries={header['entry_count']}  "
              f"index@{header['index_offset']} size={header['index_size']}")

        f.seek(header["index_offset"])
        index_blob = f.read(header["index_size"])

        free_count = struct.unpack_from("<I", index_blob, 0)[0]
        print(f"free entries: {free_count}")

        manifest = []
        body = index_blob[4:]
        n = header["entry_count"]
        decompressed_ok = 0
        decompressed_fail = 0

        for i in range(n):
            entry_buf = body[i * INDEX_ENTRY_SIZE:(i + 1) * INDEX_ENTRY_SIZE]
            if len(entry_buf) < INDEX_ENTRY_SIZE:
                break
            e = parse_index_entry(entry_buf)
            name = tgi_name(e)

            f.seek(e["offset"])
            raw = f.read(e["stored_size"])

            # Choose extension based on type/magic of the raw bytes.
            ext = detect_extension(e["type"], raw)

            # Always write the raw (on-disk) resource.
            (raw_dir / f"{name}{ext}").write_bytes(raw)

            dec_path = None
            if e["compressed"]:
                data = None
                # Flag set but sizes match => stored uncompressed.
                if e["stored_size"] == e["uncompressed_size"]:
                    data = raw
                if data is None:
                    try:
                        data = dbpf_decompress(
                            raw, e["uncompressed_size"] or None)
                    except Exception:
                        data = None
                if data is not None:
                    # Re-detect extension from decompressed content.
                    ext = detect_extension(e["type"], data)
                    dec_path = dec_dir / f"{name}{ext}"
                    dec_path.write_bytes(data)
                    decompressed_ok += 1
                else:
                    decompressed_fail += 1

            manifest.append({
                "index": i,
                "type": f"0x{e['type']:08X}",
                "group": f"0x{e['group']:08X}",
                "instance": f"0x{e['instance_full']:016X}",
                "offset": e["offset"],
                "stored_size": e["stored_size"],
                "compressed": e["compressed"],
                "uncompressed_size": e["uncompressed_size"],
                "raw_file": f"raw/{name}{ext}",
                "decompressed_file": (
                    f"decompressed/{name}{ext}" if dec_path else None
                ),
            })

            if (i + 1) % 1000 == 0:
                print(f"  ...processed {i + 1}/{n} entries")

    with open(out / "manifest.json", "w") as mf:
        json.dump({
            "file": package_path,
            "header": header,
            "free_count": free_count,
            "entry_count": len(manifest),
            "decompressed_ok": decompressed_ok,
            "decompressed_failed": decompressed_fail,
            "entries": manifest,
        }, mf, indent=2)

    print(f"\nExtracted {len(manifest)} resources to {out}")
    print(f"  decompressed OK: {decompressed_ok}")
    print(f"  decompressed failed: {decompressed_fail}")
    print(f"  manifest: {out / 'manifest.json'}")


def main(argv):
    if len(argv) < 2:
        print(f"usage: {argv[0]} <package> [output_dir]", file=sys.stderr)
        return 2
    package = argv[1]
    out_dir = argv[2] if len(argv) > 2 else (
        Path(package).stem + "_extracted")
    extract(package, out_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

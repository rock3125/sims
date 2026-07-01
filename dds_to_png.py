#!/usr/bin/env python3
"""Convert a DDS file (including BC5/DST5 normal-map format) to PNG.

Falls back to Pillow when the DDS format is natively supported; otherwise
decodes BC1/BC3/BC5 blocks manually with numpy.
"""
import argparse
import math
import struct
import sys

import numpy as np
from PIL import Image

try:
    import texture2ddecoder as _t2d
except ImportError:
    _t2d = None


def decode_bc5(data: bytes, width: int, height: int, reconstruct_z: bool) -> Image.Image:
    blocks_x = (width + 3) // 4
    blocks_y = (height + 3) // 4
    expected = blocks_x * blocks_y * 16
    if len(data) < expected:
        raise ValueError(f"BC5 data too small: have {len(data)}, need {expected}")

    arr = np.frombuffer(data, dtype=np.uint8, count=expected).reshape(-1, 16)

    def decode_channel(block_bytes: np.ndarray) -> np.ndarray:
        # BC4: byte0=e0, byte1=e1, bytes 2-7 = 16 x 3-bit indices (48 bits)
        e0 = block_bytes[:, 0].astype(np.float32) / 255.0
        e1 = block_bytes[:, 1].astype(np.float32) / 255.0
        raw = (block_bytes[:, 2].astype(np.uint64)
               | (block_bytes[:, 3].astype(np.uint64) << 8)
               | (block_bytes[:, 4].astype(np.uint64) << 16)
               | (block_bytes[:, 5].astype(np.uint64) << 24)
               | (block_bytes[:, 6].astype(np.uint64) << 32)
               | (block_bytes[:, 7].astype(np.uint64) << 40))
        b = np.empty((block_bytes.shape[0], 16), dtype=np.float32)
        for i in range(16):
            b[:, i] = (raw >> (i * 3)) & 0x7

        e0c = e0[:, None]
        e1c = e1[:, None]
        gt = e0c > e1c
        # BC4 interpolation (3-bit index, 8 levels)
        out = np.where(b == 0, e0c, 0.0)
        out = np.where(b == 1, e1c, out)
        out = np.where(b == 2, np.where(gt, (6 * e0c + e1c) / 7.0, (4 * e0c + 3 * e1c) / 7.0), out)
        out = np.where(b == 3, np.where(gt, (5 * e0c + 2 * e1c) / 7.0, (3 * e0c + 4 * e1c) / 7.0), out)
        out = np.where(b == 4, np.where(gt, (4 * e0c + 3 * e1c) / 7.0, (2 * e0c + 5 * e1c) / 7.0), out)
        out = np.where(b == 5, np.where(gt, (3 * e0c + 4 * e1c) / 7.0, (1 * e0c + 6 * e1c) / 7.0), out)
        out = np.where(b == 6, np.where(gt, (2 * e0c + 5 * e1c) / 7.0, 0.0), out)
        out = np.where(b == 7, np.where(gt, (1 * e0c + 6 * e1c) / 7.0, 1.0), out)
        return out

    red = decode_channel(arr[:, 0:8])
    green = decode_channel(arr[:, 8:16])

    red = red.reshape(blocks_y, blocks_x, 4, 4).swapaxes(1, 2).reshape(height, width)
    green = green.reshape(blocks_y, blocks_x, 4, 4).swapaxes(1, 2).reshape(height, width)

    if reconstruct_z:
        z2 = 1.0 - red * red - green * green
        blue = np.sqrt(np.clip(z2, 0.0, 1.0))
        rgb = np.stack([red, green, blue], axis=-1)
    else:
        rgb = np.stack([red, green, np.zeros_like(red)], axis=-1)

    rgb = np.clip(rgb * 255.0 + 0.5, 0, 255).astype(np.uint8)
    return Image.fromarray(rgb, "RGB")


def decode_bc3(data: bytes, width: int, height: int) -> Image.Image:
    blocks_x = (width + 3) // 4
    blocks_y = (height + 3) // 4
    expected = blocks_x * blocks_y * 16
    arr = np.frombuffer(data, dtype=np.uint8, count=expected).reshape(-1, 16)

    # alpha via BC4 on bytes 0:8
    a0 = arr[:, 0].astype(np.float32) / 255.0
    a1 = arr[:, 1].astype(np.float32) / 255.0
    araw = arr[:, 0:8].copy()
    idx = np.zeros((arr.shape[0], 16), dtype=np.float32)
    raw = (arr[:, 2].astype(np.uint32) | (arr[:, 3].astype(np.uint32) << 8)
           | (arr[:, 4].astype(np.uint32) << 16) | (arr[:, 5].astype(np.uint32) << 24)
           | (arr[:, 6].astype(np.uint64) << 32) | (arr[:, 7].astype(np.uint64) << 40))
    for i in range(16):
        idx[:, i] = (raw >> (i * 3)) & 0x7
    a0c = a0[:, None]; a1c = a1[:, None]
    gt = a0c > a1c
    out = np.where(idx == 0, a0c, 0.0)
    out = np.where(idx == 1, a1c, out)
    out = np.where(idx == 2, np.where(gt, (6 * a0c + a1c) / 7, 4 / 8.0), out)
    out = np.where(idx == 3, np.where(gt, (5 * a0c + 2 * a1c) / 7, 5 / 8.0), out)
    out = np.where(idx == 4, np.where(gt, (4 * a0c + 3 * a1c) / 7, 6 / 8.0), out)
    out = np.where(idx == 5, np.where(gt, (3 * a0c + 4 * a1c) / 7, 7 / 8.0), out)
    out = np.where(idx == 6, np.where(gt, (2 * a0c + 5 * a1c) / 7, 1.0), out)
    out = np.where(idx == 7, np.where(gt, (a0c + 6 * a1c) / 7, 0.0), out)
    alpha = out.reshape(blocks_y, blocks_x, 4, 4).swapaxes(1, 2).reshape(height, width)

    rgb = decode_bc1_bytes(arr[:, 8:16], blocks_x, blocks_y, width, height)
    rgba = np.dstack([rgb, alpha])
    return Image.fromarray(rgba, "RGBA")


def decode_bc1_block_colors(c0: int, c1: int):
    r0 = (c0 >> 11) & 0x1F; g0 = (c0 >> 5) & 0x3F; b0 = c0 & 0x1F
    r1 = (c1 >> 11) & 0x1F; g1 = (c1 >> 5) & 0x3F; b1 = c1 & 0x1F
    def expand(r, g, b):
        return np.array([r * 255 // 31, g * 255 // 63, b * 255 // 31], dtype=np.float32)
    c0f = expand(r0, g0, b0)
    c1f = expand(r1, g1, b1)
    if c0 > c1:
        c2 = (2 * c0f + c1f) / 3
        c3 = (c0f + 2 * c1f) / 3
    else:
        c2 = (c0f + c1f) / 2
        c3 = np.zeros(3, dtype=np.float32)
    return c0f, c1f, c2, c3


def decode_bc1_bytes(arr8: np.ndarray, blocks_x, blocks_y, width, height) -> np.ndarray:
    n = arr8.shape[0]
    out = np.empty((n, 16, 3), dtype=np.float32)
    c0 = (arr8[:, 0].astype(np.uint16) | (arr8[:, 1].astype(np.uint16) << 8)).astype(np.uint32)
    c1 = (arr8[:, 2].astype(np.uint16) | (arr8[:, 3].astype(np.uint16) << 8)).astype(np.uint32)
    bits = (arr8[:, 4].astype(np.uint32) | (arr8[:, 5].astype(np.uint32) << 8)
            | (arr8[:, 6].astype(np.uint32) << 16) | (arr8[:, 7].astype(np.uint32) << 24))
    gt = c0 > c1
    r0 = ((c0 >> 11) & 0x1F).astype(np.float32) * (255 / 31)
    g0 = ((c0 >> 5) & 0x3F).astype(np.float32) * (255 / 63)
    b0 = (c0 & 0x1F).astype(np.float32) * (255 / 31)
    r1 = ((c1 >> 11) & 0x1F).astype(np.float32) * (255 / 31)
    g1 = ((c1 >> 5) & 0x3F).astype(np.float32) * (255 / 63)
    b1 = (c1 & 0x1F).astype(np.float32) * (255 / 31)
    r2 = np.where(gt, (2 * r0 + r1) / 3, (r0 + r1) / 2)
    g2 = np.where(gt, (2 * g0 + g1) / 3, (g0 + g1) / 2)
    b2 = np.where(gt, (2 * b0 + b1) / 3, (b0 + b1) / 2)
    r3 = np.where(gt, (r0 + 2 * r1) / 3, 0.0)
    g3 = np.where(gt, (g0 + 2 * g1) / 3, 0.0)
    b3 = np.where(gt, (b0 + 2 * b1) / 3, 0.0)
    rs = np.stack([r0, r1, r2, r3], axis=1)
    gs = np.stack([g0, g1, g2, g3], axis=1)
    bs = np.stack([b0, b1, b2, b3], axis=1)
    idx = np.empty((n, 16), dtype=np.int32)
    for i in range(16):
        idx[:, i] = (bits >> (i * 2)) & 0x3
    out[..., 0] = np.take_along_axis(rs, idx, axis=1)
    out[..., 1] = np.take_along_axis(gs, idx, axis=1)
    out[..., 2] = np.take_along_axis(bs, idx, axis=1)
    out = np.clip(out, 0, 255).astype(np.uint8)
    out = out.reshape(blocks_y, blocks_x, 4, 4, 3).swapaxes(1, 2).reshape(height, width, 3)
    return out


def parse_dds(path: str):
    with open(path, "rb") as f:
        blob = f.read()
    if blob[:4] != b"DDS ":
        raise ValueError("not a DDS file")
    (dwSize, dwFlags, dwHeight, dwWidth, dwPitch, dwDepth, dwMip,
     *_reserved) = struct.unpack_from("<IIIIIIIIIIIIII", blob, 4)
    pf_off = 4 + 4 + 4 + 4 * 6 + 4 * 11  # 4 (magic) + 4 (size) + flags+h+w+pitch+depth+mip (6*4) + reserved1[11] (44)
    pf_off = 4 + 4 + 24 + 44  # = 76
    (pfSize, pfFlags, pfFourCC, pfRGBBits, pfRMask, pfGMask, pfBMask, pfAMask
     ) = struct.unpack_from("<II4sIIIII", blob, pf_off)
    data_off = 4 + dwSize  # magic + DDSURFACEDESC2
    return blob, dwSize, dwFlags, dwHeight, dwWidth, dwMip, pfFlags, pfFourCC, data_off


def main():
    ap = argparse.ArgumentParser(description="Convert DDS to PNG (supports BC1/BC3/BC5)")
    ap.add_argument("input", help="input .dds file")
    ap.add_argument("output", help="output .png file")
    ap.add_argument("--no-reconstruct-z", action="store_true",
                    help="for BC5: do not reconstruct Z channel (B stays 0)")
    ap.add_argument("--mip", type=int, default=0,
                    help="mipmap level to extract (0 = largest)")
    args = ap.parse_args()

    blob, dwSize, dwFlags, dwHeight, dwWidth, dwMip, pfFlags, pfFourCC, data_off = parse_dds(args.input)
    fourcc = pfFourCC.decode("ascii", "replace")
    print(f"DDS {dwWidth}x{dwHeight} mip={dwMip} fourcc={fourcc!r}")

    # Try Pillow first for any format it supports.
    try:
        img = Image.open(args.input)
        img.load()
        img.save(args.output)
        print("decoded via Pillow")
        return 0
    except NotImplementedError:
        pass

    data = blob[data_off:]
    if fourcc in ("DXT5", "BC3S"):
        if args.mip != 0:
            # skip mip chain sizes
            w, h = dwWidth, dwHeight
            off = 0
            for i in range(args.mip):
                bx = (w + 3) // 4; by = (h + 3) // 4
                off += bx * by * 16
                w = max(1, w // 2); h = max(1, h // 2)
            w = max(1, dwWidth >> args.mip); h = max(1, dwHeight >> args.mip)
            data = data[off:]
            img = decode_bc3(data, w, h)
        else:
            img = decode_bc3(data, dwWidth, dwHeight)
    elif fourcc in ("DXT1", "BC1S"):
        blocks_x = (dwWidth + 3) // 4
        blocks_y = (dwHeight + 3) // 4
        arr = np.frombuffer(data[:blocks_x * blocks_y * 8], dtype=np.uint8).reshape(-1, 8)
        rgb = decode_bc1_bytes(arr, blocks_x, blocks_y, dwWidth, dwHeight)
        img = Image.fromarray(rgb, "RGB")
    elif fourcc in ("DST5", "BC5S", "ATI2"):
        w = max(1, dwWidth >> args.mip)
        h = max(1, dwHeight >> args.mip)
        if args.mip != 0:
            off = 0
            ww, hh = dwWidth, dwHeight
            for i in range(args.mip):
                bx = (ww + 3) // 4; by = (hh + 3) // 4
                off += bx * by * 16
                ww = max(1, ww // 2); hh = max(1, hh // 2)
            data = data[off:]
        if _t2d is not None:
            out = _t2d.decode_bc5(data, w, h)
            buf = np.frombuffer(out, dtype=np.uint8).reshape(h, w, 4)
            r, g = buf[:, :, 2].astype(np.float32) / 255.0, buf[:, :, 1].astype(np.float32) / 255.0
            if args.no_reconstruct_z:
                b = np.zeros_like(r)
            else:
                b = np.sqrt(np.clip(1.0 - r * r - g * g, 0.0, 1.0))
            rgb = np.stack([r, g, b], axis=-1)
            rgb = np.clip(rgb * 255.0 + 0.5, 0, 255).astype(np.uint8)
            img = Image.fromarray(rgb, "RGB")
        else:
            img = decode_bc5(data, w, h, reconstruct_z=not args.no_reconstruct_z)
    else:
        raise SystemExit(f"unsupported DDS fourcc: {fourcc!r}")

    img.save(args.output)
    print(f"wrote {args.output} ({img.size[0]}x{img.size[1]})")
    return 0


if __name__ == "__main__":
    sys.exit(main())

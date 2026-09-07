"""Independently decode generated test QR images; zxing-cpp is test-only."""

import argparse
import importlib.metadata
from pathlib import Path

import zxingcpp


def variants(pixels, size):
    yield "original", pixels, size
    yield "dim-contrast", bytes(16 if value < 128 else 96 for value in pixels), size
    yield "rotated", bytes(pixels[(size - x - 1) * size + y]
                            for y in range(size) for x in range(size)), size
    doubled = bytearray()
    for y in range(size):
        row = bytes(value for value in pixels[y * size:(y + 1) * size] for _ in range(2))
        doubled.extend(row)
        doubled.extend(row)
    yield "integer-dpi-2x", doubled, size * 2
    damaged = bytearray(pixels)
    for y in range(size // 2 - 3, size // 2 + 4):
        for x in range(size // 2 - 3, size // 2 + 4):
            damaged[y * size + x] = 255 - damaged[y * size + x]
    yield "small-central-damage", damaged, size


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixtures", type=Path)
    parser.add_argument("--compare-fixtures", type=Path)
    args = parser.parse_args()
    fixtures = sorted(args.fixtures.glob("*.pgm"))
    if len(fixtures) != 4:
        raise SystemExit("Expected four QR test fixtures")
    cases = 0
    for path in fixtures:
        encoded = path.read_bytes()
        magic, dimensions, depth, pixels = encoded.split(b"\n", 3)
        width, height = map(int, dimensions.split())
        if magic != b"P5" or depth != b"255" or width != height or len(pixels) != width * height:
            raise SystemExit(f"Invalid generated PGM: {path.name}")
        expected = path.with_suffix(".payload").read_bytes()
        if args.compare_fixtures:
            other = args.compare_fixtures / path.name
            if encoded != other.read_bytes() or expected != other.with_suffix(".payload").read_bytes():
                raise SystemExit(f"Cross-platform fixture mismatch: {path.name}")
        for variant, values, size in variants(pixels, width):
            decoded = zxingcpp.read_barcode(memoryview(values).cast("B", shape=[size, size]),
                                           formats=zxingcpp.BarcodeFormat.QRCode)
            if decoded is None or not decoded.valid or decoded.bytes != expected:
                raise SystemExit(f"Independent QR decode failed: {path.name}/{variant}")
            cases += 1
        print(f"{path.stem}: 5 independent decode cases passed ({len(expected)} bytes)")
    print(f"{cases} QR image cases passed with zxing-cpp {importlib.metadata.version('zxing-cpp')}")
    print("Synthetic image validation only; phone camera and venue scanning remain pending")


if __name__ == "__main__":
    main()

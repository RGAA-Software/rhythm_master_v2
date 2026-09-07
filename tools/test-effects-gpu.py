"""Validate captured blur pixels, including transparent input and pyramid filtering."""

import argparse
from pathlib import Path
import struct
import subprocess
import uuid


def read_tga(path):
    data = path.read_bytes()
    identifier, color_map, kind = data[:3]
    width, height, bits, flags = struct.unpack_from("<HHBB", data, 12)
    if color_map or kind not in (2, 10) or bits not in (24, 32):
        raise ValueError("Unexpected TGA format")
    offset = 18 + identifier
    stride = bits // 8
    pixels = []
    while len(pixels) < width * height:
        packet = data[offset] if kind == 10 else 0
        offset += kind == 10
        count = (packet & 127) + 1
        if len(pixels) + count > width * height:
            raise ValueError("TGA packet overflow")
        for _ in range(1 if packet & 128 else count):
            pixel = data[offset:offset + stride]
            if len(pixel) != stride:
                raise ValueError("Truncated pixels")
            offset += stride
            pixels.extend([tuple(pixel[2::-1])] * (count if packet & 128 else 1))
    rows = [pixels[y * width:(y + 1) * width] for y in range(height)]
    if not flags & 32:
        rows.reverse()
    return rows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = args.output.resolve()
    if not output.is_relative_to(root / "out"):
        raise ValueError("Captures must stay in project output")
    output = output / uuid.uuid4().hex
    output.mkdir(parents=True)
    subprocess.run([str(args.executable.resolve()), str(output)], check=True, timeout=60)
    images = [read_tga(output / f"blur-{index}.tga") for index in range(4)]
    if any(abs(a - b) > 2 for row in images[0] for pixel in row
           for a, b in zip(pixel, (50, 100, 20))):
        raise AssertionError(f"Constant/alpha not preserved: {output}")
    for index in (1, 2):
        image = images[index]
        for y in range(64):
            for x in range(64):
                pixel = image[y][x]
                if max(pixel) - min(pixel) > 1:
                    raise AssertionError(f"Transparent color bleed: {output}")
                if abs(pixel[0] - image[y][63-x][0]) > 3 or abs(pixel[0] - image[63-y][x][0]) > 3:
                    raise AssertionError(f"Asymmetric blur: {output}")
        if not 0 < image[31][31][0] < 255 or image[28][31][0] == 0:
            raise AssertionError(f"Missing center/halo: {output}")
    if images[2][31][31][0] >= images[1][31][31][0]:
        raise AssertionError(f"Wide blur did not spread energy: {output}")
    if images[3][31][31] != (255, 255, 255) or images[3][28][31] != (0, 0, 0):
        raise AssertionError(f"Zero radius changed pixels: {output}")
    noise = [read_tga(output / f"noise-{index}.tga") for index in range(5)]
    values = [pixel[0] for row in noise[0] for pixel in row]
    if max(values) - min(values) < 50:
        raise AssertionError(f"Missing spatial noise detail: {output}")
    if noise[0] != noise[3] or noise[0] == noise[1] or noise[0] == noise[2]:
        raise AssertionError(f"Noise repeatability, phase or seed failed: {output}")
    if any(pixel != (0, 0, 0) for row in noise[4] for pixel in row):
        raise AssertionError(f"Transparent noise leaked color: {output}")
    mapping = [read_tga(output / f"mapping-{index}.tga") for index in range(5)]
    if any(abs(channel - 120) > 2 for row in mapping[0] for pixel in row for channel in pixel):
        raise AssertionError(f"Mapping changed constant image: {output}")
    levels = [pixel[0] for row in mapping[1] for pixel in row]
    if max(levels) - min(levels) < 70:
        raise AssertionError(f"Kaleidoscope lost source detail: {output}")
    for y in range(64):
        for x in range(64):
            if abs(mapping[1][y][x][0] - mapping[1][x][y][0]) > 3:
                raise AssertionError(f"Kaleidoscope quarter-turn symmetry failed: {output}")
    if max(pixel[0] for row in mapping[2] for pixel in row) < 100:
        raise AssertionError(f"Polar extreme produced empty output: {output}")
    line = [pixel[0] for pixel in mapping[3][32]]
    peaks = sum(line[i] > 100 and line[i-1] <= 100 for i in range(1, 64))
    if not 7 <= peaks <= 9 or min(line) > 10:
        raise AssertionError(f"Contour line count/spacing failed: {output}, {peaks}")
    if any(pixel != (0, 0, 0) for row in mapping[4] for pixel in row):
        raise AssertionError(f"Transparent contours leaked color: {output}")
    resized = read_tga(output / "cached-resize.tga")
    if any(abs(a-b) > 2 for row in resized for pixel in row
           for a,b in zip(pixel, (0,255,0))):
        raise AssertionError(f"Static graph disappeared after resize: {output}")
    print(f"Four blur, five noise, five mapping/contour GPU cases and cached resize passed; evidence: {output}")


if __name__ == "__main__":
    main()

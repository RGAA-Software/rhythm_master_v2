"""Verify exact D3D scene fixture pixels captured by the native test host."""

import argparse
from pathlib import Path
import struct
import subprocess
import uuid

ROOT = Path(__file__).resolve().parents[1]


def read_tga(path):
    data = path.read_bytes()
    if len(data) < 18:
        raise ValueError("truncated TGA")
    identifier, color_map, kind = data[:3]
    width, height, bits, flags = struct.unpack_from("<HHBB", data, 12)
    if color_map or kind not in (2, 10) or (width, height) != (16, 16) or bits not in (24, 32):
        raise ValueError("unexpected scene capture format")
    stride = bits // 8
    offset = 18 + identifier
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
                raise ValueError("truncated pixels")
            offset += stride
            pixels.extend([tuple(reversed(pixel[:3]))] * (count if packet & 128 else 1))
    x, y = 8, 8
    if not flags & 32:
        y = height - 1 - y
    if flags & 16:
        x = width - 1 - x
    return pixels[y * width + x]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / "out"):
        raise ValueError("capture directory must stay in project build output")
    output = output / uuid.uuid4().hex
    output.mkdir(parents=True)
    subprocess.run([str(args.executable), str(output), str(ROOT / "third_party/assets/khronos-box/Box.glb")], check=True, timeout=60)
    expected = [(0, 255, 0), (0, 255, 0), (0, 0, 0), (0, 255, 0), (255, 0, 0),
                (0, 255, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (82, 82, 82),
                (0, 0, 0), (20, 20, 20), (64, 128, 255)]
    for scenario, value in enumerate(expected):
        actual = read_tga(output / f"scene-{scenario}.tga")
        if any(abs(a - b) > 2 for a, b in zip(actual, value)):
            raise RuntimeError(f"scene {scenario}: expected {value}, got {actual}; evidence {output}")
    print(f"D3D scene pixels passed; evidence {output}")
    expected_graph = [(0, 255, 0), (0, 255, 0), (0, 0, 0), (0, 0, 255),
                      (0, 255, 0), (0, 0, 0), (0, 255, 0), (82, 82, 82), (20, 20, 20)]
    for scenario, value in enumerate(expected_graph):
        actual = read_tga(output / f"graph-{scenario}.tga")
        if any(abs(a - b) > 2 for a, b in zip(actual, value)):
            raise RuntimeError(f"graph {scenario}: expected {value}, got {actual}; evidence {output}")
    print("Published scene graph pixels: cube/sphere, translation, material, cameras and node preview passed")
    for scenario, value in enumerate([(0, 255, 0), (0, 255, 0), (0, 0, 0)]):
        actual = read_tga(output / f"model-{scenario}.tga")
        if any(abs(a - b) > 2 for a, b in zip(actual, value)):
            raise RuntimeError(f"model {scenario}: expected {value}, got {actual}; evidence {output}")
    print("Embedded GLB Player pixels: material override, GPU resource recreation and transform passed")


if __name__ == "__main__":
    main()

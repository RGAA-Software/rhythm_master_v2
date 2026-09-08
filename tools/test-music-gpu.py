"""Verify real decoded PCM changes the composed D3D image at identical scene times."""

import argparse
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--fixtures', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--expected-nodes', type=int, default=164)
    parser.add_argument('--reference-output', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(args.executable), str(args.package), str(args.fixtures), str(args.output),
                    str(args.expected_nodes)], check=True, timeout=90)
    ffmpeg = 'C:/source/vcpkg/installed/x64-windows-static-release/tools/ffmpeg/ffmpeg.exe'
    images = {}
    for name in ('resonance_demo', 'silence', 'low', 'high'):
        path = args.output / (name + '.tga')
        images[name] = subprocess.run([ffmpeg, '-v', 'error', '-i', str(path), '-frames:v', '1',
                                      '-pix_fmt', 'rgb24', '-f', 'rawvideo', '-'], check=True, capture_output=True).stdout
        if len(images[name]) != 1280 * 720 * 3:
            raise ValueError('Unexpected GPU capture dimensions')
        subprocess.run([ffmpeg, '-v', 'error', '-y', '-i', str(path), '-frames:v', '1',
                        str(args.output / (name + '.png'))], check=True)
    differences = {}
    for first, second in (('resonance_demo', 'silence'), ('low', 'silence'), ('high', 'silence'), ('low', 'high')):
        difference = sum(abs(a - b) for a, b in zip(images[first], images[second])) / len(images[first])
        differences[f'{first}_vs_{second}'] = difference
        if difference < 0.15:
            raise AssertionError(f'Audio response too small: {first} / {second}: {difference}')
    (args.output / 'pixel-differences.json').write_text(json.dumps(differences, indent=4) + '\n', encoding='utf-8')
    if args.reference_output:
        for name, pixels in images.items():
            reference = subprocess.run([ffmpeg, '-v', 'error', '-i', str(args.reference_output / (name + '.png')),
                                        '-frames:v', '1', '-pix_fmt', 'rgb24', '-f', 'rawvideo', '-'],
                                       check=True, capture_output=True).stdout
            if pixels != reference:
                raise AssertionError(f'Rendered pixels differ from the reference: {name}')
        print('All four decoded-PCM images exactly match the reference project')
    print(differences)


if __name__ == '__main__':
    main()

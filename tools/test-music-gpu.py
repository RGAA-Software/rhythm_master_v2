"""Verify real decoded PCM changes the composed D3D image at identical scene times."""

import argparse
import hashlib
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
    parser.add_argument('--quality', action='store_true',
                        help='Also check mid-band and quiet/loud real PCM inputs')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    names = ('resonance_demo', 'silence', 'low', 'high')
    if args.quality:
        names += ('mid', 'quiet', 'loud')
    identity = {'package_sha256': hashlib.sha256(args.package.read_bytes()).hexdigest(),
                'executable_sha256': hashlib.sha256(args.executable.read_bytes()).hexdigest(),
                'expected_nodes': args.expected_nodes, 'quality': args.quality,
                'extent': [1280, 720], 'seconds': 4,
                'pcm_sha256': {name: hashlib.sha256((args.fixtures / (name + '.wav')).read_bytes()).hexdigest()
                               for name in names}}
    (args.output / 'input-identity.json').write_text(json.dumps(identity, indent=4) + '\n', encoding='utf-8')
    command = [str(args.executable), str(args.package), str(args.fixtures), str(args.output),
               str(args.expected_nodes)]
    if args.quality:
        command.append('--quality')
    subprocess.run(command, check=True, timeout=150 if args.quality else 90)
    ffmpeg = 'C:/source/vcpkg/installed/x64-windows-static-release/tools/ffmpeg/ffmpeg.exe'
    images = {}
    for name in names:
        path = args.output / (name + '.tga')
        images[name] = subprocess.run([ffmpeg, '-v', 'error', '-i', str(path), '-frames:v', '1',
                                      '-pix_fmt', 'rgb24', '-f', 'rawvideo', '-'], check=True, capture_output=True).stdout
        if len(images[name]) != 1280 * 720 * 3:
            raise ValueError('Unexpected GPU capture dimensions')
        subprocess.run([ffmpeg, '-v', 'error', '-y', '-i', str(path), '-frames:v', '1',
                        str(args.output / (name + '.png'))], check=True)
    differences = {}
    comparisons = [('resonance_demo', 'silence'), ('low', 'silence'), ('high', 'silence'), ('low', 'high')]
    if args.quality:
        comparisons += [('mid', 'silence'), ('mid', 'low'), ('mid', 'high'),
                        ('quiet', 'silence'), ('loud', 'silence'), ('quiet', 'loud')]
    failures = []
    for first, second in comparisons:
        difference = sum(abs(a - b) for a, b in zip(images[first], images[second])) / len(images[first])
        differences[f'{first}_vs_{second}'] = difference
        if difference < 0.15:
            failures.append(f'{first} / {second}: {difference}')
    (args.output / 'pixel-differences.json').write_text(json.dumps(differences, indent=4) + '\n', encoding='utf-8')
    if failures:
        raise AssertionError('Audio response too small: ' + '; '.join(failures))
    if args.reference_output:
        for name, pixels in images.items():
            reference = subprocess.run([ffmpeg, '-v', 'error', '-i', str(args.reference_output / (name + '.png')),
                                        '-frames:v', '1', '-pix_fmt', 'rgb24', '-f', 'rawvideo', '-'],
                                       check=True, capture_output=True).stdout
            if pixels != reference:
                raise AssertionError(f'Rendered pixels differ from the reference: {name}')
        print(f'All {len(names)} decoded-PCM images exactly match the reference project')
    print(differences)


if __name__ == '__main__':
    main()

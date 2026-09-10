"""Capture autonomous movement and cycle boundaries; run through verify_windows.py."""

import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import uuid

from content_identity import verify_package_source
from verify_windows import run_logged

ROOT = Path(__file__).resolve().parents[1]
NAMES = ('aureate_vortex', 'porcelain_bloom', 'stratified_ink', 'lumen_corridor')
FFMPEG = Path('C:/source/vcpkg/installed/x64-windows-static-release/tools/ffmpeg/ffmpeg.exe')


def pixels(path):
    return subprocess.check_output([str(FFMPEG), '-v', 'error', '-i', str(path),
        '-vf', 'scale=160:90', '-frames:v', '1', '-pix_fmt', 'rgb24', '-f', 'rawvideo', '-'])


def difference(first, second):
    assert len(first) == len(second) == 160 * 90 * 3
    return sum(abs(a - b) for a, b in zip(first, second)) / len(first)


def review(work, name):
    frames = sorted(set(range(0, 961, 60)) | {479, 481, 959})
    metrics = {}
    for scenario in ('resonance_demo', 'silence'):
        images = {}
        for frame in frames:
            source = work / f'{scenario}-{frame}.tga'
            destination = source.with_suffix('.png')
            subprocess.run([str(FFMPEG), '-v', 'error', '-y', '-i', str(source),
                            '-frames:v', '1', str(destination)], check=True)
            images[frame] = pixels(destination)
            # Keep lossless PNG evidence; avoid retaining a second uncompressed
            # representation of every GPU readback on constrained workspaces.
            assert source.resolve().parent == work.resolve()
            source.unlink()
        travel = [difference(images[a], images[a + 60]) for a in range(0, 960, 60)]
        boundary = [difference(images[a], images[b]) for a, b in ((479, 480), (480, 481), (959, 960))]
        metrics[scenario] = dict(two_second_differences=travel, boundary_differences=boundary)
        (work / 'motion-metrics.json').write_text(json.dumps(metrics, indent=4) + '\n', encoding='utf-8')
        if min(travel) < .3:
            raise AssertionError(f'{name}/{scenario}: a two-second interval appears static')
        if max(boundary) > max(1.0, sum(travel) / len(travel) * .6):
            raise AssertionError(f'{name}/{scenario}: cycle seam is too large relative to sustained travel')
        # Compare the seam against nearby one-frame travel as well. A large
        # two-second displacement alone could conceal a visible reset.
        if max(boundary[0], boundary[2]) > max(1.0, boundary[1] * 2):
            raise AssertionError(f'{name}/{scenario}: cycle seam exceeds adjacent-frame travel')
        camera_path = work / f'{scenario}-camera.csv'
        with camera_path.open(encoding='utf-8', newline='') as stream:
            camera = list(csv.DictReader(stream))
        if name != 'stratified_ink':
            if len(camera) != 961:
                raise AssertionError('Expected one live camera sample per frame')
            x = [float(row['eye_x']) for row in camera]
            if max(x) - min(x) < .5:
                raise AssertionError('Camera does not establish a spatial trajectory')
            if name == 'lumen_corridor':
                z = [float(row['eye_z']) for row in camera]
                if any(z[i] >= z[i - 1] for i in range(1, len(z)) if i % 480):
                    raise AssertionError('Forward camera stopped or reversed inside its cycle')
        contact = work / (scenario + '-contact.png')
        selected = [work / f'{scenario}-{frame}.png' for frame in range(0, 961, 120)]
        command = [str(FFMPEG), '-v', 'error', '-y']
        for path in selected:
            command += ['-i', str(path)]
        filters = ';'.join(f'[{i}:v]scale=320:180[p{i}]' for i in range(9))
        filters += ';' + ''.join(f'[p{i}]' for i in range(9)) + 'xstack=inputs=9:layout=0_0|320_0|640_0|0_180|320_180|640_180|0_360|320_360|640_360'
        subprocess.run(command + ['-filter_complex', filters, '-frames:v', '1', str(contact)], check=True)
    (work / 'motion-metrics.json').write_text(json.dumps(metrics, indent=4) + '\n', encoding='utf-8')
    return metrics


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--name', action='append', choices=NAMES)
    args = parser.parse_args()
    build = ROOT / 'out/windows-release'
    output = ROOT / 'out/concept-motion' / uuid.uuid4().hex
    output.mkdir(parents=True)
    print('Motion evidence:', output, flush=True)
    records = {}
    for name in args.name or NAMES:
        source = ROOT / 'content/templates' / name
        package = build / 'content/packages' / (name + '.rhythmpack')
        digest = verify_package_source(source, package)
        nodes = len(re.findall(r'^nodes \{', (source / 'graph.textproto').read_text(encoding='utf-8'), re.MULTILINE))
        work = output / name
        work.mkdir()
        records[name] = dict(source_sha256=digest, package_sha256=hashlib.sha256(package.read_bytes()).hexdigest(),
                             expected_nodes=nodes, status='running', visual_acceptance='pending')
        try:
            run_logged([str(build / 'src/windows_spike/music_gpu_tests.exe'), str(package),
                        str(ROOT / 'out/p7-quality-fixtures'), str(work), str(nodes), '--motion'],
                       output / (name + '.log'))
            records[name]['metrics'] = review(work, name)
            records[name]['status'] = 'passed'
        except Exception:
            records[name]['status'] = 'failed'
            raise
        finally:
            (output / 'results.json').write_text(json.dumps(records, indent=4) + '\n', encoding='utf-8')
        print(name, 'motion checks passed; moving-image review still required', flush=True)


if __name__ == '__main__':
    main()

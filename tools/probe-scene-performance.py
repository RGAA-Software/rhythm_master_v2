"""Short, serial Windows Studio/Player comparison; invoke through verify_windows.py."""

import argparse
import csv
import hashlib
import json
from pathlib import Path
import statistics
import uuid

from content_identity import verify_package_source
from verify_windows import run_logged

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--name', default='lumen_corridor')
    parser.add_argument('--mode', action='append', choices=('player', 'studio-no-previews', 'studio', 'studio-detail', 'studio-focus'))
    args = parser.parse_args()
    build = ROOT / 'out/windows-release'
    source = ROOT / 'content/templates' / args.name
    package = build / 'content/packages' / (args.name + '.rhythmpack')
    digest = verify_package_source(source, package)
    output = ROOT / 'out/scene-performance' / uuid.uuid4().hex
    output.mkdir(parents=True)
    print('Performance evidence:', output, flush=True)
    results = {'source_sha256': digest, 'host_extent': [1280, 720],
               'probe_sha256': hashlib.sha256((build / 'src/windows_spike/scene_performance_probe.exe').read_bytes()).hexdigest(),
               'warmup_frames': 120, 'detail_warmup_frames': 240, 'measured_frames': 480,
               'timing_note': 'Host stages include synchronization; bgfx GPU interval may include pacing and is not isolated GPU workload.',
               'modes': {}}
    for mode in args.mode or ('player', 'studio-no-previews', 'studio'):
        work = output / mode
        run_logged([str(build / 'src/windows_spike/scene_performance_probe.exe'),
                    str(build / 'src/windows_spike'), str(build / 'content/templates' / args.name),
                    str(package), str(work), mode], output / (mode + '.log'))
        warmup = 240 if mode in ('studio-detail', 'studio-focus') else 120
        with (work / 'frames.csv').open(encoding='utf-8', newline='') as stream:
            frames = list(csv.DictReader(stream))[warmup:]
        if len(frames) != 480:
            raise ValueError('Incomplete probe')
        values = {}
        for key in frames[0]:
            if key == 'frame':
                continue
            samples = sorted(float(frame[key]) for frame in frames)
            values[key] = {'p50': statistics.median(samples), 'p95': samples[int(len(samples) * .95)],
                           'max': samples[-1]}
        if values['audio_rms']['max'] <= .001:
            raise ValueError('No real audio features observed during measurement')
        if mode in ('studio-detail', 'studio-focus') and values['inline_previews']['max'] < 1:
            raise ValueError('Detail view never displayed an active node preview')
        results['modes'][mode] = values
        (output / 'results.json').write_text(json.dumps(results, indent=4) + '\n', encoding='utf-8')
        print(mode, values, flush=True)


if __name__ == '__main__':
    main()

"""Run via verify_windows.py: real 200 Hz/8 kHz response of input components."""

import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
import uuid

from content_identity import verify_package_source
from verify_windows import run_logged

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'out/windows-release')
    parser.add_argument('--name', action='append')
    parser.add_argument('--browser', action='store_true',
                        help='Exercise actual library preview/insertion instead of PCM images')
    args = parser.parse_args()
    catalog = json.loads((ROOT / 'provenance/input_components.json').read_text(encoding='utf-8'))
    available = [entry['id'] for entry in catalog['components']]
    names = args.name or available
    if any(name not in available for name in names):
        parser.error('Expected an input component from the current provenance catalog')
    output = ROOT / 'out/input-component-bands' / uuid.uuid4().hex
    fixtures = output / 'fixtures'
    output.mkdir(parents=True)
    if not args.browser:
        subprocess.run([sys.executable, str(ROOT / 'tools/create-music-fixture.py'), '--output', str(fixtures),
                        '--tones', '--low-hz', '200', '--high-hz', '8000'], check=True)
    results = {}
    for name in names:
        source = ROOT / 'content/semantic' / name
        package = args.build / 'content/semantic_packages' / (name + '.rhythmpack')
        digest = verify_package_source(source, package)
        # These authoring fixtures contain one component instance. Its internal
        # graph must remain reachable after expansion, with the instance removed.
        nodes = len(re.findall(r'^nodes \{', (source / 'graph.textproto').read_text(encoding='utf-8'), re.MULTILINE)) - 1
        results[name] = {'source_sha256': digest, 'expected_nodes': nodes, 'status': 'running',
                         'mode': 'browser' if args.browser else 'pcm'}
        try:
            command = ([str(args.build / 'src/windows_spike/semantic_browser_gpu_tests.exe'),
                        str(args.build / 'src/windows_spike'), 'zh-CN', str(output / name),
                        'official.semantic.' + name] if args.browser else
                       [sys.executable, str(ROOT / 'tools/test-music-gpu.py'),
                        '--executable', str(args.build / 'src/windows_spike/music_gpu_tests.exe'),
                        '--package', str(package), '--fixtures', str(fixtures),
                        '--output', str(output / name), '--expected-nodes', str(nodes)])
            run_logged(command, output / (name + '.log'))
            results[name]['status'] = 'passed'
        except subprocess.CalledProcessError:
            results[name]['status'] = 'failed'
        finally:
            (output / 'results.json').write_text(json.dumps(results, indent=4) + '\n', encoding='utf-8')
    print('Input component evidence:', output, flush=True)
    if any(item['status'] != 'passed' for item in results.values()):
        raise SystemExit('Input component frequency response failed; all available results retained')


if __name__ == '__main__':
    main()

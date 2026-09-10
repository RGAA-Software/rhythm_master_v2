"""Run through verify_windows.py: source-bound review of the four concept works."""

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
NAMES = ('aureate_vortex', 'porcelain_bloom', 'stratified_ink', 'lumen_corridor')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('quality', 'controls', 'export', 'replace'), default='quality')
    parser.add_argument('--name', action='append', choices=NAMES)
    args = parser.parse_args()
    build = ROOT / 'out/windows-release'
    resources = build / 'src/windows_spike'
    output = ROOT / 'out/concept-review' / uuid.uuid4().hex
    output.mkdir(parents=True)
    print('Concept evidence:', output, flush=True)
    results = {}
    for name in args.name or NAMES:
        source = ROOT / 'content/templates' / name
        package = build / 'content/packages' / (name + '.rhythmpack')
        digest = verify_package_source(source, package)
        nodes = len(re.findall(r'^nodes \{', (source / 'graph.textproto').read_text(encoding='utf-8'), re.MULTILINE))
        results[name] = dict(source_sha256=digest, expected_nodes=nodes, mode=args.mode, status='running')
        if args.mode in ('quality', 'controls'):
            command = [sys.executable, str(ROOT / 'tools/test-music-gpu.py'),
                       '--executable', str(resources / 'music_gpu_tests.exe'), '--package', str(package),
                       '--fixtures', str(ROOT / 'out/p7-quality-fixtures'), '--output', str(output / name),
                       '--expected-nodes', str(nodes), '--quality' if args.mode == 'quality' else '--control-extremes']
        elif args.mode == 'export':
            command = [str(resources / 'export_ui_gpu_tests.exe'), str(resources),
                       str(build / 'content/templates' / name), '--arranged', str(output / name)]
        else:
            command = [str(resources / 'soundtrack_studio_gpu_tests.exe'), str(resources),
                       str(build / 'content/templates' / name), str(ROOT / 'out/p7-quality-fixtures/quiet.wav'),
                       str(output / name)]
        try:
            run_logged(command, output / (name + '.log'))
            results[name]['status'] = 'passed'
        except subprocess.CalledProcessError:
            results[name]['status'] = 'failed'
        finally:
            (output / 'results.json').write_text(json.dumps(results, indent=4) + '\n', encoding='utf-8')
    if any(result['status'] != 'passed' for result in results.values()):
        raise SystemExit('Concept review failed; individual evidence retained')


if __name__ == '__main__':
    main()

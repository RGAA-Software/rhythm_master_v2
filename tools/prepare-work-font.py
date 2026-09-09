"""Verify and copy the existing unmodified work font and OFL notice incrementally."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    origin = json.loads((ROOT / 'third_party/notices/noto-cjk/PROVENANCE.json').read_text(encoding='utf-8'))
    args.output.mkdir(parents=True, exist_ok=True)
    for entry in origin['files']:
        source = ROOT / entry['imported_file']
        data = source.read_bytes()
        if hashlib.sha256(data).hexdigest() != entry['sha256']:
            raise ValueError('Work font/notice differs from the recorded upstream revision')
        target = args.output / source.name
        if not target.is_file() or target.read_bytes() != data:
            shutil.copyfile(source, target)


if __name__ == '__main__':
    main()

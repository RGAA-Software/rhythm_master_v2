"""Exercise a data-authored recipe through the real Studio UI from an empty graph."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import uuid


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--resources', required=True, type=Path)
    parser.add_argument('--recipe', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--music-executable', required=True, type=Path)
    parser.add_argument('--fixtures', required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = args.output.resolve() / uuid.uuid4().hex
    if not output.is_relative_to(root / 'out'):
        raise ValueError('Authoring evidence must stay inside project output')
    output.mkdir(parents=True)
    recipe_bytes = args.recipe.read_bytes()
    recipe_path = output / 'recipe.json'
    recipe_path.write_bytes(recipe_bytes)
    print(f'From-empty Studio evidence: {output}', flush=True)
    subprocess.run([str(args.executable.resolve()), str(args.resources.resolve()),
                    str(recipe_path), str(output)], check=True, timeout=140)
    recipe = json.loads(recipe_bytes)
    subprocess.run([sys.executable, str(root / 'tools/test-music-gpu.py'),
                    '--executable', str(args.music_executable.resolve()),
                    '--package', str(output / 'Published/work.rhythmpack'),
                    '--fixtures', str(args.fixtures.resolve()), '--output', str(output / 'music'),
                    '--expected-nodes', str(len(recipe['nodes']))], check=True, timeout=120)
    (output / 'acceptance.json').write_text(json.dumps({
        'recipe': recipe['id'], 'recipe_sha256': hashlib.sha256(recipe_bytes).hexdigest(),
        'ui_authoring': 'passed', 'decoded_pcm_gpu': 'passed',
        'visual_review': 'pending', 'android': 'pending', 'acoustic_capture': False,
    }, indent=4) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()

"""Preserve an accepted Studio-authored project as an editable example template."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def read_json(path):
    return json.loads(path.read_text(encoding='utf-8'))


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=4) + '\n', encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', type=Path, required=True)
    parser.add_argument('--recipe', type=Path, required=True)
    parser.add_argument('--name', required=True)
    parser.add_argument('--protoc', type=Path,
                        default=Path('C:/source/vcpkg/installed/x64-windows/tools/protobuf/protoc.exe'))
    args = parser.parse_args()
    if not re.fullmatch(r'[a-z][a-z0-9_]{1,63}', args.name):
        raise ValueError('Invalid template directory name')
    run = args.run.resolve()
    if not run.is_relative_to(ROOT / 'out'):
        raise ValueError('Expected project-owned acceptance evidence')
    accepted = read_json(run / 'acceptance.json')
    recipe = read_json(args.recipe)
    tested_recipe = (run / 'recipe.json').read_bytes()
    if (accepted['ui_authoring'] != 'passed' or accepted['decoded_pcm_gpu'] != 'passed'
            or accepted['recipe'] != recipe['id']
            or accepted['recipe_sha256'] != hashlib.sha256(tested_recipe).hexdigest()
            or json.loads(tested_recipe) != recipe):
        raise ValueError('The authoring and decoded-audio workflow must pass first')
    project = run / 'Projects/work.rhythmproj'
    revision_id = (project / 'CURRENT').read_text(encoding='utf-8').strip()
    if not re.fullmatch(r'[a-zA-Z0-9_-]+', revision_id):
        raise ValueError('Invalid revision identity')
    revision = project / 'revisions' / revision_id
    manifest = read_json(revision / 'manifest.json')
    graph = (revision / 'graph.pb').read_bytes()
    editor = (revision / 'editor.json').read_bytes()
    if (hashlib.sha256(graph).hexdigest() != manifest['graph_sha256']
            or hashlib.sha256(editor).hexdigest() != manifest['editor_sha256']):
        raise ValueError('Saved project hash mismatch')
    schema = ROOT / 'src/project_io/schema/graph.proto'
    text = subprocess.run([str(args.protoc), '--decode=rhythm.schema.GraphProject',
                           f'--proto_path={schema.parent}', schema.name],
                          input=graph, capture_output=True, check=True).stdout.decode('utf-8').replace('\r\n', '\n')
    text = '\n'.join(' ' * (len(line) - len(line.lstrip(' '))) + line
                     for line in text.splitlines()) + '\n'
    destination = ROOT / 'content/templates' / args.name
    destination.mkdir(parents=True, exist_ok=True)
    for record in manifest.get('assets', []):
        digest = record['sha256']
        if not re.fullmatch(r'[0-9a-f]{64}', digest):
            raise ValueError('Invalid asset identity')
        relative = Path('assets/sha256') / digest[:2] / digest
        source = (project / relative).resolve()
        if not source.is_relative_to(project.resolve()):
            raise ValueError('Asset escapes the authored project')
        data = source.read_bytes()
        if hashlib.sha256(data).hexdigest() != digest or len(data) != record['bytes']:
            raise ValueError('Authored asset hash mismatch')
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    for field in ('graph_sha256', 'editor_sha256', 'revision_id'):
        manifest.pop(field, None)
    chinese, english = recipe['title'].split(' / ', 1)
    license_status = ('First-party Studio-authored graph and original demo music; '
                      'existing attributed renderer adapters; outbound license pending')
    if recipe.get('builtin_font', False):
        provenance = read_json(ROOT / 'third_party/notices/noto-cjk/PROVENANCE.json')
        assets = {record['sha256'] for record in manifest.get('assets', [])}
        if any(record['sha256'] not in assets for record in provenance['files']):
            raise ValueError('The full bundled font and its OFL notice must both be published')
        license_status += '; unmodified Noto Sans CJK SC under SIL OFL 1.1, font and notice embedded'
    manifest.update(kind='template', content_id='official.templates.' + args.name,
                    content_version='0.1.0', default_locale='zh-CN',
                    titles={'zh-CN': chinese, 'en-US': english}, category='audio', tier='example',
                    maturity='visual-review-pending', author='Rhythm Master',
                    license_status=license_status,
                    compatible_players=['windows'], external_assets=[],
                    descriptions=recipe['descriptions'])
    (destination / 'graph.textproto').write_text(text, encoding='utf-8')
    write_json(destination / 'editor.json', json.loads(editor))
    write_json(destination / 'manifest.json', manifest)
    write_json(destination / 'authoring_evidence.json', {
        'run': run.relative_to(ROOT).as_posix(), 'recipe': args.recipe.as_posix(),
        'recipe_sha256': hashlib.sha256(args.recipe.read_bytes()).hexdigest(),
        'saved_graph_sha256': hashlib.sha256(graph).hexdigest(),
        'published_sha256': hashlib.sha256((run / 'Published/work.rhythmpack').read_bytes()).hexdigest(),
        'acceptance': accepted,
    })
    print(f'Preserved Studio-authored example: {destination}')


if __name__ == '__main__':
    main()

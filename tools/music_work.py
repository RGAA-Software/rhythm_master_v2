"""Write editable music works using our existing graph, cue and original-media formats."""

import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=4) + '\n', encoding='utf-8')


def original_arrangement(destination):
    source = ROOT / 'content/templates/luminous_concerto'
    original = json.loads((source / 'manifest.json').read_text(encoding='utf-8'))
    soundtrack = original['soundtrack']
    used = {clip['sha256'] for clip in soundtrack['clips']}
    records = [record for record in original['assets'] if record['sha256'] in used]
    if {record['sha256'] for record in records} != used:
        raise ValueError('Original soundtrack asset records incomplete')
    for record in records:
        digest = record['sha256']
        relative = Path('assets/sha256') / digest[:2] / digest
        data = (source / relative).read_bytes()
        if hashlib.sha256(data).hexdigest() != digest or len(data) != record['bytes']:
            raise ValueError('Original soundtrack integrity')
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    return records, soundtrack


def write(name, graph, output, controls, snapshots, cues, titles, descriptions,
          tier='basic', platforms=('windows',), extra_assets=(), components=()):
    destination = ROOT / 'content/templates' / name
    destination.mkdir(parents=True, exist_ok=True)
    metadata = ['controls {']
    for identity, title in controls:
        metadata.append(f'    titles {{ key: {identity} value: {json.dumps(title, ensure_ascii=False)} }}')
    for identity, title, values in snapshots:
        if len(values) != len(controls):
            raise ValueError('Snapshot control count mismatch')
        metadata.append(f'    snapshots {{ id: {identity} title: {json.dumps(title, ensure_ascii=False)}')
        for (key, _), value in zip(controls, values):
            metadata.append(f'        values {{ key: {key} value: {value} }}')
        metadata.append('    }')
    for identity, (title, seconds, target, fade) in enumerate(cues, 1):
        metadata.append(f'    cues {{ id: {identity} title: {json.dumps(title, ensure_ascii=False)} seconds: {seconds} snapshot: {target} fade: {fade} smooth: true }}')
    metadata.append('}')
    identity = 'official-' + name.replace('_', '-')
    definitions = [line for text, _ in components for line in text]
    (destination / 'graph.textproto').write_text(
        f'schema_version: 5\nid: "{identity}"\noutput: {output}\ncanvas {{ width: 1280 height: 720 }}\n' +
        '\n'.join(graph.nodes + graph.edges + definitions + metadata) + '\n', encoding='utf-8')
    editor = dict(version=2, positions=graph.positions)
    if components:
        editor['components'] = [layout for _, layout in components]
    write_json(destination / 'editor.json', editor)
    records, soundtrack = original_arrangement(destination)
    manifest = dict(format='rhythm.project', manifest_version=3, kind='template',
                    content_id='official.templates.'+name, content_version='0.1.0',
                    project_id=identity, graph_revision=0, title=titles['zh-CN']+' / '+titles['en-US'],
                    default_locale='zh-CN', titles=titles, category='audio', tier=tier,
                    maturity='visual-review-pending', author='Rhythm Master',
                    license_status='First-party graph and original synthesized music; existing attributed renderer adapters; outbound license pending',
                    compatible_players=list(platforms), external_assets=[], assets=records+list(extra_assets),
                    soundtrack=soundtrack, descriptions=descriptions)
    write_json(destination / 'manifest.json', manifest)
    return manifest

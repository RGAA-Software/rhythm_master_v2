"""Reuse Ink Tide's immutable editable shaders as an input-processing component."""

import hashlib
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('components', ROOT / 'tools/author-input-components.py')
COMPONENTS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(COMPONENTS)


def build(graph, records):
    node = graph.node
    source = node('texture.blur', 0, 0, blur_radius=2)
    time = node('core.time', 0, 300)
    pace = node('scalar.constant', 0, 600, value=0.7)
    response = node('scalar.constant', 0, 900, value=1)
    clock = node('scalar.expression', 340, 300, dict(time=time, a=pace), expression='time * a')
    bass = node('audio.band', 340, 650, audio_band=12)
    high = node('audio.band', 340, 950, audio_band=48)
    bass = node('scalar.expression', 680, 650, dict(a=bass, b=response), expression='a * b')
    high = node('scalar.expression', 680, 950, dict(a=high, b=response), expression='a * b')
    shaders = []
    for index, record in enumerate(records):
        result = node('texture.shader', 1020, index * 350,
                      dict(source=source, time=clock, a=bass, b=high))
        graph.nodes[-1] = graph.nodes[-1][:-1] + (
            f'    properties {{ key: "asset" value {{ asset_sha256: "{record["sha256"]}" }} }}\n}}')
        shaders.append(result)
    fine = node('texture.contours', 1020, 700, dict(source=source, phase=high),
                contour_count=16, line_width=0.035, color_a=(0.1, 0.2, 0.15, 0.6),
                color_b=(0.6, 0.4, 0.15, 0.7))
    etched = node('texture.composite', 1360, 0, dict(a=shaders[0], b=fine), amount=0.4)
    coast = node('texture.composite', 1700, 0, dict(a=etched, b=shaders[1]), amount=1)
    output = node('texture.fxaa', 2040, 0, dict(source=coast))
    return source, output, [('pace', pace, 'value'), ('response', response, 'value'),
                            ('softness', source, 'blur_radius'), ('stripes', fine, 'contour_count'),
                            ('overlap', etched, 'amount'), ('glow', coast, 'amount')]


def main():
    source = ROOT / 'content/templates/ink_tide'
    original = json.loads((source / 'manifest.json').read_text(encoding='utf-8'))
    records = [record for record in original['assets']
               if record['media_type'] == 'application/x-rhythm-image-shader']
    if len(records) != 2:
        raise ValueError('Expected the existing pigment and coastline shader bundles')
    recipe = dict(name='ink_cartography', titles=('Ink cartography', '墨绘地形'),
                  descriptions=(
                      'Connect an image as a height field. Editable pigment and gold coastline shaders turn it into an ink map; bass moves the shoreline and highs animate the contour etching. Shader assets travel with insertion; preview inputs do not.',
                      '把图像连接为高度场。可编辑的墨色与金色岸线 Shader 将它绘成地图；低频推动海岸，高频改变等高刻线。插入时自动携带 Shader 资源，预览输入不会进入工程。'),
                  variant_titles=('Fine atlas', '细绘图谱'), fixture_noise=True, fixture_lines=0,
                  variant=dict(pace=0.25, response=1.7, softness=6, stripes=28, overlap=0.7, glow=0.6),
                  build=lambda graph: build(graph, records))
    entry = COMPONENTS.write_component(recipe)
    destination = ROOT / 'content/semantic/ink_cartography'
    manifest = json.loads((destination / 'manifest.json').read_text(encoding='utf-8'))
    manifest['assets'] = records
    for record in records:
        digest = record['sha256']
        relative = Path('assets/sha256') / digest[:2] / digest
        data = (source / relative).read_bytes()
        if len(data) != record['bytes'] or hashlib.sha256(data).hexdigest() != digest:
            raise ValueError('Existing shader bundle no longer matches its recorded identity')
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    COMPONENTS.write_json(destination / 'manifest.json', manifest)
    COMPONENTS.write_json(ROOT / 'provenance/ink_cartography.json', dict(
        ownership='first-party', source_commit='646e9e4',
        source_files=['tools/author-ink-tide.py', 'tools/author-input-components.py',
                      'content/templates/ink_tide/manifest.json'],
        authoring_tool='tools/author-ink-component.py',
        authoring_tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        assets=records, imported_third_party_files=[],
        existing_renderer_provenance=['provenance/tixl_effects.json'],
        modifications=['Reuse both existing shader bundles unchanged; expose image input and six controls.',
                       'Compose shared music signals, contour etching and FXAA from existing nodes.'],
        component=entry, target='content/semantic/ink_cartography'))


if __name__ == '__main__':
    main()

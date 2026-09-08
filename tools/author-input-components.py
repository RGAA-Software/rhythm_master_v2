"""Compose input-processing components from the existing attributed operators."""

import hashlib
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('graph_writer', ROOT / 'tools/author-resonance-gate.py')
WRITER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(WRITER)


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=4) + '\n', encoding='utf-8')


def flow_glass():
    graph = WRITER.Graph()
    node = graph.node
    time = node('core.time', 0, 0)
    pace = node('scalar.constant', 0, 300, value=0.12)
    phase = node('scalar.expression', 340, 0, dict(time=time, a=pace), expression='time * a')
    bass = node('audio.band', 0, 600, audio_band=12)
    high = node('audio.band', 0, 900, audio_band=48)
    response = node('scalar.constant', 0, 1200, value=1)
    strength = node('scalar.expression', 340, 650, dict(a=bass, b=high, c=response),
                    expression='(0.045 + a * 0.24 + b * 0.12) * c')
    field = node('texture.noise', 700, 0, dict(phase=phase), noise_scale=3.8, contrast=2,
                 seed=471, color_a=(0.1, 0.7, 0, 1), color_b=(0.9, 0.3, 0, 1))
    displaced = node('texture.displace', 1040, 0, dict(displace_map=field, displace_strength=strength),
                     displace_mode=1, rotation=70)
    blur = node('texture.blur', 1380, 300, dict(source=displaced), blur_radius=5)
    output = node('texture.composite', 1720, 0, dict(a=displaced, b=blur), composite_mode=1, amount=0.18)
    parameters = [('flow', pace, 'value'), ('response', response, 'value'),
                  ('pattern_scale', field, 'noise_scale'), ('direction', displaced, 'rotation'),
                  ('softness', blur, 'blur_radius'), ('glow', output, 'amount')]
    return graph, displaced, output, parameters


def main():
    graph, input_node, output, parameters = flow_glass()
    name = 'flow_glass'
    component = 'component.official.' + name
    destination = ROOT / 'content/semantic' / name
    destination.mkdir(parents=True, exist_ok=True)
    # The four-node harness provides a visible fixture only. Insertion keeps
    # the component and asks the author to wire their own texture to source.
    harness = WRITER.Graph()
    fixture = harness.node('texture.gradient', 20, 80,
                           color_a=(0, 0, 0, 1), color_b=(1, 1, 1, 1))
    fixture = harness.node('texture.contours', 360, 80, dict(source=fixture),
                           contour_count=14, line_width=0.16,
                           color_a=(0.95, 0.56, 0.12, 1), color_b=(0.008, 0.04, 0.09, 1))
    instance = harness.node(component, 700, 80, dict(source=fixture))
    final = harness.node('output.texture', 1040, 80, dict(source=instance))
    text = ['schema_version: 4', f'id: "semantic-{name}"', 'canvas { width: 640 height: 360 }',
            f'output: {final}', *harness.nodes, *harness.edges,
            f'components {{ type_key: "{component}" schema_version: 1 title: "Flow glass" output: {output}',
            *graph.nodes, *graph.edges, f'inputs {{ key: "source" node: {input_node} input: "source" }}']
    for key, identity, property_name in parameters:
        text.append(f'parameters {{ key: "{key}" node: {identity} property: "{property_name}" group: "component.pattern" }}')
    (destination / 'graph.textproto').write_text('\n'.join(text + ['}']) + '\n', encoding='utf-8')
    write_json(destination / 'editor.json', dict(version=2, positions=harness.positions,
                                                components=[dict(type=component, positions=graph.positions)]))
    write_json(destination / 'manifest.json', dict(format='rhythm.project', manifest_version=1,
               kind='template', content_id='official.semantic.'+name, content_version='0.1.0',
               project_id='semantic-'+name, graph_revision=0, title='流纹玻璃 / Flow glass',
               default_locale='zh-CN', titles={'en-US':'Flow glass', 'zh-CN':'流纹玻璃'},
               category='compositing', maturity='visual-review-pending', author='Rhythm Master',
               license_status='First-party composition using existing attributed operators; outbound license pending',
               compatible_players=['windows', 'android'], external_assets=[], semantic=True, default=False,
               descriptions={'en-US':'Connect your image to source. Animated noise bends it like flowing glass; bass/high bands increase refraction, and a soft glow follows the image. The preview stripes are not inserted.',
                             'zh-CN':'把自己的图像连接到 source。动态噪声形成流动玻璃折射，低频和高频增强形变，柔光跟随输入画面。预览条纹不会插入工程。'}))
    common = dict(version='1.0.0', operator=component, reset=True)
    write_json(destination / 'presets.json', dict(schema_version=1, presets=[
        dict(common, id='official.semantic.flow_glass.default',
             titles={'en-US':'Default', 'zh-CN':'默认参数'}, properties={}),
        dict(common, id='official.semantic.flow_glass.variant',
             titles={'en-US':'Broad ripples', 'zh-CN':'宽幅涟漪'},
             properties=dict(flow=0.06, response=1.8, pattern_scale=1.8, direction=115, softness=12, glow=0.3))]))
    write_json(ROOT / 'provenance/input_components.json', dict(ownership='first-party',
               authoring_tool='tools/author-input-components.py',
               authoring_tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
               reused_graph_writer='tools/author-resonance-gate.py',
               existing_adapters='provenance/tixl_effects.json', imported_third_party_files=[],
               components=[dict(id=name, nodes=len(graph.nodes), input='texture',
                                composition='Noise/displace from the existing Ink Tide layer and existing blur/additive glow; exposed as an input-processing graph.')]))


if __name__ == '__main__':
    main()

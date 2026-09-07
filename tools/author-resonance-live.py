"""Compose an editable music performance from the existing first-party graph builder.

No new renderer/effect implementation: the 3D rings use the already attributed
Godot-derived torus primitive. Component definitions remain embedded and editable.
"""

import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('resonance_gate', ROOT / 'tools/author-resonance-gate.py')
GATE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GATE)


def component(graph, output, kind, title, parameters):
    lines = [f'components {{ type_key: "{kind}" schema_version: 1',
             f'title: {json.dumps(title, ensure_ascii=False)} output: {output}']
    lines.extend(graph.nodes + graph.edges)
    for key, node, prop, group, minimum, maximum in parameters:
        lines.append(f'parameters {{ key: "{key}" node: {node} property: "{prop}" '
                     f'group: "{group}" minimum: {minimum} maximum: {maximum} }}')
    return '\n'.join(lines) + '\n}'


def build_core(radius=0.47):
    graph = GATE.Graph()
    node = graph.node
    time = node('core.time', 0, 0)
    bass = node('audio.band', 0, 260, audio_band=12)
    loudness = node('audio.feature', 0, 520, audio_feature=1)
    scale = node('scalar.map', 320, 260, dict(value=bass), output_min=0.88, output_max=1.12)
    emission = node('scalar.map', 320, 520, dict(value=loudness), output_min=0.8, output_max=2.8)
    cyan = node('material.pbr', 660, 0, dict(emission=emission),
                color_a=(0.03, 0.45, 0.7, 1), color_b=(0.05, 0.65, 1, 1),
                metallic=0.7, roughness=0.24)
    gold = node('material.pbr', 660, 300, dict(emission=emission),
                color_a=(0.8, 0.28, 0.05, 1), color_b=(1, 0.3, 0.06, 1),
                metallic=0.8, roughness=0.21)
    geometry = node('geometry.torus', 660, 600, radius=radius, tube_ratio=0.025,
                    radial_segments=96, tube_segments=12)
    assembled = None
    for index in range(4):
        y = 960 + index * 350
        turn = node('scalar.expression', 0, y, dict(time=time),
                    expression=f'time * {9 if index % 2 else -7} + {index * 45}')
        instance = node('scene.instance', 660, y,
                        dict(geometry=geometry, material=cyan if index % 2 else gold))
        transform = node('scene.transform', 1000, y,
                         dict(scene=instance, rotation_y=turn, scale=scale),
                         rotation_x=35 + index * 32, rotation_z=index * 25)
        assembled = transform if assembled is None else node(
            'scene.merge', 1340, y, dict(a=assembled, b=transform))
    light = node('scene.directional_light', 1340, 0, light_energy=2.5,
                 light_x=-0.4, light_y=0.8, light_z=1)
    scene = node('scene.merge', 1680, 0, dict(a=assembled, b=light))
    camera = node('scene.camera', 1680, 320, eye_z=4, field_of_view=55)
    output = node('scene.render', 2020, 0, dict(scene=scene, camera=camera))
    return graph, output, [("pulse", scale, "output_max", "Music", 0.9, 1.5),
                           ("emission", emission, "output_max", "Music", 0.8, 5)]


def main():
    field, output = GATE.build_graph()
    # The field component returns its existing color-adjust result directly.
    field.nodes.pop()
    field.positions.pop()
    field.edges.pop()
    field_type = 'component.official.resonance_field'
    core_type = 'component.official.resonance_core'
    core, core_output, core_parameters = build_core()
    graph = GATE.Graph()
    node = graph.node
    time = node('core.time', 0, 0)
    intro = node('scalar.curve', 340, 0, dict(time=time))
    graph.nodes[intro - 1] = graph.nodes[intro - 1][:-2] + '''
    properties { key: "curve" value { curve {
        keys { seconds: 0 value: 0 interpolation: INTERPOLATION_SMOOTH }
        keys { seconds: 3 value: 1 interpolation: INTERPOLATION_LINEAR }
    } } }
}'''
    field_node = node(field_type, 0, 420)
    core_node = node(core_type, 340, 420)
    combined = node('texture.composite', 700, 420, dict(a=field_node, b=core_node),
                    composite_mode=1, amount=0.85)
    blur = node('texture.blur', 1040, 720, dict(source=core_node), blur_radius=9)
    glow = node('texture.composite', 1380, 420, dict(a=combined, b=blur),
                composite_mode=1, amount=0.45)
    fade = node('texture.affine', 1740, 420, dict(source=glow, opacity=intro))
    final = node('output.texture', 2080, 420, dict(source=fade))
    definitions = [component(field, output - 1, field_type, '共振光场 / Resonance field',
                            [('pulse', 8, 'output_max', 'Music', 0.9, 1.5),
                             ('exposure', output - 1, 'exposure', 'Look', -1, 2)]),
                   component(core, core_output, core_type, '星门核心 / Gate core', core_parameters)]
    destination = ROOT / 'content/templates/resonance_live'
    destination.mkdir(parents=True, exist_ok=True)
    header = [f'schema_version: 4\nid: "official-resonance-live"\noutput: {final}\n'
              'canvas { width: 1280 height: 720 }']
    (destination / 'graph.textproto').write_text(
        '\n'.join(header + graph.nodes + graph.edges + definitions) + '\n', encoding='utf-8')
    layout = dict(version=2, positions=graph.positions,
                  components=[dict(type=field_type, positions=field.positions),
                              dict(type=core_type, positions=core.positions)])
    (destination / 'editor.json').write_text(json.dumps(layout, indent=4) + '\n', encoding='utf-8')
    manifest = dict(format='rhythm.project', manifest_version=1, kind='template',
                    content_id='official.templates.resonance_live', content_version='0.1.0',
                    project_id='official-resonance-live', graph_revision=0,
                    title='星门演出 / Resonance Live', default_locale='zh-CN',
                    titles={'zh-CN': '星门演出', 'en-US': 'Resonance Live'},
                    category='audio', tier='advanced', maturity='visual-review-pending',
                    author='Rhythm Master',
                    license_status='First-party composition; existing attributed Godot torus and effect adapters; outbound license pending',
                    compatible_players=['windows'], external_assets=[],
                    descriptions={
                        'zh-CN': '完整音乐演出：24 频段光场、三维交织星环、粒子拖尾与开场关键帧。展开两个组件编辑内部节点，或保存到我的组件库复用。播放音乐后，时间轴跟随音乐；静音不会伪造节拍。',
                        'en-US': 'Music performance with a 24-band field, interlocking 3D rings, particle trails and an opening keyframe. Edit either embedded component or save it to your library. Music drives the shared timeline; silence never fabricates beats.'})
    (destination / 'manifest.json').write_text(
        json.dumps(manifest, ensure_ascii=False, indent=4) + '\n', encoding='utf-8')
    print(f'Resonance Live: {len(graph.nodes)} root nodes, '
          f'{len(field.nodes) + len(core.nodes)} component nodes')


if __name__ == '__main__':
    main()

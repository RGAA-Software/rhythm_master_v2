"""Compose a 64-column music sculpture using existing graph/GLM/Godot adapters.

This first-party composition reuses the Resonance graph writer, shares one cube
and four pillar materials, and merges instances in a balanced tree. No new shader
or renderer is introduced. Audio bands, rather than generated beats, set heights.
"""

import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('gate', ROOT / 'tools/author-resonance-gate.py')
GATE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GATE)


def build_graph():
    graph = GATE.Graph()
    node = graph.node
    time = node('core.time', 0, 0)
    turn = node('scalar.expression', 320, 0, dict(time=time), expression='sin(time * 0.12) * 18')
    rms = node('audio.feature', 0, 280, audio_feature=1)
    emission = node('scalar.map', 320, 280, dict(value=rms),
                    input_max=0.35, output_min=0.35, output_max=1.8)
    cube = node('geometry.cube', 640, 0)
    palette = ((0.02, 0.48, 0.75, 1), (0.24, 0.07, 0.65, 1),
               (0.8, 0.10, 0.32, 1), (0.85, 0.38, 0.05, 1))
    instances = []
    for index, color in enumerate(palette):
        material = node('material.pbr', 640, 280 + index * 280, dict(emission=emission),
                        color_a=color, color_b=color, metallic=0.4, roughness=0.32)
        instances.append(node('scene.instance', 960, 280 + index * 280,
                              dict(geometry=cube, material=material)))
    heights, centers = [], []
    for index in range(32):
        x, y = (index // 8) * 1280, 1700 + (index % 8) * 280
        band = node('audio.band', x, y, audio_band=index * 2)
        height = node('scalar.map', x + 320, y, dict(value=band),
                      input_max=0.10, output_min=0.18, output_max=3.7)
        heights.append(height)
        centers.append(node('scalar.expression', x + 640, y, dict(a=height), expression='a * 0.5'))
    transforms = []
    for row in range(8):
        for column in range(8):
            index = row * 8 + column
            band = (column * 4 + row % 4) % 32
            transforms.append(node('scene.transform', 5500 + column * 320, 1700 + row * 320,
                                   dict(scene=instances[column // 2], scale_y=heights[band],
                                        translate_y=centers[band]),
                                   scale_x=0.62, scale_z=0.62,
                                   translate_x=(column - 3.5) * 0.9,
                                   translate_z=(row - 3.5) * 0.9))
    floor_material = node('material.pbr', 1280, 0, color_a=(0.045, 0.07, 0.12, 1),
                          metallic=0.35, roughness=0.65)
    floor = node('scene.instance', 1600, 0, dict(geometry=cube, material=floor_material))
    transforms.append(node('scene.transform', 1920, 0, dict(scene=floor),
                           scale_x=8, scale_y=0.12, scale_z=8, translate_y=-0.08))
    grid_material = node('material.unlit', 1280, 320, color_a=(0.015, 0.15, 0.24, 1))
    grid = node('scene.instance', 1600, 320, dict(geometry=cube, material=grid_material))
    for index in range(9):
        position = (index - 4) * 0.9
        transforms.append(node('scene.transform', 1920 + index * 320, 400, dict(scene=grid),
                               scale_x=7.2, scale_y=0.015, scale_z=0.018,
                               translate_y=0.002, translate_z=position))
        transforms.append(node('scene.transform', 1920 + index * 320, 720, dict(scene=grid),
                               scale_x=0.018, scale_y=0.015, scale_z=7.2,
                               translate_y=0.002, translate_x=position))
    level = 0
    while len(transforms) > 1:
        merged = []
        for index in range(0, len(transforms), 2):
            if index + 1 == len(transforms):
                merged.append(transforms[index])
            else:
                merged.append(node('scene.merge', 8200 + level * 320, index * 180,
                                   dict(a=transforms[index], b=transforms[index + 1])))
        transforms = merged
        level += 1
    sculpture = node('scene.transform', 10600, 0, dict(scene=transforms[0], rotation_y=turn))
    key = node('scene.directional_light', 10600, 300, light_x=-0.6, light_y=0.9, light_z=0.7,
               light_energy=3.0, color_a=(0.6, 0.8, 1, 1))
    fill = node('scene.directional_light', 10600, 600, light_x=0.8, light_y=0.5, light_z=-0.6,
                light_energy=1.8, color_a=(1, 0.3, 0.2, 1))
    lit = node('scene.merge', 10920, 0, dict(a=sculpture, b=key))
    lit = node('scene.merge', 11240, 0, dict(a=lit, b=fill))
    camera = node('scene.camera', 11240, 320, eye_x=8, eye_y=7, eye_z=10,
                  target_y=0.7, field_of_view=43)
    render = node('scene.render', 11560, 0, dict(scene=lit, camera=camera))
    back = node('texture.gradient', 11560, 320, color_a=(0.004, 0.01, 0.028, 1),
                color_b=(0.025, 0.006, 0.035, 1))
    combined = node('texture.composite', 11880, 0, dict(a=back, b=render), composite_mode=1)
    blur = node('texture.blur', 11880, 320, dict(source=render), blur_radius=8)
    glow = node('texture.composite', 12200, 0, dict(a=combined, b=blur),
                composite_mode=1, amount=0.25)
    final = node('output.texture', 12520, 0, dict(source=glow))
    return graph, final


def main():
    graph, final = build_graph()
    destination = ROOT / 'content/templates/harmonic_city'
    destination.mkdir(parents=True, exist_ok=True)
    header = (f'schema_version: 4\nid: "official-harmonic-city"\noutput: {final}\n'
              'canvas { width: 1280 height: 720 }')
    (destination / 'graph.textproto').write_text(
        '\n'.join([header, *graph.nodes, *graph.edges]) + '\n', encoding='utf-8')
    (destination / 'editor.json').write_text(
        json.dumps(dict(version=2, positions=graph.positions), indent=4) + '\n', encoding='utf-8')
    manifest = dict(format='rhythm.project', manifest_version=1, kind='template',
                    content_id='official.templates.harmonic_city', content_version='0.1.0',
                    project_id='official-harmonic-city', graph_revision=0,
                    title='律动之城 / Harmonic City', default_locale='zh-CN',
                    titles={'zh-CN': '律动之城', 'en-US': 'Harmonic City'},
                    category='audio', tier='advanced', maturity='visual-review-pending',
                    author='Rhythm Master',
                    license_status='First-party composition; existing attributed scene/math/BRDF adapters; outbound license pending',
                    compatible_players=['windows'], external_assets=[],
                    descriptions={
                        'zh-CN': '32 个真实频段控制 64 根三维音柱，独立轴缩放、双色灯光和柔光合成。音柱底部固定在地台，静音回落；一个共享网格、四种柱体材质及平衡场景合并树。所有节点均可编辑。',
                        'en-US': '32 real audio bands animate 64 grounded 3D columns, with independent axis scale, two lights and soft glow. Silence lowers the columns. One shared mesh, four pillar materials and a balanced scene merge tree; every node remains editable.'})
    (destination / 'manifest.json').write_text(
        json.dumps(manifest, ensure_ascii=False, indent=4) + '\n', encoding='utf-8')
    print(f'Harmonic City: {len(graph.nodes)} nodes, {len(graph.edges)} edges, 83 instances')


if __name__ == '__main__':
    main()

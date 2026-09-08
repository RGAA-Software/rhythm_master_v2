"""Author a light ceramic/brass kinetic performance with editable music controls.

Reuses our Graph writer, procedural geometry/material/shadow contracts and the
original pulse/chime media authored for Luminous Concerto. No external artwork.
"""

import importlib.util
import json
import math
from pathlib import Path

import music_work

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('gate', ROOT / 'tools/author-resonance-gate.py')
GATE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GATE)


def build_graph():
    graph = GATE.Graph()
    node = graph.node
    response = node('control.scalar', -1000, 0, value=1, control_minimum=0, control_maximum=2)
    pace = node('control.scalar', -1000, 300, value=1, control_minimum=0.2, control_maximum=2)
    exposure = node('control.scalar', -1000, 600, value=0, control_minimum=-1, control_maximum=1)
    time = node('core.time', 0, 0)
    clock = node('scalar.expression', 320, 0, dict(time=time, a=pace), expression='time * a')
    bands = []
    for index, band in enumerate((12, 28, 48)):
        source = node('audio.band', 0, 350 + index * 300, audio_band=band)
        bands.append(node('scalar.expression', 320, 350 + index * 300,
                          dict(a=source, b=response), expression='a * b'))
    ceramic = node('material.pbr', 700, 0, color_a=(0.83, 0.80, 0.73, 1), metallic=0.05, roughness=0.32)
    brass = node('material.pbr', 700, 300, color_a=(0.63, 0.38, 0.12, 1), metallic=0.8, roughness=0.24)
    red = node('material.pbr', 700, 600, color_a=(0.27, 0.015, 0.025, 1), metallic=0.25, roughness=0.3)
    body_mesh = node('geometry.torus', 1000, 0, radius=1.92, tube_ratio=0.075,
                     radial_segments=96, tube_segments=16)
    body = node('scene.instance', 1320, 0, dict(geometry=body_mesh, material=ceramic))
    body = node('scene.transform', 1640, 0, dict(scene=body), rotation_x=90, translate_y=1.35)
    parts = [body]
    for index, (radius, ratio, material) in enumerate(((1.65, 0.024, brass), (1.15, 0.048, red))):
        y = 1200 + index * 1300
        angle = node('scalar.expression', 700, y, dict(time=clock, a=bands[index]),
                     expression=f'sin(time * {0.31 + index * 0.19}) * {28 + index * 22} + a * 35')
        bend = node('scalar.map', 700, y + 300, dict(value=bands[index]), input_max=0.3,
                    output_min=5, output_max=40)
        ring = node('geometry.torus', 1040, y, radius=radius, tube_ratio=ratio,
                    radial_segments=96, tube_segments=12)
        ring = node('geometry.deform', 1380, y, dict(geometry=ring, deform_twist=bend), deform_axis=0)
        instance = node('scene.instance', 1720, y, dict(geometry=ring, material=material))
        parts.append(node('scene.transform', 2060, y, dict(scene=instance, rotation_y=angle),
                          rotation_x=90, translate_y=1.35, rotation_z=index * 40))
    cube = node('geometry.cube', 1000, 4200)
    tick = node('scene.instance', 1320, 4200, dict(geometry=cube, material=brass))
    sphere = node('geometry.sphere', 1000, 4500, radius=0.11, height=0.22,
                  radial_segments=24, rings=12)
    bead = node('scene.instance', 1320, 4500, dict(geometry=sphere, material=red))
    pulse = node('scalar.map', 1000, 4800, dict(value=bands[2]), input_max=0.25,
                 output_min=0.8, output_max=1.7)
    for index in range(12):
        angle = index * math.pi / 6
        x, y = math.sin(angle), math.cos(angle)
        row = 4200 + index * 280
        parts.append(node('scene.transform', 1720, row, dict(scene=tick),
                          translate_x=x * 1.92, translate_y=1.35 + y * 1.92, translate_z=0.19,
                          scale_x=0.04, scale_y=0.16 if index % 3 else 0.23, scale_z=0.025,
                          rotation_z=-index * 30))
        parts.append(node('scene.transform', 2060, row, dict(scene=bead, scale=pulse),
                          translate_x=x * 2.22, translate_y=1.35 + y * 2.22, translate_z=0))
    rod_mesh = node('geometry.sphere', 2500, 1200, radius=0.042, height=1.55,
                    radial_segments=16, rings=16)
    rod = node('scene.instance', 2820, 1200, dict(geometry=rod_mesh, material=brass))
    rod = node('scene.transform', 3140, 1200, dict(scene=rod), translate_y=-0.75)
    bob_mesh = node('geometry.sphere', 2500, 1600, radius=0.28, height=0.56,
                    radial_segments=48, rings=24)
    bob = node('scene.instance', 2820, 1600, dict(geometry=bob_mesh, material=red))
    bob = node('scene.transform', 3140, 1600, dict(scene=bob), translate_y=-1.55)
    pendulum = node('scene.merge', 3460, 1200, dict(a=rod, b=bob))
    swing = node('scalar.expression', 3140, 2100, dict(time=clock, a=bands[0]),
                 expression='sin(time * 2.4) * (18 + a * 65)')
    parts.append(node('scene.transform', 3780, 1200, dict(scene=pendulum, rotation_z=swing),
                      translate_y=1.35, translate_z=0.24))
    center_mesh = node('geometry.sphere', 2500, 2600, radius=0.19, height=0.38,
                       radial_segments=32, rings=16)
    center = node('scene.instance', 2820, 2600, dict(geometry=center_mesh, material=brass))
    parts.append(node('scene.transform', 3140, 2600, dict(scene=center),
                      translate_y=1.35, translate_z=0.27))
    floor_mat = node('material.pbr', 2500, 3100, color_a=(0.72, 0.69, 0.61, 1),
                     metallic=0, roughness=0.9)
    floor = node('scene.instance', 2820, 3100, dict(geometry=cube, material=floor_mat))
    parts.append(node('scene.transform', 3140, 3100, dict(scene=floor),
                      translate_y=-1.05, scale_x=14, scale_y=0.08, scale_z=12))
    stage = parts[0]
    for index, part in enumerate(parts[1:]):
        stage = node('scene.merge', 4200 + (index % 6) * 320, (index // 6) * 300, dict(a=stage, b=part))
    light = node('scene.spot_light', 6200, 1900, translate_x=-4, translate_y=7, translate_z=5,
                 light_x=0.45, light_y=-0.75, light_z=-0.5, light_energy=95, light_range=20,
                 spot_angle=58, color_a=(1, 0.91, 0.78, 1))
    stage = node('scene.merge', 6520, 0, dict(a=stage, b=light))
    fill = node('scene.directional_light', 6200, 2300, light_x=0.6, light_y=0.5,
                light_z=0.7, light_energy=0.8, color_a=(0.58, 0.72, 1, 1))
    stage = node('scene.merge', 6840, 0, dict(a=stage, b=fill))
    stage = node('scene.shadow', 7160, 0, dict(scene=stage), shadow_light=0,
                 shadow_resolution=1, shadow_bias=0.0008, shadow_normal_bias=0.02)
    environment = node('texture.gradient', 6840, 700, color_a=(0.8, 0.84, 0.9, 1),
                       color_b=(0.45, 0.37, 0.26, 1))
    stage = node('scene.environment', 7480, 0, dict(scene=stage, environment_texture=environment),
                 environment_energy=0.9)
    camera = node('scene.camera', 7480, 800, eye_x=3.6, eye_y=3.5, eye_z=8.5, target_y=1.0,
                  field_of_view=41, near_plane=0.1, far_plane=35)
    captured = node('scene.capture', 7800, 0, dict(scene=stage, camera=camera))
    color = node('scene.color', 8120, 0, dict(capture=captured))
    depth = node('scene.depth', 8120, 400, dict(capture=captured))
    focused = node('texture.dof', 8440, 0, dict(source=color, depth=depth),
                   focus_distance=9.4, focus_scale=32, dof_radius=2, dof_samples=12)
    background = node('texture.gradient', 8120, 800, color_a=(0.90, 0.87, 0.79, 1),
                      color_b=(0.60, 0.65, 0.72, 1))
    background = node('texture.linearize', 8440, 800, dict(source=background))
    composed = node('texture.composite', 8760, 0, dict(a=background, b=focused), texture_precision=0)
    display = node('texture.display', 9080, 0, dict(source=composed, exposure=exposure))
    final = node('output.texture', 9400, 0, dict(source=display))
    return graph, final, (response, pace, exposure)


def main():
    destination = ROOT / 'content/templates/porcelain_pendulum'
    destination.mkdir(parents=True, exist_ok=True)
    graph, final, controls = build_graph()
    response, pace, exposure = controls
    metadata = ['controls {']
    for identity, title in zip(controls, ('Music response / 音乐响应', 'Mechanical pace / 机械节奏', 'Exposure / 曝光')):
        metadata.append(f'    titles {{ key: {identity} value: "{title}" }}')
    for identity, title, values in [(1, 'Rest', (0.6, 0.65, -0.05)),
                                     (2, 'Drive', (1.1, 1, 0)), (3, 'Resonate', (1.6, 1.35, 0.08))]:
        metadata.append(f'    snapshots {{ id: {identity} title: "{title}"')
        for key, value in zip(controls, values):
            metadata.append(f'        values {{ key: {key} value: {value} }}')
        metadata.append('    }')
    for identity, (title, seconds, target, fade) in enumerate(
            [('Awaken', 0, 1, 0), ('Swing', 3, 2, 2), ('Resonate', 8, 3, 2), ('Settle', 12, 1, 3)], 1):
        metadata.append(f'    cues {{ id: {identity} title: "{title}" seconds: {seconds} snapshot: {target} fade: {fade} smooth: true }}')
    metadata.append('}')
    (destination / 'graph.textproto').write_text(
        f'schema_version: 5\nid: "official-porcelain-pendulum"\noutput: {final}\ncanvas {{ width: 1280 height: 720 }}\n' +
        '\n'.join(graph.nodes + graph.edges + metadata) + '\n', encoding='utf-8')
    (destination / 'editor.json').write_text(json.dumps(dict(version=2, positions=graph.positions), indent=4)+'\n', encoding='utf-8')
    records, soundtrack = music_work.original_arrangement(destination)
    manifest = dict(format='rhythm.project', manifest_version=3, kind='template',
                    content_id='official.templates.porcelain_pendulum', content_version='0.1.0',
                    project_id='official-porcelain-pendulum', graph_revision=0,
                    title='瓷光钟摆 / Porcelain Pendulum', default_locale='zh-CN',
                    titles={'zh-CN':'瓷光钟摆', 'en-US':'Porcelain Pendulum'}, category='audio', tier='basic',
                    maturity='visual-review-pending', author='Rhythm Master',
                    license_status='First-party graph and original synthesized music; existing attributed renderer adapters; outbound license pending',
                    compatible_players=['windows', 'android'], external_assets=[], assets=records, soundtrack=soundtrack,
                    descriptions={'zh-CN':'浅色陶瓷与黄铜环架围绕酒红钟摆。低频推动摆幅，中频扭转内环，高频点亮刻度；阴影、环境反射、景深、三个演出宏和 16 秒配乐编排均可编辑。',
                                  'en-US':'A burgundy pendulum swings inside ceramic and brass gimbals. Bass drives swing, mids twist the inner rings and highs pulse hour markers. Edit shadows, reflections, focus, three performance macros and the 16-second soundtrack arrangement.'})
    (destination / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=4)+'\n', encoding='utf-8')
    provenance = dict(ownership='first-party', baseline='2ce4ec2', imported_third_party_files=[],
                      source_files=['tools/author-resonance-gate.py', 'tools/author-torque-garden.py',
                                    'tools/author-sonic-enamel.py', 'tools/author-luminous-concerto.py'],
                      modifications=['New ceramic clock composition using existing geometry/deformation/light contracts.',
                                     'Reuse original pulse/chime bytes and their existing four-clip arrangement.',
                                     'Add three exposed macros, three snapshots and four cues.'],
                      music_assets=records, target='content/templates/porcelain_pendulum')
    (ROOT / 'provenance/porcelain_pendulum.json').write_text(json.dumps(provenance, ensure_ascii=False, indent=4)+'\n', encoding='utf-8')
    print(f'Porcelain Pendulum: {len(graph.nodes)} nodes, {len(graph.edges)} edges, 16-second music')


if __name__ == '__main__':
    main()

"""Author Crystal Choir: an editable, articulated, music-controlled flower stage."""

import hashlib
import importlib.util
import json
from pathlib import Path

from author_petal_model import build

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('gate', ROOT / 'tools/author-resonance-gate.py')
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)


def main():
    destination = ROOT / 'content/templates/crystal_choir'
    assets = destination / 'assets'
    assets.mkdir(parents=True, exist_ok=True)
    model = build()
    digest = hashlib.sha256(model).hexdigest()
    blob = assets / 'sha256' / digest[:2] / digest
    blob.parent.mkdir(parents=True, exist_ok=True)
    blob.write_bytes(model)
    graph = gate.Graph()
    node = graph.node
    gain = node('control.scalar', -1100, 0, value=1, control_minimum=0, control_maximum=3)
    speed = node('control.scalar', -1100, 350, value=4, control_minimum=0, control_maximum=16)
    exposure = node('control.scalar', -1100, 700, value=0.5, control_minimum=-2, control_maximum=2)
    bloom = node('control.scalar', -1100, 1050, value=0.3, control_minimum=0, control_maximum=1)
    time = node('core.time', 0, 0)
    turn = node('scalar.expression', 320, 0, dict(time=time, a=speed), expression='time * a')
    mesh = node('geometry.glb', 0, 400)
    graph.nodes[-1] = graph.nodes[-1][:-1] + f'    properties {{ key: "asset" value {{ asset_sha256: "{digest}" }} }}\n}}'
    bodies = []
    for group, color in enumerate(((0.025, 0.7, 0.85, 1), (0.75, 0.08, 0.32, 1), (1, 0.58, 0.10, 1))):
        y = 900 + group * 1400
        band = node('audio.band', 0, y, audio_band=10 + group * 19)
        band = node('scalar.expression', -500, y, dict(a=band, b=gain), expression='a * b')
        response = node('scalar.map', 320, y, dict(value=band), input_max=0.28, output_max=1)
        flare = node('scalar.map', 320, y + 320, dict(value=band), input_max=0.35,
                     output_min=0.1, output_max=0.8)
        ridge = node('scalar.expression', 640, y + 320, dict(time=time, a=response),
                     expression=f'0.15 + sin(time * 0.7 + {group * 2}) * 0.15 + a * 0.6')
        emission = node('scalar.map', 640, y + 640, dict(value=band), input_max=0.28,
                        output_min=0.04, output_max=0.9)
        animated = node('geometry.animate', 640, y, dict(geometry=mesh, animation_blend=response),
                        animation_second=1, animation_speed=0.65, animation_offset=group * 0.65)
        morphed = node('geometry.morph', 980, y,
                       dict(geometry=animated, morph_weight_1=flare, morph_weight_2=ridge,
                            morph_weight_4=response), morph_weight_3=0.25)
        material = node('material.pbr', 980, y + 640, dict(emission=emission), color_a=color,
                        color_b=color, metallic=0.65, roughness=0.26, double_sided=1)
        instance = node('scene.instance', 1320, y, dict(geometry=morphed, material=material))
        for side in range(2):
            bodies.append(node('scene.transform', 1660, y + side * 460, dict(scene=instance),
                               rotation_z=group * 60 + side * 180))
            bodies.append(node('scene.transform', 2020, y + side * 460, dict(scene=instance),
                               rotation_z=group * 60 + side * 180 + 30, scale=0.58, translate_z=0.55))
    merged = bodies[0]
    for i, body in enumerate(bodies[1:]):
        merged = node('scene.merge', 2400 + (i % 5) * 320, 5200 + (i // 5) * 320, dict(a=merged, b=body))
    merged = node('scene.transform', 3640, 900, dict(scene=merged, rotation_z=turn), rotation_x=-12)
    torus = node('geometry.torus', 3000, 2700, radius=2.85, tube_ratio=0.01,
                 radial_segments=128, tube_segments=8)
    gold = node('material.pbr', 3000, 3100, color_a=(0.12, 0.7, 0.8, 1),
                color_b=(0.05, 0.8, 1, 1), emission=0.9, metallic=0.65, roughness=0.3)
    ring = node('scene.instance', 3340, 2700, dict(geometry=torus, material=gold))
    ring = node('scene.transform', 3680, 2700, dict(scene=ring), rotation_x=90, translate_z=-0.35)
    merged = node('scene.merge', 4000, 900, dict(a=merged, b=ring))
    sphere = node('geometry.sphere', 3340, 3600, radius=0.3, height=0.6, radial_segments=48, rings=24)
    core = node('scene.instance', 3680, 3600, dict(geometry=sphere, material=gold))
    merged = node('scene.merge', 4340, 900, dict(a=merged, b=core))
    for index, (position, color) in enumerate((((-3, 3, 5), (0.35, 0.75, 1, 1)),
                                               ((3, -2, 4), (1, 0.3, 0.15, 1)))):
        light = node('scene.point_light', 4000, 1900 + index * 500,
                     translate_x=position[0], translate_y=position[1], translate_z=position[2],
                     light_energy=65, light_range=18, color_a=color)
        merged = node('scene.merge', 4680 + index * 340, 900, dict(a=merged, b=light))
    environment = node('texture.noise', 4340, 0, noise_scale=2.5, contrast=1.3, seed=913,
                       color_a=(0.012, 0.026, 0.06, 1), color_b=(0.75, 0.6, 0.4, 1))
    merged = node('scene.environment', 5360, 900, dict(scene=merged, environment_texture=environment),
                  environment_energy=1.3)
    camera = node('scene.camera', 5360, 1600, eye_y=0.2, eye_z=10, field_of_view=41,
                  near_plane=0.1, far_plane=30)
    rendered = node('scene.render', 5700, 900, dict(scene=merged, camera=camera))
    back = node('texture.gradient', 5360, 0, color_a=(0.004, 0.012, 0.024, 1),
                color_b=(0.025, 0.005, 0.02, 1))
    back = node('texture.linearize', 5700, 0, dict(source=back))
    combined = node('texture.composite', 6040, 900, dict(a=back, b=rendered), texture_precision=0)
    halo = node('texture.blur', 6040, 1300, dict(source=rendered), blur_radius=12, texture_precision=0)
    combined = node('texture.composite', 6380, 900, dict(a=combined, b=halo, amount=bloom),
                    composite_mode=1, amount=0.3, texture_precision=0)
    display = node('texture.display', 6720, 900, dict(source=combined, exposure=exposure), exposure=0.5)
    spectrum = node('texture.spectrum', 6380, 2100, spectrum_layout=1, spectrum_radius=0.40,
                    bar_count=96, spectrum_gain=1.3, bar_gap=0.7,
                    color_a=(0.06, 0.4, 0.55, 0.5), color_b=(0.9, 0.22, 0.12, 0.6))
    display = node('texture.composite', 7060, 900, dict(a=display, b=spectrum), composite_mode=1)
    final = node('output.texture', 7400, 900, dict(source=display))
    metadata = ['controls {']
    for control, title in ((gain, 'Music response'), (speed, 'Orbit speed'),
                           (exposure, 'Exposure'), (bloom, 'Bloom')):
        metadata.append(f'  titles {{ key: {control} value: "{title}" }}')
    for index, (title, values) in enumerate((('Quiet', (0.35, 1, 0.2, 0.15)),
                                            ('Concert', (1, 4, 0.5, 0.3)),
                                            ('Peak', (2, 10, 1.2, 0.65))), 1):
        metadata.append(f'  snapshots {{ id: {index} title: "{title}"')
        for control, value in zip((gain, speed, exposure, bloom), values):
            metadata.append(f'    values {{ key: {control} value: {value} }}')
        metadata.append('  }')
    metadata.append('}')
    (destination / 'graph.textproto').write_text(
        f'schema_version: 4\nid: "official-crystal-choir"\noutput: {final}\n'
        'canvas { width: 1280 height: 720 }\n' + '\n'.join(graph.nodes + graph.edges + metadata) + '\n', encoding='utf-8')
    (destination / 'editor.json').write_text(json.dumps(dict(version=2, positions=graph.positions), indent=4) + '\n', encoding='utf-8')
    manifest = json.loads((ROOT / 'content/templates/torque_garden/manifest.json').read_text(encoding='utf-8'))
    manifest.update(content_id='official.templates.crystal_choir', project_id='official-crystal-choir',
                    title='晶瓣合唱 / Crystal Choir', titles={'zh-CN': '晶瓣合唱', 'en-US': 'Crystal Choir'},
                    assets=[dict(sha256=digest, bytes=len(model), media_type='model/gltf-binary')],
                    descriptions={'zh-CN': '十二片带光脊的三骨骼晶瓣组成双层音乐花冠。低中高频分别混合舒展与脉冲动画，控制四路 GPU 形变及发光。共享模型、双点光与环境反射、近景光环和真实频谱均可在节点图中编辑。',
                                  'en-US': 'Twelve ribbed articulated petals form a layered music crown. Bass, mids and treble blend two skeletal clips and drive four GPU morph targets and emission. Edit the shared GLB, twin lights, environment reflections, halo and live spectrum in the graph.'})
    (destination / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=4) + '\n', encoding='utf-8')
    print(f'Crystal Choir: {len(graph.nodes)} nodes, {len(graph.edges)} edges, {len(model)} GLB bytes')


if __name__ == '__main__':
    main()

"""Author a music-driven woven sculpture from shared path meshes and GPU deformation."""

import hashlib
import importlib.util
from pathlib import Path

import music_work
import audio_band_groups

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('writer', ROOT / 'tools/author-resonance-gate.py')
WRITER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(WRITER)


def build_graph():
    graph = WRITER.Graph()
    node = graph.node
    response = node('control.scalar', -800, 0, value=1, control_minimum=0, control_maximum=2)
    pace = node('control.scalar', -800, 300, value=1, control_minimum=0.2, control_maximum=2)
    exposure = node('control.scalar', -800, 600, value=0.35, control_minimum=-1, control_maximum=1)
    time = node('core.time', 0, 0)
    clock = node('scalar.expression', 340, 0, dict(time=time, a=pace), expression='time * a')
    bands = []
    for index, source in enumerate(audio_band_groups.build_groups(node)):
        bands.append(node('scalar.expression', 340, 400 + index * 300,
                          dict(a=source, b=response), expression='a * b'))
    emission = node('scalar.expression', 700, 0, dict(a=bands[2]), expression='0.04 + a * 0.55')
    materials = []
    for index, tint in enumerate(((0.04, 0.62, 0.62, 1), (0.12, 0.25, 0.48, 1),
                                  (0.83, 0.51, 0.19, 1), (0.86, 0.7, 0.43, 1))):
        shimmer = node('scalar.expression', 700, 400 + index * 350, dict(a=emission, time=clock),
                       expression=f'a * (0.8 + 0.6 * sin(time * 0.9 + {index * 1.57}))')
        materials.append(node('material.pbr', 1050, index * 350, dict(emission=shimmer),
                              color_a=tint, color_b=tint, metallic=0.48, roughness=0.27))
    # The helix pitch equals two strand spacings. Alternating phases put each
    # crossing on opposite sides in depth without rebuilding paths every frame.
    instances = []
    for axis in range(2):
        for parity in range(2):
            row = axis * 1700 + parity * 700
            path = node('path.helix', 1450, row, path_height=6, path_radius=0.15,
                        path_turns=5, path_samples=256, path_phase=parity * 180)
            mesh = node('geometry.tube', 1800, row, dict(path=path), tube_radius=0.09, tube_sides=12)
            twist = node('scalar.expression', 1450, row + 300, dict(time=clock, a=bands[axis]),
                         expression=f'sin(time * 0.65 + {axis}) * 2 + a * 9')
            mesh = node('geometry.deform', 2150, row, dict(geometry=mesh, deform_twist=twist))
            instances.append(node('scene.instance', 2500, row,
                                  dict(geometry=mesh, material=materials[axis * 2 + parity])))
    parts = []
    for axis in range(2):
        for index in range(10):
            row = axis * 2000 + index * 350
            location = -2.7 + index * 0.6
            parts.append(node('scene.transform', 2900, row,
                              dict(scene=instances[axis * 2 + index % 2]),
                              rotation_z=90 if axis else 0,
                              translate_y=location if axis else 0,
                              translate_x=0 if axis else location))
    cube = node('geometry.cube', 2900, 8000)
    frame_mat = node('material.pbr', 3250, 8000, color_a=(0.04, 0.06, 0.085, 1),
                     metallic=0.72, roughness=0.32)
    frame = node('scene.instance', 3600, 8000, dict(geometry=cube, material=frame_mat))
    for index, (x, y, sx, sy) in enumerate(((-3.3, 0, 0.14, 6.75), (3.3, 0, 0.14, 6.75),
                                           (0, -3.3, 6.75, 0.14), (0, 3.3, 6.75, 0.14))):
        parts.append(node('scene.transform', 3950, 7600 + index * 350, dict(scene=frame),
                          translate_x=x, translate_y=y, translate_z=-0.12,
                          scale_x=sx, scale_y=sy, scale_z=0.3))
    clasp_mat = node('material.pbr', 3250, 9600, color_a=(0.71, 0.55, 0.27, 1),
                     metallic=0.8, roughness=0.22)
    clasp = node('scene.instance', 3600, 9600, dict(geometry=cube, material=clasp_mat))
    for axis in range(2):
        for index in range(10):
            for end in (-1, 1):
                x, y = (-2.7 + index * 0.6 + (0.15 if index % 2 == 0 else -0.15), end * 3.12)
                if axis:
                    x, y = y, x
                parts.append(node('scene.transform', 4300 + axis * 700 + (350 if end > 0 else 0),
                                  9300 + index * 350,
                                  dict(scene=clasp), translate_x=x, translate_y=y,
                                  scale_x=0.28 if axis else 0.16,
                                  scale_y=0.16 if axis else 0.28, scale_z=0.22))
    shuttle_mesh = node('geometry.sphere', 3950, 13500, radius=0.11, height=0.22,
                        radial_segments=24, rings=12)
    shuttle_scale = node('scalar.expression', 4300, 13500, dict(a=bands[2]), expression='0.8 + a * 2.4')
    for index, tint in enumerate(((1, 0.53, 0.09, 1), (0.02, 0.8, 1, 1))):
        row = 13900 + index * 1200
        material = node('material.pbr', 3950, row, color_a=tint, color_b=tint,
                        emission=2.5, metallic=0.2, roughness=0.18)
        shuttle = node('scene.instance', 4300, row, dict(geometry=shuttle_mesh, material=material))
        x = node('scalar.expression', 4650, row, dict(time=clock, a=bands[index]),
                 expression=f'sin(time * 0.8 + {index * 3.14}) * (2.35 + a * 0.4)')
        y = node('scalar.expression', 4650, row + 350, dict(time=clock),
                 expression=f'sin(time * 0.27 + {index * 2.2}) * 2.65')
        parts.append(node('scene.transform', 5000, row,
                          dict(scene=shuttle, translate_x=x, translate_y=y, scale=shuttle_scale),
                          translate_z=0.55))
    scene = parts[0]
    for index, part in enumerate(parts[1:]):
        scene = node('scene.merge', 4900 + index % 8 * 320, index // 8 * 320,
                     dict(a=scene, b=part))
    turn = node('scalar.expression', 7600, 0, dict(time=clock, a=bands[2]),
                expression='sin(time * 0.21) * 9 + a * 24 - 5')
    rock = node('scalar.expression', 7600, 350, dict(time=clock, a=bands[0]),
                expression='sin(time * 0.35) * 5 + a * 12')
    scene = node('scene.transform', 7950, 0, dict(scene=scene, rotation_z=turn, rotation_y=rock),
                 rotation_x=-8)
    light = node('scene.directional_light', 7600, 800, light_x=-0.4, light_y=0.7, light_z=0.8,
                 light_energy=2.4, color_a=(1, 0.91, 0.75, 1))
    scene = node('scene.merge', 8300, 0, dict(a=scene, b=light))
    power = node('scalar.expression', 7950, 1200, dict(a=bands[1]), expression='28 + a * 65')
    light = node('scene.point_light', 8300, 1200, dict(light_energy=power),
                 translate_x=3, translate_y=-2, translate_z=4, light_range=16,
                 color_a=(0.17, 0.6, 1, 1))
    scene = node('scene.merge', 8650, 0, dict(a=scene, b=light))
    scene = node('scene.shadow', 9000, 0, dict(scene=scene), shadow_resolution=2,
                 shadow_extent=9, shadow_distance=18)
    environment = node('texture.gradient', 8650, 1600, color_a=(0.5, 0.64, 0.7, 1),
                       color_b=(0.045, 0.065, 0.09, 1))
    scene = node('scene.environment', 9350, 0, dict(scene=scene, environment_texture=environment),
                 environment_energy=0.9)
    camera = node('scene.camera', 9350, 900, eye_x=2, eye_y=1.5, eye_z=13,
                  field_of_view=38, near_plane=0.1, far_plane=40)
    capture = node('scene.capture', 9700, 0, dict(scene=scene, camera=camera))
    color = node('scene.color', 10050, 0, dict(capture=capture))
    background = node('texture.gradient', 9700, 800, color_a=(0.013, 0.025, 0.045, 1),
                      color_b=(0.065, 0.11, 0.16, 1))
    background = node('texture.linearize', 10050, 800, dict(source=background))
    combined = node('texture.composite', 10400, 0, dict(a=background, b=color), texture_precision=0)
    display = node('texture.display', 10750, 0, dict(source=combined, exposure=exposure))
    smooth = node('texture.fxaa', 11100, 0, dict(source=display))
    output = node('output.texture', 11450, 0, dict(source=smooth))
    return graph, output, (response, pace, exposure)


def main():
    graph, output, controls = build_graph()
    manifest = music_work.write('chromatic_loom', graph, output,
        list(zip(controls, ('Music response / 音乐响应', 'Weaving pace / 编织节奏', 'Exposure / 曝光'))),
        [(1, 'Threading', (0.55, 0.55, 0.25)), (2, 'Weaving', (1, 1, 0.35)),
         (3, 'Shimmer', (1.5, 1.3, 0.5))],
        [('Threading', 0, 1, 0), ('Weaving', 3, 2, 2), ('Shimmer', 8, 3, 2), ('Settle', 12, 1, 3)],
        {'zh-CN': '织光机', 'en-US': 'Chromatic Loom'},
        {'zh-CN': '青蓝经线与金色纬线在立体织面中前后交错，两枚光梭在四十个金属线扣之间游走。低中频拧动经纬、高频点亮织线并改变朝向；路径网格、材质、灯光、三个宏和四段 16 秒音乐编排均可编辑。',
         'en-US': 'Teal warp and golden weft interlace while two luminous shuttles roam inside forty metal clasps. Bass and mids twist the strands; highs brighten the weave and turn its framing. Edit shared path meshes, materials, lighting, three macros and four cues with 16-second music.'},
        tier='advanced', platforms=('windows', 'android'), version='0.2.0')
    music_work.write_json(ROOT / 'provenance/chromatic_loom.json', dict(
        ownership='first-party', baseline='9463f68', imported_third_party_files=[],
        authoring_tool='tools/author-chromatic-loom.py',
        authoring_tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        reused_sources=['tools/author-resonance-gate.py', 'tools/music_work.py',
                        'tools/author-daylight-mobile.py', 'tools/audio_band_groups.py'],
        revision='P7: replace three isolated bins with complete low/mid/high peak groups.',
        composition='Original alternating-phase warp/weft sculpture with shared static path meshes, GPU twist, metallic clasps and music-driven framing.',
        assets=manifest['assets'], target='content/templates/chromatic_loom'))
    print(f'Chromatic Loom: {len(graph.nodes)} nodes, {len(graph.edges)} edges; 16-second music')


if __name__ == '__main__':
    main()

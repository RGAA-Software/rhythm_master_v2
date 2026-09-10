"""Compose an editable architectural music performance from existing operators.

The wall panels embed the official Prism fold component, including its editable
definition and layout. Geometry, PBR, lighting and music use existing contracts.
"""

import hashlib
import importlib.util
from pathlib import Path

import input_component_recipes
import music_work
import audio_band_groups

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('components', ROOT / 'tools/author-input-components.py')
COMPONENTS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(COMPONENTS)


def build_graph():
    graph = COMPONENTS.WRITER.Graph()
    node = graph.node
    response = node('control.scalar', -1000, 0, value=1, control_minimum=0, control_maximum=2)
    pace = node('control.scalar', -1000, 300, value=1, control_minimum=0.2, control_maximum=2)
    exposure = node('control.scalar', -1000, 600, value=-0.25, control_minimum=-1, control_maximum=1)
    time = node('core.time', 0, 0)
    clock = node('scalar.expression', 340, 0, dict(time=time, a=pace), expression='time * a')
    bands = []
    for index, source in enumerate(audio_band_groups.build_groups(node)):
        bands.append(node('scalar.expression', 340, 350 + index * 300,
                          dict(a=source, b=response), expression='a * b'))
    phase = node('scalar.expression', 700, 0, dict(time=clock, a=bands[1]),
                 expression='time * 0.045 + a * 0.7')
    field = node('texture.noise', 1040, 0, dict(phase=phase), noise_scale=2.7, contrast=1.8,
                 seed=617, color_a=(0, 0, 0, 1), color_b=(1, 1, 1, 1))
    lines = node('texture.contours', 1380, 0, dict(source=field), contour_count=5, line_width=0.14,
                 color_a=(0.04, 0.72, 0.82, 1), color_b=(0.003, 0.022, 0.038, 1))
    folded = node('component.official.prism_fold', 1720, 0, dict(source=lines),
                  pace=4, folds=5, scale=1.25, saturation=0.9, glow=0.1)
    folded = node('texture.blur', 2060, 0, dict(source=folded), blur_radius=1.2)
    glow = node('scalar.expression', 1040, 400, dict(a=bands[0], b=bands[2]),
                expression='0.7 + a * 3.2 + b * 2')
    screen_mat = node('material.pbr', 1720, 400, dict(emission=glow),
                      color_a=(0.13, 0.23, 0.28, 1), color_b=(0.5, 0.85, 1, 1),
                      metallic=0.3, roughness=0.25)
    screen_mat = node('material.textures', 2400, 0,
                      dict(material=screen_mat, base_texture=folded, emission_texture=folded))
    cube = node('geometry.cube', 1040, 900)
    screen = node('scene.instance', 2740, 0, dict(geometry=cube, material=screen_mat))
    floor_mat = node('material.pbr', 1040, 1200, color_a=(0.018, 0.03, 0.045, 1),
                     metallic=0.7, roughness=0.3)
    floor = node('scene.instance', 1380, 1200, dict(geometry=cube, material=floor_mat))
    parts = [node('scene.transform', 1720, 1200, dict(scene=floor), translate_y=-0.08,
                  translate_z=-3, scale_x=7, scale_y=0.15, scale_z=28)]
    path = node('path.helix', 1040, 1700, path_samples=65, path_radius=2.5,
                path_height=0, path_turns=0.5, path_phase=180)
    tube = node('geometry.tube', 1380, 1700, dict(path=path), tube_radius=0.045, tube_sides=8)
    for family, tint in enumerate(((0.015, 0.62, 0.85, 1), (1, 0.36, 0.065, 1))):
        y = 2100 + family * 1800
        energy = node('scalar.expression', 700, y, dict(a=bands[family], b=bands[2]),
                      expression='1.1 + a * 5 + b * 2.5')
        material = node('material.pbr', 1040, y, dict(emission=energy), color_a=tint,
                        color_b=tint, metallic=0.2, roughness=0.3)
        arch = node('scene.instance', 1720, y, dict(geometry=tube, material=material))
        arch = node('scene.transform', 2060, y, dict(scene=arch), rotation_x=90, translate_y=2.5)
        post = node('scene.instance', 1720, y + 300, dict(geometry=cube, material=material))
        for side in (-1, 1):
            upright = node('scene.transform', 2060, y + 450 + side * 150, dict(scene=post),
                           translate_x=side * 2.5, translate_y=1.25, scale_x=0.09, scale_y=2.5, scale_z=0.09)
            arch = node('scene.merge', 2400, y + 450 + side * 150, dict(a=arch, b=upright))
        for index in range(family, 7, 2):
            row = y + 900 + index * 200
            height = node('scalar.expression', 2740, row, dict(time=clock, a=bands[family]),
                          expression=f'1 + sin(time * 1.3 - {index * 0.65}) * 0.018 + a * {0.3 + index * 0.04}')
            parts.append(node('scene.transform', 3080, row, dict(scene=arch, scale_y=height),
                              translate_z=6 - index * 3))
    for index in range(6):
        for side in (-1, 1):
            parts.append(node('scene.transform', 3500 + (side + 1) * 180, index * 300,
                              dict(scene=screen), translate_x=side * 2.65, translate_y=2.15,
                              translate_z=4.5 - index * 3, scale_x=0.05, scale_y=3.65, scale_z=2.5))
    # Inlaid guide lines and cross ties articulate the floor without pretending
    # that the renderer implements screen-space reflections or transparent glass.
    gold = node('material.pbr', 3500, 2100, color_a=(0.25, 0.13, 0.04, 1),
                color_b=(0.8, 0.25, 0.025, 1), emission=0.6, metallic=0.75, roughness=0.24)
    strip = node('scene.instance', 3840, 2100, dict(geometry=cube, material=gold))
    for side in (-1, 1):
        parts.append(node('scene.transform', 4180, 2400 + side * 150, dict(scene=strip),
                          translate_x=side * 1.85, translate_y=0.015, translate_z=-3,
                          scale_x=0.025, scale_y=0.02, scale_z=27))
    for index in range(10):
        parts.append(node('scene.transform', 4180, 2900 + index * 220, dict(scene=strip),
                          translate_y=0.012, translate_z=9 - index * 2.4,
                          scale_x=3.7, scale_y=0.02, scale_z=0.018))
    # A distant circular terminus anchors the vanishing point.
    ring = node('geometry.torus', 3500, 5400, radius=0.85, tube_ratio=0.025,
                radial_segments=64, tube_segments=8)
    ring_mat = node('material.pbr', 3500, 5750, dict(emission=glow),
                    color_a=(0.04, 0.15, 0.2, 1), color_b=(0.04, 0.65, 1, 1), metallic=0.5)
    ring = node('scene.instance', 3840, 5400, dict(geometry=ring, material=ring_mat))
    pulse = node('scalar.expression', 3840, 5750, dict(a=bands[0]), expression='1 + a * 0.85')
    parts.append(node('scene.transform', 4180, 5400, dict(scene=ring, scale=pulse),
                      translate_y=2.4, translate_z=-15, rotation_x=90))
    stage = parts[0]
    for index, part in enumerate(parts[1:]):
        stage = node('scene.merge', 4600 + (index % 6) * 320, (index // 6) * 300,
                     dict(a=stage, b=part))
    light = node('scene.directional_light', 6600, 0, light_x=-0.3, light_y=0.9, light_z=0.4,
                 light_energy=0.7, color_a=(0.55, 0.75, 1, 1))
    stage = node('scene.merge', 6940, 0, dict(a=stage, b=light))
    for index, (z, tint) in enumerate(((5, (0.05, 0.55, 1, 1)), (-5, (1, 0.26, 0.04, 1)))):
        power = node('scalar.expression', 6600, 400 + index * 700,
                     dict(a=bands[index]), expression='14 + a * 65')
        light = node('scene.point_light', 6940, 400 + index * 700, dict(light_energy=power),
                     translate_y=2.8, translate_z=z, light_range=12, color_a=tint)
        stage = node('scene.merge', 7280 + index * 340, 0, dict(a=stage, b=light))
    environment = node('texture.gradient', 7620, 800, color_a=(0.08, 0.16, 0.22, 1),
                       color_b=(0.025, 0.035, 0.065, 1))
    stage = node('scene.environment', 7960, 0, dict(scene=stage, environment_texture=environment),
                 environment_energy=0.6)
    sway = node('scalar.expression', 7620, 1200, dict(time=clock), expression='sin(time * 0.22) * 1.4')
    stage = node('scene.transform', 8300, 0, dict(scene=stage, rotation_z=sway))
    camera = node('scene.camera', 8300, 700, eye_y=2.2, eye_z=10, target_y=2.25, target_z=-5,
                  field_of_view=58, near_plane=0.1, far_plane=45)
    capture = node('scene.capture', 8640, 0, dict(scene=stage, camera=camera))
    color = node('scene.color', 8980, 0, dict(capture=capture))
    depth = node('scene.depth', 8980, 400, dict(capture=capture))
    focused = node('texture.dof', 9320, 0, dict(source=color, depth=depth),
                   focus_distance=11, focus_scale=38, dof_radius=1.5, dof_samples=12)
    background = node('texture.gradient', 8980, 800, color_a=(0.004, 0.009, 0.018, 1),
                      color_b=(0.012, 0.02, 0.035, 1))
    background = node('texture.linearize', 9320, 800, dict(source=background))
    composed = node('texture.composite', 9660, 0, dict(a=background, b=focused), texture_precision=0)
    bloom = node('texture.blur', 10000, 400, dict(source=composed), blur_radius=7)
    composed = node('texture.composite', 10340, 0, dict(a=composed, b=bloom),
                    composite_mode=1, amount=0.18, texture_precision=0)
    display = node('texture.display', 10680, 0, dict(source=composed, exposure=exposure))
    display = node('texture.fxaa', 11020, 0, dict(source=display))
    final = node('output.texture', 11360, 0, dict(source=display))
    return graph, final, (response, pace, exposure)


def main():
    graph, output, controls = build_graph()
    recipe = next(item for item in input_component_recipes.RECIPES if item['name'] == 'prism_fold')
    inner, definition, layout = COMPONENTS.component_definition(recipe)
    manifest = music_work.write('resonant_arcade', graph, output,
        list(zip(controls, ('Music response / 音乐响应', 'Arcade pace / 拱廊节奏', 'Exposure / 曝光'))),
        [(1, 'Arrival', (0.6, 0.65, -0.35)), (2, 'Procession', (1.1, 1, -0.2)),
         (3, 'Resonance', (1.6, 1.35, -0.05))],
        [('Enter', 0, 1, 0), ('Procession', 3, 2, 2), ('Resonance', 8, 3, 2), ('Return', 12, 1, 3)],
        {'zh-CN': '共振拱廊', 'en-US': 'Resonant Arcade'},
        {'zh-CN': '青金拱门沿纵深交替呼吸，棱镜纹理屏幕与地面导光线构成音乐建筑。低频推动拱高和终点环，中频改变纹理，高频增强发光；三个公开宏、四段 Cue 与 16 秒配乐可编辑。',
         'en-US': 'Cyan and amber arches breathe along a deep arcade. Editable prism-pattern screens and floor inlays form a musical architecture. Bass drives arches and the end ring, mids move textures, and highs brighten emission. Three macros, four cues and a 16-second arrangement.'},
        tier='advanced', platforms=('windows', 'android'), components=[(definition, layout)], version='0.2.0')
    music_work.write_json(ROOT / 'provenance/resonant_arcade.json', dict(
        ownership='first-party', baseline='cef5e04', imported_third_party_files=[],
        authoring_tool='tools/author-resonant-arcade.py',
        authoring_tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        reused_sources=['tools/author-resonance-gate.py', 'tools/music_work.py',
                        'tools/author-input-components.py', 'tools/input_component_recipes.py',
                        'tools/audio_band_groups.py'],
        revision='P7: replace root isolated bins with complete low/mid/high peak groups; retain the embedded Prism fold definition.',
        composition='Original architectural arrangement of shared arch geometry, textured PBR screens, inlaid floor, three lights and music-driven framing.',
        embedded_component=dict(type=layout['type'], nodes=len(inner.nodes),
                                source='content/semantic/prism_fold/graph.textproto'),
        assets=manifest['assets'], target='content/templates/resonant_arcade'))
    print(f'Resonant Arcade: {len(graph.nodes)} root nodes, {len(graph.edges)} root edges; '
          f'{len(graph.nodes) - 1 + len(inner.nodes)} expanded instructions, 16-second music')


if __name__ == '__main__':
    main()

"""Author a pastel hanging music sculpture using existing scene primitives."""

import hashlib
import importlib.util
from pathlib import Path

import music_work

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('writer', ROOT / 'tools/author-resonance-gate.py')
WRITER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(WRITER)


def build_graph():
    graph = WRITER.Graph()
    node = graph.node
    response = node('control.scalar', -800, 0, value=1, control_minimum=0, control_maximum=2)
    pace = node('control.scalar', -800, 300, value=1, control_minimum=0.2, control_maximum=2)
    exposure = node('control.scalar', -800, 600, value=0.5, control_minimum=-1, control_maximum=1)
    time = node('core.time', 0, 0)
    clock = node('scalar.expression', 320, 0, dict(time=time, a=pace), expression='time * a')
    bands = []
    for index, band in enumerate((12, 28, 48)):
        source = node('audio.band', 0, 350 + index * 300, audio_band=band)
        bands.append(node('scalar.expression', 320, 350 + index * 300,
                          dict(a=source, b=response), expression='a * b'))
    cube = node('geometry.cube', 700, 0)
    disk = node('geometry.sphere', 700, 350, radius=0.55, height=0.13,
                radial_segments=48, rings=20)
    brass = node('material.pbr', 700, 700, color_a=(0.25, 0.19, 0.10, 1),
                 metallic=0.7, roughness=0.3)
    wire = node('scene.instance', 1040, 700, dict(geometry=cube, material=brass))
    materials = []
    for index, tint in enumerate(((0.72, 0.12, 0.07, 1), (0.045, 0.32, 0.30, 1),
                                  (0.88, 0.58, 0.13, 1), (0.30, 0.46, 0.62, 1))):
        materials.append(node('material.pbr', 1040, 1100 + index * 300, color_a=tint,
                              metallic=0.12, roughness=0.28))

    def merge(parts, x, y):
        scene = parts[0]
        for index, part in enumerate(parts[1:]):
            scene = node('scene.merge', x + (index % 4) * 320, y + (index // 4) * 260,
                         dict(a=scene, b=part))
        return scene

    assemblies = []
    for branch, (width, drop) in enumerate(((2.8, 0.85), (2.6, 1.25))):
        row = 2400 + branch * 3700
        parts = [node('scene.transform', 1500, row, dict(scene=wire),
                      translate_y=drop / 2, scale_x=0.018, scale_y=drop, scale_z=0.018),
                 node('scene.transform', 1500, row + 300, dict(scene=wire),
                      scale_x=width, scale_y=0.027, scale_z=0.027)]
        for index, (horizontal, length) in enumerate(((-0.43, 0.70), (0, 1.2), (0.43, 0.85))):
            y = row + 700 + index * 550
            parts.append(node('scene.transform', 1840, y, dict(scene=wire),
                              translate_x=horizontal * width, translate_y=-length / 2,
                              scale_x=0.014, scale_y=length, scale_z=0.014))
            material = materials[(index + branch * 2) % len(materials)]
            pendant = node('scene.instance', 2180, y, dict(geometry=disk if index != 1 else cube,
                                                        material=material))
            spin = node('scalar.expression', 2180, y + 260, dict(time=clock, a=bands[(index + branch) % 3]),
                        expression=f'sin(time * {0.5 + index * 0.2} + {branch + index}) * 18 + a * 100')
            pendant = node('scene.transform', 2520, y, dict(scene=pendant, rotation_y=spin),
                           rotation_x=90 if index != 1 else 0, rotation_z=45 if index == 1 else 0,
                           scale_x=0.75 if index == 1 else 1,
                           scale_y=0.75 if index == 1 else 1,
                           scale_z=0.09 if index == 1 else 1)
            parts.append(node('scene.transform', 2860, y, dict(scene=pendant),
                              translate_x=horizontal * width, translate_y=-length - 0.42))
        assembly = merge(parts, 3300, row)
        rocking = node('scalar.expression', 3300, row + 1400, dict(time=clock, a=bands[branch]),
                       expression=f'sin(time * {0.7 + branch * 0.17} + {branch * 1.7}) * 4 + a * {32 if branch == 0 else -38}')
        # Move the local suspension point to the pivot before rocking the branch.
        assembly = node('scene.transform', 4600, row, dict(scene=assembly), translate_y=-drop)
        assembly = node('scene.transform', 4940, row, dict(scene=assembly, rotation_z=rocking),
                        translate_x=-1.9 if branch == 0 else 1.9)
        assemblies.append(assembly)
    main_beam = node('scene.transform', 4600, 800, dict(scene=wire),
                     scale_x=4.1, scale_y=0.035, scale_z=0.035)
    main = merge([main_beam, *assemblies], 5300, 1200)
    angle = node('scalar.expression', 5300, 2000, dict(time=clock, a=bands[0]),
                 expression='sin(time * 0.4) * 3 + a * 18 - 3')
    main = node('scene.transform', 6600, 1200, dict(scene=main, rotation_z=angle), translate_y=2.15)
    ceiling_wire = node('scene.transform', 6600, 2000, dict(scene=wire),
                        translate_y=3.35, scale_x=0.02, scale_y=2.4, scale_z=0.02)
    main = node('scene.merge', 6940, 1200, dict(a=main, b=ceiling_wire))
    turn = node('scalar.expression', 6600, 2400, dict(time=clock, a=bands[2]),
                expression='sin(time * 0.23) * 5 + a * 45')
    main = node('scene.transform', 7280, 800, dict(scene=main, rotation_y=turn))
    floor_mat = node('material.pbr', 6600, 2700, color_a=(0.87, 0.85, 0.76, 1),
                     metallic=0, roughness=0.95)
    floor = node('scene.instance', 6940, 2700, dict(geometry=cube, material=floor_mat))
    floor = node('scene.transform', 7280, 2700, dict(scene=floor), translate_y=-2.5,
                 scale_x=100, scale_y=0.1, scale_z=100)
    scene = node('scene.merge', 7280, 1200, dict(a=main, b=floor))
    light = node('scene.spot_light', 7280, 2000, translate_x=-3, translate_y=7, translate_z=5,
                 light_x=0.3, light_y=-0.8, light_z=-0.5, light_energy=100, light_range=22,
                 spot_angle=62, color_a=(1, 0.88, 0.70, 1))
    scene = node('scene.merge', 7620, 1200, dict(a=scene, b=light))
    fill = node('scene.directional_light', 7620, 2300, light_x=0.5, light_y=0.2, light_z=0.7,
                light_energy=1.8, color_a=(0.76, 0.87, 1, 1))
    scene = node('scene.merge', 7960, 1200, dict(a=scene, b=fill))
    scene = node('scene.shadow', 8300, 1200, dict(scene=scene), shadow_light=0,
                 shadow_resolution=2, shadow_bias=0.0006, shadow_normal_bias=0.01)
    environment = node('texture.gradient', 7960, 2700, color_a=(0.83, 0.89, 0.95, 1),
                       color_b=(0.52, 0.46, 0.35, 1))
    scene = node('scene.environment', 8640, 1200, dict(scene=scene, environment_texture=environment),
                 environment_energy=1)
    camera = node('scene.camera', 8640, 2300, eye_x=2.5, eye_y=2.4, eye_z=9.5, target_y=0.6,
                  field_of_view=40, near_plane=0.1, far_plane=40)
    capture = node('scene.capture', 8980, 1200, dict(scene=scene, camera=camera))
    color = node('scene.color', 9320, 1200, dict(capture=capture))
    depth = node('scene.depth', 9320, 1600, dict(capture=capture))
    focused = node('texture.dof', 9660, 1200, dict(source=color, depth=depth),
                   focus_distance=12, focus_scale=25, dof_radius=1.5, dof_samples=12)
    background = node('texture.gradient', 9320, 2100, color_a=(0.87, 0.84, 0.76, 1),
                      color_b=(0.65, 0.75, 0.78, 1))
    background = node('texture.linearize', 9660, 2100, dict(source=background))
    composed = node('texture.composite', 10000, 1200, dict(a=background, b=focused), texture_precision=0)
    display = node('texture.display', 10340, 1200, dict(source=composed, exposure=exposure))
    display = node('texture.fxaa', 10680, 1200, dict(source=display))
    final = node('output.texture', 11020, 1200, dict(source=display))
    return graph, final, (response, pace, exposure)


def main():
    graph, output, controls = build_graph()
    manifest = music_work.write('daylight_mobile', graph, output,
        list(zip(controls, ('Music response / 音乐响应', 'Swing pace / 摆动节奏', 'Exposure / 曝光'))),
        [(1, 'Breeze', (0.55, 0.65, 0.45)), (2, 'Dance', (1.05, 1, 0.5)),
         (3, 'Chorus', (1.5, 1.25, 0.6))],
        [('Breeze', 0, 1, 0), ('Dance', 3, 2, 2), ('Chorus', 8, 3, 2), ('Rest', 12, 1, 3)],
        {'zh-CN': '晴空悬音', 'en-US': 'Daylight Mobile'},
        {'zh-CN': '珊瑚红、青绿、金黄与雾蓝吊片组成浅色悬挂雕塑。低中频推动两级横梁，高频转动吊片；灯光、落影、材质、三个演出宏及 16 秒四段 Cue 均可编辑。',
         'en-US': 'Coral, teal, gold and mist-blue pendants form a sunlit hanging sculpture. Bass and mids rock its two-level beams while highs turn pendants. Edit lighting, shadows, materials, three macros and a 16-second cue arrangement.'},
        platforms=('windows', 'android'))
    music_work.write_json(ROOT / 'provenance/daylight_mobile.json', dict(
        ownership='first-party', baseline='782a294', imported_third_party_files=[],
        authoring_tool='tools/author-daylight-mobile.py',
        authoring_tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        reused_sources=['tools/author-resonance-gate.py', 'tools/music_work.py',
                        'tools/author-porcelain-pendulum.py'],
        composition='Original asymmetric hanging arrangement of six pendants and two pivoted branches; existing geometry, PBR, shadow and music contracts.',
        assets=manifest['assets'], target='content/templates/daylight_mobile'))
    print(f'Daylight Mobile: {len(graph.nodes)} nodes, {len(graph.edges)} edges, 16-second music')


if __name__ == '__main__':
    main()

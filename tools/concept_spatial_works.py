"""Spatial revision of the concept vortex and ceramic sculpture."""

import hashlib
import math

from concept_mesh_assets import ceramic_shell, vortex_mesh
from concept_work_common import ROOT, start, asset_node, compile_expression, merge, finish, publish


def mesh_asset(graph, name, data, x, y):
    digest = hashlib.sha256(data).hexdigest()
    path = ROOT / 'content/templates' / name / 'assets/sha256' / digest[:2] / digest
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return asset_node(graph, 'geometry.glb', x, y, digest), dict(
        sha256=digest, bytes=len(data), media_type='model/gltf-binary')


def studio_environment(graph, name, scene, assets, energy=1):
    expression = ('vec4(vec3(0.015, 0.025, 0.045) + vec3(7.0, 6.1, 4.8) * '
        'exp(-pow((uv.x - 0.22) * 13.0, 4.0) - pow((uv.y - 0.35) * 5.0, 4.0)) + '
        'vec3(4.5, 5.5, 7.0) * exp(-pow((uv.x - 0.72) * 19.0, 4.0) - pow((uv.y - 0.42) * 4.0, 4.0)) + '
        'vec3(2.8, 2.4, 1.9) * exp(-pow((uv.x - 0.49) * 25.0, 4.0) - pow((uv.y - 0.18) * 8.0, 4.0)), 1.0)')
    record = compile_expression(name, 'studio_softboxes', expression)
    assets.append(record)
    texture = asset_node(graph, 'texture.shader', 8700, 1500, record['sha256'], texture_precision=2)
    return graph.node('scene.environment', 9100, 0, dict(scene=scene, environment_texture=texture),
                      environment_energy=energy)


def render(graph, scene, camera, focus=None):
    node = graph.node
    if focus is not None:
        captured = node('scene.capture', 9500, 0, dict(scene=scene, camera=camera))
        image = node('scene.render', 9800, 0, dict(scene=scene, camera=camera), scene_antialiasing=1)
        depth = node('scene.depth', 9800, 400, dict(capture=captured))
        image = node('texture.dof', 10100, 0, dict(source=image, depth=depth),
                     focus_distance=focus, focus_scale=4, dof_radius=10, dof_samples=64, texture_precision=0)
    else:
        image = node('scene.render', 9800, 0, dict(scene=scene, camera=camera), scene_antialiasing=1)
    back = node('texture.gradient', 9800, 800, color_a=(.001, .003, .008, 1),
                color_b=(.018, .033, .055, 1))
    back = node('texture.linearize', 10100, 800, dict(source=back))
    return node('texture.composite', 10400, 0, dict(a=back, b=image), texture_precision=0)


def vortex():
    name = 'aureate_vortex'
    graph, clock, bands, controls = start()
    node = graph.node
    assets, parts = [], []
    for family, tint in enumerate(((.008, .43, .62, 1), (1, .45, .06, 1))):
        for population in range(2):
            row = (family * 2 + population) * 950
            mesh, asset = mesh_asset(graph, name, vortex_mesh(family, population), 1200, row)
            assets.append(asset)
            power = node('scalar.expression', 1500, row + 300,
                         dict(a=bands[2] if population else bands[1], b=bands[0]),
                         expression=('12 + a * 40 + b * 20' if population else '.6 + a * 2.8 + b * 2.5'))
            material = node('material.pbr', 1850, row, dict(emission=power),
                            color_a=(0, 0, 0, 1), color_b=tint, double_sided=1)
            instance = node('scene.instance', 2200, row, dict(geometry=mesh, material=material))
            turn = node('scalar.expression', 2200, row + 350, dict(time=clock, a=bands[0]),
                        expression='-time * 22.5')
            parts.append(node('scene.transform', 2600, row, dict(scene=instance, rotation_z=turn),
                              rotation_x=-8, rotation_y=12))
    stage = merge(graph, parts)
    eye_x = node('scalar.expression', 8100, 800, dict(time=clock), expression='1.1 * sin(time * .3926990817)')
    eye_y = node('scalar.expression', 8100, 1100, dict(time=clock), expression='.55 * cos(time * .3926990817)')
    eye_z = node('scalar.expression', 8100, 1400, dict(time=clock), expression='9.7 + .3 * cos(time * .3926990817)')
    camera = node('scene.camera', 8700, 800, dict(eye_x=eye_x, eye_y=eye_y, eye_z=eye_z),
                  field_of_view=45, near_plane=.1, far_plane=30)
    image = render(graph, stage, camera, 9.3)
    field = node('texture.blur', 10400, 800, dict(source=image), blur_radius=6, texture_precision=0)
    field = node('texture.color_adjust', 10700, 800, dict(source=field), contrast=1.04, texture_precision=0)
    emission = node('scalar.expression', 1100, 4400, dict(a=bands[2], b=bands[0]), expression='.7 + a * 5 + b * 2')
    flow = node('scalar.expression', 1100, 4750, dict(a=bands[1]), expression='.025 + a * .045')
    orbit = node('scalar.expression', 1100, 5100, dict(time=clock), expression='time * 45')
    for index, (capacity, size) in enumerate(((32768, .0035), (2048, .016))):
        row = 5100 + index * 900
        particles = node('gpu.particles', 1500, row, dict(emission=emission, flow_strength=flow),
                         particle_capacity=capacity, seed=617 + index * 43, initial_fill=1,
                         emission_rate=capacity / 5, lifetime=5, emitter_radius=.72,
                         particle_speed=.035, drag=.15, flow_frequency=12, flow_evolution=.3,
                         point_size=size, color_a=(1, .52, .08, .8), color_b=(1, .88, .43, 1))
        particles = node('gpu.map', 1700, row + 350, dict(points=particles, rotation=orbit))
        sampled = node('gpu.texture_sample', 1850, row, dict(points=particles, source=field),
                       sample_color=1, sample_size=.75 if index == 0 else .35)
        sparks = node('gpu.render', 2200, row, dict(points=sampled), point_blend=1, texture_precision=2)
        sparks = node('texture.color_adjust', 2550, row, dict(source=sparks),
                      exposure=2.2 if index == 0 else 3.5, texture_precision=0)
        image = node('texture.composite', 10700, row, dict(a=image, b=sparks),
                     composite_mode=1, amount=1, texture_precision=0)
    output = finish(graph, image, controls[2], .3)
    publish(name, ('鎏光流涡', 'Aureate Vortex'),
            ('三臂青金流涡持续旋转，相机沿空间轨迹绕行；几何光点与 34816 容量双层 GPU 粒子保持流动，静音也持续运动。低频增强光丝与粒子发射，中频控制细线与流动，高频控制闪点和发射。几何层使用场景景深，GPU 粒子层使用屏幕空间颜色取样与生命周期，叠加前景柔光；需要 GLES 3.1 compute。',
             'Continuously rotating cyan/gold arms, an orbiting camera and two flowing GPU particle fields (34816 capacity) move autonomously even in silence. Bass energizes filaments and particle emission, mids drive strands/flow and highs drive spark emission. The GPU particle overlay uses screen-space color sampling and lifetimes; requires GLES 3.1 compute.'),
            graph, output, controls, assets, compute=True)


def porcelain():
    name = 'porcelain_bloom'
    graph, clock, bands, controls = start()
    node = graph.node
    mesh, asset = mesh_asset(graph, name, ceramic_shell(), 1200, 0)
    assets, parts = [asset], []
    sheen = node('scalar.expression', 1200, 350, dict(a=bands[2], b=bands[1]), expression='.025 + a * .32 + b * .12')
    cream = node('material.pbr', 1500, 0, dict(emission=sheen), color_a=(1, 1, 1, 1),
                 color_b=(.65, .55, .42, 1), metallic=1, roughness=1, double_sided=1)
    noise = node('texture.noise', 1200, 900, noise_scale=3.8, contrast=1.7, seed=821,
                 color_a=(0, 0, 0, 1), color_b=(1, 1, 1, 1))
    veins = node('texture.contours', 1500, 900, dict(source=noise), contour_count=2, line_width=.012,
                 color_a=(.61, .49, .33, .35), color_b=(.76, .65, .47, .35))
    edge = '(1.0 - smoothstep(0.006, 0.02, min(min(uv.y, 1.0 - uv.y), 1.0 - uv.x)))'
    base_record = compile_expression(name, 'ceramic_glaze',
        f'vec4(mix(vec3(0.92, 0.88, 0.78) * (0.34 + 0.66 * smoothstep(0.0, 0.7, uv.x)), vec3(0.82, 0.54, 0.2), {edge}), 1.0)')
    orm_record = compile_expression(name, 'ceramic_roughness',
        f'vec4(0.2 + 0.8 * smoothstep(0.0, 0.65, uv.x), 0.22, 0.035 + 0.925 * {edge}, 1.0)')
    assets.extend((base_record, orm_record))
    base = asset_node(graph, 'texture.shader', 1500, 1250, base_record['sha256'], texture_precision=2)
    orm = asset_node(graph, 'texture.shader', 1850, 1250, orm_record['sha256'], texture_precision=2)
    surface = node('texture.composite', 1850, 900, dict(a=base, b=veins))
    cream = node('material.textures', 2200, 0, dict(material=cream, base_texture=surface, orm_texture=orm))
    petal = node('scene.instance', 2550, 0, dict(geometry=mesh, material=cream))
    for layer, count in enumerate((7, 4)):
        ring = []
        turn = node('scalar.expression', 2550, 500 + layer * 350, dict(time=clock),
                    expression=f'time * {22.5 if layer == 0 else -45}')
        opening = node('scalar.expression', 2550, 1400 + layer * 350, dict(a=bands[0], time=clock),
                       expression=f'{1 if layer == 0 else .63} + .06 * sin(time * .7853981634 + {layer}) + a * .08')
        for index in range(count):
            ring.append(node('scene.transform', 3000 + layer * 350, 2200 + index * 300,
                             dict(scene=petal), rotation_z=index * 360 / count + layer * 26,
                             rotation_y=22 * math.sin(index * 2), rotation_x=15 * math.cos(index * 1.7),
                             translate_z=layer * .55 + .12 * math.sin(index * 3)))
        ring = merge(graph, ring, 4200 + layer * 350)
        parts.append(node('scene.transform', 4900, layer * 400,
                          dict(scene=ring, rotation_z=turn, scale=opening)))
    brass = node('material.pbr', 3500, 0, color_a=(.83, .62, .31, 1), metallic=.95, roughness=.16)
    ball = node('geometry.sphere', 3500, 350, radius=.48, height=.96, radial_segments=64, rings=32)
    core = node('scene.instance', 3850, 0, dict(geometry=ball, material=brass))
    parts.append(node('scene.transform', 4900, 900, dict(scene=core), translate_z=.8))
    for index in range(4):
        path = node('path.helix', 3500, 5000 + index * 400, path_radius=2.7 + index * .23,
                    path_height=0, path_turns=.22 + index * .08, path_phase=25 + index * 93, path_samples=128)
        tube = node('geometry.tube', 3850, 5000 + index * 400, dict(path=path), tube_radius=.028, tube_sides=12)
        arc = node('scene.instance', 4200, 5000 + index * 400, dict(geometry=tube, material=brass))
        parts.append(node('scene.transform', 4550, 5000 + index * 400, dict(scene=arc),
                          rotation_x=90, rotation_y=index * 6 - 9, translate_z=.2))
    for index in range(11):
        angle = index * 2.39996
        radius = 2.8 + .35 * math.sin(index * 7)
        parts.append(node('scene.transform', 5000, 1800 + index * 250, dict(scene=core),
                          translate_x=math.cos(angle) * radius, translate_y=math.sin(angle) * radius,
                          translate_z=math.sin(index * 3), scale=.08 + (index % 4) * .065))
    stage = merge(graph, parts)
    for index, (direction, tint, power) in enumerate((((-.4, .8, 1), (1, .94, .83, 1), 1.2),
            ((.8, -.1, .4), (.65, .8, 1, 1), .7))):
        light = node('scene.directional_light', 7800, index * 400, light_x=direction[0],
                     light_y=direction[1], light_z=direction[2], color_a=tint, light_energy=power)
        stage = node('scene.merge', 8200, index * 400, dict(a=stage, b=light))
    stage = studio_environment(graph, name, stage, assets, .45)
    stage = node('scene.shadow', 9300, 800, dict(scene=stage), shadow_extent=4.1,
                 shadow_distance=12, shadow_resolution=3, shadow_bias=.00035, shadow_normal_bias=.015)
    eye_x = node('scalar.expression', 8100, 900, dict(time=clock), expression='3.1 * cos(time * .3926990817)')
    eye_y = node('scalar.expression', 8100, 1200, dict(time=clock), expression='2.4 + .8 * sin(time * .3926990817)')
    eye_z = node('scalar.expression', 8100, 1500, dict(time=clock), expression='9.8 + .7 * sin(time * .3926990817)')
    camera = node('scene.camera', 8700, 800, dict(eye_x=eye_x, eye_y=eye_y, eye_z=eye_z), target_z=.6,
                  field_of_view=43, near_plane=.1, far_plane=30)
    output = finish(graph, render(graph, stage, camera), controls[2], .08)
    publish(name, ('瓷金绽放', 'Porcelain Bloom'),
            ('瓷质壳面围绕黄铜核心以不同速度持续分层旋转和舒展，相机连续绕行；断金弧和游离珠点组成非对称空间，静音仍保持主运动。低频舒展、中频暖光、高频釉光；新曲面网格、细釉纹、HDR 棚灯环境、宏与音乐均可编辑。',
             'Ceramic shells rotate continuously at independent speeds, unfurl and reveal changing parallax through an orbiting camera, even in silence. Bass unfurls, mids warm the shells and highs lift glaze. Editable curved meshes, subtle veins and HDR studio lighting.'),
            graph, output, controls, assets)

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
    # The mesh vortex owns slow rotation. Independent low/mid/high events add
    # energy without replacing sustained motion with jitter.
    low_onset = node('event.audio_onset', 700, 4750, threshold=.012, band_first=0, band_last=20)
    low_envelope = node('event.envelope', 950, 4750, dict(events=low_onset), attack=.003,
                        decay=.05, sustain=.22, duration=.06, release=.18)
    mid_onset = node('event.audio_onset', 700, 5050, threshold=.010, band_first=21, band_last=41)
    mid_envelope = node('event.envelope', 950, 5050, dict(events=mid_onset), attack=.004,
                        decay=.04, sustain=.2, duration=.05, release=.15)
    high_onset = node('event.audio_onset', 700, 5350, threshold=.009, band_first=42, band_last=62)
    high_envelope = node('event.envelope', 950, 5350, dict(events=high_onset), attack=.002,
                         decay=.025, sustain=.16, duration=.035, release=.1)
    low_rate = node('scalar.expression', 1100, 4400, dict(a=bands[0], b=low_envelope),
                    expression='.28 + a * .34 + b * 1.05')
    mid_rate = node('scalar.expression', 1100, 4620, dict(a=bands[1], b=mid_envelope),
                    expression='.30 + a * .30 + b * .7')
    high_rate = node('scalar.expression', 1100, 4800, dict(a=bands[2], b=high_envelope),
                     expression='.22 + a * .22 + b * .42')
    emission = node('scalar.expression', 1400, 4400, dict(a=low_rate, b=mid_rate, c=high_rate),
                    expression='a + b + c')
    flow = node('scalar.expression', 1200, 5050, dict(a=bands[1], b=mid_envelope, c=low_envelope),
                expression='.022 + a * .065 + b * .16 + c * .055')
    orbit = node('scalar.expression', 1100, 5700, dict(time=clock, a=bands[1]),
                 expression='time * 45.0 + a * 16.0')
    for index, (capacity, size) in enumerate(((24576, .0035), (6144, .012))):
        row = 6000 + index * 900
        particles = node('gpu.particles', 1500, row,
                         dict(emission=emission, flow_strength=flow),
                         particle_capacity=capacity, seed=617 + index * 43, initial_fill=0,
                         emission_rate=820 if index == 0 else 310, lifetime=6 if index == 0 else 3.4,
                         emitter_radius=.72, particle_speed=.03 if index == 0 else .12, drag=.15,
                         flow_frequency=10 if index == 0 else 17, flow_evolution=.26 if index == 0 else .42,
                         point_size=size, color_a=(.08, .62, .88, .42) if index == 0 else (1, .42, .06, .85),
                         color_b=(1, .72, .18, .9) if index == 0 else (1, .93, .55, 1))
        particles = node('gpu.map', 1700, row + 350, dict(points=particles, rotation=orbit))
        sampled = node('gpu.texture_sample', 1850, row, dict(points=particles, source=field),
                       sample_color=1, sample_size=.72 if index == 0 else .3)
        sparks = node('gpu.render', 2200, row, dict(points=sampled), point_blend=1, texture_precision=2)
        sparks = node('texture.color_adjust', 2550, row, dict(source=sparks),
                      exposure=2.0 if index == 0 else 3.6, texture_precision=0)
        image = node('texture.composite', 10700, row, dict(a=image, b=sparks),
                     composite_mode=1, amount=1, texture_precision=0)
    output = finish(graph, image, controls[2], .3)
    publish(name, ('鎏光流涡', 'Aureate Vortex'),
            ('三臂青金流涡持续旋转，相机沿空间轨迹绕行；双层 GPU 粒子从空场按每秒速率持续生成。低、中、高频只提高生成速率和流向能量，不使用大批次 burst；几何层使用景深，粒子层从屏幕颜色取样；需要 GLES 3.1 compute。',
             'Continuously rotating cyan/gold arms and an orbiting camera remain autonomous. Two GPU particle layers start empty and spawn at a sustained per-second rate. Bass, mids and treble raise rate and flow energy without large burst batches; requires GLES 3.1 compute.'),
            graph, output, controls, assets, compute=True, schema_version=7)


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
    image = render(graph, stage, camera)
    # The flower remains a quiet, continuous sculpture in silence. A separate
    # foreground layer gives its three musical ranges distinct material gestures.
    low_onset = node('event.audio_onset', 6800, 4400, threshold=.013, band_first=0, band_last=20)
    low_envelope = node('event.envelope', 7150, 4400, dict(events=low_onset), attack=.004,
                        decay=.06, sustain=.2, duration=.06, release=.2)
    mid_onset = node('event.audio_onset', 6800, 4700, threshold=.010, band_first=21, band_last=41)
    mid_envelope = node('event.envelope', 7150, 4700, dict(events=mid_onset), attack=.004,
                        decay=.045, sustain=.2, duration=.05, release=.15)
    high_onset = node('event.audio_onset', 6800, 5000, threshold=.009, band_first=42, band_last=62)
    high_envelope = node('event.envelope', 7150, 5000, dict(events=high_onset), attack=.002,
                         decay=.025, sustain=.15, duration=.035, release=.11)
    low_rate = node('scalar.expression', 7500, 4400, dict(a=bands[0], b=low_envelope),
                    expression='.22 + a * .25 + b * .7')
    mid_rate = node('scalar.expression', 7500, 4580, dict(a=bands[1], b=mid_envelope),
                    expression='.25 + a * .35 + b * .6')
    high_rate = node('scalar.expression', 7500, 4760, dict(a=bands[2], b=high_envelope),
                     expression='.18 + a * .24 + b * .9')
    petal_emission = node('scalar.expression', 7850, 4400, dict(a=low_rate, b=mid_rate, c=high_rate),
                          expression='a + b + c')
    petal_flow = node('scalar.expression', 7500, 4700, dict(a=bands[1], b=mid_envelope),
                     expression='.018 + a * .075 + b * .15')
    for index, (capacity, size) in enumerate(((16384, .0034), (4096, .010))):
        row = 5400 + index * 700
        particles = node('gpu.particles', 7900, row,
                         dict(emission=petal_emission, flow_strength=petal_flow),
                         particle_capacity=capacity, seed=1042 + index * 73, initial_fill=0,
                         emission_rate=620 if index == 0 else 190, lifetime=7 if index == 0 else 3.6,
                         emitter_radius=.56, particle_speed=.024 if index == 0 else .095, drag=.18,
                         flow_frequency=7 if index == 0 else 15, flow_evolution=.15 if index == 0 else .32,
                         point_size=size, color_a=(.95, .78, .4, .32) if index == 0 else (1, .84, .5, .8),
                         color_b=(.48, .72, .8, .62) if index == 0 else (1, .96, .78, 1))
        turn = node('scalar.expression', 8250, row + 250, dict(time=clock, a=bands[1]),
                    expression=f'time * {18 if index == 0 else -36} + a * 12')
        particles = node('gpu.map', 8500, row, dict(points=particles, rotation=turn))
        render_particles = node('gpu.render', 8850, row, dict(points=particles), point_blend=1,
                                texture_precision=2)
        image = node('texture.composite', 10700, row, dict(a=image, b=render_particles),
                     composite_mode=1, amount=1, texture_precision=0)
    output = finish(graph, image, controls[2], .12)
    publish(name, ('瓷金绽放', 'Porcelain Bloom'),
            ('瓷质壳面围绕黄铜核心以不同速度持续分层旋转和舒展，相机连续绕行；釉光粒子从空场按每秒速率逐步补充。低、中、高频分别提高生长速度、流向和闪光密度，避免节拍时整团喷发；静音仍保持雕塑主运动。',
             'Ceramic shells rotate and unfurl continuously around brass while the camera orbits. Glaze particles start empty and replenish at a per-second rate. Bass, mids and treble separately raise growth rate, flow and sparkle density without batch bursts.'),
            graph, output, controls, assets, compute=True, schema_version=7)


def dunhuang_ribbons():
    """First-party flying-ribbon scene inspired by Dunhuang mural movement."""
    name = 'dunhuang_ribbons'
    graph, clock, bands, controls = start()
    node = graph.node
    assets = []
    parts = []
    for index, tint in enumerate(((.95, .38, .08, 1), (.98, .72, .22, 1), (.18, .62, .56, 1))):
        phase = node('scalar.expression', 900, index * 420, dict(time=clock),
                     expression=f'time * {22.5 + index * 22.5} + {index * 120}')
        path = node('path.helix', 1250, index * 420, dict(path_phase=phase), path_radius=2.2 + index * .42,
                    path_height=3.5 + index * .5, path_turns=1.15 + index * .23, path_samples=192)
        tube = node('geometry.tube', 1600, index * 420, dict(path=path), tube_radius=.045 + index * .012,
                    tube_sides=12)
        glow = node('scalar.expression', 1950, index * 420, dict(a=bands[index]),
                    expression='0.9 + a * 22.0')
        material = node('material.pbr', 2300, index * 420, dict(emission=glow), color_a=tint,
                        color_b=(1, .78, .35, 1), metallic=.55, roughness=.24)
        ribbon = node('scene.instance', 2650, index * 420, dict(geometry=tube, material=material))
        ribbon_scale = node('scalar.expression', 2650, index * 420 + 180,
                            dict(time=clock, a=bands[index]),
                            expression=f'1.0 + .018 * sin(time * {math.pi / 2:.10f} + {index}) + a * .115')
        parts.append(node('scene.transform', 3000, index * 420,
                          dict(scene=ribbon, scale=ribbon_scale), rotation_x=70,
                          rotation_z=index * 37 - 30))
    spark_onset = node('event.audio_onset', 620, 2020, threshold=.009, band_first=42, band_last=62)
    spark_envelope = node('event.envelope', 900, 2020, dict(events=spark_onset), attack=.002,
                          decay=.025, sustain=.18, duration=.04, release=.11)
    low_rate = node('scalar.expression', 620, 1700, dict(a=bands[0]),
                    expression='.30 + a * .26')
    mid_rate = node('scalar.expression', 620, 1860, dict(a=bands[1]),
                    expression='.34 + a * .35')
    high_rate = node('scalar.expression', 620, 2020, dict(a=bands[2], b=spark_envelope),
                     expression='.26 + a * .28 + b * 1.65')
    spark_emission = node('scalar.expression', 900, 1860, dict(a=low_rate, b=mid_rate, c=high_rate),
                          expression='a + b + c')
    dust_flow = node('scalar.expression', 620, 2340, dict(a=bands[1], b=bands[0], c=spark_envelope),
                     expression='.024 + a * .082 + b * .036 + c * .19')
    dust_turn = node('scalar.expression', 620, 2660, dict(time=clock, a=bands[1]),
                     expression='time * 22.5 + a * 28.0')
    # A broad, slow particle population gives the cave depth a stable current in
    # silence. It is deliberately separate from the brighter foreground sparks:
    # music changes their energy, while time owns the primary movement.
    dust = node('gpu.particles', 900, 1700,
                dict(emission=spark_emission, flow_strength=dust_flow),
                particle_capacity=24576, seed=1064, initial_fill=0, emission_rate=650, lifetime=8,
                emitter_radius=3.45, particle_speed=.026, drag=.12, flow_frequency=7.5,
                flow_evolution=.16, point_size=.0037, color_a=(.95, .24, .025, .3),
                color_b=(1, .72, .18, .72))
    dust = node('gpu.map', 1250, 1700, dict(points=dust, rotation=dust_turn))
    dust = node('gpu.render', 1600, 1700, dict(points=dust), point_blend=1, texture_precision=2)
    sparks = node('gpu.particles', 900, 2700,
                  dict(emission=spark_emission, flow_strength=dust_flow),
                  particle_capacity=8192, seed=1861, initial_fill=0, emission_rate=240, lifetime=4.5,
                  emitter_radius=1.15, particle_speed=.11, drag=.1, flow_frequency=14,
                  flow_evolution=.3, point_size=.009, color_a=(1, .46, .08, .85),
                  color_b=(1, .94, .58, 1))
    sparks = node('gpu.map', 1250, 2700, dict(points=sparks, rotation=dust_turn))
    sparks = node('gpu.render', 1600, 2700, dict(points=sparks), point_blend=1, texture_precision=2)
    # The physical layer is intentionally modest (384 bodies): it remains inside
    # the runtime physics budget while supplying visibly colliding, tumbling gold
    # leaves. Bass adds downward weight and treble raises its continuous emission rate.
    shard_onset = node('event.audio_onset', 620, 4360, threshold=.012, band_first=0, band_last=20)
    shard_envelope = node('event.envelope', 900, 4360, dict(events=shard_onset), attack=.004,
                          decay=.035, sustain=.28, duration=.06, release=.14)
    shard_emission = node('scalar.expression', 620, 3400,
                          dict(a=bands[2], b=bands[0], c=shard_envelope),
                          expression='.16 + a * 1.4 + b * .55 + c * .72')
    shard_gravity = node('scalar.expression', 620, 3720, dict(a=bands[0]),
                         expression='.18 + a * 1.45')
    shard_flow = node('scalar.expression', 620, 4040, dict(a=bands[1], b=bands[0]),
                      expression='.014 + a * .14 + b * .05')
    shards = node('point.emitter', 900, 3400,
                  dict(emission=shard_emission, flow_strength=shard_flow), particle_capacity=384,
                  seed=2026, emission_rate=46, lifetime=8, lifetime_variation=.2, emitter_shape=0,
                  center_x=.5, center_y=.08, emitter_width=.74, emitter_height=.015, direction=90,
                  spread=32, particle_speed=.19, speed_variation=.52,
                  flow_frequency=2.6, flow_evolution=.11, point_size=.012, size_variation=.68,
                  angular_speed=7, fade_in=.02, fade_out=.12, color_a=(1, .6, .12, .95),
                  color_b=(.24, .72, .55, .12))
    shards = node('point.physics2d', 1250, 3400, dict(points=shards, gravity_y=shard_gravity),
                  body_shape=1, physics_bounds=2, restitution=.72, friction=.18, density=.8)
    shards = node('point.render', 1600, 3400, dict(points=shards), point_style=1, point_blend=1)
    shards = node('texture.trail', 1950, 3400, dict(source=shards), trail_half_life=.13,
                  trail_zoom_rate=.006)
    stage = merge(graph, parts)
    light = node('scene.point_light', 6000, 0, translate_x=-2.5, translate_y=3.2, translate_z=5,
                 light_energy=38, light_range=14, color_a=(1, .46, .12, 1))
    stage = node('scene.merge', 6350, 0, dict(a=stage, b=light))
    stage = studio_environment(graph, name, stage, assets, .32)
    eye_x = node('scalar.expression', 7200, 0, dict(time=clock), expression='4.8 * sin(time * .3926990817)')
    eye_y = node('scalar.expression', 7200, 320, dict(time=clock), expression='1.8 + .7 * cos(time * .3926990817)')
    camera = node('scene.camera', 7600, 0, dict(eye_x=eye_x, eye_y=eye_y), eye_z=10.5,
                  target_y=.5, target_z=.2, field_of_view=43, near_plane=.1, far_plane=30)
    image = render(graph, stage, camera, 10)
    image = node('texture.composite', 10400, 0, dict(a=image, b=dust), composite_mode=1,
                 amount=1, texture_precision=0)
    image = node('texture.composite', 10700, 0, dict(a=image, b=sparks), composite_mode=1,
                 amount=1, texture_precision=0)
    image = node('texture.composite', 11000, 0, dict(a=image, b=shards), composite_mode=1,
                 amount=.86, texture_precision=0)
    output = finish(graph, image, controls[2], .2)
    publish(name, ('敦煌飞天', 'Dunhuang Flying Ribbons'),
            ('三层飘带沿独立螺旋路径持续飞行并随频段呼吸缩放；星尘、流光和金箔都从空场按每秒速率持续生成。低频提高少量金箔刚体的重力，中频改变流向，高频 onset 短促提高生成速率；静音仍持续流动，不使用整团 burst。',
             'Three independently moving flying ribbons continuously breathe with music bands. Dust, sparks and gold leaves start empty and spawn continuously at per-second rates. Bass weights the limited colliding gold leaves, mids redirect flow and treble onsets briefly raise rate; there are no whole-population bursts.'),
            graph, output, controls, assets, compute=True, schema_version=7)

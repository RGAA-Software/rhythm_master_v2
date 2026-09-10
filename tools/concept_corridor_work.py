"""Architectural corridor with an authored mirrored scene beneath its floor."""

import math

from concept_work_common import start, merge, finish, publish
from concept_spatial_works import render, studio_environment


def corridor():
    name = 'lumen_corridor'
    graph, clock, bands, controls = start()
    node = graph.node
    assets, parts = [], []
    travel = node('scalar.expression', 700, 0, dict(time=clock), expression='time * 4.9')
    eye_z = node('scalar.expression', 700, 300, dict(a=travel), expression='5 - a')
    target_z = node('scalar.expression', 700, 600, dict(a=travel), expression='-25 - a')
    floor_z = node('scalar.expression', 700, 900, dict(a=travel), expression='-16 - a')
    light_z = node('scalar.expression', 700, 1200, dict(a=travel), expression='-a')

    def place(kind, x, y, inputs, **properties):
        # All architectural primitives are axis-aligned cubes: reflecting their
        # center is equivalent to reflecting their geometry. Positive extents
        # preserve the current non-singular scene-transform contract.
        original = node(kind, x, y, inputs, **properties)
        reflected_inputs, reflected_properties = dict(inputs), dict(properties)
        if 'translate_y' in inputs:
            reflected_inputs['translate_y'] = node('scalar.expression', x, y + 120,
                dict(a=inputs['translate_y']), expression='-a')
        else:
            reflected_properties['translate_y'] = -properties.get('translate_y', 0)
        reflected = node(kind, x + 160, y, reflected_inputs, **reflected_properties)
        return node('scene.merge', x + 320, y, dict(a=original, b=reflected))
    cube = node('geometry.cube', 1100, 0)
    noise = node('texture.noise', 1100, 450, noise_scale=24, contrast=1.5, seed=513,
                 color_a=(.12, .14, .17, 1), color_b=(.37, .4, .46, 1))
    metal = node('material.pbr', 1450, 0, color_a=(.08, .09, .12, 1), metallic=.65, roughness=.3)
    metal = node('material.textures', 1800, 0, dict(material=metal, base_texture=noise), uv_scale_x=3, uv_scale_y=6)
    dark = node('scene.instance', 2150, 0, dict(geometry=cube, material=metal))
    energy = node('scalar.expression', 1450, 850, dict(a=bands[2]), expression='2.0 + a * 5.5')
    lamps = []
    for index, tint in enumerate(((.82, .025, .28, 1), (.62, .8, 1, 1))):
        material = node('material.pbr', 1800, 850 + index * 350, dict(emission=energy),
                        color_a=tint, color_b=tint, metallic=.3, roughness=.22)
        lamps.append(node('scene.instance', 2150, 850 + index * 350, dict(geometry=cube, material=material)))
    for index in range(14):
        row = index * 1000
        z = 1 - index * 2.8
        offset = .25 * math.sin(index * 1.9)
        scale = node('scalar.expression', 2600, row, dict(a=bands[0], time=clock),
                     expression=f'1.0 + a * .09 + sin(time * .3926990817 - {index * .5}) * .035')
        group = []
        for side in (-1, 1):
            group.append(place('scene.transform', 3000, row + (side + 1) * 150, dict(scene=dark),
                              translate_x=side * 2.6 + offset, translate_y=2.65, translate_z=z,
                              scale_x=.48, scale_y=5.3, scale_z=.62))
            group.append(place('scene.transform', 3400, row + (side + 1) * 150, dict(scene=lamps[index % 2]),
                              translate_x=side * 2.42 + offset, translate_y=2.6, translate_z=z + .325,
                              scale_x=.13, scale_y=5.2, scale_z=.04))
            # Projecting masonry establishes occlusion, scale and floor contact.
            group.append(place('scene.transform', 3800, row + (side + 1) * 150, dict(scene=dark),
                              translate_x=side * (2.05 + .18 * math.sin(index)), translate_y=.27,
                              translate_z=z + .65, scale_x=1.05, scale_y=.54, scale_z=1.55))
        height = node('scalar.expression', 2600, row + 650, dict(a=bands[1], time=clock),
                      expression=f'5.15 + a * .24 + sin(time * .3926990817 - {index * .35}) * .11')
        group.append(place('scene.transform', 3400, row + 650, dict(scene=dark, translate_y=height),
                          translate_x=offset, translate_z=z, scale_x=5.7, scale_y=.48, scale_z=.65))
        group.append(place('scene.transform', 3800, row + 650, dict(scene=lamps[index % 2], translate_y=height),
                          translate_x=offset, translate_z=z + .33, scale_x=5, scale_y=.12, scale_z=.045))
        if index % 2 == 0:
            group.append(place('scene.transform', 3800, row + 900, dict(scene=dark), translate_x=.7,
                              translate_y=4.35, translate_z=z + .8, scale_x=3.9, scale_y=.2, scale_z=.85))
        group = merge(graph, group, 4500)
        # Wrap only once a gate is eight units behind the camera. Its replacement
        # enters at the distant end; the visible near field never jumps backward.
        recycle = node('scalar.expression', 5400, row + 600, dict(a=travel),
                       expression=f'39.2 * floor(({12 + index * 2.8} - a) / 39.2)')
        parts.append(node('scene.transform', 5700, row,
                          dict(scene=group, scale_x=scale, translate_z=recycle)))
    architecture = merge(graph, parts)
    stage = architecture
    # This is a focused planar mirror construction for this authored scene.
    # It is not a generic SSR/ray-tracing claim. Opaque geometry ends at y=0;
    # the translucent floor is drawn after both halves and attenuates the image.
    floor_material = node('material.pbr', 7000, 1300, color_a=(.009, .012, .02, .64),
                          metallic=.75, roughness=.24)
    floor = node('scene.instance', 7350, 1300, dict(geometry=cube, material=floor_material))
    floor = node('scene.transform', 7700, 1300, dict(scene=floor, translate_z=floor_z),
                 translate_y=-.025, scale_x=18, scale_y=.04, scale_z=60)
    stage = node('scene.merge', 8400, 0, dict(a=stage, b=floor))
    for index, tint in enumerate(((1, .08, .32, 1), (.5, .65, 1, 1), (1, .08, .3, 1))):
        light = node('scene.point_light', 7700, 2000 + index * 350, color_a=tint,
                     translate_y=3.7, translate_z=1 - index * 8, light_energy=30, light_range=14)
        light = node('scene.transform', 8050, 2000 + index * 350, dict(scene=light, translate_z=light_z))
        stage = node('scene.merge', 8400, 2000 + index * 350, dict(a=stage, b=light))
    stage = studio_environment(graph, name, stage, assets, .12)
    eye_x = node('scalar.expression', 8100, 600, dict(time=clock), expression='-.75 + .45 * sin(time * .3926990817)')
    eye_y = node('scalar.expression', 8100, 950, dict(time=clock), expression='1.3 + .15 * cos(time * .3926990817)')
    camera = node('scene.camera', 8700, 800,
                  dict(eye_x=eye_x, eye_y=eye_y, eye_z=eye_z, target_z=target_z),
                  target_x=-4.5, target_y=1.5, field_of_view=62, near_plane=.1, far_plane=70)
    image = render(graph, stage, camera)
    # Camera travel is continuous and owns the scene. Passing particulate light
    # layers make the long corridor legible at every energy level.
    low_onset = node('event.audio_onset', 8850, 1700, threshold=.013, band_first=0, band_last=20)
    low_envelope = node('event.envelope', 9200, 1700, dict(events=low_onset), attack=.004,
                        decay=.055, sustain=.2, duration=.06, release=.18)
    mid_onset = node('event.audio_onset', 8850, 2000, threshold=.010, band_first=21, band_last=41)
    mid_envelope = node('event.envelope', 9200, 2000, dict(events=mid_onset), attack=.004,
                        decay=.045, sustain=.18, duration=.05, release=.14)
    high_onset = node('event.audio_onset', 8850, 2300, threshold=.009, band_first=42, band_last=62)
    high_envelope = node('event.envelope', 9200, 2300, dict(events=high_onset), attack=.002,
                         decay=.025, sustain=.15, duration=.035, release=.1)
    low_rate = node('scalar.expression', 9550, 1700, dict(a=bands[0], b=low_envelope),
                    expression='.20 + a * .24 + b * .5')
    mid_rate = node('scalar.expression', 9550, 1860, dict(a=bands[1], b=mid_envelope),
                    expression='.24 + a * .34 + b * .55')
    high_rate = node('scalar.expression', 9550, 2020, dict(a=bands[2], b=high_envelope),
                     expression='.18 + a * .24 + b * .85')
    spark_emission = node('scalar.expression', 9900, 1700, dict(a=low_rate, b=mid_rate, c=high_rate),
                          expression='a + b + c')
    forward_flow = node('scalar.expression', 9550, 2000, dict(a=bands[1], b=mid_envelope),
                        expression='.024 + a * .14 + b * .30')
    particle_pulse = node('scalar.expression', 9900, 1900,
                          dict(a=low_envelope, b=mid_envelope, c=high_envelope),
                          expression='1.0 + a * .16 + b * .15 + c * .72')
    flash = node('scalar.expression', 9900, 2100,
                 dict(a=low_envelope, b=mid_envelope, c=high_envelope),
                 expression='a * .10 + b * .11 + c * .28')
    drift = node('scalar.expression', 9900, 1700, dict(time=clock, a=bands[0]),
                 expression='time * -34.0 - a * 8.0')
    for index, (capacity, size) in enumerate(((16384, .0028), (4096, .009))):
        row = 2750 + index * 700
        particles = node('gpu.particles', 10250, row,
                         dict(emission=spark_emission, flow_strength=forward_flow),
                         particle_capacity=capacity, seed=606 + index * 77, initial_fill=0,
                         emission_rate=600 if index == 0 else 170, lifetime=6 if index == 0 else 2.8,
                         emitter_radius=1.4, particle_speed=.075 if index == 0 else .18, drag=.13,
                         gravity_y=.04 if index == 0 else -.03, flow_frequency=8 if index == 0 else 18,
                         flow_evolution=.18 if index == 0 else .4, point_size=size,
                         color_a=(.42, .68, 1, .3) if index == 0 else (1, .08, .34, .8),
                         color_b=(1, .08, .3, .7) if index == 0 else (.72, .86, 1, 1))
        particles = node('gpu.map', 10600, row,
                         dict(points=particles, rotation=drift, point_size_scale=particle_pulse))
        rendered = node('gpu.render', 10950, row, dict(points=particles), point_blend=1, texture_precision=2)
        image = node('texture.composite', 11300, row, dict(a=image, b=rendered), composite_mode=1,
                     amount=1, texture_precision=0)
    image = node('texture.color_adjust', 11650, 4800, dict(source=image, exposure=flash),
                 saturation=1.08, texture_precision=0)
    output = finish(graph, image, controls[2], .18)
    publish(name, ('光门空间', 'Lumen Corridor'),
            ('低机位持续向前穿行玫红与冷白光廊并缓慢横移，门段在镜头后方回收，形成无尽前进感；双层光尘从空场按每秒速率穿过镜头前景。低频增加空间能量，中频引导前进流向，高频 onset 提高闪点生成速率；静音仍前进，不加入不合场景的刚体碎片。',
             'A low camera continuously advances through recycled magenta/white architecture with lateral parallax. Dual light dust starts empty and passes through the foreground at sustained per-second rates. Bass raises spatial energy, mids steer forward flow and treble onsets raise spark rate; silence retains travel, without unsuitable rigid debris.'),
            graph, output, controls, assets, compute=True, schema_version=7)

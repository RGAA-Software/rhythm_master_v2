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
                     expression=f'1.0 + a * .025 + sin(time * .3926990817 - {index * .5}) * .025')
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
                      expression=f'5.15 + a * .12 + sin(time * .3926990817 - {index * .35}) * .08')
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
    output = finish(graph, render(graph, stage, camera), controls[2], .13)
    publish(name, ('光门空间', 'Lumen Corridor'),
            ('低机位持续向前穿行玫红与冷白光廊并缓慢横移，门段在镜头后方回收，形成无尽前进感；地面石台与倒影提供视差，静音仍前进。低频展开门翼、中频抬升横梁、高频增强灯带；倒影采用本场景镜像几何，不是通用屏幕空间反射。',
             'A continuously advancing low camera travels through recycled magenta/white architecture with lateral parallax; movement continues in silence. A mirrored authored scene produces planar floor reflections. Bass widens gates, mids raise beams, highs light strips; not generic SSR.'),
            graph, output, controls, assets)

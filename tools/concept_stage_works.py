"""Shared-mesh ceramic sculpture and geometric light corridor concept works."""

import hashlib
import math

from author_petal_model import build as build_petal
from concept_work_common import ROOT, start, asset_node, merge, capture, finish, publish


def porcelain():
    name = 'porcelain_bloom'
    graph, clock, bands, controls = start()
    node = graph.node
    data = build_petal()
    digest = hashlib.sha256(data).hexdigest()
    path = ROOT / 'content/templates' / name / 'assets/sha256' / digest[:2] / digest
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    record = dict(sha256=digest, bytes=len(data), media_type='model/gltf-binary')
    mesh = asset_node(graph, 'geometry.glb', 1000, 0, digest)
    fold = node('scalar.expression', 1000, 400, dict(a=bands[0], time=clock), expression='0.4 + a * 0.7 + sin(time * 0.35) * 0.15')
    mesh = node('geometry.morph', 1340, 0, dict(geometry=mesh, morph_weight_2=fold),
                morph_weight_1=0.7, morph_weight_3=0.6)
    sheen = node('scalar.expression', 1000, 800, dict(a=bands[2]), expression='0.02 + a * 0.28')
    cream = node('material.pbr', 1340, 500, dict(emission=sheen), color_a=(0.9, 0.86, 0.76, 1),
                 metallic=0.035, roughness=0.23, double_sided=1)
    grain = node('texture.noise', 1000, 1100, noise_scale=2, contrast=1.4, seed=821,
                 color_a=(0, 0, 0, 1), color_b=(1, 1, 1, 1))
    veins = node('texture.contours', 1340, 1300, dict(source=grain), contour_count=4, line_width=0.018,
                 color_a=(0.66, 0.58, 0.45, 1), color_b=(0.98, 0.96, 0.9, 1))
    glaze = node('texture.gradient', 1340, 1650, color_a=(0.98, 0.96, 0.9, 1),
                 color_b=(0.98, 0.96, 0.9, 1))
    surface = node('texture.composite', 1680, 1300, dict(a=glaze, b=veins))
    cream = node('material.textures', 1680, 800, dict(material=cream, base_texture=surface))
    gold = node('material.pbr', 1340, 900, color_a=(0.78, 0.48, 0.16, 1), metallic=0.82, roughness=0.2,
                double_sided=1)
    petal = node('scene.instance', 1680, 0, dict(geometry=mesh, material=cream))
    parts = []
    for layer in range(2):
        turn = node('scalar.expression', 1680, 1500 + layer * 300, dict(time=clock, a=bands[1]),
                    expression=f'time * {3 if layer == 0 else -5} + a * 22')
        ring = []
        count = 9 if layer == 0 else 7
        for index in range(count):
            ring.append(node('scene.transform', 2020 + layer * 350, 2200 + index * 300,
                             dict(scene=petal), rotation_z=index * 360 / count + layer * 15,
                             rotation_y=-24, scale=1.3 if layer == 0 else 0.76,
                             translate_z=layer * 0.55))
        ring = merge(graph, ring, 3000 + layer * 500)
        parts.append(node('scene.transform', 4000, 1000 + layer * 350, dict(scene=ring, rotation_z=turn)))
    for index in range(3):
        path_node = node('path.helix', 4500, 1200 + index * 500, path_radius=3.05 + index * 0.23,
                         path_height=0, path_turns=0.72, path_phase=index * 115, path_samples=192)
        tube = node('geometry.tube', 4840, 1200 + index * 500, dict(path=path_node), tube_radius=0.027, tube_sides=10)
        arc = node('scene.instance', 5180, 1200 + index * 500, dict(geometry=tube, material=gold))
        parts.append(node('scene.transform', 5520, 1200 + index * 500, dict(scene=arc), rotation_x=90,
                          rotation_y=index * 9, translate_z=-0.2))
    ball = node('geometry.sphere', 4500, 3000, radius=0.36, height=0.72, radial_segments=48, rings=24)
    core = node('scene.instance', 4840, 3000, dict(geometry=ball, material=gold))
    parts.append(node('scene.transform', 5180, 3000, dict(scene=core), translate_z=0.6))
    pulse = node('scalar.expression', 4500, 3500, dict(a=bands[2]), expression='0.18 + a * 0.22')
    for index in range(12):
        angle = index * math.tau / 12
        parts.append(node('scene.transform', 5520, 3600 + index * 260, dict(scene=core, scale=pulse),
                          translate_x=math.cos(angle) * 3.35, translate_y=math.sin(angle) * 3.35,
                          translate_z=0.15 * math.sin(angle * 3)))
    stage = merge(graph, parts)
    light = node('scene.directional_light', 8000, 1000, light_x=-0.5, light_y=0.8, light_z=0.6,
                 light_energy=3.5, color_a=(1, 0.9, 0.72, 1))
    stage = node('scene.merge', 8340, 0, dict(a=stage, b=light))
    stage = node('scene.shadow', 8680, 0, dict(scene=stage), shadow_extent=4.3, shadow_distance=12,
                 shadow_resolution=2, shadow_bias=0.0025, shadow_normal_bias=0.045)
    camera = node('scene.camera', 8680, 800, eye_x=1.7, eye_y=1.4, eye_z=11.4,
                  field_of_view=39, near_plane=0.1, far_plane=35)
    image = capture(graph, stage, camera, 0.95)
    output = finish(graph, image, controls[2], 0.025)
    publish(name, ('瓷金绽放', 'Porcelain Bloom'),
            ('双层瓷瓣围绕黄铜核心反向旋转，三道断开的金弧构成外部空间。低频弯折瓷瓣，中频推动层间旋转，高频点亮环绕珠点；共享花瓣模型、形变、材质、光影、三宏与四段音乐均可编辑。',
             'Two ceramic petal layers counter-rotate around a brass core, framed by three broken gold arcs. Bass folds petals, mids rotate layers and highs grow orbiting beads. Shared morph geometry, PBR lighting, three macros and four music cues are editable.'),
            graph, output, controls, [record])


def corridor():
    name = 'lumen_corridor'
    graph, clock, bands, controls = start()
    node = graph.node
    cube = node('geometry.cube', 1000, 0)
    dark = node('material.pbr', 1000, 400, color_a=(0.018, 0.022, 0.035, 1), metallic=0.55, roughness=0.3)
    rib = node('scene.instance', 1340, 0, dict(geometry=cube, material=dark))
    brightness = node('scalar.expression', 1000, 800, dict(a=bands[2]), expression='1.4 + a * 3')
    lamps = []
    for index, tint in enumerate(((0.9, 0.018, 0.3, 1), (0.45, 0.7, 1, 1))):
        material = node('material.pbr', 1340, 700 + index * 400, dict(emission=brightness),
                        color_a=tint, color_b=tint, metallic=0.2, roughness=0.3)
        lamps.append(node('scene.instance', 1680, 700 + index * 400, dict(geometry=cube, material=material)))
    parts = []
    for index in range(16):
        row = index * 950
        width = node('scalar.expression', 2100, row, dict(a=bands[0], time=clock),
                     expression=f'2.55 + a * 0.4 + sin(time * 0.65 - {index * 0.42}) * 0.12')
        negative = node('scalar.expression', 2440, row, dict(a=width), expression='-a')
        top = node('scalar.expression', 2440, row + 300, dict(a=bands[1], time=clock),
                   expression=f'2.25 + a * 0.35 + sin(time * 0.4 - {index * 0.3}) * 0.12')
        z = -index * 1.75
        for side, position in enumerate((negative, width)):
            parts.append(node('scene.transform', 2800 + side * 350, row, dict(scene=rib, translate_x=position),
                              translate_z=z, scale_x=0.35, scale_y=4.8, scale_z=0.4))
            parts.append(node('scene.transform', 3500 + side * 350, row, dict(scene=lamps[index % 2], translate_x=position),
                              translate_z=z + 0.23, scale_x=0.09, scale_y=4.5, scale_z=0.055))
        parts.append(node('scene.transform', 2800, row + 350, dict(scene=rib, translate_y=top),
                          translate_z=z, scale_x=5.65, scale_y=0.3, scale_z=0.4))
        parts.append(node('scene.transform', 3500, row + 350, dict(scene=lamps[index % 2], translate_y=top),
                          translate_z=z + 0.23, scale_x=5.2, scale_y=0.085, scale_z=0.055))
        parts.append(node('scene.transform', 3850, row + 650, dict(scene=lamps[index % 2]),
                          translate_y=-2.27, translate_z=z, scale_x=5.2, scale_y=0.025, scale_z=0.12))
    parts.append(node('scene.transform', 4500, 0, dict(scene=rib), translate_y=-2.4, translate_z=-12,
                      scale_x=12, scale_y=0.2, scale_z=44))
    for index, tint in enumerate(((1, 0.08, 0.3, 1), (0.15, 0.45, 1, 1), (1, 0.2, 0.4, 1))):
        parts.append(node('scene.point_light', 4500, 700 + index * 400, color_a=tint,
                          light_energy=55, light_range=12, translate_y=1, translate_z=-index * 8))
    stage = merge(graph, parts)
    sway = node('scalar.expression', 8000, 800, dict(time=clock, a=bands[1]), expression='0.7 + sin(time * 0.16) * 0.25 + a * 0.25')
    # Camera position ports are intentionally static in the current contract.
    stage = node('scene.transform', 8340, 0, dict(scene=stage, translate_x=sway), rotation_z=3)
    camera = node('scene.camera', 8680, 800, eye_y=0.05, eye_z=5.5, target_x=0.2, target_z=-12,
                  field_of_view=58, near_plane=0.1, far_plane=50)
    image = capture(graph, stage, camera, 0.22)
    output = finish(graph, image, controls[2], 0.2)
    publish(name, ('光门空间', 'Lumen Corridor'),
            ('错落的玫红与冷白光门沿纵深传播，暗色肋架形成遮挡与节奏。低频展开两翼，中频抬升门楣并移动构图，高频增强灯带。可编辑实例、灯光、三宏与四段配乐；地面采用实时材质，不宣称镜面反射。',
             'Magenta and cool-white portals recede through dark structural ribs. Bass opens the sides, mids lift lintels and shift framing, highs brighten light strips. Editable instances, lighting, three macros and four music cues; material floor without planar mirror claims.'),
            graph, output, controls)

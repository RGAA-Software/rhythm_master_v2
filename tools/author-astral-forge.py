"""Author an editable material-and-particle stage for the P7.3 advanced batch."""

import math

from concept_work_common import finish, merge, publish, start


def create_astral_forge():
    name = "astral_forge"
    graph, clock, bands, controls = start()
    node = graph.node

    low, mid, high = bands
    core_energy = node("scalar.expression", 720, 0, dict(a=low, b=high),
                       expression="1.1 + a * 5.8 + b * 2.6")
    gold = node("material.pbr", 1060, 0, dict(emission=core_energy),
                color_a=(.85, .34, .04, 1), color_b=(1, .83, .33, 1), metallic=.91, roughness=.13)
    blue_energy = node("scalar.expression", 720, 300, dict(a=mid, b=high),
                       expression=".45 + a * 2.8 + b * 3.5")
    azure = node("material.pbr", 1060, 300, dict(emission=blue_energy),
                 color_a=(.015, .2, .52, 1), color_b=(.14, .82, 1, 1), metallic=.72, roughness=.2)
    dark = node("material.pbr", 1060, 600, color_a=(.012, .018, .035, 1),
                metallic=.82, roughness=.3)
    torus = node("geometry.torus", 1400, 0, radius=1.18, tube_ratio=.055,
                 radial_segments=96, tube_segments=20)
    sphere = node("geometry.sphere", 1400, 300, radius=.065, radial_segments=32, rings=16)
    cube = node("geometry.cube", 1400, 600)
    gold_ring = node("scene.instance", 1740, 0, dict(geometry=torus, material=gold))
    blue_ring = node("scene.instance", 1740, 240, dict(geometry=torus, material=azure))
    orbit_body = node("scene.instance", 1740, 480, dict(geometry=sphere, material=azure))
    obelisk = node("scene.instance", 1740, 720, dict(geometry=cube, material=dark))
    pieces = []
    for index, speed in enumerate((18, -29, 43, -57)):
        turn = node("scalar.expression", 2080, index * 220, dict(time=clock, a=mid),
                    expression=f"time * {speed} + a * 19")
        pieces.append(node("scene.transform", 2420, index * 220,
                           dict(scene=gold_ring if index % 2 == 0 else blue_ring, rotation_z=turn),
                           rotation_x=62 + index * 17, rotation_y=index * 23,
                           scale=1 - index * .11))
    for index in range(14):
        angle = index * math.tau / 14
        orbit = node("scalar.expression", 2080, 1100 + index * 140, dict(time=clock, a=low),
                     expression=f"time * {20 + index % 4 * 8} + a * 17 + {angle * 57.2958}")
        x = node("scalar.expression", 2420, 1100 + index * 140, dict(a=orbit),
                 expression=f"{2.0 + (index % 3) * .34} * cos(a * .0174532925)")
        z = node("scalar.expression", 2760, 1100 + index * 140, dict(a=orbit),
                 expression=f"{2.0 + (index % 3) * .34} * sin(a * .0174532925)")
        y = node("scalar.expression", 3100, 1100 + index * 140, dict(time=clock, a=high),
                 expression=f".25 * sin(time * .7853981634 + {index}) + a * .18")
        pieces.append(node("scene.transform", 3440, 1100 + index * 140,
                           dict(scene=orbit_body, translate_x=x, translate_y=y, translate_z=z),
                           scale=.20 + (index % 3) * .03))
    for index in range(12):
        angle = index * math.tau / 12
        swell = node("scalar.expression", 2080, 3400 + index * 150, dict(time=clock, a=low),
                     expression=f"1.0 + .06 * sin(time * .3926990817 + {index}) + a * .13")
        pieces.append(node("scene.transform", 2420, 3400 + index * 150,
                           dict(scene=obelisk, scale_y=swell),
                           translate_x=math.cos(angle) * 4.2, translate_z=math.sin(angle) * 4.2,
                           translate_y=.7, scale_x=.18, scale_z=.18))
    stage = merge(graph, pieces, 3900)
    for index, tint in enumerate(((1, .42, .08, 1), (.08, .65, 1, 1))):
        energy = node("scalar.expression", 7000, index * 240, dict(a=low if index == 0 else mid),
                      expression="1.0 + a * 3.2")
        light = node("scene.directional_light", 7340, index * 240, dict(light_energy=energy),
                     light_x=-.45 + index * .9, light_y=.75, light_z=.6, color_a=tint)
        stage = node("scene.merge", 7680, index * 240, dict(a=stage, b=light))
    eye_x = node("scalar.expression", 8020, 0, dict(time=clock), expression="2.35 * sin(time * .3926990817)")
    eye_y = node("scalar.expression", 8020, 260, dict(time=clock), expression="1.25 + .42 * cos(time * .3926990817)")
    eye_z = node("scalar.expression", 8020, 520, dict(time=clock), expression="5.6 + .28 * sin(time * .3926990817)")
    camera = node("scene.camera", 8360, 0, dict(eye_x=eye_x, eye_y=eye_y, eye_z=eye_z),
                  target_y=.15, field_of_view=46, near_plane=.1, far_plane=30)
    sculpture = node("scene.render", 8700, 0, dict(scene=stage, camera=camera), scene_antialiasing=1)
    sculpture_halo = node("texture.blur", 8700, 300, dict(source=sculpture), blur_radius=7.5)
    background = node("texture.gradient", 8700, 600, color_a=(.001, .004, .012, 1),
                      color_b=(.018, .01, .045, 1))
    image = node("texture.composite", 9040, 0, dict(a=background, b=sculpture_halo), composite_mode=1)
    image = node("texture.composite", 9040, 300, dict(a=image, b=sculpture), composite_mode=1)

    emission = node("scalar.expression", 9040, 700, dict(a=high, b=low),
                    expression=".16 + a * 1.45 + b * .38")
    flow = node("scalar.expression", 9040, 960, dict(a=mid), expression=".025 + a * .22")
    particle_scale = node("scalar.expression", 9040, 1220, dict(a=low, b=high),
                          expression=".62 + a * .18 + b * .24")
    particle_turn = node("scalar.expression", 9040, 1480, dict(time=clock, a=mid),
                         expression="time * -31 + a * 14")
    for index, (capacity, rate, size, tint) in enumerate(((24576, 720, .0017, (.08, .66, 1, .28)),
                                                            (4096, 150, .0038, (1, .48, .08, .58)))):
        particles = node("gpu.particles", 9380, 700 + index * 580,
                         dict(emission=emission, flow_strength=flow), particle_capacity=capacity,
                         seed=3129 + index * 71, initial_fill=0, emission_rate=rate, lifetime=5.8 - index * 2.3,
                         emitter_radius=.76, particle_speed=.032 + index * .075, drag=.14,
                         flow_frequency=8 + index * 10, flow_evolution=.2 + index * .17, point_size=size,
                         color_a=tint, color_b=(1, .83, .28, .94))
        particles = node("gpu.map", 9720, 700 + index * 580,
                         dict(points=particles, rotation=particle_turn, point_size_scale=particle_scale))
        sparks = node("gpu.render", 10060, 700 + index * 580, dict(points=particles), point_blend=1)
        halo = node("texture.blur", 10400, 700 + index * 580, dict(source=sparks), blur_radius=1.5 + index * .8)
        image = node("texture.composite", 10740, 700 + index * 580, dict(a=image, b=halo), composite_mode=1)
        image = node("texture.composite", 11080, 700 + index * 580, dict(a=image, b=sparks), composite_mode=1)
    flash = node("scalar.expression", 11420, 2000, dict(a=low, b=high), expression="a * .16 + b * .18")
    image = node("texture.color_adjust", 11760, 0, dict(source=image, exposure=flash), saturation=1.08)
    output = finish(graph, image, controls[2], .18)
    publish(name, ("星铸圣坛", "Astral Forge"),
            ("鎏金环体、蓝色轨道与十二座黑曜石柱组成持续转动的材质舞台；相机环绕，双层 GPU 粒子从空场以每秒速率生成。低频驱动主体与柱体呼吸，中频控制轨道和流场，高频强化火花与光照。",
             "Gold rings, blue orbitals and twelve obsidian pillars form a continuously rotating material stage. An orbiting camera and two sustained-rate GPU particle layers remain active in silence; bass shapes the body, mids steer orbit/flow and treble sharpens sparks and light."),
            graph, output, controls, compute=True, schema_version=7)


if __name__ == "__main__":
    create_astral_forge()

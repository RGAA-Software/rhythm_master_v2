"""Author a fine-particle, continuously traveling glazed gate for the P7.3 batch."""

import math

from concept_work_common import finish, merge, publish, start


def create_glazed_celestial_gate():
    name = "glazed_celestial_gate"
    # 32-second cycle: every angular speed below is a multiple of 11.25 deg/s
    # (360/32) and every periodic phase divides 32, so all trajectories close.
    graph, clock, bands, controls = start(32)
    node = graph.node
    low, mid, high = bands

    # Treble transient path: the smoothed high band stays too small on real
    # music to make the treble control visible, so hat onsets drive an
    # envelope gated directly by the treble-detail control.
    high_onset = node("event.audio_onset", 700, 900, threshold=.009,
                      band_first=42, band_last=62)
    # Long release keeps treble detail alive between sparse onsets instead of
    # collapsing to zero a beat after each hat.
    high_envelope = node("event.envelope", 700, 1150, dict(events=high_onset),
                         attack=.002, decay=.05, sustain=.4, duration=.05, release=2.0)
    high_detail = node("scalar.expression", 700, 1450,
                       dict(a=high_envelope, b=controls[5]), expression="a * b")

    jade_energy = node("scalar.expression", 700, 0, dict(a=mid, b=high),
                       expression=".38 + a * 3.6 + b * 80")
    gold_energy = node("scalar.expression", 700, 280, dict(a=low, b=high, c=high_detail),
                       expression=".65 + a * 4.7 + b * 120 + c * 5")
    jade = node("material.pbr", 1040, 0, dict(emission=jade_energy),
                color_a=(.01, .16, .20, 1), color_b=(.08, .88, .75, 1),
                metallic=.72, roughness=.17)
    gold = node("material.pbr", 1040, 300, dict(emission=gold_energy),
                color_a=(.46, .12, .015, 1), color_b=(1, .72, .18, 1),
                metallic=.92, roughness=.12)
    ink = node("material.pbr", 1040, 600, color_a=(.006, .009, .016, 1),
               metallic=.7, roughness=.28)
    ring_mesh = node("geometry.torus", 1380, 0, radius=1.0, tube_ratio=.042,
                     radial_segments=96, tube_segments=18)
    orb_mesh = node("geometry.sphere", 1380, 300, radius=.075, radial_segments=28, rings=14)
    pillar_mesh = node("geometry.cube", 1380, 600)
    jade_ring = node("scene.instance", 1720, 0, dict(geometry=ring_mesh, material=jade))
    gold_ring = node("scene.instance", 1720, 230, dict(geometry=ring_mesh, material=gold))
    lantern = node("scene.instance", 1720, 460, dict(geometry=orb_mesh, material=gold))
    pillar = node("scene.instance", 1720, 690, dict(geometry=pillar_mesh, material=ink))

    pieces = []
    for index, (size, speed, tilt) in enumerate(((1.0, 11.25, 61), (.78, -22.5, 76),
                                                   (.57, 22.5, 38), (1.29, -11.25, 88),
                                                   (.39, 33.75, 17), (.94, -11.25, 49))):
        turn = node("scalar.expression", 2060, index * 200, dict(time=clock, a=mid),
                    expression=f"time * {speed} + a * {7 + index}")
        source = jade_ring if index % 2 == 0 else gold_ring
        pieces.append(node("scene.transform", 2400, index * 200,
                           dict(scene=source, rotation_z=turn), rotation_x=tilt,
                           rotation_y=index * 29, scale=size))
    for index in range(20):
        angle = index * math.tau / 20
        orbit = node("scalar.expression", 2060, 1400 + index * 135, dict(time=clock, a=high),
                     expression=f"time * {11.25 * (1 + index % 3)} + a * 12 + {angle * 57.2958}")
        x = node("scalar.expression", 2400, 1400 + index * 135, dict(a=orbit),
                 expression=f"{1.62 + (index % 4) * .18} * cos(a * .0174532925)")
        z = node("scalar.expression", 2740, 1400 + index * 135, dict(a=orbit),
                 expression=f"{1.62 + (index % 4) * .18} * sin(a * .0174532925)")
        y = node("scalar.expression", 3080, 1400 + index * 135, dict(time=clock, a=low),
                 expression=f".16 * sin(time * .7853981634 + {index}) + a * .12")
        pieces.append(node("scene.transform", 3420, 1400 + index * 135,
                           dict(scene=lantern, translate_x=x, translate_y=y, translate_z=z),
                           scale=.55 + (index % 3) * .08))
    for index in range(16):
        angle = index * math.tau / 16
        sway = node("scalar.expression", 2060, 4200 + index * 128, dict(time=clock, a=low),
                    expression=f".84 + .06 * sin(time * .3926990817 + {index}) + a * .14")
        pieces.append(node("scene.transform", 2400, 4200 + index * 128,
                           dict(scene=pillar, scale_y=sway),
                           translate_x=math.cos(angle) * 3.35, translate_z=math.sin(angle) * 3.35,
                           translate_y=.55, scale_x=.11, scale_z=.11))
    stage = merge(graph, pieces, 3900)
    for index, tint in enumerate(((.05, .9, .78, 1), (1, .54, .12, 1), (.12, .32, 1, 1))):
        energy = node("scalar.expression", 6920, index * 220,
                      dict(a=(low, mid, high)[index]),
                      expression=".5 + a * " + ("3.4", "3.4", "20")[index])
        light = node("scene.directional_light", 7260, index * 220, dict(light_energy=energy),
                     light_x=-.65 + index * .62, light_y=.72, light_z=.5, color_a=tint)
        stage = node("scene.merge", 7600, index * 220, dict(a=stage, b=light))

    eye_x = node("scalar.expression", 7940, 0, dict(time=clock),
                 expression="1.92 * sin(time * .19634954085)")
    eye_y = node("scalar.expression", 7940, 240, dict(time=clock),
                 expression=".9 + .34 * cos(time * .19634954085)")
    eye_z = node("scalar.expression", 7940, 480, dict(time=clock),
                 expression="5.15 + .24 * sin(time * .19634954085)")
    camera = node("scene.camera", 8280, 0, dict(eye_x=eye_x, eye_y=eye_y, eye_z=eye_z),
                  target_y=.05, field_of_view=43, near_plane=.1, far_plane=26)
    sculpture = node("scene.render", 8620, 0, dict(scene=stage, camera=camera), scene_antialiasing=1)
    sculpture_halo = node("texture.blur", 8620, 310, dict(source=sculpture), blur_radius=6.5)
    background = node("texture.gradient", 8620, 620, color_a=(.001, .006, .013, 1),
                      color_b=(.024, .006, .036, 1))
    image = node("texture.composite", 8960, 0, dict(a=background, b=sculpture_halo), composite_mode=1)
    image = node("texture.composite", 8960, 290, dict(a=image, b=sculpture), composite_mode=1)

    emission = node("scalar.expression", 8960, 650, dict(a=high, b=low, c=high_detail),
                    expression=".15 + a * 8 + b * .32 + c * 3")
    flow = node("scalar.expression", 8960, 910, dict(a=mid), expression=".03 + a * .18")
    scale = node("scalar.expression", 8960, 1170, dict(a=low, b=high),
                 expression=".60 + a * .16 + b * .8")
    turn = node("scalar.expression", 8960, 1430, dict(time=clock, a=mid),
                expression="time * 22.5 + a * 9")
    for index, (capacity, rate, size, tint) in enumerate(((16384, 610, .0015, (.04, .74, .62, .28)),
                                                            (4096, 120, .0032, (1, .54, .12, .54)))):
        particles = node("gpu.particles", 9300, 650 + index * 580,
                         dict(emission=emission, flow_strength=flow), particle_capacity=capacity,
                         seed=5107 + index * 109, initial_fill=0, emission_rate=rate,
                         lifetime=5.7 - index * 2.2, emitter_radius=.82,
                         particle_speed=.026 + index * .066, drag=.16,
                         flow_frequency=10 + index * 9, flow_evolution=.19 + index * .16,
                         point_size=size, color_a=tint, color_b=(1, .78, .28, .82))
        particles = node("gpu.map", 9640, 650 + index * 580,
                         dict(points=particles, rotation=turn, point_size_scale=scale))
        sparks = node("gpu.render", 9980, 650 + index * 580, dict(points=particles),
                      point_blend=1, point_glow_radius=1.35 + index * .35)
        image = node("texture.composite", 10320, 650 + index * 580, dict(a=image, b=sparks), composite_mode=1)
    exposure = node("scalar.expression", 11000, 1900, dict(a=low, b=high),
                    expression="a * .12 + b * .5")
    image = node("texture.color_adjust", 11340, 0, dict(source=image, exposure=exposure), saturation=1.1)
    # Full-screen warm shimmer carries the treble response even where the
    # smoothed high band is small; the composite amount port scales linearly.
    flash_amount = node("scalar.expression", 11340, 300, dict(a=high, b=high_detail),
                        expression="a * 6 + b * 3")
    flash = node("texture.gradient", 11340, 560, color_a=(.015, .008, .002, 1),
                 color_b=(.08, .05, .012, 1))
    image = node("texture.composite", 11600, 0, dict(a=image, b=flash, amount=flash_amount),
                 composite_mode=1)
    output = finish(graph, image, controls[2], .17)
    publish(name, ("琉璃天门", "Glazed Celestial Gate"),
            ("青绿琉璃环门、鎏金航标与黑曜石柱组成持续环行的天门。相机缓慢绕行，细尘以稳定速率生成；低频推动门体呼吸，中频调节流场，高频点亮金色微光。",
             "Jade gates, gold beacons and obsidian pillars form a continuously orbiting celestial threshold. The camera travels steadily while fine dust emits at a sustained rate; bass breathes the gate, mids steer the flow and treble lights gold details."),
            graph, output, controls, compute=True, schema_version=7)


if __name__ == "__main__":
    create_glazed_celestial_gate()

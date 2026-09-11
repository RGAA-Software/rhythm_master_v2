"""Publish reusable continuous-motion layers extracted from the concept works.

The components deliberately retain autonomous travel in silence.  Their three
audio bands modulate separate visual qualities instead of becoming the clock.
"""

import hashlib
import json
from pathlib import Path

from concept_work_common import ROOT

import importlib.util

SPEC = importlib.util.spec_from_file_location(
    "motion_component_graph", ROOT / "tools/author-resonance-gate.py")
WRITER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(WRITER)


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=4) + "\n", encoding="utf-8")


def controls(graph):
    node = graph.node
    pace = node("scalar.constant", 0, 0, value=1.0)
    response = node("scalar.constant", 0, 260, value=1.0)
    phase = node("time.phase", 340, 0, dict(speed=pace), duration=1000000)
    low = node("audio.band", 0, 520, audio_band=10)
    mid = node("audio.band", 0, 780, audio_band=31)
    high = node("audio.band", 0, 1040, audio_band=52)
    low = node("scalar.expression", 340, 520, dict(a=low, b=response), expression="a * b")
    mid = node("scalar.expression", 340, 780, dict(a=mid, b=response), expression="a * b")
    high = node("scalar.expression", 340, 1040, dict(a=high, b=response), expression="a * b")
    return phase, low, mid, high, [("pace", pace, "value", 0.1, 2.0),
                                   ("response", response, "value", 0.0, 2.0)]


def orbital_aureole(graph):
    node = graph.node
    phase, low, mid, high, public_controls = controls(graph)
    turn = node("scalar.expression", 680, 0, dict(time=phase, a=mid),
                expression="time * 26 + a * 15")
    tilt = node("scalar.expression", 680, 260, dict(time=phase, a=high),
                expression="18 * sin(time * .45) + a * 12")
    power = node("scalar.expression", 680, 520, dict(a=low, b=high),
                 expression="1.2 + a * 4.6 + b * 2.4")
    geometry = node("geometry.torus", 1020, 0, radius=1.35, tube_ratio=0.08,
                    radial_segments=96, tube_segments=24)
    material = node("material.pbr", 1020, 280, dict(emission=power),
                    color_a=(0.015, 0.32, 0.55, 1), color_b=(1, 0.38, 0.05, 1),
                    metallic=0.72, roughness=0.2)
    ring = node("scene.instance", 1360, 0, dict(geometry=geometry, material=material))
    stage = []
    for index, angle in enumerate((0, 60, 120)):
        local_turn = node("scalar.expression", 1360, 320 + index * 250, dict(a=turn),
                          expression=f"a + {angle}")
        stage.append(node("scene.transform", 1700, index * 300, dict(scene=ring, rotation_z=local_turn,
                                                                       rotation_x=tilt),
                          rotation_y=24 + index * 31, scale=1 - index * .13))
    scene = stage[0]
    for index, part in enumerate(stage[1:]):
        scene = node("scene.merge", 2040 + index * 300, 0, dict(a=scene, b=part))
    eye_x = node("scalar.expression", 2380, 0, dict(time=phase), expression="1.4 * sin(time * .31)")
    eye_y = node("scalar.expression", 2380, 260, dict(time=phase), expression=".65 * cos(time * .31)")
    camera = node("scene.camera", 2720, 0, dict(eye_x=eye_x, eye_y=eye_y), eye_z=7.8,
                  field_of_view=43, near_plane=.1, far_plane=30)
    render = node("scene.render", 3060, 0, dict(scene=scene, camera=camera), scene_antialiasing=1)
    back = node("texture.gradient", 3060, 340, color_a=(.001, .004, .012, 1),
                color_b=(.014, .028, .055, 1))
    image = node("texture.composite", 3400, 0, dict(a=back, b=render), composite_mode=1)
    public_controls += [("ring_radius", geometry, "radius", 0.4, 2.4),
                 ("metallic", material, "metallic", 0.0, 1.0),
                 ("camera_fov", camera, "field_of_view", 25.0, 75.0)]
    return image, public_controls


def kinetic_blossom(graph):
    node = graph.node
    phase, low, mid, high, public_controls = controls(graph)
    geometry = node("geometry.sphere", 680, 0, radius=.52, radial_segments=48, rings=24)
    sheen = node("scalar.expression", 680, 260, dict(a=mid, b=high), expression=".2 + a * 1.8 + b * 2.6")
    material = node("material.pbr", 1020, 0, dict(emission=sheen), color_a=(.75, .62, .36, 1),
                    color_b=(1, .94, .72, 1), metallic=.86, roughness=.18)
    petal = node("scene.instance", 1360, 0, dict(geometry=geometry, material=material))
    scene = None
    for index in range(9):
        turn = node("scalar.expression", 1360, 280 + index * 210, dict(time=phase, a=low),
                    expression=f"time * {18 if index % 2 else -14} + {index * 40}")
        opening = node("scalar.expression", 1700, 280 + index * 210, dict(a=low, b=high),
                       expression=f"1.25 + a * .32 + b * .10 + {index % 3} * .08")
        item = node("scene.transform", 2040, index * 230,
                    dict(scene=petal, rotation_z=turn, scale=opening),
                    translate_x=(index % 3 - 1) * .72, translate_y=(index // 3 - 1) * .68,
                    translate_z=(index % 2) * .32)
        scene = item if scene is None else node("scene.merge", 2380, index * 180, dict(a=scene, b=item))
    eye_x = node("scalar.expression", 2720, 0, dict(time=phase), expression="2.7 * cos(time * .22)")
    eye_y = node("scalar.expression", 2720, 260, dict(time=phase), expression="1.8 + .55 * sin(time * .22)")
    camera = node("scene.camera", 3060, 0, dict(eye_x=eye_x, eye_y=eye_y), eye_z=7.4,
                  target_z=.25, field_of_view=46, near_plane=.1, far_plane=30)
    image = node("scene.render", 3400, 0, dict(scene=scene, camera=camera), scene_antialiasing=1)
    public_controls += [("petal_radius", geometry, "radius", 0.15, 1.0),
                 ("roughness", material, "roughness", 0.05, 1.0),
                 ("camera_fov", camera, "field_of_view", 25.0, 75.0)]
    return image, public_controls


def mineral_advection(graph):
    node = graph.node
    phase, low, mid, high, public_controls = controls(graph)
    drift_x = node("scalar.expression", 680, 0, dict(time=phase), expression=".24 * sin(time * .5)")
    drift_y = node("scalar.expression", 680, 260, dict(time=phase), expression=".22 * cos(time * .5)")
    broad = node("texture.noise", 1020, 0, dict(offset_x=drift_x, offset_y=drift_y), noise_scale=2.4,
                 contrast=1.55, seed=129, color_a=(.002, .01, .03, 1), color_b=(.35, .015, .01, 1))
    grain = node("texture.noise", 1020, 340, dict(offset_x=drift_y, offset_y=drift_x), noise_scale=18,
                 contrast=1.7, seed=887, color_a=(0, 0, 0, 1), color_b=(1, .48, .08, 1))
    strata = node("texture.contours", 1360, 0, dict(source=broad, phase=phase), contour_count=22,
                  line_width=.055, color_a=(.82, .34, .06, .9), color_b=(.005, .025, .065, 1))
    swirl = node("scalar.expression", 1360, 340, dict(a=mid, b=high), expression=".015 + a * .09 + b * .06")
    warped = node("texture.displace", 1700, 0, dict(source=strata, displace_map=grain, displace_strength=swirl),
                  rotation=32, sample_radius=10)
    glow = node("scalar.expression", 1700, 340, dict(a=low, b=high), expression=".08 + a * .32 + b * .26")
    image = node("texture.color_adjust", 2040, 0, dict(source=warped, exposure=glow), saturation=1.12)
    public_controls += [("strata", strata, "contour_count", 4.0, 48.0),
                 ("grain_scale", grain, "noise_scale", .25, 32.0),
                 ("displacement", warped, "sample_radius", 1.0, 24.0)]
    return image, public_controls


def endless_passage(graph):
    node = graph.node
    phase, low, mid, high, public_controls = controls(graph)
    travel = node("scalar.expression", 680, 0, dict(time=phase), expression="time * .16")
    twist = node("scalar.expression", 680, 260, dict(a=mid, b=high), expression=".16 + a * .45 - b * .32")
    source = node("texture.gradient", 1020, 340, color_a=(.005, .008, .024, 1),
                  color_b=(.13, .015, .09, 1))
    tunnel = node("texture.mapping", 1020, 0, dict(source=source, travel=travel, twist=twist),
                  mapping_mode=1, radial_power=-.72, scale=1.1)
    bands = node("texture.contours", 1360, 0, dict(source=tunnel, phase=travel), contour_count=18,
                 line_width=.045, color_a=(.2, .72, 1, .9), color_b=(1, .035, .28, .55))
    pulse = node("scalar.expression", 1360, 340, dict(a=low, b=high), expression=".3 + a * .55 + b * .38")
    spectrum = node("texture.spectrum", 1700, 340, spectrum_layout=1, spectrum_radius=.24,
                    bar_count=96, spectrum_gain=1.7, bar_gap=.6,
                    color_a=(.4, .82, 1, .8), color_b=(1, .08, .34, .85))
    spectrum = node("texture.affine", 2040, 340, dict(source=spectrum, scale=pulse, rotation=travel), opacity=.85)
    image = node("texture.composite", 2380, 0, dict(a=bands, b=spectrum), composite_mode=1)
    public_controls += [("tunnel_depth", tunnel, "radial_power", -1.5, -.1),
                 ("gate_count", bands, "contour_count", 4.0, 48.0),
                 ("spectrum_opacity", spectrum, "opacity", 0.0, 1.0)]
    return image, public_controls


def ribbon_constellation(graph):
    node = graph.node
    phase, low, mid, high, public_controls = controls(graph)
    glow = node("scalar.expression", 680, 0, dict(a=low, b=high), expression=".7 + a * 2.2 + b * 3.0")
    material = node("material.pbr", 1020, 0, dict(emission=glow), color_a=(.08, .58, .52, 1),
                    color_b=(1, .48, .08, 1), metallic=.64, roughness=.28)
    scene = None
    for index in range(3):
        ribbon_phase = node("scalar.expression", 1020, 300 + index * 300, dict(time=phase, a=mid),
                            expression=f"time * {22 + index * 15} + a * 11 + {index * 120}")
        path = node("path.helix", 1360, index * 300, dict(path_phase=ribbon_phase),
                    path_radius=1.35 + index * .4, path_height=2.8 + index * .55,
                    path_turns=1.1 + index * .24, path_samples=160)
        tube = node("geometry.tube", 1700, index * 300, dict(path=path), tube_radius=.045 + index * .018,
                    tube_sides=12)
        item = node("scene.instance", 2040, index * 300, dict(geometry=tube, material=material))
        scene = item if scene is None else node("scene.merge", 2380, index * 300, dict(a=scene, b=item))
    eye_x = node("scalar.expression", 2720, 0, dict(time=phase), expression="4.3 * sin(time * .24)")
    eye_y = node("scalar.expression", 2720, 260, dict(time=phase), expression="1.5 + .6 * cos(time * .24)")
    camera = node("scene.camera", 3060, 0, dict(eye_x=eye_x, eye_y=eye_y), eye_z=8.8,
                  field_of_view=45, near_plane=.1, far_plane=30)
    image = node("scene.render", 3400, 0, dict(scene=scene, camera=camera), scene_antialiasing=1)
    public_controls += [("metallic", material, "metallic", 0.0, 1.0),
                 ("roughness", material, "roughness", 0.05, 1.0),
                 ("camera_fov", camera, "field_of_view", 25.0, 75.0)]
    return image, public_controls


def golden_dust_current(graph):
    node = graph.node
    phase, low, mid, high, public_controls = controls(graph)
    emission = node("scalar.expression", 680, 0, dict(a=high, b=low),
                    expression=".18 + a * 1.7 + b * .45")
    flow = node("scalar.expression", 680, 260, dict(a=mid), expression=".04 + a * .32")
    pulse = node("scalar.expression", 680, 520, dict(a=low, b=high), expression=".8 + a * .7 + b * .9")
    turn = node("scalar.expression", 680, 780, dict(time=phase, a=mid), expression="time * 34 + a * 12")
    emitter = node("gpu.particles", 1020, 0, dict(emission=emission, flow_strength=flow),
                     particle_capacity=24576, seed=2309, initial_fill=0, emission_rate=460,
                     lifetime=5.2, emitter_radius=.72, particle_speed=.035, drag=.12,
                     flow_frequency=9, flow_evolution=.22, point_size=.0036,
                     color_a=(.98, .42, .05, .18), color_b=(1, .86, .36, .92))
    particles = node("gpu.map", 1360, 0, dict(points=emitter, rotation=turn, point_size_scale=pulse))
    dust = node("gpu.render", 1700, 0, dict(points=particles), point_blend=1)
    near = node("texture.blur", 2040, 0, dict(source=dust), blur_radius=2.4)
    wide = node("texture.blur", 2040, 300, dict(source=dust), blur_radius=13)
    glow = node("texture.composite", 2380, 0, dict(a=near, b=wide), composite_mode=1, amount=.34)
    image = node("texture.composite", 2720, 0, dict(a=dust, b=glow), composite_mode=1)
    public_controls += [("emission_rate", emitter, "emission_rate", 10.0, 4000.0),
                        ("flow_frequency", emitter, "flow_frequency", 0.0, 40.0),
                        ("glow", glow, "amount", 0.0, 1.0)]
    return image, public_controls


def spectral_plinth(graph):
    node = graph.node
    phase, low, mid, high, public_controls = controls(graph)
    points = node("point.grid", 680, 0, columns=30, rows=20, grid_width=4, grid_height=3,
                  point_size=.012)
    geometry = node("geometry.cube", 680, 300)
    energy = node("scalar.expression", 1020, 0, dict(a=low, b=mid), expression=".45 + a * 2.4 + b * 1.5")
    material = node("material.pbr", 1020, 300, dict(emission=energy), color_a=(.02, .22, .52, 1),
                    color_b=(.2, .92, 1, 1), metallic=.72, roughness=.22)
    height = node("scalar.expression", 1360, 0, dict(a=low, b=mid, c=high),
                  expression=".35 + a * 2.2 + b * 1.1 + c * .45")
    scene = node("scene.point_instances", 1700, 0, dict(geometry=geometry, points=points,
                                                           material=material, height=height),
                 instance_limit=1024, instance_span=8, scale=.22, audio_gain=10)
    turn = node("scalar.expression", 1700, 300, dict(time=phase), expression="time * 9")
    scene = node("scene.transform", 2040, 0, dict(scene=scene, rotation_z=turn), rotation_x=62)
    camera = node("scene.camera", 2380, 0, eye_y=5.8, eye_z=8.8, target_y=.2,
                  field_of_view=48, near_plane=.1, far_plane=30)
    image = node("scene.render", 2720, 0, dict(scene=scene, camera=camera), scene_antialiasing=1)
    public_controls += [("columns", points, "columns", 4.0, 32.0),
                        ("rows", points, "rows", 4.0, 32.0),
                        ("metallic", material, "metallic", 0.0, 1.0)]
    return image, public_controls


def neon_archway(graph):
    node = graph.node
    phase, low, mid, high, public_controls = controls(graph)
    geometry = node("geometry.cube", 680, 0)
    power = node("scalar.expression", 680, 260, dict(a=low, b=high), expression=".55 + a * 2.2 + b * 3.5")
    material = node("material.pbr", 1020, 0, dict(emission=power), color_a=(.9, .025, .3, 1),
                    color_b=(.2, .75, 1, 1), metallic=.6, roughness=.18)
    block = node("scene.instance", 1360, 0, dict(geometry=geometry, material=material))
    scene = None
    for index in range(8):
        sway = node("scalar.expression", 1360, 280 + index * 180, dict(time=phase, a=mid),
                    expression=f"sin(time * .7 + {index}) * (.08 + a * .12)")
        item = node("scene.transform", 1700, index * 180, dict(scene=block, translate_y=sway),
                    translate_x=(-1 if index % 2 else 1) * (1.5 + (index // 2) * .46),
                    translate_z=-index * 1.25, scale_x=.12, scale_y=2.5, scale_z=.11)
        scene = item if scene is None else node("scene.merge", 2040, index * 180, dict(a=scene, b=item))
    eye_x = node("scalar.expression", 2380, 0, dict(time=phase), expression=".45 * sin(time * .28)")
    camera = node("scene.camera", 2720, 0, dict(eye_x=eye_x), eye_y=.7, eye_z=6.8,
                  target_z=-4.4, field_of_view=58, near_plane=.1, far_plane=30)
    image = node("scene.render", 3060, 0, dict(scene=scene, camera=camera), scene_antialiasing=1)
    public_controls += [("metallic", material, "metallic", 0.0, 1.0),
                        ("roughness", material, "roughness", .05, 1.0),
                        ("camera_fov", camera, "field_of_view", 25.0, 75.0)]
    return image, public_controls


def chromatic_orbit(graph):
    node = graph.node
    phase, low, mid, high, public_controls = controls(graph)
    field = node("texture.noise", 680, 0, dict(phase=phase), noise_scale=5.5, contrast=1.9, seed=817,
                 color_a=(.004, .008, .03, 1), color_b=(.3, .03, .42, 1))
    spin = node("scalar.expression", 680, 300, dict(time=phase, a=mid), expression="time * 32 + a * 28")
    scale = node("scalar.expression", 1020, 300, dict(a=low, b=high), expression="1.05 + a * .22 + b * .16")
    folded = node("texture.mapping", 1020, 0, dict(source=field, rotation=spin, scale=scale), sectors=7)
    ring = node("texture.shape", 1360, 300, shape_type=2, shape_width=.74, shape_height=.74,
                inner_ratio=.972, color_a=(.1, .8, 1, .82))
    ring = node("texture.affine", 1700, 300, dict(source=ring, rotation=spin, scale=scale))
    image = node("texture.composite", 2040, 0, dict(a=folded, b=ring), composite_mode=1, amount=.7)
    public_controls += [("noise_scale", field, "noise_scale", .25, 32.0),
                        ("sectors", folded, "sectors", 1.0, 32.0),
                        ("ring_mix", image, "amount", 0.0, 1.0)]
    return image, public_controls


def helix_beacons(graph):
    node = graph.node
    phase, low, mid, high, public_controls = controls(graph)
    power = node("scalar.expression", 680, 260, dict(a=low, b=high), expression=".7 + a * 2.8 + b * 2.2")
    material = node("material.pbr", 1020, 0, dict(emission=power), color_a=(.1, .62, .94, 1),
                    color_b=(1, .46, .08, 1), metallic=.77, roughness=.24)
    scene = None
    for index in range(5):
        local = node("scalar.expression", 1020, 300 + index * 230, dict(time=phase, a=mid),
                     expression=f"time * {18 + index * 6} + {index * 72} + a * 10")
        path = node("path.helix", 1360, index * 230, dict(path_phase=local), path_radius=.7 + index * .27,
                    path_height=2.2 + index * .22, path_turns=.55 + index * .12, path_samples=128)
        tube = node("geometry.tube", 1700, index * 230, dict(path=path), tube_radius=.018 + index * .009,
                    tube_sides=10)
        item = node("scene.instance", 2040, index * 230, dict(geometry=tube, material=material))
        scene = item if scene is None else node("scene.merge", 2380, index * 230, dict(a=scene, b=item))
    camera = node("scene.camera", 2720, 0, eye_y=.6, eye_z=7.2, field_of_view=44,
                  near_plane=.1, far_plane=30)
    image = node("scene.render", 3060, 0, dict(scene=scene, camera=camera), scene_antialiasing=1)
    public_controls += [("metallic", material, "metallic", 0.0, 1.0),
                        ("roughness", material, "roughness", .05, 1.0),
                        ("camera_fov", camera, "field_of_view", 25.0, 75.0)]
    return image, public_controls


RECIPES = [
    ("orbital_aureole", ("Orbital aureole", "环绕光冕"), "scene",
     "Continuous camera orbit and counter-rotating emissive rings extracted from Aureate Vortex.", orbital_aureole,
     {"pace": .65, "response": 1.45, "ring_radius": 1.7, "metallic": .92, "camera_fov": 38}),
    ("kinetic_blossom", ("Kinetic blossom", "动势花芯"), "scene",
     "Layered quiet rotation, camera orbit and separate bass opening from Porcelain Bloom.", kinetic_blossom,
     {"pace": 1.25, "response": 1.5, "petal_radius": .66, "roughness": .11, "camera_fov": 40}),
    ("mineral_advection", ("Mineral advection", "矿彩流移"), "compositing",
     "A continuous multi-scale pigment current extracted from Stratified Ink.", mineral_advection,
     {"pace": .7, "response": 1.6, "strata": 32, "grain_scale": 28, "displacement": 16}),
    ("endless_passage", ("Endless passage", "无尽光廊"), "compositing",
     "Continuous tunnel travel and separate frequency accents extracted from Lumen Corridor.", endless_passage,
     {"pace": 1.35, "response": 1.5, "tunnel_depth": -1.05, "gate_count": 28, "spectrum_opacity": .9}),
    ("ribbon_constellation", ("Ribbon constellation", "飞带星图"), "scene",
     "Three continuous helix ribbons and a slow orbiting camera extracted from Dunhuang Ribbons.", ribbon_constellation,
     {"pace": .8, "response": 1.4, "metallic": .82, "roughness": .15, "camera_fov": 39}),
    ("golden_dust_current", ("Golden dust current", "鎏金尘流"), "particles",
     "Continuously emitted gold dust with visible per-band energy modulation, extracted from the concept particle layers.", golden_dust_current,
     {"pace": .8, "response": 1.55, "emission_rate": 880, "flow_frequency": 16, "glow": .55}),
    ("spectral_plinth", ("Spectral plinth", "频谱台阵"), "audio",
     "A batched three-dimensional plinth that turns a spectrum into a slowly rotating stage.", spectral_plinth,
     {"pace": .75, "response": 1.4, "columns": 30, "rows": 24, "metallic": .9}),
    ("neon_archway", ("Neon archway", "霓虹门架"), "scene",
     "A deep sequence of emissive architectural gates with continuous parallax and separate band energy.", neon_archway,
     {"pace": 1.2, "response": 1.5, "metallic": .86, "roughness": .11, "camera_fov": 50}),
    ("chromatic_orbit", ("Chromatic orbit", "彩光轨道"), "compositing",
     "A continuously folding chromatic field with a music-modulated orbital frame.", chromatic_orbit,
     {"pace": .9, "response": 1.4, "noise_scale": 8, "sectors": 11, "ring_mix": .9}),
    ("helix_beacons", ("Helix beacons", "螺旋灯标"), "scene",
     "Five independent luminous helix beacons with separate bass, mid and treble response.", helix_beacons,
     {"pace": 1.15, "response": 1.45, "metallic": .9, "roughness": .12, "camera_fov": 39}),
]


def write_component(name, titles, category, description, build, variant):
    graph = WRITER.Graph()
    output, controls = build(graph)
    component = "component.official." + name
    definition = [f'components {{ type_key: "{component}" schema_version: 1 title: "{titles[0]}" output: {output}',
                  *graph.nodes, *graph.edges]
    for key, identity, property_name, minimum, maximum in controls:
        definition.append(f'parameters {{ key: "{key}" node: {identity} property: "{property_name}" '
                          f'group: "component.motion" minimum: {minimum} maximum: {maximum} }}')
    definition.append("}")
    destination = ROOT / "content/semantic" / name
    destination.mkdir(parents=True, exist_ok=True)
    root = WRITER.Graph()
    instance = root.node(component, 80, 80)
    final = root.node("output.texture", 420, 80, dict(source=instance))
    text = ["schema_version: 4", f'id: "semantic-{name}"', "canvas { width: 640 height: 360 }",
            f"output: {final}", *root.nodes, *root.edges, *definition]
    (destination / "graph.textproto").write_text("\n".join(text) + "\n", encoding="utf-8")
    write_json(destination / "editor.json", {"version": 2, "positions": root.positions,
                                              "components": [{"type": component, "positions": graph.positions}]})
    manifest = {"format": "rhythm.project", "manifest_version": 1, "kind": "template",
                "content_id": "official.semantic." + name, "content_version": "0.1.0",
                "project_id": "semantic-" + name, "graph_revision": 0,
                "title": f"{titles[1]} / {titles[0]}", "default_locale": "zh-CN",
                "titles": {"en-US": titles[0], "zh-CN": titles[1]}, "category": category,
                "maturity": "visual-review-pending", "author": "Rhythm Master",
                "license_status": "First-party extracted graph; existing attributed renderer adapters; outbound license pending",
                "compatible_players": ["windows", "android"], "external_assets": [], "semantic": True,
                "default": False, "descriptions": {"en-US": description, "zh-CN": description}}
    write_json(destination / "manifest.json", manifest)
    common = {"version": "1.0.0", "operator": component, "reset": True}
    write_json(destination / "presets.json", {"schema_version": 1, "presets": [
        {**common, "id": "official.semantic." + name + ".default",
         "titles": {"en-US": "Default", "zh-CN": "默认参数"}, "properties": {}},
        {**common, "id": "official.semantic." + name + ".variant",
         "titles": {"en-US": "Performance", "zh-CN": "演出参数"}, "properties": variant}]})
    return {"component": name, "nodes": len(graph.nodes), "source_components":
            ["aureate_vortex", "porcelain_bloom", "stratified_ink", "lumen_corridor", "dunhuang_ribbons"]}


def main():
    entries = [write_component(*recipe) for recipe in RECIPES]
    sources = ["tools/concept_spatial_works.py", "tools/concept_ink_work.py",
               "tools/concept_corridor_work.py", "tools/concept_work_common.py"]
    write_json(ROOT / "provenance/motion_components_p7_2.json", {
        "ownership": "first-party", "imported_third_party_files": [],
        "authoring_tool": "tools/author-motion-components.py",
        "authoring_tool_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "source_files": sources,
        "source_sha256": {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for path in sources},
        "reuse": "Focused graph extraction from the completed concept-work batch. Existing renderer adapters retain their provenance.",
        "entries": entries})
    print(f"Published {len(entries)} continuous-motion components")


if __name__ == "__main__":
    main()

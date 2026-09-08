"""Author an editable music sculpture exercising R3 texture materials and local lights."""
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("gate", ROOT / "tools/author-resonance-gate.py")
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)


def build_graph():
    graph = gate.Graph()
    node = graph.node
    time = node("core.time", 0, 0)
    bass = node("audio.band", 0, 260, audio_band=12)
    high = node("audio.band", 0, 520, audio_band=48)
    rms = node("audio.feature", 0, 780, audio_feature=1)
    phase = node("scalar.expression", 320, 0, dict(time=time), expression="time * 0.12")
    rotation = node("scalar.expression", 320, 260, dict(time=time), expression="time * 7")
    energy = node("scalar.map", 320, 520, dict(value=bass), input_max=0.35,
                  output_min=25, output_max=100)
    emission = node("scalar.map", 320, 780, dict(value=high), input_max=0.3,
                    output_min=0.02, output_max=0.9)
    pulse = node("scalar.map", 320, 1040, dict(value=rms), input_max=0.3,
                 output_min=0.95, output_max=1.12)
    noise = node("texture.noise", 660, 0, dict(phase=phase), noise_scale=3, contrast=2,
                 seed=741, color_a=(0.02, 0.22, 0.3, 1), color_b=(0.08, 0.8, 0.65, 1))
    veins = node("texture.contours", 1000, 0, dict(source=noise), contour_count=8,
                 line_width=0.07, color_a=(1, 0.48, 0.055, 1), color_b=(1, 0.7, 0.15, 1))
    surface = node("texture.composite", 1000, -300, dict(a=noise, b=veins))
    black = node("texture.gradient", 660, -600, color_a=(0, 0, 0, 1), color_b=(0, 0, 0, 1))
    emissive_map = node("texture.composite", 1000, -600, dict(a=black, b=veins))
    normal = node("texture.noise", 660, 300, dict(phase=phase), noise_scale=22, contrast=2,
                  seed=184, color_a=(0.3, 0.4, 0.9, 1), color_b=(0.7, 0.6, 0.9, 1))
    orm = node("texture.gradient", 660, 600, color_a=(1, 0.4, 0.4, 1), color_b=(1, 0.8, 0.85, 1))
    pbr = node("material.pbr", 1000, 600, dict(emission=emission), color_a=(1, 1, 1, 1),
               color_b=(1, 0.65, 0.2, 1), metallic=0.45, roughness=0.7)
    enamel = node("material.textures", 1340, 0,
                  dict(material=pbr, base_texture=surface, normal_texture=normal,
                       orm_texture=orm, emission_texture=emissive_map),
                  uv_scale_x=2, uv_scale_y=1, normal_scale=0.25)
    sphere = node("geometry.sphere", 1000, 1000, radius=1, height=2,
                  radial_segments=96, rings=48)
    sculpture = node("scene.instance", 1680, 0, dict(geometry=sphere, material=enamel))
    sculpture = node("scene.transform", 2020, 0, dict(scene=sculpture, rotation_y=rotation, scale=pulse),
                     rotation_x=18, rotation_z=12, scale_y=1.18)
    satellite = node("scene.instance", 1680, 340, dict(geometry=sphere, material=enamel))
    for index, (x, y, z, size) in enumerate([(-1.7, -0.75, 0.25, 0.4), (1.6, 0.75, -0.6, 0.3)]):
        body = node("scene.transform", 2020, 400 + index * 300,
                    dict(scene=satellite, rotation_y=rotation), scale=size,
                    translate_x=x, translate_y=y, translate_z=z, rotation_z=index * 45)
        sculpture = node("scene.merge", 2360 + index * 320, 0, dict(a=sculpture, b=body))
    torus = node("geometry.torus", 1680, 1080, radius=1.5, tube_ratio=0.012,
                 radial_segments=128, tube_segments=8)
    gold = node("material.pbr", 2020, 1080, dict(emission=emission),
                color_a=(0.8, 0.38, 0.05, 1), color_b=(1, 0.25, 0.03, 1),
                metallic=0.6, roughness=0.3)
    orbit = node("scene.instance", 2360, 1080, dict(geometry=torus, material=gold))
    for index in range(4):
        ring = node("scene.transform", 2700, 350 + index * 280,
                    dict(scene=orbit, rotation_y=rotation), rotation_x=30 + index * 28,
                    rotation_z=index * 43, scale=1 + index * 0.09)
        sculpture = node("scene.merge", 3020 + index * 320, 0, dict(a=sculpture, b=ring))
    floor_mesh = node("geometry.cube", 3340, 1600)
    floor_material = node("material.pbr", 3340, 1860,
                          color_a=(0.12, 0.16, 0.2, 1), metallic=0.1, roughness=0.85)
    floor = node("scene.instance", 3660, 1600, dict(geometry=floor_mesh, material=floor_material))
    floor = node("scene.transform", 3980, 1600, dict(scene=floor),
                 scale_x=6.5, scale_y=0.06, scale_z=5, translate_y=-2.2)
    sculpture = node("scene.merge", 4140, 0, dict(a=sculpture, b=floor))
    key = node("scene.point_light", 3340, 400, dict(light_energy=energy),
               translate_x=-2.5, translate_y=2, translate_z=3, light_range=12,
               color_a=(0.25, 0.8, 1, 1))
    spot = node("scene.spot_light", 3340, 700, translate_x=2.5, translate_y=3, translate_z=2,
                light_x=-0.6, light_y=-0.7, light_z=-0.6, light_energy=45,
                light_range=14, spot_angle=50, spot_decay=1, color_a=(1, 0.38, 0.08, 1))
    fill = node("scene.directional_light", 3340, 1000,
                light_x=0, light_y=0.5, light_z=-1, light_energy=0.8,
                color_a=(0.25, 0.3, 1, 1))
    for index, light in enumerate([key, spot, fill]):
        sculpture = node("scene.merge", 4300 + index * 320, 0, dict(a=sculpture, b=light))
    sculpture = node("scene.shadow", 5080, -300, dict(scene=sculpture),
                     shadow_light=2, shadow_resolution=2, shadow_near=0.1,
                     shadow_bias=0.0005, shadow_normal_bias=0.02)
    environment = node("texture.noise", 4620, -920, noise_scale=2, contrast=1.6,
                       seed=271, color_a=(0.015, 0.04, 0.12, 1), color_b=(0.9, 0.76, 0.45, 1))
    sculpture = node("scene.environment", 5260, -520,
                     dict(scene=sculpture, environment_texture=environment, environment_rotation=rotation),
                     environment_energy=0.8, environment_srgb=1)
    camera = node("scene.camera", 4900, 500, eye_x=0, eye_y=2.8, eye_z=7,
                  target_y=-0.2,
                  field_of_view=43, near_plane=0.1, far_plane=30)
    capture = node("scene.capture", 5260, 0, dict(scene=sculpture, camera=camera))
    color = node("scene.color", 5600, 0, dict(capture=capture))
    depth = node("scene.depth", 5600, 300, dict(capture=capture))
    focused = node("texture.dof", 5940, 0, dict(source=color, depth=depth),
                   focus_distance=7.4, focus_scale=40, dof_radius=5, dof_samples=24)
    back = node("texture.gradient", 5600, 800,
                color_a=(0.008, 0.014, 0.03, 1), color_b=(0.015, 0.045, 0.06, 1))
    back = node("texture.linearize", 5940, 800, dict(source=back))
    combined = node("texture.composite", 6280, 0, dict(a=back, b=focused), texture_precision=0)
    bloom = node("texture.blur", 6280, 400, dict(source=focused), blur_radius=8, texture_precision=0)
    combined = node("texture.composite", 6620, 0, dict(a=combined, b=bloom),
                    composite_mode=1, amount=0.2, texture_precision=0)
    display = node("texture.display", 6960, 0, dict(source=combined), exposure=0.2)
    final = node("output.texture", 7300, 0, dict(source=display))
    return graph, final


def main():
    graph, final = build_graph()
    destination = ROOT / "content/templates/sonic_enamel"
    destination.mkdir(parents=True, exist_ok=True)
    (destination / "graph.textproto").write_text(
        f'schema_version: 4\nid: "official-sonic-enamel"\noutput: {final}\n'
        'canvas { width: 1280 height: 720 }\n' + "\n".join(graph.nodes + graph.edges) + "\n", encoding="utf-8")
    (destination / "editor.json").write_text(json.dumps(dict(version=2, positions=graph.positions),
                                                        indent=4) + "\n", encoding="utf-8")
    manifest = json.loads((ROOT / "content/templates/spectral_foundry/manifest.json").read_text(encoding="utf-8"))
    manifest.update(content_id="official.templates.sonic_enamel", project_id="official-sonic-enamel",
                    title="音律珐琅 / Sonic Enamel", titles={"zh-CN": "音律珐琅", "en-US": "Sonic Enamel"},
                    descriptions={"zh-CN": "青金色流纹雕塑与四重金属轨道：动态图内贴图驱动颜色、法线、金属度和自发光。低频控制点光，高频点亮纹路，响度驱动呼吸；旋转环境反射、聚光投影、承影舞台、景深和浮点柔光全部可编辑。",
                                  "en-US": "Teal-and-gold enamel sculpture inside four metallic orbits. Live graph textures drive color, normals, metal and emission. Bass controls the point light, treble illuminates veins and loudness drives breathing. Edit rotating environment reflections, spot shadows, the stage, depth focus and floating bloom."})
    (destination / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=4) + "\n", encoding="utf-8")
    print(f"Sonic Enamel: {len(graph.nodes)} nodes, {len(graph.edges)} edges")


if __name__ == "__main__":
    main()

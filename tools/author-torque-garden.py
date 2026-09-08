"""Author a metallic music sculpture with cached GPU deformation and cast shadows."""
import importlib.util
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("gate", ROOT / "tools/author-resonance-gate.py")
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)


def build_graph():
    graph = gate.Graph()
    node = graph.node
    time = node("core.time", 0, 0)
    rotation = node("scalar.expression", 320, 0, dict(time=time), expression="time * 5")
    torus = node("geometry.torus", 320, 400, radius=0.9, tube_ratio=0.07,
                 radial_segments=128, tube_segments=20)
    bodies = []
    for group, color in enumerate(((0.035, 0.65, 0.75, 1), (1, 0.45, 0.075, 1), (0.7, 0.08, 0.28, 1))):
        y = 900 + group * 1300
        band = node("audio.band", 0, y, audio_band=10 + group * 19)
        twist = node("scalar.expression", 320, y, dict(time=time, a=band),
                     expression=f"60 + sin(time * 0.45 + {group * 2}) * 40 + a * 100")
        taper = node("scalar.map", 320, y + 300, dict(value=band), input_max=0.35,
                     output_min=-0.12, output_max=0.4)
        emission = node("scalar.map", 320, y + 600, dict(value=band), input_max=0.35,
                        output_min=0.03, output_max=0.55)
        geometry = node("geometry.deform", 660, y, dict(geometry=torus, deform_twist=twist), deform_axis=0)
        geometry = node("geometry.deform", 1000, y, dict(geometry=geometry, deform_taper=taper),
                        deform_axis=1, deform_twist=55)
        material = node("material.pbr", 1000, y + 360, dict(emission=emission),
                        color_a=color, color_b=color, metallic=0.75, roughness=0.24)
        instance = node("scene.instance", 1340, y, dict(geometry=geometry, material=material))
        for side in range(2):
            angle = (group * 60 + side * 180) * math.pi / 180
            body = node("scene.transform", 1680, y + side * 500, dict(scene=instance, rotation_y=rotation),
                        translate_x=math.cos(angle) * 1.25, translate_y=math.sin(angle) * 1.25,
                        translate_z=-0.2, rotation_z=group * 60 + side * 180, scale=0.8)
            bodies.append(body)
    sculpture = bodies[0]
    for index, body in enumerate(bodies[1:]):
        sculpture = node("scene.merge", 2050 + index * 320, 900, dict(a=sculpture, b=body))
    sphere = node("geometry.sphere", 2700, 400, radius=0.44, height=0.88, radial_segments=64, rings=32)
    center_material = node("material.pbr", 3000, 100, color_a=(0.1, 0.3, 0.35, 1),
                           color_b=(0.01, 0.16, 0.2, 1), metallic=0.85, roughness=0.16, emission=0.2)
    core = node("scene.instance", 3320, 400, dict(geometry=sphere, material=center_material))
    sculpture = node("scene.merge", 3680, 900, dict(a=sculpture, b=core))
    floor_geometry = node("geometry.cube", 2700, 3500)
    floor_material = node("material.pbr", 3000, 3500,
                          color_a=(0.07, 0.09, 0.13, 1), metallic=0.05, roughness=0.7)
    floor = node("scene.instance", 3320, 3500, dict(geometry=floor_geometry, material=floor_material))
    floor = node("scene.transform", 3640, 3500, dict(scene=floor),
                 translate_y=-2.4, scale_x=7, scale_y=0.06, scale_z=5)
    sculpture = node("scene.merge", 4000, 900, dict(a=sculpture, b=floor))
    spot = node("scene.spot_light", 3640, 2200, translate_x=2.5, translate_y=4, translate_z=4,
                light_x=-0.45, light_y=-0.65, light_z=-0.7, light_energy=70,
                light_range=16, spot_angle=50, color_a=(1, 0.6, 0.22, 1))
    fill = node("scene.point_light", 3640, 2600, translate_x=-3, translate_y=1.5, translate_z=3,
                light_energy=35, light_range=14, color_a=(0.15, 0.65, 1, 1))
    for index, light in enumerate((spot, fill)):
        sculpture = node("scene.merge", 4340 + index * 320, 900, dict(a=sculpture, b=light))
    sculpture = node("scene.shadow", 5000, 900, dict(scene=sculpture), shadow_light=0,
                     shadow_resolution=2, shadow_near=0.1, shadow_bias=0.0005, shadow_normal_bias=0.02)
    environment = node("texture.noise", 4340, 0, noise_scale=2.5, contrast=1.4, seed=611,
                       color_a=(0.018, 0.03, 0.12, 1), color_b=(0.8, 0.7, 0.5, 1))
    sculpture = node("scene.environment", 5340, 900, dict(scene=sculpture, environment_texture=environment),
                     environment_energy=1.1)
    camera = node("scene.camera", 5000, 1600, eye_y=2.4, eye_z=8.5, target_y=-0.15,
                  field_of_view=43, near_plane=0.1, far_plane=35)
    capture = node("scene.capture", 5680, 900, dict(scene=sculpture, camera=camera))
    color = node("scene.color", 6020, 900, dict(capture=capture))
    depth = node("scene.depth", 6020, 1200, dict(capture=capture))
    focused = node("texture.dof", 6360, 900, dict(source=color, depth=depth),
                   focus_distance=8.8, focus_scale=35, dof_radius=4, dof_samples=24)
    back = node("texture.gradient", 6020, 300, color_a=(0.007, 0.014, 0.035, 1), color_b=(0.03, 0.014, 0.02, 1))
    back = node("texture.linearize", 6360, 300, dict(source=back))
    combined = node("texture.composite", 6700, 900, dict(a=back, b=focused), texture_precision=0)
    bloom = node("texture.blur", 6700, 1200, dict(source=focused), blur_radius=9, texture_precision=0)
    combined = node("texture.composite", 7040, 900, dict(a=combined, b=bloom),
                    composite_mode=1, amount=0.23, texture_precision=0)
    display = node("texture.display", 7380, 900, dict(source=combined), exposure=0.5)
    final = node("output.texture", 7720, 900, dict(source=display))
    return graph, final


def main():
    graph, final = build_graph()
    destination = ROOT / "content/templates/torque_garden"
    destination.mkdir(parents=True, exist_ok=True)
    (destination / "graph.textproto").write_text(
        f'schema_version: 4\nid: "official-torque-garden"\noutput: {final}\n'
        'canvas { width: 1280 height: 720 }\n' + "\n".join(graph.nodes + graph.edges) + "\n", encoding="utf-8")
    (destination / "editor.json").write_text(json.dumps(dict(version=2, positions=graph.positions), indent=4) + "\n", encoding="utf-8")
    manifest = json.loads((ROOT / "content/templates/sonic_enamel/manifest.json").read_text(encoding="utf-8"))
    manifest.update(content_id="official.templates.torque_garden", project_id="official-torque-garden",
                    title="扭光庭院 / Torque Garden", titles={"zh-CN": "扭光庭院", "en-US": "Torque Garden"},
                    descriptions={"zh-CN": "六片青金与玫红金属瓣环绕反光核心，三组频段分别控制 GPU 扭转、收缩和亮度。所有瓣共享基础网格，变形同步参与聚光阴影、环境反射和法线光照；景深与浮点柔光完成舞台。",
                                  "en-US": "Six teal, gold and rose metallic petals orbit a reflective core. Three frequency bands drive GPU twist, taper and emission. Petals share one base mesh; deformation participates in spot shadows, environment reflections and normal lighting, with depth focus and floating bloom."})
    (destination / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=4) + "\n", encoding="utf-8")
    print(f"Torque Garden: {len(graph.nodes)} nodes, {len(graph.edges)} edges")


if __name__ == "__main__":
    main()

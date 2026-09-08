"""Author a music-driven three-strand sculpture using editable path geometry."""
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
    rotation = node("scalar.expression", 320, 0, dict(time=time), expression="time * 9")
    bodies = []
    for index, color in enumerate(((0.025, 0.65, 1, 1), (1, 0.22, 0.065, 1), (0.6, 0.08, 1, 1))):
        y = 400 + index * 1400
        band = node("audio.band", 0, y, audio_band=10 + index * 19)
        radius = node("scalar.map", 320, y, dict(value=band), input_max=0.4,
                      output_min=0.7, output_max=1.15)
        thickness = node("scalar.map", 320, y + 300, dict(value=band), input_max=0.35,
                         output_min=0.026, output_max=0.09)
        emission = node("scalar.map", 320, y + 600, dict(value=band), input_max=0.35,
                        output_min=0.4, output_max=3.0)
        phase = node("scalar.expression", 640, y, dict(time=time),
                     expression=f"time * 18 + {index * 120}")
        helix = node("path.helix", 980, y, dict(path_radius=radius, path_phase=phase),
                     path_samples=64, path_height=4.2, path_turns=2.4)
        smooth = node("path.resample", 1320, y, dict(path=helix), path_samples=256)
        tube = node("geometry.tube", 1660, y, dict(path=smooth, tube_radius=thickness), tube_sides=10)
        pbr = node("material.pbr", 1660, y + 400, dict(emission=emission),
                   color_a=color, color_b=color, roughness=0.3, metallic=0.55)
        body = node("scene.instance", 2000, y, dict(geometry=tube, material=pbr))
        # Thin emissive rails share the ordered centerline and preserve the layered silhouette.
        fine = node("geometry.tube", 1660, y + 850, dict(path=smooth), tube_radius=0.01, tube_sides=6)
        rail = node("scene.instance", 2000, y + 850, dict(geometry=fine, material=pbr))
        rail = node("scene.transform", 2340, y + 850, dict(scene=rail), scale_x=1.18, scale_z=1.18)
        body = node("scene.merge", 2680, y, dict(a=body, b=rail))
        bodies.append(body)
    sculpture = bodies[0]
    for index, body in enumerate(bodies[1:]):
        sculpture = node("scene.merge", 3000 + index * 320, 400, dict(a=sculpture, b=body))
    sculpture = node("scene.transform", 3660, 400, dict(scene=sculpture, rotation_y=rotation), rotation_z=-30)
    ring_mesh = node("geometry.torus", 3000, 2200, radius=1.1, tube_ratio=0.01,
                     radial_segments=96, tube_segments=6)
    ring_material = node("material.pbr", 3000, 2500, color_a=(0.1, 0.5, 0.7, 1),
                         color_b=(0.03, 0.2, 0.4, 1), metallic=0.7, roughness=0.35, emission=0.8)
    ring = node("scene.instance", 3340, 2200, dict(geometry=ring_mesh, material=ring_material))
    for index in range(5):
        hoop = node("scene.transform", 3680, 1900 + index * 280, dict(scene=ring),
                    rotation_x=90, rotation_z=-30, translate_x=(-1 + index * 0.5) * 0.5,
                    translate_y=(-1 + index * 0.5) * 0.866, scale=1.4)
        sculpture = node("scene.merge", 4020 + index * 320, 400, dict(a=sculpture, b=hoop))
    light = node("scene.point_light", 4340, 1000, translate_x=-3, translate_y=3, translate_z=4,
                 light_energy=55, light_range=15, color_a=(0.5, 0.8, 1, 1))
    sculpture = node("scene.merge", 5680, 400, dict(a=sculpture, b=light))
    env = node("texture.gradient", 4680, -400,
               color_a=(0.02, 0.08, 0.2, 1), color_b=(0.6, 0.4, 0.7, 1))
    sculpture = node("scene.environment", 6000, 400, dict(scene=sculpture, environment_texture=env),
                     environment_energy=0.7)
    camera = node("scene.camera", 6000, 800, eye_y=0.7, eye_z=8.5,
                  field_of_view=42, near_plane=0.1, far_plane=30)
    capture = node("scene.capture", 6340, 400, dict(scene=sculpture, camera=camera))
    color = node("scene.color", 6680, 400, dict(capture=capture))
    depth = node("scene.depth", 6680, 800, dict(capture=capture))
    focused = node("texture.dof", 7020, 400, dict(source=color, depth=depth),
                   focus_distance=8.5, focus_scale=30, dof_radius=4, dof_samples=20)
    background = node("texture.gradient", 6680, -400,
                      color_a=(0.005, 0.012, 0.03, 1), color_b=(0.035, 0.009, 0.055, 1))
    background = node("texture.linearize", 7020, -400, dict(source=background))
    combined = node("texture.composite", 7360, 400, dict(a=background, b=focused), texture_precision=0)
    bloom = node("texture.blur", 7360, 800, dict(source=focused), blur_radius=12, texture_precision=0)
    combined = node("texture.composite", 7700, 400, dict(a=combined, b=bloom),
                    composite_mode=1, amount=0.45, texture_precision=0)
    display = node("texture.display", 8040, 400, dict(source=combined), exposure=0.2)
    final = node("output.texture", 8380, 400, dict(source=display))
    return graph, final


def main():
    graph, final = build_graph()
    destination = ROOT / "content/templates/aurora_braid"
    destination.mkdir(parents=True, exist_ok=True)
    (destination / "graph.textproto").write_text(
        f'schema_version: 4\nid: "official-aurora-braid"\noutput: {final}\n'
        'canvas { width: 1280 height: 720 }\n' + "\n".join(graph.nodes + graph.edges) + "\n", encoding="utf-8")
    (destination / "editor.json").write_text(json.dumps(dict(version=2, positions=graph.positions),
                                                        indent=4) + "\n", encoding="utf-8")
    manifest = json.loads((ROOT / "content/templates/sonic_enamel/manifest.json").read_text(encoding="utf-8"))
    manifest.update(content_id="official.templates.aurora_braid", project_id="official-aurora-braid",
                    title="极光织带 / Aurora Braid", titles={"zh-CN": "极光织带", "en-US": "Aurora Braid"},
                    descriptions={"zh-CN": "三股交织光带穿过五重悬浮光环，低、中、高频分别改变路径半径、管径与亮度。螺旋路径、曲线重采样、管线网格、金属材质、环境反射、景深与浮点柔光均可在节点上编辑。",
                                  "en-US": "Three braided light strands pass through five floating hoops. Bass, mids and treble separately drive path radius, tube thickness and emission. Edit helix paths, curve resampling, tube meshes, metallic materials, environment reflections, depth focus and floating bloom."})
    (destination / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=4) + "\n", encoding="utf-8")
    print(f"Aurora Braid: {len(graph.nodes)} nodes, {len(graph.edges)} edges")


if __name__ == "__main__":
    main()

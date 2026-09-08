"""Author an editable, batched 4096-column audio sculpture from shared nodes."""
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
    turn = node("scalar.expression", 320, 0, dict(time=time), expression="sin(time * 0.12) * 12")
    rms = node("audio.feature", 0, 260, audio_feature=1)
    emission = node("scalar.map", 320, 260, dict(value=rms), input_max=0.3,
                    output_min=0.10, output_max=0.65)
    points = node("point.grid", 640, 0, columns=64, rows=64, grid_width=1, grid_height=1,
                  point_size=0.009)
    cube = node("geometry.cube", 640, 260)
    copper = node("material.pbr", 640, 520, dict(emission=emission),
                  color_a=(0.025, 0.20, 0.32, 1), color_b=(0.01, 0.45, 0.85, 1),
                  metallic=0.25, roughness=0.45)
    field = node("scene.point_instances", 960, 0, dict(geometry=cube, points=points, material=copper),
                 instance_limit=4096, instance_span=12, audio_gain=40, height=0.5)
    floor_mat = node("material.pbr", 640, 800, color_a=(0.025, 0.035, 0.055, 1),
                     metallic=0.35, roughness=0.7)
    floor = node("scene.instance", 960, 520, dict(geometry=cube, material=floor_mat))
    floor = node("scene.transform", 1280, 520, dict(scene=floor),
                 scale_x=12.6, scale_y=0.12, scale_z=12.6, translate_y=-0.08)
    sculpture = node("scene.merge", 1600, 0, dict(a=field, b=floor))
    torus = node("geometry.torus", 640, 1100, radius=7.5, tube_ratio=0.01,
                 radial_segments=128, tube_segments=8)
    hot = node("material.unlit", 960, 1100, color_a=(1, 0.32, 0.055, 1))
    ring = node("scene.instance", 1280, 1100, dict(geometry=torus, material=hot))
    for index in range(3):
        orbit = node("scene.transform", 1600, 850 + index * 280, dict(scene=ring),
                     rotation_x=15 + index * 35, rotation_z=index * 60,
                     translate_y=0.7, scale=1 + index * 0.06)
        sculpture = node("scene.merge", 1920 + index * 320, 0, dict(a=sculpture, b=orbit))
    sculpture = node("scene.transform", 2880, 0, dict(scene=sculpture, rotation_y=turn))
    key = node("scene.directional_light", 2560, 350, light_x=-0.5, light_y=1,
               light_z=0.8, light_energy=3, color_a=(0.55, 0.8, 1, 1))
    fill = node("scene.directional_light", 2560, 650, light_x=0.7, light_y=0.5,
                light_z=-0.6, light_energy=2, color_a=(1, 0.3, 0.1, 1))
    sculpture = node("scene.merge", 3200, 0, dict(a=sculpture, b=key))
    sculpture = node("scene.merge", 3520, 0, dict(a=sculpture, b=fill))
    camera = node("scene.camera", 3520, 350, eye_x=15, eye_y=24, eye_z=20,
                  target_y=0.7, field_of_view=43)
    scene = node("scene.render", 3840, 0, dict(scene=sculpture, camera=camera))
    back = node("texture.gradient", 3840, 350, color_a=(0.002, 0.006, 0.018, 1),
                color_b=(0.018, 0.004, 0.028, 1))
    combined = node("texture.composite", 4160, 0, dict(a=back, b=scene))
    blur = node("texture.blur", 4160, 350, dict(source=scene), blur_radius=9)
    glow = node("texture.composite", 4480, 0, dict(a=combined, b=blur),
                composite_mode=1, amount=0.4)
    final = node("output.texture", 4800, 0, dict(source=glow))
    return graph, final


def main():
    graph, final = build_graph()
    destination = ROOT / "content/templates/spectral_foundry"
    destination.mkdir(parents=True, exist_ok=True)
    header = (f'schema_version: 4\nid: "official-spectral-foundry"\noutput: {final}\n'
              'canvas { width: 1280 height: 720 }\n')
    (destination / "graph.textproto").write_text(
        header + "\n".join(graph.nodes + graph.edges) + "\n", encoding="utf-8")
    (destination / "editor.json").write_text(json.dumps(
        dict(version=2, positions=graph.positions), indent=4) + "\n", encoding="utf-8")
    manifest = json.loads((ROOT / "content/templates/harmonic_city/manifest.json").read_text(encoding="utf-8"))
    manifest.update(content_id="official.templates.spectral_foundry", project_id="official-spectral-foundry",
                    title="频谱铸场 / Spectral Foundry", titles={"zh-CN": "频谱铸场", "en-US": "Spectral Foundry"},
                    descriptions={"zh-CN": "4096 根共享网格音柱由一张点阵生成，各列直接响应真实频段，三重铜色轨道环绕。双灯光、金属材质与柔光合成；静音回落，全部节点可编辑。",
                                  "en-US": "4096 shared-mesh columns from one point grid respond to real frequency bands, surrounded by three copper orbits. Two lights, metallic shading and bloom; silence lowers the columns. Every node is editable."})
    (destination / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=4) + "\n", encoding="utf-8")
    print(f"Spectral Foundry: {len(graph.nodes)} nodes, {len(graph.edges)} edges, 4100 instances")


if __name__ == "__main__":
    main()

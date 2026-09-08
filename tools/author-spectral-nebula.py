"""Author a GPU-resident, two-population music nebula as an editable node graph."""
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
    bass = node("audio.band", 0, 250, audio_band=12)
    high = node("audio.band", 0, 500, audio_band=48)
    rms = node("audio.feature", 0, 750, audio_feature=1)
    motion = node("scalar.map", 320, 250, dict(value=bass), input_max=0.4, output_min=0.035, output_max=0.35)
    flux = node("scalar.map", 320, 500, dict(value=high), input_max=0.3, output_min=0.35, output_max=3)
    opacity = node("scalar.map", 320, 750, dict(value=rms), input_max=0.2, output_min=0.35, output_max=1.0)
    rotation = node("scalar.expression", 320, 0, dict(time=time), expression="time * 2.5")
    cloud = node("gpu.particles", 700, 0, dict(flow_strength=motion, emission=flux),
                 particle_capacity=65536, seed=181, emitter_radius=0.34,
                 emission_rate=10000, initial_fill=1, lifetime=7, particle_speed=0.005,
                 flow_frequency=17, flow_evolution=0.19, point_size=0.003,
                 color_a=(0.06, 0.5, 1, 0.4), color_b=(0.35, 0.95, 1, 0.5))
    mist = node("gpu.render", 1040, 0, dict(points=cloud, opacity=opacity))
    lace = node("texture.mapping", 1380, 0, dict(source=mist, rotation=rotation),
                sectors=6, scale=1.05)
    embers = node("gpu.particles", 700, 650, dict(flow_strength=motion, emission=flux),
                  particle_capacity=32768, seed=891, emitter_radius=0.16,
                  emission_rate=4000, initial_fill=1, lifetime=5, particle_speed=0.026,
                  flow_frequency=29, flow_evolution=0.11, point_size=0.0028,
                  color_a=(0.8, 0.13, 0.5, 0.25), color_b=(1, 0.7, 0.25, 0.6))
    sparks = node("gpu.render", 1040, 650, dict(points=embers, opacity=opacity))
    turning = node("scalar.expression", 1380, 650, dict(time=time), expression="-time * 4")
    spiral = node("texture.mapping", 1720, 650, dict(source=sparks, rotation=turning), sectors=9, scale=0.85)
    merged = node("texture.composite", 2060, 0, dict(a=lace, b=spiral), composite_mode=1)
    bloom = node("texture.blur", 2400, 300, dict(source=merged), blur_radius=9)
    aura = node("texture.composite", 2740, 0, dict(a=merged, b=bloom), composite_mode=1, amount=0.65)
    background = node("texture.gradient", 2400, 650, color_a=(0.002, 0.005, 0.018, 1), color_b=(0.025, 0.002, 0.04, 1))
    composed = node("texture.composite", 3080, 0, dict(a=background, b=aura), composite_mode=1)
    final = node("output.texture", 3420, 0, dict(source=composed))
    return graph, final

def main():
    graph, final = build_graph()
    directory = ROOT / "content/templates/spectral_nebula"
    directory.mkdir(parents=True, exist_ok=True)
    header = f'schema_version: 4\nid: "official-spectral-nebula"\noutput: {final}\ncanvas {{ width: 1280 height: 720 }}\n'
    (directory / "graph.textproto").write_text(header + "\n".join(graph.nodes + graph.edges) + "\n", encoding="utf-8")
    (directory / "editor.json").write_text(json.dumps(dict(version=2, positions=graph.positions), indent=4) + "\n", encoding="utf-8")
    manifest = json.loads((ROOT / "content/templates/spectral_foundry/manifest.json").read_text(encoding="utf-8"))
    manifest.update(content_id="official.templates.spectral_nebula", project_id="official-spectral-nebula",
        title="频谱星云 / Spectral Nebula", titles={"zh-CN": "频谱星云", "en-US": "Spectral Nebula"},
        compatible_players=["windows", "android-gles31-compute"],
        license_status="First-party composition; TiXL MIT particle adaptation and attributed texture adapters; outbound license pending",
        descriptions={"zh-CN": "98304 个 GPU 粒子构成冰蓝星尘与暖色余烬，卷曲流场、六重与九重折射、柔光共同形成星云。真实低频控制流速，高频控制发射，响度控制光芒。需要 compute 与实例化支持；跳转会按固定种子重置，全部节点可编辑。",
        "en-US": "98304 GPU particles form icy stardust and warm embers with curl flow, six/ninefold refraction and bloom. Real bass controls flow, treble controls emission and loudness controls glow. Requires compute and instancing; seeking restarts seeded state. Every node is editable."})
    (directory / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=4) + "\n", encoding="utf-8")
    print(f"Spectral Nebula: {len(graph.nodes)} nodes, {len(graph.edges)} edges, 98304 GPU particles")
if __name__ == "__main__":
    main()

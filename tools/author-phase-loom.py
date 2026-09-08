"""Author editable interference ribbons using the constrained image shader profile."""
import importlib.util
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("gate", ROOT / "tools/author-resonance-gate.py")
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)


def expression():
    # TiXL RadialGradient reference: center UV and correct X for output aspect,
    # then use radial distance. Interference/color composition is first-party.
    radius = "length((uv - 0.5) * vec2(resolution.x / resolution.y, 1.0))"
    wave = f"abs(sin({radius} * 42.0 - time * 0.8 + Sample(uv).r * 3.5 + a * 2.0 + d))"
    intensity = f"pow(max(0.0, 1.0 - {wave}), 14.0 - clamp(c, 0.0, 1.0) * 6.0)"
    palette = f"(0.5 + 0.5 * cos(vec3(0.1, 2.0, 4.1) + {radius} * 5.0 + b * 2.0 + d))"
    envelope = f"exp(-{radius} * 2.2) * smoothstep(0.025, 0.13, {radius})"
    return f"vec4({palette} * {intensity} * {envelope} * (1.5 + a * 5.0), 1.0)"


def main():
    destination = ROOT / "content/templates/phase_loom"
    destination.mkdir(parents=True, exist_ok=True)
    work = ROOT / "out/phase-loom-authoring"
    work.mkdir(parents=True, exist_ok=True)
    source = work / "ribbons.expression"
    source.write_text(expression(), encoding="utf-8")
    record = json.loads(subprocess.check_output([
        str(ROOT / "out/windows-release/src/shader_authoring/shader_author_tool.exe"),
        "C:/source/shark_dynamics_wallpaper/cmake-build-qt6/generated/bgfx_tools/bin/shaderc.exe",
        str(ROOT / "third_party/sources/bgfx/src"),
        str(ROOT / "src/rhythm_render/shaders/varying.def.sc"), str(source),
        str(destination / "assets")], encoding="utf-8"))
    graph = gate.Graph()
    node = graph.node
    time = node("core.time", 0, 0)
    phase = node("scalar.expression", 320, 0, dict(time=time), expression="time * 0.09")
    bands = [node("audio.band", 0, 400 + index * 330, audio_band=band)
             for index, band in enumerate((10, 29, 48))]
    controls = [node("scalar.map", 320, 400 + index * 330, dict(value=band),
                     input_max=0.35, output_max=1)
                for index, band in enumerate(bands)]
    cloud = node("texture.noise", 700, 0, dict(phase=phase), noise_scale=3.3,
                 contrast=1.2, seed=733, color_a=(0, 0, 0, 1), color_b=(1, 1, 1, 1))
    fields = []
    for index in range(2):
        field = node("texture.shader", 1060, index * 700,
                     dict(source=cloud, a=controls[0], b=controls[1], c=controls[2]),
                     texture_precision=2, d=index * 1.57)
        graph.nodes[-1] = graph.nodes[-1][:-1] + f'    properties {{ key: "asset" value {{ asset_sha256: "{record["sha256"]}" }} }}\n}}'
        field = node("texture.affine", 1420, index * 700, dict(source=field),
                     rotation=index * 37, scale=1 - index * 0.14, texture_precision=0)
        fields.append(field)
    combined = node("texture.composite", 1780, 350, dict(a=fields[0], b=fields[1]),
                    composite_mode=1, amount=0.45, texture_precision=0)
    ring = node("texture.spectrum", 1420, 1400, spectrum_layout=1, bar_count=64,
                spectrum_radius=0.34, spectrum_gain=2.5, bar_gap=0.5,
                color_a=(0.2, 0.8, 1, 1), color_b=(1, 0.35, 0.1, 1))
    ring = node("texture.linearize", 1780, 1400, dict(source=ring))
    combined = node("texture.composite", 2140, 350, dict(a=combined, b=ring), composite_mode=1,
                    amount=0.6, texture_precision=0)
    near = node("texture.blur", 2500, 0, dict(source=combined), blur_radius=2, texture_precision=0)
    wide = node("texture.blur", 2500, 700, dict(source=combined), blur_radius=14, texture_precision=0)
    halo = node("texture.composite", 2860, 0, dict(a=near, b=wide), composite_mode=1,
                amount=0.65, texture_precision=0)
    combined = node("texture.composite", 3220, 350, dict(a=combined, b=halo), composite_mode=1,
                    amount=0.5, texture_precision=0)
    final = node("texture.display", 3580, 350, dict(source=combined), exposure=0.3)
    final = node("output.texture", 3940, 350, dict(source=final))
    (destination / "graph.textproto").write_text(
        f'schema_version: 4\nid: "official-phase-loom"\noutput: {final}\n'
        'canvas { width: 1280 height: 720 }\n' + "\n".join(graph.nodes + graph.edges) + "\n", encoding="utf-8")
    (destination / "editor.json").write_text(json.dumps(dict(version=2, positions=graph.positions), indent=4) + "\n", encoding="utf-8")
    manifest = json.loads((ROOT / "content/templates/torque_garden/manifest.json").read_text(encoding="utf-8"))
    manifest.update(content_id="official.templates.phase_loom", project_id="official-phase-loom",
                    title="相位织光 / Phase Loom", titles={"zh-CN": "相位织光", "en-US": "Phase Loom"}, assets=[record],
                    descriptions={"zh-CN": "两层可编辑 Shader 干涉织带围绕频谱环展开，低频推动波面，中频改变光色，高频拓宽细丝。浮点近远辉光叠加后显式映射到显示；选中图像 Shader 节点可修改源码并为 Windows 与 Android 一起编译。",
                                  "en-US": "Two editable interference shaders surround a spectrum ring: bass displaces waves, mids shift color, treble widens filaments. Floating near/far bloom is display-mapped explicitly. Select an Image shader node to edit source and compile Windows and Android together."})
    (destination / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=4) + "\n", encoding="utf-8")
    print(f"Phase Loom: {len(graph.nodes)} nodes, {len(graph.edges)} edges, {record['sha256']}")


if __name__ == "__main__":
    main()

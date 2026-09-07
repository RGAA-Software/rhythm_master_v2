"""Author a four-section teaching arrangement from the existing Resonance components.

This is a functional composition example, not an additional independent quality
template. No new effect implementation or third-party source is introduced.
"""

import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "resonance_live", ROOT / "tools/author-resonance-live.py")
LIVE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(LIVE)


def main():
    field, field_output = LIVE.GATE.build_graph()
    field.nodes.pop()
    field.positions.pop()
    field.edges.pop()
    # Render the largest ring geometry before downscaling its texture. Enlarging
    # a small rendered ring by 2.6 magnifies its raster edges in the outro.
    core, core_output, core_parameters = LIVE.build_core(radius=0.47 * 2.6)
    field_type = "component.official.resonance_field"
    core_type = "component.official.resonance_core"
    graph = LIVE.GATE.Graph()
    node = graph.node
    clock = node("core.time", 0, 0)
    time = node("time.local", 320, 0, dict(time=clock), duration=16, time_mode=1)
    sections = []
    for index, (start, duration, fade_in, fade_out) in enumerate(
            ((0, 5, 1.5, 1), (3, 6, 1, 1), (8, 5, 0.35, 1), (12, 4, 1, 2))):
        sections.append(node("time.envelope", 660, index * 250, dict(time=time),
                             clip_start=start, clip_duration=duration,
                             fade_in=fade_in, fade_out=fade_out, fade_shape=1))
    intro, build, drop, outro = sections
    core_main = node("scalar.expression", 1000, 0,
                     dict(a=intro, b=build, c=drop), expression="min(1, a + 0.7*b + c)")
    core_gain = node("scalar.expression", 1340, 0,
                     dict(a=core_main, b=outro), expression="min(1, a + b)")
    field_gain = node("scalar.expression", 1000, 300,
                      dict(a=build, b=drop, c=outro), expression="0.45*a + b + 0.18*c")
    scale = node("scalar.expression", 1000, 600,
                 dict(a=build, b=drop, c=outro), expression="(2.1 - 0.8*a - 0.9*b + 0.5*c) / 2.6")
    glow_gain = node("scalar.expression", 1000, 900, dict(a=drop), expression="0.18 + 0.45*a")
    field_node = node(field_type, 1340, 350)
    core_node = node(core_type, 1340, 750)
    core_fade = node("texture.affine", 1700, 750,
                     dict(source=core_node, opacity=core_gain, scale=scale))
    background = node("texture.gradient", 1700, 0,
                      color_a=(0.001, 0.002, 0.008, 1), color_b=(0.008, 0.001, 0.012, 1))
    field_fade = node("texture.composite", 2060, 0,
                      dict(a=background, b=field_node, amount=field_gain), composite_mode=1)
    combined = node("texture.composite", 2420, 0,
                    dict(a=field_fade, b=core_fade), composite_mode=1, amount=0.9)
    blur = node("texture.blur", 2060, 750, dict(source=core_fade), blur_radius=10)
    glow = node("texture.composite", 2780, 0,
                dict(a=combined, b=blur, amount=glow_gain), composite_mode=1)
    final = node("output.texture", 3140, 0, dict(source=glow))
    definitions = [
        LIVE.component(field, field_output - 1, field_type, "共振光场 / Resonance field",
                       [("pulse", 8, "output_max", "Music", 0.9, 1.5),
                        ("exposure", field_output - 1, "exposure", "Look", -1, 2)]),
        LIVE.component(core, core_output, core_type, "星门核心 / Gate core", core_parameters),
    ]
    destination = ROOT / "content/templates/resonance_arrangement"
    destination.mkdir(parents=True, exist_ok=True)
    header = [f'schema_version: 4\nid: "official-resonance-arrangement"\noutput: {final}\n'
              'canvas { width: 1280 height: 720 }']
    (destination / "graph.textproto").write_text(
        "\n".join(header + graph.nodes + graph.edges + definitions) + "\n", encoding="utf-8")
    layout = dict(version=2, positions=graph.positions,
                  components=[dict(type=field_type, positions=field.positions),
                              dict(type=core_type, positions=core.positions)])
    (destination / "editor.json").write_text(json.dumps(layout, indent=4) + "\n", encoding="utf-8")
    manifest = dict(
        format="rhythm.project", manifest_version=1, kind="template",
        content_id="official.templates.resonance_arrangement", content_version="0.1.0",
        project_id="official-resonance-arrangement", graph_revision=0,
        title="星门编排 / Resonance Arrangement", default_locale="zh-CN",
        titles={"zh-CN": "星门编排", "en-US": "Resonance Arrangement"},
        category="audio", maturity="experimental", author="Rhythm Master",
        license_status="First-party arrangement of Resonance Live components; existing attributed effects; outbound license pending",
        compatible_players=["windows"], external_assets=[],
        descriptions={
            "zh-CN": "16 秒四段编排教学：开场星环、光场推进、频谱高潮、星环收束。四个时间段落控制透明度、缩放和辉光；组件内仍由真实音乐驱动。播放内置演示音乐，打开时间线编辑段落。局部时间每 16 秒循环，粒子历史连续。此示例复用星门演出，不计入独立品质模板。",
            "en-US": "Four-section, 16-second teaching arrangement: ring reveal, field buildup, spectrum climax and ring outro. Four envelopes control opacity, scale and glow; real music still drives the components. Play the demo track and edit sections in the timeline. Local time loops every 16 seconds while particle history continues. Reuses Resonance Live; excluded from independent quality-template counts.",
        })
    (destination / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=4) + "\n", encoding="utf-8")
    print(f"Resonance Arrangement: {len(graph.nodes)} root nodes, "
          f"{len(field.nodes) + len(core.nodes)} component nodes, four sections")


if __name__ == "__main__":
    main()

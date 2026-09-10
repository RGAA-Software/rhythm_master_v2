"""Author a matte ink coastline with editable shader pigment and native contour layers."""

import importlib.util
import json
from pathlib import Path
import subprocess

import music_work
import audio_band_groups

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('gate', ROOT / 'tools/author-resonance-gate.py')
GATE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GATE)


def shader_assets():
    # Pigment and metallic coastline share the same editable height field and
    # threshold. Outputs are straight RGBA, matching the image-shader contract.
    level = '(Sample(uv).r + (uv.y - 0.5) * 0.22 + sin(uv.x * 5.0 + time * 0.08) * 0.07)'
    threshold = '(0.51 - clamp(a, 0.0, 1.0) * 0.12)'
    pigment = f'smoothstep({threshold} - 0.08, {threshold} + 0.08, {level})'
    coast = f'(1.0 - smoothstep(0.002, 0.009 + b * 0.006, abs({level} - {threshold})))'
    expressions = {
        'pigment': f'vec4(mix(vec3(0.89, 0.86, 0.76), vec3(0.006, 0.048, 0.06), {pigment}), 1.0)',
        'coastline': f'vec4(vec3(0.74, 0.49, 0.17), {coast} * 0.8)',
    }
    directory = ROOT / 'out/ink-tide-authoring'
    directory.mkdir(parents=True, exist_ok=True)
    records = []
    for name, expression in expressions.items():
        source = directory / (name + '.expression')
        source.write_text(expression, encoding='utf-8')
        record = json.loads(subprocess.check_output([
            str(ROOT / 'out/windows-release/src/shader_authoring/shader_author_tool.exe'),
            str(ROOT / 'out/windows-release/src/windows_spike/deploy/shader_tools/shaderc.exe'),
            str(ROOT / 'third_party/sources/bgfx/src'),
            str(ROOT / 'src/rhythm_render/shaders/varying.def.sc'), str(source),
            str(ROOT / 'content/templates/ink_tide/assets')], encoding='utf-8'))
        records.append(record)
    return records


def build_graph(records):
    graph = GATE.Graph()
    node = graph.node
    response = node('control.scalar', 0, 0, value=1, control_minimum=0, control_maximum=2)
    flow = node('control.scalar', 0, 280, value=1, control_minimum=0.2, control_maximum=2)
    etching = node('control.scalar', 0, 560, value=0.45, control_minimum=0, control_maximum=1)
    time = node('core.time', 0, 840)
    clock = node('scalar.expression', 320, 280, dict(time=time, a=flow), expression='time * a')
    bands = []
    for index, source in enumerate(audio_band_groups.build_groups(node)):
        bands.append(node('scalar.expression', 320, 1160 + index * 300,
                          dict(a=source, b=response), expression='a * b'))
    phase = node('scalar.expression', 660, 0, dict(time=clock, a=bands[0]), expression='time * 0.075 + a * 0.12')
    warp_phase = node('scalar.expression', 660, 320, dict(time=clock, a=bands[1]), expression='time * 0.11 + a * 0.22')
    strength = node('scalar.map', 660, 660, dict(value=bands[1]), input_max=0.4,
                    output_min=0.18, output_max=0.65)
    line_phase = node('scalar.expression', 660, 1000, dict(time=clock, a=bands[2]), expression='time * 0.1 + a * 0.65')
    field = node('texture.noise', 1000, 0, dict(phase=phase), noise_scale=2.7, contrast=2.7,
                 seed=218, color_a=(0, 0, 0, 1), color_b=(1, 1, 1, 1))
    warp = node('texture.noise', 1000, 360, dict(phase=warp_phase), noise_scale=3.8, contrast=3,
                seed=471, color_a=(0.1, 0.7, 0, 1), color_b=(0.9, 0.3, 0, 1))
    field = node('texture.displace', 1340, 0, dict(source=field, displace_map=warp, displace_strength=strength),
                 displace_mode=1, rotation=27)
    shaders = []
    for index, record in enumerate(records):
        result = node('texture.shader', 1680, index * 360, dict(source=field, time=clock, a=bands[0], b=bands[2]))
        graph.nodes[-1] = graph.nodes[-1][:-1] + f'    properties {{ key: "asset" value {{ asset_sha256: "{record["sha256"]}" }} }}\n}}'
        shaders.append(result)
    fine = node('texture.contours', 1680, 800, dict(source=field, phase=line_phase),
                contour_count=22, line_width=0.035, color_a=(0.11, 0.18, 0.15, 0.55),
                color_b=(0.59, 0.41, 0.19, 0.7))
    fine = node('texture.affine', 2020, 800, dict(source=fine, opacity=etching))
    composed = node('texture.composite', 2020, 0, dict(a=shaders[0], b=fine))
    composed = node('texture.composite', 2360, 0, dict(a=composed, b=shaders[1]))
    grain = node('texture.noise', 2020, 1160, noise_scale=32, contrast=0.35, seed=834,
                 color_a=(0.53, 0.51, 0.45, 1), color_b=(0.82, 0.8, 0.73, 1))
    composed = node('texture.composite', 2700, 0, dict(a=composed, b=grain), amount=0.055)
    inset = node('texture.shape', 2360, 500, shape_type=0, shape_width=0.89, shape_height=0.81,
                 color_a=(1, 1, 1, 1))
    composed = node('texture.mask', 3040, 0, dict(source=composed, mask=inset))
    paper = node('texture.gradient', 2700, 600, color_a=(0.94, 0.92, 0.85, 1), color_b=(0.87, 0.84, 0.74, 1))
    composed = node('texture.composite', 3380, 0, dict(a=paper, b=composed))
    spectrum = node('texture.spectrum', 3040, 1100, bar_count=96, spectrum_layout=0,
                    spectrum_gain=1.3, bar_gap=0.72, color_a=(0.04, 0.12, 0.13, 0.9),
                    color_b=(0.64, 0.39, 0.11, 0.9))
    spectrum = node('texture.affine', 3380, 1100, dict(source=spectrum), scale_x=0.87, scale_y=0.065,
                    translate_y=0.395)
    composed = node('texture.composite', 3720, 0, dict(a=composed, b=spectrum))
    output = node('output.texture', 4060, 0, dict(source=composed))
    return graph, output, (response, flow, etching)


def main():
    records = shader_assets()
    graph, output, controls = build_graph(records)
    manifest = music_work.write('ink_tide', graph, output,
        list(zip(controls, ('Music response / 音乐响应', 'Tide flow / 潮汐流速', 'Etching / 等高纹理'))),
        [(1, 'Inlet', (0.7, 0.45, 0.25)), (2, 'Tide', (1, 0.8, 0.45)), (3, 'Surge', (1.4, 1.3, 0.8))],
        [('Inlet', 0, 1, 0), ('Rising tide', 3, 2, 2), ('Surge', 8, 3, 2), ('Recede', 12, 1, 3)],
        {'zh-CN': '墨潮', 'en-US': 'Ink Tide'},
        {'zh-CN': '墨绿潮汐在纸色留白中展开，金色岸线与细密等高纹理随音乐流动。低频改变墨量，中频驱动空间扭曲，高频推进金线和谱带。三个公开宏、四段 Cue 和 16 秒配乐均可编辑；两个图像 Shader 可直接修改。',
         'en-US': 'Deep teal tides cross warm paper, traced by fine contour etching and a metallic coastline. Bass changes pigment coverage, mids warp the field, highs advance the gold edge and spectrum. Edit three macros, four cues, 16-second music and both image shaders.'},
        platforms=('windows', 'android'), extra_assets=records, version='0.2.0')
    music_work.write_json(ROOT / 'provenance/ink_tide.json', dict(ownership='first-party', baseline='b5d36dd',
        source_files=['tools/author-phase-loom.py', 'tools/author-porcelain-pendulum.py',
                      'tools/author-resonance-gate.py', 'tools/author-luminous-concerto.py'],
        imported_third_party_files=[], renderer_provenance=['provenance/tixl_effects.json'],
        modifications=['New editable pigment/coastline expressions and framed contour composition.',
                       'Reuse original music arrangement, bounded image-shader compiler and native effect operators.',
                       'P7: reuse tools/audio_band_groups.py to cover all 63 FFT bands in three editable peak groups.'],
        assets=manifest['assets'], target='content/templates/ink_tide'))
    print(f'Ink Tide: {len(graph.nodes)} nodes, {len(graph.edges)} edges')


if __name__ == '__main__':
    main()

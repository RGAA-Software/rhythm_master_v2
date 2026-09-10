"""Shared authoring contracts for the four concept-board performances."""

import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess

import audio_band_groups
import music_work

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('concept_graph', ROOT / 'tools/author-resonance-gate.py')
WRITER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(WRITER)


def start():
    graph = WRITER.Graph()
    node = graph.node
    response = node('control.scalar', -1000, 0, value=1, control_minimum=0, control_maximum=2)
    pace = node('control.scalar', -1000, 300, value=1, control_minimum=0.2, control_maximum=2)
    exposure = node('control.scalar', -1000, 600, value=0.3, control_minimum=-1, control_maximum=1)
    time = node('core.time', 0, 0)
    clock = node('scalar.expression', 340, 0, dict(time=time, a=pace), expression='time * a')
    bands = [node('scalar.expression', 340, 400 + i * 300, dict(a=band, b=response),
                  expression='a * b') for i, band in enumerate(audio_band_groups.build_groups(node))]
    return graph, clock, bands, (response, pace, exposure)


def asset_node(graph, kind, x, y, digest, inputs=None, **properties):
    result = graph.node(kind, x, y, inputs, **properties)
    graph.nodes[-1] = graph.nodes[-1][:-1] + f'    properties {{ key: "asset" value {{ asset_sha256: "{digest}" }} }}\n}}'
    return result


def compile_expression(name, label, expression):
    source = ROOT / 'out/concept-authoring' / name / (label + '.expression')
    source.parent.mkdir(parents=True, exist_ok=True)
    source.write_text(expression, encoding='utf-8')
    return json.loads(subprocess.check_output([
        str(ROOT / 'out/windows-release/src/shader_authoring/shader_author_tool.exe'),
        str(ROOT / 'out/windows-release/src/windows_spike/deploy/shader_tools/shaderc.exe'),
        str(ROOT / 'third_party/sources/bgfx/src'),
        str(ROOT / 'src/rhythm_render/shaders/varying.def.sc'), str(source),
        str(ROOT / 'content/templates' / name / 'assets')], encoding='utf-8'))


def finish(graph, image, exposure, bloom=0.12):
    node = graph.node
    if bloom:
        soft = node('texture.blur', 11000, 400, dict(source=image), blur_radius=8, texture_precision=0)
        image = node('texture.composite', 11340, 0, dict(a=image, b=soft),
                     composite_mode=1, amount=bloom, texture_precision=0)
    display = node('texture.display', 11700, 0, dict(source=image, exposure=exposure))
    smooth = node('texture.fxaa', 12040, 0, dict(source=display))
    return node('output.texture', 12380, 0, dict(source=smooth))


def merge(graph, parts, x=6500):
    scene = parts[0]
    for i, part in enumerate(parts[1:]):
        scene = graph.node('scene.merge', x + i % 8 * 320, 5000 + i // 8 * 300, dict(a=scene, b=part))
    return scene


def capture(graph, scene, camera, environment=0.8):
    node = graph.node
    env = node('texture.gradient', 9000, 1100, color_a=(0.7, 0.75, 0.8, 1),
               color_b=(0.015, 0.025, 0.05, 1))
    scene = node('scene.environment', 9340, 0, dict(scene=scene, environment_texture=env),
                 environment_energy=environment)
    color = node('scene.render', 9680, 0, dict(scene=scene, camera=camera), scene_antialiasing=1)
    back = node('texture.gradient', 9680, 700, color_a=(0.004, 0.009, 0.018, 1),
                color_b=(0.016, 0.032, 0.053, 1))
    back = node('texture.linearize', 10020, 700, dict(source=back))
    return node('texture.composite', 10360, 0, dict(a=back, b=color), texture_precision=0)


def publish(name, title, description, graph, output, controls, assets=(), compute=False):
    manifest = music_work.write(name, graph, output,
        list(zip(controls, ('Music response / 音乐响应', 'Motion pace / 运动速度', 'Exposure / 曝光'))),
        [(1, 'Gather', (0.6, 0.55, 0.25)), (2, 'Develop', (1, 0.9, 0.3)),
         (3, 'Crest', (1.6, 1.3, 0.45))],
        [('Gather', 0, 1, 0), ('Develop', 3, 2, 2), ('Crest', 8, 3, 2), ('Resolve', 12, 1, 3)],
        {'zh-CN': title[0], 'en-US': title[1]}, {'zh-CN': description[0], 'en-US': description[1]},
        tier='advanced', platforms=('windows', 'android-gles31-compute' if compute else 'android'),
        extra_assets=list(assets), version='0.2.0')
    music_work.write_json(ROOT / 'provenance' / (name + '.json'), dict(
        ownership='first-party', baseline='506f432', concept='docs/design/concepts/music_visual_directions_v1.png',
        concept_role='AI-generated design reference only; not used as runtime texture',
        reused_sources=['tools/author-resonance-gate.py', 'tools/author-ink-tide.py',
                        'tools/author-crystal-choir.py', 'tools/author_petal_model.py',
                        'tools/author-resonant-arcade.py', 'tools/audio_band_groups.py', 'tools/music_work.py'],
        authoring_sources={p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in
                           ('tools/concept_work_common.py', 'tools/concept_mesh_assets.py',
                            'tools/concept_spatial_works.py', 'tools/concept_ink_work.py',
                            'tools/concept_corridor_work.py', 'tools/author-concept-works.py')},
        imported_third_party_files=[], renderer_provenance=['provenance/tixl_effects.json',
            'provenance/tixl_particles.json', 'provenance/depth_pipeline.json',
            'provenance/environment_lighting.json', 'provenance/godot_shadows.json'],
        assets=manifest['assets'], target='content/templates/' + name))
    print(name, len(graph.nodes), 'nodes', flush=True)

"""Compose a reusable three-band textured stage from existing scene operators."""

import hashlib
import importlib.util
from pathlib import Path

from audio_band_groups import build_groups

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('input_writer', ROOT / 'tools/author-input-components.py')
WRITER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(WRITER)


def build(graph):
    node = graph.node
    source = node('texture.color_adjust', 0, 0, saturation=1)
    bands = build_groups(node)
    response = node('scalar.constant', 0, 350, value=1)
    spread = node('scalar.constant', 0, 700, value=2.25)
    time = node('core.time', 0, 1050)
    pace = node('scalar.constant', 0, 1400, value=0.5)
    clock = node('scalar.expression', 350, 1050, dict(time=time, a=pace), expression='time * a')
    cube = node('geometry.cube', 350, 0)
    material = node('material.pbr', 700, 0, color_a=(1, 1, 1, 1),
                    color_b=(1, 1, 1, 1), emission=0.45, metallic=0.15, roughness=0.3)
    textured = node('material.textures', 1050, 0,
                    dict(material=material, base_texture=source, emission_texture=source))
    panel = node('scene.instance', 1400, 0, dict(geometry=cube, material=textured))
    parts = []
    for index, band in enumerate(bands):
        row = index * 1400
        energy = node('scalar.expression', 1750, row, dict(a=band, b=response), expression='a * b')
        turn = node('scalar.expression', 2100, row, dict(a=energy, time=clock),
                    expression=f'{(index - 1) * -18} + sin(time + {index * 2.1}) * 8 + a * 32')
        lift = node('scalar.expression', 2100, row + 350, dict(a=energy, time=clock),
                    expression=f'sin(time * 0.7 + {index * 2.1}) * 0.18 + a * 0.5')
        position = node('scalar.expression', 2100, row + 700, dict(a=spread),
                        expression=f'a * {index - 1}')
        parts.append(node('scene.transform', 2450, row,
                          dict(scene=panel, rotation_y=turn, translate_y=lift, translate_x=position),
                          scale_x=1.9, scale_y=3.1, scale_z=0.12))
    scene = node('scene.merge', 2800, 0, dict(a=parts[0], b=parts[1]))
    scene = node('scene.merge', 3150, 0, dict(a=scene, b=parts[2]))
    light = node('scene.directional_light', 2800, 700, light_x=-0.4, light_y=0.7,
                 light_z=0.8, light_energy=2)
    scene = node('scene.merge', 3500, 0, dict(a=scene, b=light))
    camera = node('scene.camera', 3500, 700, eye_x=2, eye_y=1.5, eye_z=11,
                  field_of_view=38, near_plane=0.1, far_plane=30)
    capture = node('scene.capture', 3850, 0, dict(scene=scene, camera=camera))
    color = node('scene.color', 4200, 0, dict(capture=capture))
    display = node('texture.display', 4550, 0, dict(source=color), exposure=0)
    output = node('texture.fxaa', 4900, 0, dict(source=display))
    return source, output, [('response', response, 'value'), ('spacing', spread, 'value'),
                            ('flow', pace, 'value'), ('exposure', display, 'exposure'),
                            ('roughness', material, 'roughness'), ('saturation', source, 'saturation')]


def main():
    recipe = dict(name='spectral_triptych', build=build,
                  titles=('Spectral triptych', '三频折屏'),
                  descriptions=('Connect an image or text texture. Three shared-mesh screens independently turn and rise with full bass, mid and treble groups. Edit spacing, motion, response and material; the preview stripes are not inserted.',
                                '连接图像或文字纹理。三块共享网格的立体屏幕分别随完整低、中、高频段转动和升降。可编辑间距、运动、响应与材质；预览条纹不会插入工程。'),
                  variant_titles=('Close folding stage', '紧凑折叠舞台'),
                  variant=dict(spacing=1.7, flow=0.25, response=1.6, roughness=0.55, exposure=-0.3),
                  bounds=dict(spacing=(1.4, 3), exposure=(-1, 1)),
                  fixture_lines=7, fixture_rotation=25)
    entry = WRITER.write_component(recipe)
    WRITER.write_json(ROOT / 'provenance/spectral_triptych.json', dict(
        ownership='first-party', authoring_tool='tools/author-spectral-triptych.py',
        authoring_tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        reused_sources=['tools/author-input-components.py', 'tools/author-resonant-arcade.py',
                        'tools/audio_band_groups.py'],
        imported_third_party_files=[], existing_adapters='provenance/tixl_effects.json',
        component=entry))


if __name__ == '__main__':
    main()

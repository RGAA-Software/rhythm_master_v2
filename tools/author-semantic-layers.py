"""Extract reusable music-driven layers from our existing authored compositions."""

import hashlib
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Each exposed property must be inside the extracted dependency closure and must
# not already be driven by a graph input. Variants alter structure/motion as well
# as shading; this inventory does not grant visual acceptance to preset counts.
LAYERS = [
    dict(name='woven_light', source='aurora-braid', stage='texture.dof', category='scene',
         titles=('Woven light', '编织光带'),
         descriptions=('Three audio bands change tube thickness and radius around a rotating braid. Reuse the transparent sculpture over your own background.',
                       '三个频段改变旋转编织带的粗细和半径。将透明雕塑层叠加到自己的背景上。'),
         parameters=[('path_turns', 'path.helix', 0, 'path_turns', 4.0),
                     ('path_height', 'path.helix', 0, 'path_height', 3.2),
                     ('roughness', 'material.pbr', 0, 'roughness', 0.55),
                     ('field_of_view', 'scene.camera', 0, 'field_of_view', 49),
                     ('dof_radius', 'texture.dof', 0, 'dof_radius', 2)]),
    dict(name='torsion_corolla', source='torque-garden', stage='texture.dof', category='scene',
         titles=('Torsion corolla', '扭转花冠'),
         descriptions=('Six metallic petals twist with separate bands. Change the shared ring geometry, reflections and focus before compositing the sculpture.',
                       '六片金属花瓣随不同频段扭转。可调整共享环体几何、反射和焦点，再合成到自己的画面。'),
         parameters=[('radius', 'geometry.torus', 0, 'radius', 0.68),
                     ('tube_ratio', 'geometry.torus', 0, 'tube_ratio', 0.14),
                     ('roughness', 'material.pbr', 0, 'roughness', 0.5),
                     ('environment_energy', 'scene.environment', 0, 'environment_energy', 0.6),
                     ('field_of_view', 'scene.camera', 0, 'field_of_view', 49)]),
    dict(name='spectrum_columns', source='spectral-foundry', stage='texture.dof', category='audio',
         titles=('Spectrum columns', '频谱柱阵'),
         descriptions=('A batched field of up to 4096 columns responds to the spectrum. Change grid density, span and camera framing independently of the final background.',
                       '最多 4096 根批量绘制的立柱响应频谱。独立调整点阵密度、分布跨度和相机取景，不绑定最终背景。'),
         parameters=[('columns', 'point.grid', 0, 'columns', 32),
                     ('rows', 'point.grid', 0, 'rows', 24),
                     ('instance_span', 'scene.point_instances', 0, 'instance_span', 8),
                     ('roughness', 'material.pbr', 0, 'roughness', 0.7),
                     ('field_of_view', 'scene.camera', 0, 'field_of_view', 38)]),
    dict(name='enamel_orb', source='sonic-enamel', stage='texture.dof', category='scene',
         titles=('Enamel light body', '珐琅光体'),
         descriptions=('Procedural enamel, raised-looking veins and local lights form a music-driven sculpture. Control pattern scale, vein count and material UVs.',
                       '程序化珐琅、纹脉和局部灯光组成音乐雕塑。可控制纹理尺度、纹脉数量和材质 UV。'),
         parameters=[('noise_scale', 'texture.noise', 0, 'noise_scale', 5.5),
                     ('contour_count', 'texture.contours', 0, 'contour_count', 14),
                     ('uv_scale_x', 'material.textures', 0, 'uv_scale_x', 3),
                     ('normal_scale', 'material.textures', 0, 'normal_scale', 0.55),
                     ('field_of_view', 'scene.camera', 0, 'field_of_view', 49)]),
    dict(name='dual_population_mist', source='spectral-nebula', stage='texture.composite', stage_index=1,
         category='particles', titles=('Dual-population mist', '双群星雾'),
         descriptions=('Two GPU particle populations form counter-rotating folded clouds. Bass drives flow, high bands drive emission, and loudness sets opacity.',
                       '两组 GPU 粒子形成反向旋转的折叠云团。低频驱动流动，高频驱动发射，响度控制透明度。'),
         parameters=[('emitter_radius', 'gpu.particles', 0, 'emitter_radius', 0.48),
                     ('flow_frequency', 'gpu.particles', 0, 'flow_frequency', 9),
                     ('sectors', 'texture.mapping', 0, 'sectors', 4),
                     ('scale', 'texture.mapping', 1, 'scale', 1.2),
                     ('exposure', 'texture.color_adjust', 0, 'exposure', -2.1)]),
    dict(name='band_city', source='harmonic-city', stage='scene.render', category='audio',
         titles=('Band city', '频段城景'),
         descriptions=('Thirty-two bands drive 64 individually editable pillars and their centers. Reuse the lit city layer with your own post-processing.',
                       '32 个频段驱动 64 根可独立编辑的立柱及其中心位置。将受光城景层接入自己的后期处理。'),
         parameters=[('roughness', 'material.pbr', 0, 'roughness', 0.65),
                     ('metallic', 'material.pbr', 0, 'metallic', 0.1),
                     ('eye_x', 'scene.camera', 0, 'eye_x', 3),
                     ('eye_y', 'scene.camera', 0, 'eye_y', 12),
                     ('field_of_view', 'scene.camera', 0, 'field_of_view', 48)]),
]

VARIANT_TITLES = {
    'woven_light': ('Tight braid', '紧密编织'),
    'torsion_corolla': ('Broad petals', '厚瓣花冠'),
    'spectrum_columns': ('Sparse steps', '疏列阶梯'),
    'enamel_orb': ('Fine veins', '细密纹脉'),
    'dual_population_mist': ('Fourfold cloud', '四瓣云旋'),
    'band_city': ('Overhead city', '俯视城阵'),
}


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=4) + '\n', encoding='utf-8')


def parameter_group(kind):
    if kind in ('scene.camera', 'texture.dof'):
        return 'component.camera'
    if kind.startswith('material.') or kind in ('scene.environment', 'texture.color_adjust'):
        return 'component.material'
    if kind.startswith(('geometry.', 'path.')) or kind in ('point.grid', 'scene.point_instances'):
        return 'component.geometry'
    if kind in ('texture.noise', 'texture.contours', 'texture.mapping'):
        return 'component.pattern'
    return 'component.motion'


def build_layer(recipe):
    source = ROOT / 'tools' / ('author-' + recipe['source'] + '.py')
    spec = importlib.util.spec_from_file_location('layer_source', source)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    graph, _ = module.build_graph()
    matching = [record['id'] for record in graph.records if record['kind'] == recipe['stage']]
    output = matching[recipe.get('stage_index', 0)]
    # The library exposes compositable SDR textures. Conversion stays an editable node.
    output = graph.node('texture.display', 0, 0, dict(source=output), tone_mapping=1)
    records = {record['id']: record for record in graph.records}
    closure = set()
    def include(identity):
        if identity in closure:
            return
        closure.add(identity)
        for dependency in records[identity]['inputs'].values():
            include(dependency)
    include(output)
    parameters, variant = [], {}
    for key, kind, index, property_name, value in recipe['parameters']:
        identity = [record['id'] for record in graph.records if record['kind'] == kind][index]
        if identity not in closure or property_name in records[identity]['inputs']:
            raise ValueError(f'{recipe["name"]}: ineffective exposed parameter {key}')
        parameters.append(f'parameters {{ key: "{key}" node: {identity} property: "{property_name}" group: "{parameter_group(kind)}" }}')
        variant[key] = value
    name = recipe['name']
    component_type = 'component.official.' + name
    destination = ROOT / 'content/semantic' / name
    destination.mkdir(parents=True, exist_ok=True)
    header = ['schema_version: 4', f'id: "semantic-{name}"', 'canvas { width: 640 height: 360 }',
              'output: 2', f'nodes {{ id: 1 type_key: "{component_type}" schema_version: 1 }}',
              'nodes { id: 2 type_key: "output.texture" schema_version: 1 }',
              'edges { id: 1 from: 1 to: 2 input: "source" }',
              f'components {{ type_key: "{component_type}" schema_version: 1 title: "{recipe["titles"][0]}" output: {output}']
    body = ['\n'.join('    ' + line for line in graph.nodes[identity - 1].splitlines())
            for identity in sorted(closure)]
    edges = []
    for identity in sorted(closure):
        for port, dependency in records[identity]['inputs'].items():
            edges.append(f'    edges {{ id: {len(edges)+1} from: {dependency} to: {identity} input: "{port}" }}')
    (destination / 'graph.textproto').write_text(
        '\n'.join(header + body + edges + ['    ' + value for value in parameters] + ['}'])+'\n', encoding='utf-8')
    # Arrange by dependency depth, retaining original IDs for provenance and editing.
    depths, rows, positions = {}, {}, []
    for identity in sorted(closure):
        depth = max((depths[value]+1 for value in records[identity]['inputs'].values()), default=0)
        depths[identity] = depth
        row = rows.get(depth, 0)
        rows[depth] = row + 1
        positions.append(dict(id=identity, x=depth * 320, y=row * 300))
    write_json(destination / 'editor.json', dict(version=2, positions=[dict(id=1, x=50, y=70), dict(id=2, x=430, y=70)],
                                                components=[dict(type=component_type, positions=positions)]))
    titles = dict(zip(('en-US', 'zh-CN'), recipe['titles']))
    descriptions = dict(zip(('en-US', 'zh-CN'), recipe['descriptions']))
    manifest = dict(format='rhythm.project', manifest_version=1, kind='template',
                    content_id='official.semantic.'+name, content_version='0.1.0', project_id='semantic-'+name,
                    graph_revision=0, title=' / '.join(reversed(recipe['titles'])), default_locale='zh-CN',
                    titles=titles, category=recipe['category'], maturity='visual-review-pending',
                    author='Rhythm Master', license_status='First-party extracted graph; existing attributed renderer adapters; outbound license pending',
                    compatible_players=['windows', 'android'], external_assets=[], descriptions=descriptions,
                    semantic=True, default=False)
    write_json(destination / 'manifest.json', manifest)
    common = dict(version='1.0.0', operator=component_type, reset=True)
    write_json(destination / 'presets.json', dict(schema_version=1, presets=[
        dict(common, id='official.semantic.'+name+'.default', titles={'en-US':'Default', 'zh-CN':'默认参数'}, properties={}),
        dict(common, id='official.semantic.'+name+'.variant',
             titles=dict(zip(('en-US', 'zh-CN'), VARIANT_TITLES[name])), properties=variant)]))
    print(f'{name}: {len(closure)} internal nodes, {len(parameters)} controls')
    return dict(component=name, source=source.relative_to(ROOT).as_posix(),
                source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(), extracted_nodes=sorted(closure - {output}),
                modifications=['Extract dependency closure before the final background/post stack.',
                               'Add editable display conversion, public parameter aliases, variant and compact internal layout.'])


def main():
    entries = [build_layer(recipe) for recipe in LAYERS]
    write_json(ROOT / 'provenance/semantic_layers_r6.json', dict(ownership='first-party', baseline='fbf36db',
               imported_third_party_files=[], source_url=None,
               authoring_tool='tools/author-semantic-layers.py',
               authoring_tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
               graph_writer='tools/author-resonance-gate.py',
               graph_writer_sha256=hashlib.sha256((ROOT / 'tools/author-resonance-gate.py').read_bytes()).hexdigest(),
               reuse='Focused extraction of existing project-owned compositions; renderer notices remain in their adapters.',
               target_modules=['semantic_content'], entries=entries))


if __name__ == '__main__':
    main()

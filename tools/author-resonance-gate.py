"""Author the editable, 24-band Resonance Gate composition from existing operators."""

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class Graph:
    def __init__(self):
        self.nodes = []
        self.edges = []
        self.positions = []
        # Structured authoring records allow focused reuse without parsing textproto.
        self.records = []

    def node(self, kind, x, y, inputs=None, **properties):
        identity = len(self.nodes) + 1
        self.records.append(dict(id=identity, kind=kind, inputs=dict(inputs or {}),
                                 properties=dict(properties)))
        lines = [f'nodes {{ id: {identity} type_key: "{kind}" schema_version: 1']
        for key, value in properties.items():
            if isinstance(value, tuple):
                encoded = 'color { ' + ' '.join(f'{channel}: {v}' for channel, v in zip('rgba', value)) + ' }'
            elif isinstance(value, str):
                encoded = 'expression: ' + json.dumps(value)
            else:
                encoded = f'scalar: {value}'
            lines.append(f'    properties {{ key: "{key}" value {{ {encoded} }} }}')
        self.nodes.append('\n'.join(lines) + '\n}')
        self.positions.append(dict(id=identity, x=x, y=y))
        for port, source in (inputs or {}).items():
            self.edges.append(f'edges {{ id: {len(self.edges) + 1} from: {source} to: {identity} input: "{port}" }}')
        return identity


def build_graph():
    graph = Graph()
    node = graph.node
    time = node('core.time', 0, 0)
    bass = node('audio.band', 0, 240, audio_band=12)
    treble = node('audio.band', 0, 480, audio_band=48)
    loudness = node('audio.feature', 0, 720, audio_feature=1)
    phase = node('scalar.expression', 300, 0, dict(time=time), expression='time * 0.065')
    travel = node('scalar.expression', 300, 240, dict(time=time), expression='time * 0.035')
    rotation = node('scalar.expression', 300, 480, dict(time=time), expression='time * 3')
    pulse = node('scalar.map', 300, 720, dict(value=bass), output_min=0.90, output_max=1.14)
    field = node('texture.noise', 620, 0, dict(phase=phase), noise_scale=3.8, contrast=2.5,
                 seed=53, color_a=(0, 0, 0, 1), color_b=(1, 1, 1, 1))
    folded = node('texture.mapping', 960, 0, dict(source=field, rotation=rotation), sectors=9, scale=1.35)
    tunnel = node('texture.mapping', 1300, 0, dict(source=folded, travel=travel, scale=pulse),
                  mapping_mode=1, radial_power=-0.55, twist=0.5)
    threads = node('texture.contours', 1640, 0, dict(source=tunnel, phase=travel),
                   contour_count=14, line_width=0.055,
                   color_a=(0.01, 0.12, 0.20, 1), color_b=(0.12, 0.045, 0.28, 1))
    back = node('texture.gradient', 1300, 320, color_a=(0.001, 0.003, 0.014, 1),
                color_b=(0.018, 0.003, 0.029, 1))
    composed = node('texture.composite', 2000, 0, dict(a=back, b=threads), composite_mode=1, amount=0.65)
    # Shared thin radial blades. Per-band response is wired into the graph;
    # silence retains restrained geometry, never a fabricated audio envelope.
    blades = []
    for index, color in enumerate(((0.06, 0.8, 1, 0.8), (0.4, 0.15, 1, 0.8),
                                   (1, 0.2, 0.5, 0.8), (1, 0.62, 0.15, 0.8))):
        blades.append(node('texture.shape', 620 + index * 340, 560, shape_type=0,
                           shape_width=0.006, shape_height=0.20, center_y=0.22, color_a=color))
    pending_layers = [composed]
    for index in range(24):
        x = (index // 8) * 2180
        y = 1100 + (index % 8) * 320
        band = node('audio.band', x, y, audio_band=6 + index * 2)
        scale = node('scalar.map', x + 300, y, dict(value=band), output_min=0.72, output_max=1.26)
        opacity = node('scalar.map', x + 600, y, dict(value=band), input_max=0.7, output_min=0.20, output_max=0.96)
        turn = node('scalar.expression', x + 900, y, dict(time=time, a=band),
                    expression=f'{index * 15} + time * 3 + a * 5')
        blade = node('texture.affine', x + 1200, y,
                     dict(source=blades[index % 4], scale=scale, opacity=opacity, rotation=turn))
        pending_layers.append(blade)
        if len(pending_layers) == 8 or index == 23:
            composed = node('texture.stack', x + 1580, y,
                            {f'layer_{i + 1}': layer for i, layer in enumerate(pending_layers)}, composite_mode=1)
            pending_layers = [composed]
    spectrum = node('texture.spectrum', 2400, 0, spectrum_layout=1, spectrum_radius=0.20,
                    bar_count=192, spectrum_gain=2.2, bar_gap=0.7,
                    color_a=(0.1, 0.65, 1, 0.7), color_b=(1, 0.15, 0.46, 0.8))
    spectrum_pulse = node('texture.affine', 2740, 0, dict(source=spectrum, scale=pulse, rotation=rotation))
    composed = node('texture.composite', 3100, 0, dict(a=composed, b=spectrum_pulse), composite_mode=1)
    # Two intersecting orbital ellipses provide a spatial hierarchy around the core.
    ring = node('texture.shape', 2400, 400, shape_type=2, shape_width=0.60, shape_height=0.34,
                inner_ratio=0.991, color_a=(0.14, 0.6, 0.9, 0.65))
    for index in range(2):
        turn = node('scalar.expression', 2740, 400 + index * 300, dict(time=time, a=loudness),
                    expression=f'{-28 + index * 72} + sin(time * 0.25) * 12 + a * 5')
        orbit = node('texture.affine', 3080, 400 + index * 300, dict(source=ring, rotation=turn, scale=pulse))
        composed = node('texture.composite', 3440, 400 + index * 300, dict(a=composed, b=orbit), composite_mode=1)
    emission = node('scalar.map', 3800, 0, dict(value=treble), output_min=0.2, output_max=2.5)
    particles = node('point.emitter', 3800, 300, dict(emission=emission), particle_capacity=2200,
                     seed=91, emission_rate=220, lifetime=5, emitter_shape=2,
                     emitter_width=0.40, emitter_height=0.7, center_y=0.5,
                     direction=0, spread=360, particle_speed=0.032, gravity_y=0,
                     flow_strength=0.055, flow_frequency=3.2, flow_evolution=0.08,
                     point_size=0.003, color_a=(0.1, 0.65, 1, 0.85), color_b=(1, 0.25, 0.3, 0))
    dust = node('point.render', 4140, 300, dict(points=particles), point_blend=1)
    trail = node('texture.trail', 4480, 300, dict(source=dust), trail_half_life=0.18, trail_zoom_rate=0.025)
    composed = node('texture.composite', 4820, 300, dict(a=composed, b=trail), composite_mode=1)
    near = node('texture.blur', 5200, 0, dict(source=composed), blur_radius=2)
    wide = node('texture.blur', 5200, 330, dict(source=composed), blur_radius=14)
    halos = node('texture.composite', 5540, 0, dict(a=near, b=wide), composite_mode=1, amount=0.45)
    gain = node('scalar.map', 5540, 650, dict(value=loudness), output_min=0.22, output_max=0.65)
    final = node('texture.composite', 5880, 0, dict(a=composed, b=halos, amount=gain), composite_mode=1)
    exposure = node('texture.color_adjust', 6240, 0, dict(source=final), exposure=0.8)
    output = node('output.texture', 6580, 0, dict(source=exposure))
    return graph, output


def main():
    graph, output = build_graph()
    destination = ROOT / 'content/templates/resonance_gate'
    destination.mkdir(parents=True, exist_ok=True)
    text = [f'schema_version: 4\nid: "official-resonance-gate"\noutput: {output}\ncanvas {{ width: 1280 height: 720 }}']
    (destination / 'graph.textproto').write_text('\n'.join(text + graph.nodes + graph.edges) + '\n', encoding='utf-8')
    (destination / 'editor.json').write_text(json.dumps(dict(version=2, positions=graph.positions), indent=4) + '\n', encoding='utf-8')
    manifest = dict(format='rhythm.project', manifest_version=1, kind='template',
                    content_id='official.templates.resonance_gate', content_version='0.1.0',
                    project_id='official-resonance-gate', graph_revision=0,
                    title='共振星门 / Resonance Gate', default_locale='zh-CN',
                    titles={'zh-CN': '共振星门', 'en-US': 'Resonance Gate'},
                    category='audio', tier='advanced', maturity='visual-review-pending',
                    author='Rhythm Master', license_status='First-party graph; existing attributed effect adapters; outbound license pending',
                    compatible_players=['windows'], external_assets=[],
                    descriptions={'zh-CN': f'{len(graph.nodes)} 个完整节点：24 频段控制放射光束，低频推动环带，高频驱动星尘，响度控制辉光。请在音频输入中开启系统监听或播放本地音乐。',
                                  'en-US': f'{len(graph.nodes)} editable nodes: 24 bands animate radial blades; bass pulses the rings, treble emits stardust, loudness drives glow. Start system audio capture or play a local music file.'})
    (destination / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=4) + '\n', encoding='utf-8')
    print(f'Resonance Gate: {len(graph.nodes)} nodes, {len(graph.edges)} edges')


if __name__ == '__main__':
    main()

"""Multiscale pigment strata for the second concept review."""

from concept_work_common import start, compile_expression, asset_node, finish, publish


def ink():
    name = 'stratified_ink'
    graph, clock, bands, controls = start()
    node = graph.node
    field_expression = ('vec4(Sample(uv).r, fract(sin(dot(uv * vec2(1317.0, 739.0), '
        'vec2(12.9898, 78.233))) * 43758.5453), Sample(uv).r, 1.0)')
    height_expression = ('vec4(uv.y - (uv.x - 0.5) * 0.39 + (Sample(uv).r - 0.5) * (0.78 + b * 0.15) '
        '+ sin(uv.x * 6.0 + time * 0.075) * (0.075 + a * 0.05), Sample(uv).g, Sample(uv).b, 1.0)')
    s = 'Sample(uv)'
    red = f'(smoothstep(0.32, 0.35, {s}.r) * (1.0 - smoothstep(0.57, 0.6, {s}.r)))'
    palette = f'mix(vec3(0.008, 0.026, 0.052), vec3(0.46, 0.032, 0.009), {red})'
    layers = f'((0.22 + 0.78 * pow(0.5 + 0.5 * sin(floor({s}.r * 28.0) * 2.399), 0.7)) * (0.55 + 0.45 * smoothstep(0.0, 0.2, fract({s}.r * 28.0))))'
    gilding = f'pow(1.0 - abs(fract({s}.r * 14.0) - 0.5) * 2.0, 75.0)'
    pigment = (f'vec4({palette} * {layers} * (0.45 + {s}.g * 1.4) + '
        f'vec3(0.95, 0.53, 0.14) * {gilding} * (0.65 + c * 2.0) + '
        f'vec3(0.8, 0.16, 0.021) * exp(-abs({s}.r - 0.405) * 55.0) * (0.45 + a), 1.0)')
    noise = node('texture.noise', 1100, 0, noise_scale=2.1, contrast=1.7, seed=287,
                 color_a=(0, 0, 0, 1), color_b=(1, 1, 1, 1))
    medium = node('texture.noise', 1100, 400, noise_scale=8, contrast=1.8, seed=711,
                  color_a=(0, 0, 0, 1), color_b=(1, 1, 1, 1))
    fine = node('texture.noise', 1100, 800, noise_scale=28, contrast=2, seed=121,
                color_a=(0, 0, 0, 1), color_b=(1, 1, 1, 1))
    combined = node('texture.composite', 1450, 600, dict(a=noise, b=medium), amount=.3)
    combined = node('texture.composite', 1800, 600, dict(a=combined, b=fine), amount=.1)
    first = compile_expression(name, 'multiscale_pigment', field_expression)
    field = asset_node(graph, 'texture.shader', 1450, 0, first['sha256'], dict(source=combined), texture_precision=2)
    displacement = node('texture.displace', 1800, 0, dict(source=field, displace_map=noise),
                        displace_strength=.035, texture_precision=0)
    height = compile_expression(name, 'stratified_height', height_expression)
    field = asset_node(graph, 'texture.shader', 2150, 0, height['sha256'],
                       dict(source=displacement, time=clock, a=bands[0], b=bands[1]), texture_precision=2)
    color = compile_expression(name, 'mineral_pigment', pigment)
    image = asset_node(graph, 'texture.shader', 2500, 0, color['sha256'],
                       dict(source=field, a=bands[0], c=bands[2]), texture_precision=0)
    output = finish(graph, image, controls[2], 0)
    publish(name, ('层叠墨流', 'Stratified Ink'),
            ('靛蓝与朱红矿物层带形成错落河谷，多尺度扰动、颜料颗粒与不规则金边构成细节。低频推动主带，中频改变侵蚀幅度，高频增强金边；高度场、颜料 Shader、宏与配乐可编辑，非物理流体模拟。',
             'Indigo and vermilion mineral strata form an irregular valley with multiscale erosion, pigment grain and gilded edges. Bass shifts strata, mids erode contours and highs light gold. Editable procedural fields, not physical fluid simulation.'),
            graph, output, controls, [first, height, color])

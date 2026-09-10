"""Editable image-shader compositions for the vortex and stratified ink concepts."""

from concept_work_common import start, compile_expression, asset_node, finish, publish


def vortex():
    name = 'aureate_vortex'
    graph, clock, bands, controls = start()
    node = graph.node
    # Keep coordinate construction and shading in separate bounded expressions.
    # The image shader profile has no statements, loops or private variables.
    p = '((uv - vec2(0.48, 0.49)) * vec2(1.7778, 1.0))'
    radius = f'length({p})'
    # Single-argument atan is portable across the current D3D/GLES wrapper.
    angle = f'(atan({p}.y / (abs({p}.x) + 0.00001)) * sign({p}.x) + step({p}.x, 0.0) * pi * sign({p}.y))'
    coordinates = f'vec4(({angle}) / (2.0 * pi) + log({radius} + 0.035) * (0.42 + a * 0.16) - time * 0.025, {radius}, Sample(uv * 2.0).r, 1.0)'
    s = 'Sample(uv)'
    line = f'pow(0.5 + 0.5 * sin({s}.r * 440.0 + {s}.b * 5.0), 30.0)'
    dots = f'(pow(0.5 + 0.5 * sin({s}.r * 251.0), 65.0) * pow(0.5 + 0.5 * sin({s}.g * 570.0 - time * 1.2), 55.0))'
    beads = f'(pow(0.5 + 0.5 * sin({s}.r * 59.0 + {s}.g * 7.0), 60.0) * pow(0.5 + 0.5 * sin({s}.g * 167.0 + time * 0.7), 45.0))'
    arms = f'(0.12 + pow(0.5 + 0.5 * sin({s}.r * 17.0), 3.0))'
    envelope = f'(smoothstep(0.01, 0.10, {s}.g) * (1.0 - smoothstep(0.55, 1.05, {s}.g)))'
    shading = f'vec4(mix(vec3(0.015, 0.45, 0.63), vec3(1.0, 0.49, 0.085), smoothstep(-0.3, 0.5, sin({s}.r * 19.0 + time * 0.04))) * ({line} * (0.45 + b * 0.7) + {dots} * (2.2 + c * 6.0) + {beads} * (7.0 + c * 10.0)) * {envelope} * {arms} + vec3(1.0, 0.47, 0.08) * exp(-{s}.g * 65.0) * (1.0 + a), 1.0)'
    records = [compile_expression(name, 'spiral_coordinates', coordinates),
               compile_expression(name, 'filaments_and_sparks', shading)]
    noise = node('texture.noise', 1000, 0, noise_scale=5, contrast=1.3, seed=713,
                 color_a=(0, 0, 0, 1), color_b=(1, 1, 1, 1))
    field = asset_node(graph, 'texture.shader', 1400, 0, records[0]['sha256'],
                       dict(source=noise, time=clock, a=bands[0]), texture_precision=2)
    image = asset_node(graph, 'texture.shader', 1800, 0, records[1]['sha256'],
                       dict(source=field, time=clock, a=bands[0], b=bands[1], c=bands[2]), texture_precision=0)
    output = finish(graph, image, controls[2], 0.3)
    publish(name, ('鎏光流涡', 'Aureate Vortex'),
            ('青蓝与琥珀细流围绕偏心涡核交错，细点沿流线闪动。低频扭动涡核，中频改变丝线亮度，高频激活光点。两个可编辑图像 Shader、三宏与四段配乐；为解析式粒子外观，不是物理粒子模拟。',
             'Cyan and amber filaments spiral around an offset core. Bass twists the field, mids brighten strands and highs activate sparks. Two editable image shaders, three macros and four music cues; analytic particle appearance, not physical particle simulation.'),
            graph, output, controls, records)


def ink():
    name = 'stratified_ink'
    graph, clock, bands, controls = start()
    node = graph.node
    coordinates = 'vec4(uv.y + (uv.x - 0.5) * 0.48 + sin(uv.x * 7.0 + time * 0.12) * (0.11 + a * 0.08) + (Sample(uv).r - 0.5) * (0.21 + b * 0.10), Sample(uv * 4.0).r, 0.0, 1.0)'
    s = 'Sample(uv)'
    red = f'(1.0 - smoothstep(0.08, 0.23, abs({s}.r - 0.50)))'
    strata = f'(0.7 + 0.3 * sin({s}.r * 180.0 + {s}.g * 1.8))'
    palette = f'mix(vec3(0.008, 0.029, 0.067), vec3(0.48, 0.026, 0.013), {red})'
    coast = f'pow(0.5 + 0.5 * sin({s}.r * 65.0), 65.0)'
    paint = f'vec4({palette} * {strata} * (0.7 + {s}.g * 0.6) + vec3(0.95, 0.46, 0.075) * {coast} * (0.17 + c * 0.7) + vec3(0.48, 0.045, 0.007) * exp(-abs({s}.r - 0.5) * 18.0) * (0.25 + a), 1.0)'
    records = [compile_expression(name, 'sweeping_height', coordinates),
               compile_expression(name, 'pigment_and_gold', paint)]
    phase = node('scalar.expression', 1000, 0, dict(time=clock, a=bands[1]), expression='time * 0.035 + a * 0.08')
    noise = node('texture.noise', 1340, 0, dict(phase=phase), noise_scale=3.4, contrast=1.8,
                 seed=287, color_a=(0, 0, 0, 1), color_b=(1, 1, 1, 1))
    field = asset_node(graph, 'texture.shader', 1680, 0, records[0]['sha256'],
                       dict(source=noise, time=clock, a=bands[0], b=bands[1]), texture_precision=2)
    image = asset_node(graph, 'texture.shader', 2020, 0, records[1]['sha256'],
                       dict(source=field, a=bands[0], c=bands[2]), texture_precision=0)
    output = finish(graph, image, controls[2], 0)
    publish(name, ('层叠墨流', 'Stratified Ink'),
            ('靛蓝与朱红层带沿斜向河谷展开，细金线勾勒褶皱。低频推动主带，中频改变流形，高频增强金边；保留暗部留白、可编辑高度场与颜料 Shader、三宏和四段配乐。',
             'Indigo and vermilion strata sweep across a diagonal valley with delicate gold edges. Bass drives the main band, mids reshape flow and highs light the edges. Editable height/pigment shaders, three macros and four music cues.'),
            graph, output, controls, records)

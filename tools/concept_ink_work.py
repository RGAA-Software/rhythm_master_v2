"""Multiscale pigment strata for the second concept review."""

from concept_work_common import start, compile_expression, asset_node, finish, publish


def ink():
    name = 'stratified_ink'
    graph, clock, bands, controls = start()
    node = graph.node
    travel_x = node('scalar.expression', 750, 900, dict(time=clock), expression='2.4 * sin(time * .3926990817)')
    travel_y = node('scalar.expression', 750, 1200, dict(time=clock), expression='2.4 * (cos(time * .3926990817) - 1)')
    # Advect the existing continuous noise lattice, rather than pan a clamped image.
    travel = dict(offset_x=travel_x, offset_y=travel_y)
    field_expression = ('vec4(Sample(uv).r, fract(sin(dot(uv * vec2(1317.0, 739.0), '
        'vec2(12.9898, 78.233))) * 43758.5453), Sample(uv).r, 1.0)')
    height_expression = ('vec4(uv.y - (uv.x - 0.5) * 0.39 + (Sample(uv).r - 0.5) * (0.78 + b * 0.15) '
        '+ sin(uv.x * 6.0 + time * 0.3926990817) * (0.075 + a * 0.025), Sample(uv).g, Sample(uv).b, 1.0)')
    s = 'Sample(uv)'
    red = f'(smoothstep(0.32, 0.35, {s}.r) * (1.0 - smoothstep(0.57, 0.6, {s}.r)))'
    palette = f'mix(vec3(0.008, 0.026, 0.052), vec3(0.46, 0.032, 0.009), {red})'
    layers = f'((0.22 + 0.78 * pow(0.5 + 0.5 * sin(floor({s}.r * 28.0) * 2.399), 0.7)) * (0.55 + 0.45 * smoothstep(0.0, 0.2, fract({s}.r * 28.0))))'
    gilding = f'pow(1.0 - abs(fract({s}.r * 14.0) - 0.5) * 2.0, 75.0)'
    pigment = (f'vec4({palette} * {layers} * (0.45 + {s}.g * 1.4) + '
        f'vec3(0.95, 0.53, 0.14) * {gilding} * (0.65 + c * 2.0) + '
        f'vec3(0.8, 0.16, 0.021) * exp(-abs({s}.r - 0.405) * 55.0) * (0.45 + a), 1.0)')
    noise = node('texture.noise', 1100, 0, travel, noise_scale=2.1, contrast=1.7, seed=287,
                 color_a=(0, 0, 0, 1), color_b=(1, 1, 1, 1))
    medium = node('texture.noise', 1100, 400, travel, noise_scale=8, contrast=1.8, seed=711,
                  color_a=(0, 0, 0, 1), color_b=(1, 1, 1, 1))
    fine = node('texture.noise', 1100, 800, travel, noise_scale=28, contrast=2, seed=121,
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
    # The procedural river is the autonomous current. These pigment layers are
    # independent, gradually populated material accents with explicit band roles.
    low_onset = node('event.audio_onset', 2850, 500, threshold=.013, band_first=0, band_last=20)
    low_envelope = node('event.envelope', 3200, 500, dict(events=low_onset), attack=.004,
                        decay=.055, sustain=.2, duration=.06, release=.18)
    mid_onset = node('event.audio_onset', 2850, 800, threshold=.010, band_first=21, band_last=41)
    mid_envelope = node('event.envelope', 3200, 800, dict(events=mid_onset), attack=.004,
                        decay=.045, sustain=.18, duration=.05, release=.15)
    high_onset = node('event.audio_onset', 2850, 1100, threshold=.009, band_first=42, band_last=62)
    high_envelope = node('event.envelope', 3200, 1100, dict(events=high_onset), attack=.002,
                         decay=.025, sustain=.14, duration=.035, release=.1)
    low_rate = node('scalar.expression', 3550, 500, dict(a=bands[0], b=low_envelope),
                    expression='.20 + a * .22 + b * .55')
    mid_rate = node('scalar.expression', 3550, 670, dict(a=bands[1], b=mid_envelope),
                    expression='.24 + a * .32 + b * .55')
    high_rate = node('scalar.expression', 3550, 840, dict(a=bands[2], b=high_envelope),
                     expression='.18 + a * .22 + b * .85')
    pigment_emission = node('scalar.expression', 3900, 500, dict(a=low_rate, b=mid_rate, c=high_rate),
                            expression='a + b + c')
    pigment_flow = node('scalar.expression', 3550, 800, dict(a=bands[1], b=mid_envelope),
                        expression='.02 + a * .13 + b * .30')
    particle_pulse = node('scalar.expression', 3900, 760,
                          dict(a=low_envelope, b=mid_envelope, c=high_envelope),
                          expression='1.0 + a * .16 + b * .14 + c * .70')
    flash = node('scalar.expression', 3900, 950,
                 dict(a=low_envelope, b=mid_envelope, c=high_envelope),
                 expression='a * .10 + b * .10 + c * .26')
    turn = node('scalar.expression', 3900, 500, dict(time=clock, a=bands[0]),
                expression='time * -18.0 + a * 10.0')
    for index, (capacity, tint, size) in enumerate((
            (18432, ((.06, .14, .22, .34), (.7, .06, .025, .72)), .0031),
            (4096, ((.9, .42, .08, .78), (1, .78, .28, 1)), .009))):
        row = 1450 + index * 650
        particles = node('gpu.particles', 4250, row,
                         dict(emission=pigment_emission, flow_strength=pigment_flow),
                         particle_capacity=capacity, seed=427 + index * 89, initial_fill=0,
                         emission_rate=560 if index == 0 else 160, lifetime=7 if index == 0 else 3.2,
                         emitter_radius=1.0, particle_speed=.02 if index == 0 else .085, drag=.16,
                         flow_frequency=6 if index == 0 else 15, flow_evolution=.14 if index == 0 else .36,
                         point_size=size, color_a=tint[0], color_b=tint[1])
        mapped = node('gpu.map', 4600, row,
                      dict(points=particles, rotation=turn, point_size_scale=particle_pulse))
        rendered = node('gpu.render', 4950, row, dict(points=mapped), point_blend=1, texture_precision=2)
        image = node('texture.composite', 5300, row, dict(a=image, b=rendered), composite_mode=1,
                     amount=.82 if index == 0 else 1, texture_precision=0)
    image = node('texture.color_adjust', 5650, 3000, dict(source=image, exposure=flash),
                 saturation=1.06, texture_precision=0)
    output = finish(graph, image, controls[2], .1)
    publish(name, ('层叠墨流', 'Stratified Ink'),
            ('连续迁移的多尺度噪声空间带动靛蓝与朱红矿物河谷，静音仍流动；颜料尘和金色闪点从空场按每秒速率连续补充。低频加强河谷能量，中频改变颜料流，高频 onset 提高金色粒子的生成速率，不使用刚体碰撞或整团爆发。',
             'Continuous multiscale lattice advection carries indigo/vermilion strata in silence. Pigment dust and gold sparks begin empty and replenish at sustained per-second rates. Bass energizes valleys, mids steer pigment and treble onsets raise gold-particle rate without rigid bodies or batch bursts.'),
            graph, output, controls, [first, height, color], compute=True, schema_version=7)

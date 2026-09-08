"""Editable image-processing graphs composed from existing renderer operators."""


def controls(graph, pace=0.1):
    node = graph.node
    clock = node('core.time', 0, 0)
    speed = node('scalar.constant', 0, 300, value=pace)
    phase = node('scalar.expression', 340, 0, dict(time=clock, a=speed), expression='time * a')
    bass = node('audio.band', 0, 600, audio_band=12)
    high = node('audio.band', 0, 900, audio_band=48)
    response = node('scalar.constant', 0, 1200, value=1)
    return phase, bass, high, speed, response


def prism_fold(graph):
    node = graph.node
    phase, bass, high, pace, response = controls(graph, 9)
    rotation = node('scalar.expression', 680, 0, dict(time=phase, a=bass, b=high, c=response),
                    expression='time + (a * 65 - b * 95) * c')
    folded = node('texture.mapping', 1020, 0, dict(rotation=rotation), sectors=7, scale=1.15)
    colored = node('texture.color_adjust', 1360, 0, dict(source=folded), saturation=1.15, exposure=-0.1)
    blur = node('texture.blur', 1700, 300, dict(source=colored), blur_radius=3)
    output = node('texture.composite', 2040, 0, dict(a=colored, b=blur), composite_mode=1, amount=0.16)
    return folded, output, [('pace', pace, 'value'), ('response', response, 'value'),
                            ('folds', folded, 'sectors'), ('scale', folded, 'scale'),
                            ('saturation', colored, 'saturation'), ('glow', output, 'amount')]


def contour_engraving(graph):
    node = graph.node
    phase, bass, high, pace, response = controls(graph, 0.035)
    shift = node('scalar.expression', 680, 0, dict(time=phase, a=bass, b=high, c=response),
                 expression='time + (a * 0.9 - b * 0.55) * c')
    plate = node('texture.color_adjust', 680, 340, saturation=0, contrast=1.2)
    lines = node('texture.contours', 1020, 0, dict(source=plate, phase=shift), contour_count=10,
                 line_width=0.08, color_a=(0.12, 0.85, 0.77, 1), color_b=(0.008, 0.03, 0.06, 1))
    softness = node('texture.blur', 1360, 300, dict(source=lines), blur_radius=2)
    output = node('texture.composite', 1700, 0, dict(a=lines, b=softness), composite_mode=1, amount=0.25)
    return plate, output, [('pace', pace, 'value'), ('response', response, 'value'),
                           ('levels', lines, 'contour_count'), ('width', lines, 'line_width'),
                           ('contrast', plate, 'contrast'), ('glow', output, 'amount')]


def motion_echo(graph):
    node = graph.node
    phase, bass, high, pace, response = controls(graph, 7)
    spin = node('scalar.expression', 680, 0, dict(time=phase, a=bass, b=high, c=response),
                expression='time + (a * 55 - b * 80) * c')
    moving = node('texture.affine', 1020, 0, dict(rotation=spin), scale=0.8)
    echoes = node('texture.trail', 1360, 0, dict(source=moving), trail_half_life=0.55,
                  trail_zoom_rate=0.13, trail_rotation_rate=-12)
    output = node('texture.color_adjust', 1700, 0, dict(source=echoes), exposure=-0.4, saturation=1.1)
    return moving, output, [('pace', pace, 'value'), ('response', response, 'value'),
                            ('scale', moving, 'scale'), ('decay', echoes, 'trail_half_life'),
                            ('expansion', echoes, 'trail_zoom_rate'),
                            ('echo_turn', echoes, 'trail_rotation_rate')]


def soft_glow(graph):
    node = graph.node
    # This filter has no independent animation clock: it preserves source motion
    # and uses the two audio bands only for the added glow envelope.
    bass = node('audio.band', 0, 0, audio_band=12)
    high = node('audio.band', 0, 300, audio_band=48)
    response = node('scalar.constant', 0, 600, value=1)
    energy = node('scalar.expression', 340, 0, dict(a=bass, b=high, c=response),
                  expression='(0.12 + a * 1.2 + b * 0.65) * c')
    image = node('texture.color_adjust', 680, 0, exposure=-0.25)
    near = node('texture.blur', 1020, 0, dict(source=image), blur_radius=3)
    wide = node('texture.blur', 1020, 360, dict(source=image), blur_radius=18)
    glow = node('texture.composite', 1360, 300, dict(a=near, b=wide), composite_mode=1, amount=0.45)
    output = node('texture.composite', 1700, 0, dict(a=image, b=glow, amount=energy), composite_mode=1)
    return image, output, [('response', response, 'value'), ('exposure', image, 'exposure'),
                           ('near_radius', near, 'blur_radius'), ('wide_radius', wide, 'blur_radius'),
                           ('spread_mix', glow, 'amount'), ('saturation', image, 'saturation')]


def polar_vortex(graph):
    node = graph.node
    phase, bass, high, pace, response = controls(graph, 0.08)
    travel = node('scalar.expression', 680, 0, dict(time=phase, a=bass, b=response),
                  expression='time + a * b * 0.7')
    curl = node('scalar.expression', 680, 350, dict(a=bass, b=high, c=response),
                expression='(a * 1.7 - b * 2.4) * c + 0.25')
    # The existing polar sampler uses mirrored addressing. A fourfold input is
    # symmetric in U, so the two angular seam samples remain equal as it twists.
    folded = node('texture.mapping', 680, 800, sectors=4, scale=1)
    tunnel = node('texture.mapping', 1020, 0, dict(source=folded, travel=travel, twist=curl),
                  mapping_mode=1, radial_power=-0.65, scale=1.1)
    blur = node('texture.blur', 1360, 350, dict(source=tunnel), blur_radius=4)
    output = node('texture.composite', 1700, 0, dict(a=tunnel, b=blur), composite_mode=1, amount=0.14)
    return folded, output, [('pace', pace, 'value'), ('response', response, 'value'),
                            ('depth', tunnel, 'radial_power'), ('scale', tunnel, 'scale'),
                            ('softness', blur, 'blur_radius'), ('glow', output, 'amount')]


def audio_iris(graph):
    node = graph.node
    phase, bass, high, pace, response = controls(graph, 11)
    opening = node('scalar.constant', 340, 1300, value=0.75)
    size = node('scalar.expression', 680, 350, dict(a=bass, b=response, c=opening),
                expression='c * (0.65 + a * b * 1.8)')
    angle = node('scalar.expression', 680, 0, dict(time=phase, a=high, b=response),
                 expression='time + a * b * 95')
    shape = node('texture.shape', 1020, 800, shape_type=3, sides=6,
                 shape_width=0.9, shape_height=0.9)
    iris = node('texture.affine', 1360, 350, dict(source=shape, scale=size, rotation=angle))
    feather = node('texture.blur', 1700, 350, dict(source=iris), blur_radius=2)
    source = node('texture.color_adjust', 1360, 0)
    output = node('texture.mask', 2040, 0, dict(source=source, mask=feather))
    return source, output, [('pace', pace, 'value'), ('response', response, 'value'),
                            ('opening', opening, 'value'), ('sides', shape, 'sides'),
                            ('feather', feather, 'blur_radius'), ('inverse', output, 'mask_mode')]


def luma_windows(graph):
    node = graph.node
    phase, bass, high, pace, response = controls(graph, 0.025)
    sweep = node('scalar.expression', 680, 0, dict(time=phase, a=bass, b=high, c=response),
                 expression='time + (a * 0.9 - b * 0.6) * c')
    source = node('texture.affine', 680, 400)
    luminance = node('texture.color_adjust', 1020, 400, dict(source=source), saturation=0)
    windows = node('texture.contours', 1360, 0, dict(source=luminance, phase=sweep),
                   contour_count=6, line_width=0.18,
                   color_a=(1, 1, 1, 1), color_b=(0, 0, 0, 0))
    feather = node('texture.blur', 1700, 350, dict(source=windows), blur_radius=1.5)
    output = node('texture.mask', 2040, 0, dict(source=source, mask=feather))
    return source, output, [('pace', pace, 'value'), ('response', response, 'value'),
                            ('levels', windows, 'contour_count'), ('width', windows, 'line_width'),
                            ('feather', feather, 'blur_radius'), ('inverse', output, 'mask_mode')]


RECIPES = [
    dict(name='prism_fold', build=prism_fold, titles=('Prism fold', '棱镜折叠'),
         descriptions=('Fold your texture into a rotating prism. Bass and high bands steer opposite turns; the preview stripes are not inserted.',
                       '把输入纹理折叠成旋转棱镜。低频与高频驱动相反方向的转动，预览条纹不会插入工程。'),
         variant_titles=('Triangular sweep', '三角扫光'),
         variant=dict(pace=-5, response=1.4, folds=3, scale=1.8, saturation=0.7, glow=0.25)),
    dict(name='contour_engraving', build=contour_engraving, titles=('Contour engraving', '轮廓刻线'),
         descriptions=('Convert source luminance into fine colored contour lines. Music moves the height slices; adjust levels and line width for your image.',
                       '将输入亮度转为细密彩色等高线。音乐移动切片位置，可根据自己的图像调整层数和线宽。'),
         variant_titles=('Broad cuts', '宽幅切片'),
         variant=dict(pace=-0.02, response=1.3, levels=6, width=0.2, contrast=0.8, glow=0.12)),
    dict(name='motion_echo', build=motion_echo, titles=('Motion echo', '运动回声'),
         descriptions=('Rotate the source through a fading, expanding image history. Bass/high bands steer motion. Restart after seeking when a deterministic history is required.',
                       '让输入画面留下逐渐衰减、扩展的旋转历史。低频与高频驱动运动；需要确定历史时，在定位后重新开始演算。'),
         variant_titles=('Tight spiral', '紧密旋迹'),
         variant=dict(pace=-11, response=0.7, scale=0.65, decay=0.9, expansion=-0.045, echo_turn=24)),
    dict(name='soft_glow', build=soft_glow, titles=('Soft glow', '柔光合成'),
         descriptions=('Add narrow and wide blurred halos to your source. Bass and high bands change their combined energy without replacing source motion.',
                       '为自己的画面叠加窄幅和宽幅模糊光晕。低频与高频改变辉光能量，保留输入原有运动。'),
         variant_titles=('Wide halo', '宽幅光晕'),
         variant=dict(response=1.5, exposure=-0.6, near_radius=6, wide_radius=30, spread_mix=0.8, saturation=0.7)),
    dict(name='polar_vortex', build=polar_vortex, titles=('Polar vortex', '极坐标旋涡'),
         fixture_rotation=32,
         descriptions=('Mirror your image before wrapping it into a traveling spiral tunnel, keeping its angular seam continuous. Bass moves the depth and highs countertwist it; depth power and source scale are editable.',
                       '先镜像折叠输入，再卷入前进的螺旋隧道，保持角度接缝连续。低频推动纵深，高频反向扭转；可编辑深度幂次、源缩放和柔光。'),
         variant_titles=('Open spiral', '展开螺旋'),
         variant=dict(pace=-0.035, response=1.5, depth=0.65, scale=2.2, softness=2, glow=0.08)),
    dict(name='audio_iris', build=audio_iris, titles=('Audio iris', '音乐光阑'),
         fixture_lines=6,
         descriptions=('Reveal your source through a rotating polygon aperture. Bass opens it and highs turn it; feather the boundary or invert the mask. The output retains transparency.',
                       '通过旋转多边形光阑显示输入图像。低频打开孔径，高频驱动转动；边缘可柔化，也可反转遮罩。输出保留透明区域。'),
         variant_titles=('Triangular cutout', '三角镂空'),
         variant=dict(pace=-7, response=1.4, opening=1, sides=3, feather=6, inverse=1)),
    dict(name='luma_windows', build=luma_windows, titles=('Luma windows', '亮度开窗'),
         descriptions=('Cut transparent windows along source luminance bands while keeping the original image colors. Bass/high bands sweep the slices; set density, width, feather and inversion.',
                       '沿输入图像亮度带开出透明窗口，并保留原图颜色。低频和高频移动切片，可调密度、宽度、柔边与反转。'),
         variant_titles=('Broad openings', '宽幅开窗'),
         variant=dict(pace=-0.02, response=1.6, levels=3, width=0.32, feather=5, inverse=1)),
]

"""Compose editable broad-band peaks from existing audio.band/expression nodes.

First-party graph composition; the existing analyzer owns FFT, normalization and
the 63 logarithmic bands spanning 20 Hz to 16 kHz. No competing DSP path.
"""


def build_peak(node, bands, x=-5200, y=0):
    bands = tuple(bands)
    if not bands or len(set(bands)) != len(bands) or any(type(band) is not int or not 0 <= band < 63 for band in bands):
        raise ValueError('Expected distinct FFT band indices within 0..62')
    values = [node('audio.band', x + (index // 4) * 300,
                   y + (index % 4) * 280, audio_band=band)
              for index, band in enumerate(bands)]
    column = x + 1900
    while len(values) > 1:
        reduced = []
        for offset in range(0, len(values), 3):
            chunk = values[offset:offset + 3]
            if len(chunk) == 1:
                reduced.append(chunk[0])
                continue
            inputs = dict(zip('abc', chunk))
            expression = next(iter(inputs))
            for name in list(inputs)[1:]:
                expression = f'max({expression}, {name})'
            reduced.append(node('scalar.expression', column, y + (offset // 3) * 220,
                                inputs, expression=expression))
        values = reduced
        column += 340
    return values[0]


def build_groups(node, x=-5200, y=0):
    # Contiguous coverage: approximately 20-255 Hz, 255-1917 Hz, 1917-16000 Hz.
    return [build_peak(node, bands, x, y + group * 2200)
            for group, bands in enumerate((range(0, 24), range(24, 43), range(43, 63)))]


def build_low_high(node, x=-5200, y=0):
    # Input-processing components explicitly label bass and treble. Their middle
    # range is intentionally omitted, not built as unreachable internal nodes.
    return [build_peak(node, bands, x, y + group * 2200)
            for group, bands in enumerate((range(0, 24), range(43, 63)))]

"""Compose editable broad-band peaks from existing audio.band/expression nodes.

First-party graph composition; the existing analyzer owns FFT, normalization and
the 63 logarithmic bands spanning 20 Hz to 16 kHz. No competing DSP path.
"""


def build_groups(node, x=-5200, y=0):
    outputs = []
    # Contiguous coverage: approximately 20-255 Hz, 255-1917 Hz, 1917-16000 Hz.
    for group, band_range in enumerate((range(0, 24), range(24, 43), range(43, 63))):
        top = y + group * 2200
        values = [node('audio.band', x + (index // 4) * 300,
                       top + (index % 4) * 280, audio_band=band)
                  for index, band in enumerate(band_range)]
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
                reduced.append(node('scalar.expression', column, top + (offset // 3) * 220,
                                    inputs, expression=expression))
            values = reduced
            column += 340
        outputs.append(values[0])
    return outputs

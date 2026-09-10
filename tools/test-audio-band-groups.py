"""Guard full, disjoint frequency coverage in authored three-band controls."""

import unittest

from audio_band_groups import build_groups


class CoverageTests(unittest.TestCase):
    def test_every_band_reaches_exactly_its_group(self):
        records = {}

        def node(kind, x, y, inputs=None, **properties):
            identity = len(records) + 1
            records[identity] = (kind, dict(inputs or {}), properties)
            return identity

        outputs = build_groups(node)

        def evaluate(identity, excited_band):
            kind, inputs, properties = records[identity]
            if kind == 'audio.band':
                return float(properties['audio_band'] == excited_band)
            self.assertEqual(kind, 'scalar.expression')
            self.assertTrue(set(inputs) <= set('abc'))
            values = {name: evaluate(source, excited_band) for name, source in inputs.items()}
            # Evaluate only this trusted first-party generated expression. Native
            # package compilation independently checks the real expression parser.
            return eval(properties['expression'], {'__builtins__': {}, 'max': max}, values)

        self.assertEqual(len(outputs), 3)
        for band in range(63):
            expected = 0 if band < 24 else 1 if band < 43 else 2
            self.assertEqual([evaluate(output, band) for output in outputs],
                             [float(index == expected) for index in range(3)])
        self.assertEqual([evaluate(output, -1) for output in outputs], [0, 0, 0])


if __name__ == '__main__':
    unittest.main()

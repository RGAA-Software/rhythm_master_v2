"""Guard full, disjoint frequency coverage in authored three-band controls."""

import unittest

from audio_band_groups import build_groups, build_low_high, build_peak


class CoverageTests(unittest.TestCase):
    def check_coverage(self, build, ranges):
        records = {}

        def node(kind, x, y, inputs=None, **properties):
            identity = len(records) + 1
            records[identity] = (kind, dict(inputs or {}), properties)
            return identity

        outputs = build(node)

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

        self.assertEqual(len(outputs), len(ranges))
        for band in range(63):
            self.assertEqual([evaluate(output, band) for output in outputs],
                             [float(band in group) for group in ranges])
        self.assertEqual([evaluate(output, -1) for output in outputs], [0] * len(ranges))

    def test_every_band_reaches_exactly_its_group(self):
        self.check_coverage(build_groups, (range(24), range(24, 43), range(43, 63)))

    def test_low_high_components_preserve_frequency_roles(self):
        self.check_coverage(build_low_high, (range(24), range(43, 63)))

    def test_invalid_indices_reject_before_graph_mutation(self):
        def unexpected(*args, **kwargs):
            self.fail('Invalid input mutated the authored graph')
        for bands in ((), (-1,), (63,), (0, 0), (1.5,), (True,)):
            with self.subTest(bands=bands), self.assertRaises(ValueError):
                build_peak(unexpected, bands)


if __name__ == '__main__':
    unittest.main()

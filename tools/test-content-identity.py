"""Regression: a new APK must reject a valid but stale precompiled effect."""

from pathlib import Path
import tempfile
import unittest

import content_identity


class ContentIdentityTests(unittest.TestCase):
    def test_stale_source_compiled_and_package(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / 'source'
            compiled = root / 'compiled'
            source.mkdir()
            compiled.mkdir()
            for name in ('manifest.json', 'editor.json'):
                (source / name).write_text('{}', encoding='utf-8')
                (compiled / name).write_text('{}', encoding='utf-8')
            (source / 'graph.textproto').write_text('old graph', encoding='utf-8')
            (compiled / 'graph.pb').write_bytes(b'compiled old graph')
            package = root / 'effect.rhythmpack'
            package.write_bytes(b'old package')
            content_identity.write_compiled_identity(source, compiled)
            content_identity.write_package_identity(compiled, package)
            expected = content_identity.verify_package_source(source, package)
            self.assertEqual(expected, content_identity.authoring_digest(source))
            (source / 'graph.textproto').write_text('new graph', encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'Stale runtime content'):
                content_identity.verify_package_source(source, package)
            (compiled / 'graph.pb').write_bytes(b'new compiled graph')
            with self.assertRaisesRegex(ValueError, 'Compiled content identity mismatch'):
                content_identity.write_package_identity(compiled, package)
            content_identity.write_compiled_identity(source, compiled)
            package.write_bytes(b'new package')
            content_identity.write_package_identity(compiled, package)
            content_identity.verify_package_source(source, package)
            package.write_bytes(b'changed after publication')
            with self.assertRaisesRegex(ValueError, 'Stale runtime content'):
                content_identity.verify_package_source(source, package)


if __name__ == '__main__':
    unittest.main()

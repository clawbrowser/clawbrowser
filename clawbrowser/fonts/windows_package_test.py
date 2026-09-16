import json
from pathlib import PureWindowsPath
import unittest

import build_catalog_test
from windows_package import installer_text, specification, stage, validated_payload


class WindowsPackageTests(unittest.TestCase):
    run_build = build_catalog_test.BuildCatalogTests.run_build

    def setUp(self):
        build_catalog_test.BuildCatalogTests.setUp(self)
        self.output = self.root / 'build'
        self.destination = self.output / 'clawbrowser-fonts/test-catalog'
        self.package = self.root / 'portable'

    def test_installer_paths_are_versioned_exact_and_idempotent(self):
        spec = specification(self.manifest)
        source = '[GENERAL]\r\nchrome.dll: %(VersionDir)s\\\r\n[HIDPI]\r\nx.pak: %(VersionDir)s\\\r\n'
        result = installer_text(source, spec)
        self.assertEqual(result, installer_text(result, spec))
        self.assertIn('[HIDPI]\nx.pak:', result)
        lines = [line for line in result.splitlines() if line.startswith('clawbrowser-fonts')]
        self.assertEqual(len(lines), 7)
        for line in lines:
            source_path, destination = line.split(': ', 1)
            self.assertNotIn('*', source_path)
            self.assertEqual(destination,
                             '%(VersionDir)s\\' + str(PureWindowsPath(source_path).parent) + '\\')
        self.assertIn('fonts\\Example.ttf:', result)
        self.assertIn('NOTO-LICENSE:', result)

    def test_invalid_installer_sections_and_conflicts_rejected(self):
        spec = specification(self.manifest)
        for source in ('[HIDPI]\n', '[GENERAL]\n[GENERAL]\n',
                       '[GENERAL]\n# BEGIN CLAWBROWSER FONT CATALOG\n',
                       '[GENERAL]\nclawbrowser-fonts\\unknown: dest\n'):
            with self.subTest(source=source), self.assertRaises(ValueError):
                installer_text(source, spec)

    def test_windows_case_collisions_and_path_traversal_rejected(self):
        original = self.manifest.read_text()
        for name in ('../escape.ttf', 'Example.ttf', 'EXAMPLE.ttf', 'C:escape.ttf'):
            spec = json.loads(original)
            spec['fonts'][1]['file'] = name
            self.manifest.write_text(json.dumps(spec))
            with self.subTest(name=name), self.assertRaises(ValueError):
                specification(self.manifest)

    def test_portable_bytes_and_reuse(self):
        self.run_build()
        stage(self.manifest, self.root, self.output, self.package)
        stage(self.manifest, self.root, self.output, self.package)
        _, expected = validated_payload(self.manifest, self.root, self.destination)
        _, actual = validated_payload(self.manifest, self.root,
                                      self.package / 'clawbrowser-fonts/test-catalog')
        self.assertEqual(expected, actual)

    def test_corrupt_source_rejected_before_copy(self):
        self.run_build()
        (self.destination / 'fonts/Example.ttf').write_bytes(b'corrupt')
        with self.assertRaisesRegex(ValueError, 'checksum'):
            stage(self.manifest, self.root, self.output, self.package)
        self.assertFalse(self.package.exists())

    def test_conflicting_destination_is_not_overwritten(self):
        self.run_build()
        target = self.package / 'clawbrowser-fonts/test-catalog/fonts/Example.ttf'
        target.parent.mkdir(parents=True)
        target.write_bytes(b'leave me untouched')
        with self.assertRaisesRegex(ValueError, 'conflicting'):
            stage(self.manifest, self.root, self.output, self.package)
        self.assertEqual(target.read_bytes(), b'leave me untouched')

    def test_extra_asset_and_wrong_notice_rejected(self):
        self.run_build()
        (self.destination / 'fonts/Host.ttf').write_bytes(b'host')
        with self.assertRaisesRegex(ValueError, 'unexpected'):
            stage(self.manifest, self.root, self.output, self.package)
        (self.destination / 'fonts/Host.ttf').unlink()
        (self.destination / 'NOTO-LICENSE').write_bytes(b'wrong')
        with self.assertRaisesRegex(ValueError, 'notice'):
            stage(self.manifest, self.root, self.output, self.package)


if __name__ == '__main__':
    unittest.main()

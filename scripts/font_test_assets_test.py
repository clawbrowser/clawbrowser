import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'clawbrowser/test/integration'))
from font_test_assets import bundled_font_asset


class FontAssetsTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()

    def font(self, root):
        path = root / 'clawbrowser-fonts/catalog/fonts/Test.ttf'
        path.parent.mkdir(parents=True)
        path.write_bytes(b'fixture')
        return path

    def resolve(self, binary, platform, resource=None):
        return bundled_font_asset(binary, platform, 'catalog', 'Test.ttf', resource)

    def test_linux_and_windows_portable(self):
        path = self.font(self.root)
        (self.root / 'chrome.dll').write_bytes(b'fixture module')
        for platform, name in [('linux', 'clawbrowser'), ('win32', 'clawbrowser.exe')]:
            self.assertEqual(self.resolve(self.root / name, platform), path)

    def test_macos_framework_layout(self):
        contents = self.root / 'Clawbrowser.app/Contents'
        path = self.font(contents / 'Frameworks/Clawbrowser Framework.framework/Versions/A/Resources')
        self.assertEqual(self.resolve(contents / 'MacOS/Clawbrowser', 'darwin'), path)

    def test_single_windows_installed_version(self):
        path = self.font(self.root / '151.0.7922.109')
        (self.root / '151.0.7922.109/chrome.dll').write_bytes(b'fixture module')
        self.assertEqual(self.resolve(self.root / 'clawbrowser.exe', 'win32'), path)

    def test_multiple_windows_versions_require_explicit_module(self):
        self.font(self.root / 'old')
        module = self.root / 'new'
        expected = self.font(module)
        with self.assertRaisesRegex(ValueError, 'Expected one'):
            self.resolve(self.root / 'clawbrowser.exe', 'win32')
        with self.assertRaisesRegex(ValueError, 'chrome.dll'):
            self.resolve(self.root / 'clawbrowser.exe', 'win32', module)
        (module / 'chrome.dll').write_bytes(b'fixture module')
        self.assertEqual(self.resolve(self.root / 'clawbrowser.exe', 'win32', module), expected)

    def test_unrelated_fonts_do_not_satisfy_catalog(self):
        (self.root / 'Test.ttf').write_bytes(b'unrelated')
        with self.assertRaisesRegex(ValueError, 'Expected one'):
            self.resolve(self.root / 'clawbrowser', 'linux')

    def test_outside_installation_rejected(self):
        with self.assertRaisesRegex(ValueError, 'outside'):
            self.resolve(self.root / 'browser/clawbrowser.exe', 'win32', self.root)


if __name__ == '__main__':
    unittest.main()

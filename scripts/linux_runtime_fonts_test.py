"""Exercise the actual release copy function without dispatching a build."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / 'scripts/build_remote_prod_artifacts.sh').read_text()
START = SOURCE.index('\ncopy_linux_runtime() {\n') + 1
COPY_FUNCTION = SOURCE[START:SOURCE.index('\n}\n', START) + 3]


class LinuxRuntimeFontsTest(unittest.TestCase):
    def copy(self, catalog='current', active=True):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        manifest = root / 'repo/clawbrowser/fonts/catalog_build.json'
        manifest.parent.mkdir(parents=True)
        manifest.write_text(json.dumps({'catalog_id': catalog}))
        build = root / 'build'
        for name in (['current', 'obsolete'] if active else ['obsolete']):
            directory = build / 'clawbrowser-fonts' / name
            directory.mkdir(parents=True)
            (directory / 'manifest.json').write_text('{}')
            (directory / 'font.ttf').write_bytes(b'test font payload')
        (build / 'chrome').write_text('binary fixture')
        stage = root / 'stage'
        result = subprocess.run(['bash', '-c',
            'set -euo pipefail\n'
            'die() { echo "$*" >&2; exit 1; }\n' + COPY_FUNCTION +
            '\nrepo_dir="$1"; copy_linux_runtime "$2" "$3"',
            'test', str(root / 'repo'), str(build), str(stage)],
            capture_output=True, text=True)
        return result, stage

    def test_only_active_catalog_is_copied(self):
        result, stage = self.copy()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue((stage / 'chrome').is_file())
        self.assertTrue((stage / 'clawbrowser-fonts/current/font.ttf').is_file())
        self.assertFalse((stage / 'clawbrowser-fonts/obsolete').exists())

    def test_missing_active_catalog_fails(self):
        result, _ = self.copy(active=False)
        self.assertNotEqual(result.returncode, 0)

    def test_unsafe_catalog_id_fails(self):
        result, _ = self.copy(catalog='../current')
        self.assertNotEqual(result.returncode, 0)


if __name__ == '__main__':
    unittest.main()

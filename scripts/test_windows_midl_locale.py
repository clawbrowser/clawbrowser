"""Regress actual Russian-host MIDL output against pinned Chromium bytes."""
import ast
import base64
import json
from pathlib import Path
import struct
import unittest

ROOT = Path(__file__).resolve().parents[1]
PATCH = ROOT / 'clawbrowser/patches/065-windows-stable-typelib-name-hash.patch'
lines = PATCH.read_text().splitlines()
start = next(i for i, line in enumerate(lines) if line.startswith('+def normalize_typelib_library_name('))
body = []
for line in lines[start:]:
    if not line.startswith('+'):
        break
    body.append(line[1:])
namespace = {'struct': struct}
exec(compile(ast.parse('\n'.join(body)), str(PATCH), 'exec'), namespace)
normalize = namespace['normalize_typelib_library_name']
fixture = json.loads((ROOT / 'scripts/fixtures/windows_midl_typelib.json').read_text())
BASELINE = base64.b64decode(fixture['baseline'])
GENERATED = base64.b64decode(fixture['generated'])

def recorded_windows_hash(kind, lcid, name):
    # Actual LHashValOfNameSysA results recorded on the affected Windows host.
    if kind != 3 or name != b'SystemTraceSessionLib':
        raise AssertionError('Unexpected fixture library')
    return {0x409: 0x10dbe5, 0x419: 0x30dc42}[lcid]

class LocaleTests(unittest.TestCase):
    def test_actual_russian_output_matches_entire_pinned_binary(self):
        self.assertNotEqual(GENERATED, BASELINE)
        self.assertEqual(normalize(GENERATED, recorded_windows_hash, 0x419), BASELINE)

    def test_canonical_output_is_unchanged(self):
        self.assertEqual(normalize(BASELINE, recorded_windows_hash, 0x419), BASELINE)

    def test_guid_changes_are_preserved_for_strict_comparison_to_reject(self):
        changed = bytearray(GENERATED)
        types = struct.unpack_from('<I', changed, 0x20)[0]
        guid_offset = struct.unpack_from('<I', changed, 0x54 + types * 4 + 5 * 16)[0]
        changed[guid_offset] ^= 1
        result = normalize(bytes(changed), recorded_windows_hash, 0x419)
        self.assertNotEqual(result, BASELINE)
        self.assertEqual(result[guid_offset], changed[guid_offset])

    def test_unexplained_hash_is_rejected(self):
        changed = bytearray(GENERATED)
        types = struct.unpack_from('<I', changed, 0x20)[0]
        names = struct.unpack_from('<I', changed, 0x54 + types * 4 + 7 * 16)[0]
        changed[names + 10] ^= 1
        with self.assertRaisesRegex(ValueError, 'not explained'):
            normalize(bytes(changed), recorded_windows_hash, 0x419)

    def test_corrupt_index_is_rejected(self):
        changed = bytearray(GENERATED)
        types = struct.unpack_from('<I', changed, 0x20)[0]
        index = struct.unpack_from('<I', changed, 0x54 + types * 4 + 6 * 16)[0]
        changed[index] ^= 1
        with self.assertRaisesRegex(ValueError, 'ordering'):
            normalize(bytes(changed), recorded_windows_hash, 0x419)

if __name__ == '__main__':
    unittest.main()

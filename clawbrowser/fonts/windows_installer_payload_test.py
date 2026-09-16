import ctypes
from ctypes import wintypes
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

from windows_installer_payload import (catalog_members, copy_packed_resource,
                                       parse_entries, verify_packed)


def seven_zip():
    return (shutil.which('7z') or shutil.which('7zz') or shutil.which('7za')
            or (r'C:\Program Files\7-Zip\7z.exe' if os.name == 'nt' else None))


class PayloadTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.prefix = 'Chrome-bin/151.0.7922.109/clawbrowser-fonts/test-catalog/'
        self.payload = {'manifest.json': b'manifest', 'fonts/Example.ttf': b'font fixture'}
        self.entries = {self.prefix + n: len(b) for n, b in self.payload.items()}
        self.entries.update({'Chrome-bin/151.0.7922.109/chrome.dll': 3,
                             'Chrome-bin/clawbrowser.exe': 3})

    def test_exact_versioned_catalog(self):
        version, members = catalog_members(self.entries, 'test-catalog', self.payload)
        self.assertEqual(version, '151.0.7922.109')
        self.assertEqual(members, {self.prefix + n: b for n, b in self.payload.items()})

    def test_missing_extra_wrong_size_and_wrong_layout(self):
        variants = [dict(self.entries) for _ in range(6)]
        del variants[0][self.prefix + 'manifest.json']
        variants[1][self.prefix + 'fonts/Host.ttf'] = 2
        variants[2][self.prefix + 'fonts/Example.ttf'] += 1
        del variants[3]['Chrome-bin/151.0.7922.109/chrome.dll']
        variants[4]['Chrome-bin/150.0.1.1/clawbrowser-fonts/test-catalog/manifest.json'] = 8
        variants[5]['Chrome-bin/clawbrowser-fonts/test-catalog/manifest.json'] = 8
        for entries in variants:
            with self.subTest(entries=entries), self.assertRaises(ValueError):
                catalog_members(entries, 'test-catalog', self.payload)

    def test_listing_rejects_ambiguous_paths_links_and_encryption(self):
        valid = 'Path = Chrome-bin/a\nSize = 4\nFolder = -\n\n'
        self.assertEqual(parse_entries(valid), {'Chrome-bin/a': 4})
        self.assertEqual(parse_entries('Path = Chrome-bin\nSize = 0\nAttributes = D drwxr-xr-x\n'), {})
        bad = [valid + valid.replace('/a', '/A'),
               valid + 'Symbolic Link = destination\n',
               valid + 'Hard Link = destination\n',
               valid + 'Encrypted = +\n',
               valid.replace('Size = 4', 'Size = nope')]
        bad.append(valid.rstrip() + '\nAttributes = A lrwxrwxrwx\n')
        for name in ('../a', '/a', 'C:a', 'a/./b', 'a//b', 'a*', 'a?'):
            bad.append(valid.replace('Chrome-bin/a', name))
        # Link/encryption fields belong to the same member record.
        bad[1:4] = [v.replace('\n\n', '\n') for v in bad[1:4]]
        for listing in bad:
            with self.subTest(listing=listing), self.assertRaises(ValueError):
                parse_entries(listing)

    def make_packed(self, corrupt=False, extra=False):
        tool = seven_zip()
        if not tool:
            if os.environ.get('CLAWBROWSER_REQUIRE_7Z_TESTS') == '1':
                self.fail('required 7z executable unavailable')
            self.skipTest('requires 7z')
        tree = self.root / 'tree'
        for name, size in self.entries.items():
            path = tree / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(self.payload.get(name.removeprefix(self.prefix), b'x' * size))
        if corrupt:
            (tree / (self.prefix + 'fonts/Example.ttf')).write_bytes(b'bad! fixture')
        if extra:
            (tree / (self.prefix + 'fonts/Host.ttf')).write_bytes(b'host')
        inner = self.root / 'chrome.7z'
        packed = self.root / 'chrome.packed.7z'
        subprocess.run([tool, 'a', '-t7z', '-mx0', str(inner), 'Chrome-bin'],
                       cwd=tree, check=True, capture_output=True)
        subprocess.run([tool, 'a', '-t7z', str(packed), 'chrome.7z'],
                       cwd=self.root, check=True, capture_output=True)
        scratch = self.root / 'verification'
        scratch.mkdir()
        return tool, packed, scratch

    def test_real_nested_archive(self):
        tool, packed, scratch = self.make_packed()
        report = verify_packed(tool, packed, 'test-catalog', self.payload, scratch)
        self.assertEqual(report['verified_files'], 2)

    def test_real_nested_archive_wrong_font_bytes(self):
        tool, packed, scratch = self.make_packed(corrupt=True)
        with self.assertRaisesRegex(ValueError, 'checksum'):
            verify_packed(tool, packed, 'test-catalog', self.payload, scratch)

    def test_real_nested_archive_extra_font(self):
        tool, packed, scratch = self.make_packed(extra=True)
        with self.assertRaisesRegex(ValueError, 'unexpected'):
            verify_packed(tool, packed, 'test-catalog', self.payload, scratch)

    @unittest.skipUnless(os.name == 'nt', 'requires Windows resource API')
    def test_real_pe_resource_and_nested_archive_without_execution(self):
        tool, packed, scratch = self.make_packed()
        # Test-owned PE copy, never executed. Resource editing does not affect
        # the Python installation or any browser/installer artifact.
        fixture = self.root / 'fixture.exe'
        shutil.copyfile(sys.executable, fixture)
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.BeginUpdateResourceW.argtypes = [wintypes.LPCWSTR, wintypes.BOOL]
        kernel.BeginUpdateResourceW.restype = wintypes.HANDLE
        kernel.UpdateResourceW.argtypes = [wintypes.HANDLE, wintypes.LPCWSTR,
                                           wintypes.LPCWSTR, wintypes.WORD,
                                           ctypes.c_void_p, wintypes.DWORD]
        kernel.UpdateResourceW.restype = wintypes.BOOL
        kernel.EndUpdateResourceW.argtypes = [wintypes.HANDLE, wintypes.BOOL]
        kernel.EndUpdateResourceW.restype = wintypes.BOOL
        update = kernel.BeginUpdateResourceW(str(fixture), False)
        self.assertTrue(update)
        data = packed.read_bytes()
        success = kernel.UpdateResourceW(update, 'B7', 'CHROME.PACKED.7Z', 0,
                                         ctypes.c_char_p(data), len(data))
        self.assertTrue(kernel.EndUpdateResourceW(update, not success))
        self.assertTrue(success)
        extracted = scratch / 'embedded.7z'
        copy_packed_resource(fixture, extracted)
        self.assertEqual(extracted.read_bytes(), data)
        self.assertEqual(verify_packed(tool, extracted, 'test-catalog',
                                      self.payload, scratch)['verified_files'], 2)

    @unittest.skipUnless(os.name == 'nt', 'requires Windows resource API')
    def test_pe_without_catalog_resource_rejected(self):
        with self.assertRaisesRegex(ValueError, 'missing full B7'):
            copy_packed_resource(Path(sys.executable), self.root / 'missing.7z')


if __name__ == '__main__':
    unittest.main()

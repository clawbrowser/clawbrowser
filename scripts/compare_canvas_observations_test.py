"""Reject misleading cross-platform evidence, without running a browser."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

from compare_canvas_observations import observations


class EvidenceTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)

    def report(self, filename, hashes=('aaa',), status=None, mode='override'):
        root = ET.Element('testsuite')
        case = ET.SubElement(root, 'testcase', name=f'test_pixels[{mode}]')
        if status:
            ET.SubElement(case, status)
        props = ET.SubElement(case, 'properties')
        ET.SubElement(props, 'property', name='cross_context_control', value=json.dumps({
            'observations': [{'args': {'seed': 1}, 'hashes': {'window': h}} for h in hashes]
        }))
        path = Path(self.temp.name) / filename
        ET.ElementTree(root).write(path)
        return path

    def compare(self, left, right):
        result = subprocess.run([sys.executable, str(Path(__file__).with_name(
            'compare_canvas_observations.py')), str(left), str(right)],
            capture_output=True, text=True)
        return result.returncode, json.loads(result.stdout)

    def test_duplicate_identical_observations_are_valid(self):
        self.assertEqual(len(observations(self.report('a.xml', ('aaa', 'aaa')))), 1)

    def test_identical_variable_sets_cannot_pass(self):
        code, result = self.compare(self.report('a.xml', ('aaa', 'bbb')),
                                    self.report('b.xml', ('aaa', 'bbb')))
        self.assertEqual(code, 2)
        self.assertIn('Non-deterministic', result['error'])

    def test_failed_or_errored_cases_are_not_evidence(self):
        for status in ('failure', 'error'):
            with self.subTest(status=status), self.assertRaises(ValueError):
                observations(self.report('bad.xml', status=status))

    def test_skipped_and_native_cases_are_not_evidence(self):
        self.assertFalse(observations(self.report('skip.xml', status='skipped')))
        self.assertFalse(observations(self.report('native.xml', mode='native')))

    def test_match_and_difference_exit_codes(self):
        left = self.report('a.xml')
        self.assertEqual(self.compare(left, self.report('b.xml'))[0], 0)
        self.assertEqual(self.compare(left, self.report('c.xml', ('bbb',)))[0], 1)

    def test_no_comparable_evidence_is_not_a_pass(self):
        self.assertEqual(self.compare(self.report('a.xml'),
                                    self.report('b.xml', status='skipped'))[0], 2)

    def test_blend_matrix_retains_cpu_and_context_identity(self):
        path = self.report('blend.xml')
        tree = ET.parse(path)
        prop = tree.find('./testcase/properties/property')
        prop.set('name', 'blend_matrix')
        prop.set('value', json.dumps({'observations': [
            {'operation': 'overlay', 'args': args, 'kind': kind, 'hash': 'aaa'}
            for args in ([], ['--disable-skia-runtime-opts'])
            for kind in ('dom', 'offscreen')
        ]}))
        tree.write(path)
        self.assertEqual(len(observations(path)), 4)
        self.assertEqual(self.compare(path, path)[1]['comparable'], 4)


if __name__ == '__main__':
    unittest.main()

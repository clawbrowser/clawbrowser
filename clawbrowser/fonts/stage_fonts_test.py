import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET

spec=importlib.util.spec_from_file_location('stage_fonts',Path(__file__).with_name('stage_fonts.py'))
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class StageTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        self.source=self.root/'third_party/test_fonts/test_fonts'
        self.source.mkdir(parents=True)
        (self.source/'Example.ttf').write_bytes(b'font-test-fixture')
        (self.source.parent/'LICENSE').write_text('fixture notice')
        self.manifest={'release_ready':False,'chromium_revision':'test',
            'catalog_id':'test','source_directory':'third_party/test_fonts/test_fonts',
            'fonts':[{'file':'Example.ttf','sha256':hashlib.sha256(b'font-test-fixture').hexdigest()}],
            'generics':{'serif':'Example'},'fallback':['Example']}
        self.manifest_path=self.root/'manifest.json'
        self.output=self.root/'output'

    def run_stage(self):
        self.manifest_path.write_text(json.dumps(self.manifest))
        with patch.object(module.subprocess,'check_output',return_value='test\n'):
            return module.stage(self.manifest_path,self.root,self.output)

    def test_stage_and_xml(self):
        self.assertEqual(self.run_stage()['fonts'],1)
        self.assertTrue((self.output/'STAGED').is_file())
        digest=hashlib.sha256((self.output/'manifest.json').read_bytes()).hexdigest()
        self.assertEqual((self.output/'STAGED').read_text().splitlines()[1],digest)
        root=ET.parse(self.output/'fonts.conf').getroot()
        self.assertIsNotNone(root.find('reset-dirs'))
        self.assertIsNone(root.find('include'))
        self.assertEqual(root.find('alias').attrib['binding'],'strong')

    def test_corrupt(self):
        self.manifest['fonts'][0]['sha256']='0'*64
        with self.assertRaisesRegex(ValueError,'checksum'):
            self.run_stage()
        self.assertFalse(self.output.exists())

    def test_missing(self):
        self.manifest['fonts'][0]['file']='Missing.ttf'
        with self.assertRaises(FileNotFoundError): self.run_stage()
        self.assertFalse(self.output.exists())

    def test_duplicate(self):
        self.manifest['fonts']*=2
        with self.assertRaisesRegex(ValueError,'duplicate'): self.run_stage()
        self.assertFalse(self.output.exists())

    def test_existing_output_untouched(self):
        self.output.mkdir()
        (self.output/'sentinel').write_text('preserve')
        with self.assertRaises(FileExistsError): self.run_stage()
        self.assertEqual((self.output/'sentinel').read_text(),'preserve')

if __name__=='__main__': unittest.main()

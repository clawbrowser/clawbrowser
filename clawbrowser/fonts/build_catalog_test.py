"""GN adapter integrity checks with synthetic, non-renderable font assets."""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from build_catalog import build


class BuildCatalogTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        source = self.root / "third_party/test_fonts/test_fonts"
        source.mkdir(parents=True)
        (source.parent / "LICENSE").write_text("upstream fixture notice")
        vendor = self.root / "font-vendor-prototype"
        vendor.mkdir()
        (vendor / "NOTO-LICENSE").write_text("vendor fixture notice")
        entries = []
        for directory, name, kind in [(source, "Example.ttf", "chromium"), (vendor, "Vendor.ttf", "vendor")]:
            data = name.encode()
            (directory / name).write_bytes(data)
            entries.append({"file": name, "source": kind, "sha256": hashlib.sha256(data).hexdigest()})
        self.manifest = self.root / "catalog.json"
        self.manifest.write_text(json.dumps({
            "catalog_id": "test-catalog", "release_ready": False,
            "chromium_revision": "test", "source_directory": "third_party/test_fonts/test_fonts",
            "fonts": entries, "generics": {"serif": "Example"}, "fallback": ["Example"],
        }))
        self.metadata = self.root / "catalog_build.json"
        self.metadata.write_text(json.dumps({"catalog_id": "test-catalog",
            "chromium_files": ["Example.ttf"], "vendor_files": ["Vendor.ttf"]}))
        self.destination = self.root / "output"
        self.header = self.root / "generated.h"

    def run_build(self):
        with patch("subprocess.check_output", return_value="test\n"):
            build(self.manifest, self.root, self.destination, self.header)

    def test_valid_reuse_preserves_generated_header(self):
        self.run_build()
        before = self.header.stat().st_mtime_ns
        self.run_build()
        self.assertEqual(self.header.stat().st_mtime_ns, before)
        self.assertIn("test-catalog", self.header.read_text())

    def test_metadata_mismatch_fails_before_staging(self):
        self.metadata.write_text("{}")
        with self.assertRaisesRegex(ValueError, "GN catalog metadata"):
            self.run_build()
        self.assertFalse(self.destination.exists())

    def test_corrupt_font_rejected_on_reuse(self):
        self.run_build()
        (self.destination / "fonts/Example.ttf").write_bytes(b"corrupt")
        with self.assertRaisesRegex(ValueError, "corrupt staged font"):
            self.run_build()

    def test_extra_font_rejected_on_reuse(self):
        self.run_build()
        (self.destination / "fonts/Host.ttf").write_bytes(b"unexpected")
        with self.assertRaisesRegex(ValueError, "unexpected catalog files"):
            self.run_build()

    def test_modified_notice_rejected(self):
        self.run_build()
        (self.destination / "NOTO-LICENSE").write_text("modified")
        with self.assertRaisesRegex(ValueError, "font notice"):
            self.run_build()

    def test_incomplete_marker_rejected(self):
        self.run_build()
        (self.destination / "STAGED").write_text("interrupted")
        with self.assertRaisesRegex(ValueError, "staging marker"):
            self.run_build()

    def test_changed_manifest_requires_new_catalog(self):
        self.run_build()
        value = json.loads(self.manifest.read_text())
        value["fallback"] = ["Vendor"]
        self.manifest.write_text(json.dumps(value))
        with self.assertRaisesRegex(ValueError, "bump catalog_id"):
            self.run_build()


if __name__ == "__main__":
    unittest.main()

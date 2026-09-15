"""Actual macOS fallback provenance; helper-only tests cannot satisfy this gate."""
import html
import json
import os
from pathlib import Path
import sys

import pytest

from conftest import _launch_browser_with_details
from test_complex_font_fallback import platform_fonts

pytestmark = pytest.mark.skipif(sys.platform != "darwin", reason="macOS FontCache")


@pytest.mark.asyncio
async def test_managed_macos_fallback_uses_only_catalog(record_property):
    samples = [
        ("Latin 0123", "Arimo"),
        ("مرحبا بالعالم", "DejaVu Sans"),
        ("漢字", "Noto Sans CJK JP"),
        ("नमस्ते", "Lohit Devanagari"),
        ("👩\u200d💻", "Noto Color Emoji"),
    ]
    observations = []
    async with _launch_browser_with_details(
        fixture_name="valid_fingerprint.json", backend_mode="mock",
        skip_verify=True, headless=False,
    ) as launch:
        page = launch["page"]
        await page.set_content(
            '<meta charset="utf-8"><style>p{font:34px sans-serif}</style>' +
            "".join(f'<p id="s{i}">{html.escape(text)}</p>'
                    for i, (text, _) in enumerate(samples)))
        await page.evaluate("document.fonts.ready")
        for i, (text, expected) in enumerate(samples):
            fonts = await platform_fonts(page, f"#s{i}")
            observations.append({"text": text, "fonts": fonts})
            assert any(f["familyName"] == expected and f["glyphCount"] > 0
                       for f in fonts), observations[-1]
            assert all(f["familyName"] in {expected, "Arimo"}
                       for f in fonts), observations[-1]
            if expected == "Noto Color Emoji":
                assert sum(f["glyphCount"] for f in fonts
                           if f["familyName"] == expected) == 1, fonts
        evidence = os.environ.get("CLAWBROWSER_TEST_EVIDENCE_DIR")
        if evidence:
            destination = Path(evidence).resolve()
            destination.mkdir(parents=True, exist_ok=True)
            await page.screenshot(path=str(destination / "macos-catalog-samples.png"),
                                  full_page=True)
    record_property("macos_catalog_provenance", json.dumps(observations))

"""Actual macOS fallback provenance; helper-only tests cannot satisfy this gate."""
import html
import hashlib
import json
import os
from pathlib import Path
import sys

import pytest

from conftest import _launch_browser_with_details
from test_complex_font_fallback import platform_fonts

pytestmark = pytest.mark.skipif(sys.platform != "darwin", reason="macOS FontCache")


@pytest.mark.asyncio
@pytest.mark.parametrize("family", ["Noto Sans CJK " + region
                                    for region in ("JP", "KR", "SC", "TC", "HK")])
async def test_catalog_variable_weight_matches_explicit_axis(record_property, family):
    # A variable font's PostScript label can retain "Thin" even after axis
    # selection. Compare actual raster output, not the label or computed CSS.
    observations = []
    async with _launch_browser_with_details(
        fixture_name="valid_fingerprint.json", backend_mode="mock",
        skip_verify=True, headless=False,
    ) as launch:
        page = launch["page"]
        await page.set_content('<meta charset="utf-8"><style>'
            '#probe{width:500px;height:100px;background:white;color:black;'
            'font-family:"Noto Sans CJK SC";font-size:56px;line-height:100px;'
            'font-synthesis:none}</style><div id="probe">漢字中文</div>')
        probe = page.locator("#probe")
        await probe.evaluate("(element, family) => element.style.fontFamily = family", family)
        for weight in (100, 400, 900):
            hashes = []
            for explicit_axis in (False, True, False):
                await probe.evaluate("""(element, args) => {
                    element.style.fontWeight = args.explicit ? '400' : String(args.weight);
                    element.style.fontVariationSettings = args.explicit
                        ? '"wght" ' + args.weight : 'normal';
                }""", {"weight": weight, "explicit": explicit_axis})
                await page.evaluate("document.fonts.ready")
                fonts = await platform_fonts(page, "#probe")
                assert fonts and all(f["familyName"] == family
                                     and f["glyphCount"] > 0 for f in fonts), fonts
                hashes.append(hashlib.sha256(await probe.screenshot()).hexdigest())
            observations.append({"weight": weight, "hashes": hashes})
            assert len(set(hashes)) == 1, observations
        assert len({row["hashes"][0] for row in observations}) == 3, observations
    record_property("macos_variable_weight_raster", json.dumps({
        "family": family, "observations": observations}))


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

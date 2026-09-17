"""Explicitly blocked families must not change complex-text fallback.

This checks lookup isolation, not independence from the OS fallback catalog.
Platform font provenance is recorded for later cross-platform analysis.
"""
import html
import json
import sys

import pytest

from conftest import _launch_browser_with_details
from test_complex_font_fallback import platform_fonts


@pytest.mark.asyncio
async def test_blocked_family_preserves_complex_fallback(record_property):
    samples = ["Latin 0123", "مرحبا بالعالم", "漢字", "नमस्ते", "👩\u200d💻"]
    blocked = ["Papyrus", "Copperplate", "ClawbrowserDefinitelyMissingFont"]
    observations = []
    async with _launch_browser_with_details(
        fixture_name="valid_fingerprint.json", backend_mode="mock", skip_verify=True,
        headless=False,
    ) as launch:
        page = launch["page"]
        for text in samples:
            for family in blocked:
                await page.set_content(
                    '<meta charset="utf-8"><style>span{font:32px sans-serif;'
                    'display:inline-block;white-space:pre}</style>'
                    f'<span id="baseline">{html.escape(text)}</span>'
                    f'<span id="probe" style="font-family:&quot;{family}&quot;,sans-serif">'
                    f'{html.escape(text)}</span>'
                )
                await page.evaluate("document.fonts.ready")
                fonts = {}
                for selector in ("#baseline", "#probe"):
                    fonts[selector] = sorted(
                        (f["familyName"], f.get("postScriptName", ""),
                         f["glyphCount"], f["isCustomFont"])
                        for f in await platform_fonts(page, selector)
                    )
                widths = await page.evaluate("""() => ['baseline','probe'].map(
                    id => document.getElementById(id).getBoundingClientRect().width)""")
                observations.append({"text": text, "requested": family,
                                     "fonts": fonts, "widths": widths})
                assert fonts["#baseline"], observations[-1]
                assert sum(f[2] for f in fonts["#baseline"]) > 0, observations[-1]
                assert fonts["#baseline"] == fonts["#probe"], observations[-1]
                assert widths[0] == widths[1], observations[-1]
    record_property("portable_font_fallback", json.dumps({
        "platform": sys.platform, "scope": "blocked-family-equivalence",
        "observations": observations,
    }))

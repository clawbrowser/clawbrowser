"""FontFaceSet invalidation must not escape the managed character catalog."""
import base64
import hashlib
import json
from pathlib import Path
import sys

import pytest

from conftest import _launch_browser_with_details, _resolve_browser_binary
from test_complex_font_fallback import platform_fonts

pytestmark = pytest.mark.skipif(sys.platform not in ("linux", "darwin"),
                                reason="Linux/macOS bundled catalog")


@pytest.mark.asyncio
async def test_unicode_range_webfont_load_and_removal_preserve_fallback(record_property):
    manifest = json.loads((Path(__file__).parents[2] / "fonts/catalog.json").read_text())
    binary = Path(_resolve_browser_binary()).resolve()
    search_root = binary.parent if sys.platform == "linux" else binary.parents[1]
    candidates = list(search_root.rglob("Tinos-Regular.ttf"))
    expected = next(f["sha256"] for f in manifest["fonts"] if f["file"] == "Tinos-Regular.ttf")
    assert candidates, "Test requires the actual packaged font asset"
    data = candidates[0].read_bytes()
    assert hashlib.sha256(data).hexdigest() == expected
    payload = base64.b64encode(data).decode("ascii")
    samples = ["مرحبا", "नमस्ते", "สวัสดี", "👩\u200d💻"]
    async with _launch_browser_with_details(
        fixture_name="valid_fingerprint.json", backend_mode="mock",
        skip_verify=True, headless=False,
    ) as launch:
        page = launch["page"]
        await page.set_content('<meta charset="utf-8"><style>span{font:32px '
                               'WebLatin,sans-serif;display:inline-block}</style>'
                               '<span id="latin">Wide Latin WWW iii 123</span>' +
                               ''.join(f'<span id="s{i}">{text}</span>'
                                       for i, text in enumerate(samples)))

        async def snapshot():
            await page.evaluate("document.fonts.ready")
            result = {}
            for name in ["latin", "s0", "s1", "s2", "s3"]:
                fonts = await platform_fonts(page, "#" + name)
                result[name] = {
                    "fonts": sorted((f["familyName"], f.get("postScriptName", ""),
                                     f["glyphCount"], f["isCustomFont"]) for f in fonts),
                    "width": await page.locator("#" + name).evaluate(
                        "el => el.getBoundingClientRect().width"),
                }
                assert result[name]["fonts"] and result[name]["width"] > 0
            return result

        before = await snapshot()
        assert not any(f[3] for item in before.values() for f in item["fonts"])
        await page.evaluate('''async payload => {
            const bytes = Uint8Array.from(atob(payload), c => c.charCodeAt(0));
            window.qaFace = new FontFace('WebLatin', bytes, {unicodeRange:'U+0000-007F'});
            await qaFace.load(); document.fonts.add(qaFace);
        }''', payload)
        loaded = await snapshot()
        assert any(f[3] and f[2] > 0 for f in loaded["latin"]["fonts"]), loaded
        assert loaded["latin"]["width"] != before["latin"]["width"], loaded
        for name in ["s0", "s1", "s2", "s3"]:
            assert loaded[name] == before[name], (name, before[name], loaded[name])
        removed = await page.evaluate("document.fonts.delete(qaFace)")
        assert removed
        after = await snapshot()
        assert after == before, (before, after)
    record_property("dynamic_font_fallback", json.dumps({
        "platform": sys.platform, "before": before, "loaded": loaded, "after": after,
    }))

"""Selected complex-script fallback, not full language/shaping acceptance."""

import html
import sys

import pytest

from conftest import _launch_browser_with_details

pytestmark = pytest.mark.skipif(sys.platform not in ("linux", "darwin", "win32"),
                                reason="Desktop bundled catalog")


async def platform_fonts(page, selector):
    cdp = await page.context.new_cdp_session(page)
    try:
        await cdp.send("DOM.enable")
        await cdp.send("CSS.enable")
        root = (await cdp.send("DOM.getDocument"))["root"]["nodeId"]
        node = (await cdp.send("DOM.querySelector", {"nodeId": root, "selector": selector}))["nodeId"]
        return (await cdp.send("CSS.getPlatformFontsForNode", {"nodeId": node}))["fonts"]
    finally:
        await cdp.detach()


@pytest.mark.asyncio
async def test_selected_complex_text_uses_bundled_fallback():
    samples = [
        ("مرحبا بالعالم لا", "DejaVu Sans"),
        ("שָׁלוֹם עוֹלָם", "Arimo"),
        ("नमस्ते दुनिया क्ष त्र ज्ञ", "Lohit Devanagari"),
        ("বাংলা ভাষা ক্ষ শ্র", "Noto Sans Bengali"),
        ("தமிழ் மொழி", "Noto Sans Tamil"),
        ("ភាសាខ្មែរ", "Noto Sans Khmer"),
        ("สวัสดีชาวโลก", "Noto Sans Thai"),
        ("မြန်မာဘာသာ", "Noto Sans Myanmar"),
    ]
    async with _launch_browser_with_details(
        fixture_name="valid_fingerprint.json", backend_mode="mock", skip_verify=True,
    ) as launch:
        page = launch["page"]
        await page.set_content('<meta charset="utf-8"><style>p{font:34px sans-serif}</style>' +
            "".join(f'<p id="s{i}" dir="auto">{html.escape(text)}</p>'
                    for i, (text, _) in enumerate(samples)))
        await page.evaluate("document.fonts.ready")
        for i, (_, expected) in enumerate(samples):
            fonts = await platform_fonts(page, f"#s{i}")
            assert any(f["familyName"] == expected and f["glyphCount"] > 0 for f in fonts), fonts
            # Latin spaces may use Arimo; no host-dependent fallback is allowed.
            assert all(f["familyName"] in {expected, "Arimo"} and not f["isCustomFont"] for f in fonts), fonts


@pytest.mark.asyncio
async def test_emoji_sequences_compose_without_host_fallback():
    # Individual base symbols are a negative control for accidental glyph-count
    # collapse. This only covers these pinned, supported sequences.
    samples = [("👍🏽", 1), ("👩\u200d💻", 1), ("👩 💻", 2)]
    async with _launch_browser_with_details(
        fixture_name="valid_fingerprint.json", backend_mode="mock", skip_verify=True,
    ) as launch:
        page = launch["page"]
        await page.set_content('<meta charset="utf-8"><style>p{font:34px sans-serif}</style>' +
            "".join(f'<p id="s{i}">{html.escape(text)}</p>' for i, (text, _) in enumerate(samples)))
        await page.evaluate("document.fonts.ready")
        for i, (_, count) in enumerate(samples):
            fonts = await platform_fonts(page, f"#s{i}")
            assert sum(f["glyphCount"] for f in fonts if f["familyName"] == "Noto Color Emoji") == count, fonts
            assert all(f["familyName"] in {"Noto Color Emoji", "Arimo"} and not f["isCustomFont"] for f in fonts), fonts

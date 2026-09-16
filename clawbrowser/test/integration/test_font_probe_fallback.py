"""A raster group's requested family is not evidence of a local font lookup."""
import sys

import pytest

from conftest import _launch_browser_with_details
from canvas_network_mode import assert_canvas_network_mode, canvas_network_args


@pytest.mark.asyncio
@pytest.mark.parametrize("normalized", [False, True])
async def test_absent_font_labels_keep_generic_canvas_fallback(normalized, monkeypatch, record_property):
    if normalized and sys.platform != "linux":
        pytest.skip("normalized Canvas experiment is Linux-only")
    monkeypatch.setenv("CLAWBROWSER_QA_NORMALIZED_CANVAS", "1" if normalized else "0")
    extra = canvas_network_args()
    async with _launch_browser_with_details(
        fixture_name="valid_fingerprint.json", backend_mode="mock",
        skip_verify=True, headless=False, extra_browser_args=extra,
    ) as launch:
        canary = await assert_canvas_network_mode(launch["page"])
        record_property("canvas_mode", canary["mode"])
        rows = await launch["page"].evaluate("""async () => {
            const names = ['Abyssinica SIL', 'Papyrus', 'ClawbrowserMissingFontProbe'];
            const rows = [];
            const draw = (family, text) => {
                const canvas = document.createElement('canvas');
                canvas.width = 320; canvas.height = 64;
                const ctx = canvas.getContext('2d');
                ctx.fillStyle = 'white'; ctx.fillRect(0,0,320,64);
                ctx.fillStyle = 'black'; ctx.font = '23px ' + family;
                ctx.fillText(text, 3.25, 39);
                return {width:ctx.measureText(text).width,
                    pixels:ctx.getImageData(0,0,320,64).data};
            };
            for (const name of names) {
                let localLoaded = false;
                try {
                    await new FontFace('absence-control', 'local('+JSON.stringify(name)+')').load();
                    localLoaded = true;
                } catch (_) {}
                for (const generic of ['sans-serif','serif','monospace']) {
                    for (const text of ['Latin 0123', 'مرحبا بالعالم', '漢字 नमस्ते']) {
                        const baseline = draw(generic, text);
                        const probe = draw(JSON.stringify(name)+','+generic, text);
                        rows.push({name, generic, text, localLoaded,
                            equal:baseline.width === probe.width &&
                                baseline.pixels.every((v,i)=>v === probe.pixels[i]),
                            ink:baseline.pixels.some((v,i)=>i%4 !== 3 && v < 30),
                            width:baseline.width});
                    }
                }
            }
            return rows;
        }""")
        assert len(rows) == 27
        for row in rows:
            assert not row["localLoaded"], row
            assert row["ink"] and row["width"] > 0, row
            assert row["equal"], row

"""Explicit Canvas policy and an active-mode canary for opt-in network QA."""

import os


def canvas_network_args():
    value = os.environ.get("CLAWBROWSER_QA_NORMALIZED_CANVAS", "0")
    if value not in ("0", "1"):
        raise ValueError("CLAWBROWSER_QA_NORMALIZED_CANVAS must be 0 or 1")
    return ("--clawbrowser-experimental-normalized-canvas",) if value == "1" else ()


async def assert_canvas_network_mode(page):
    normalized = bool(canvas_network_args())
    result = await page.evaluate("""() => {
        const c = document.createElement('canvas'); c.width = 256; c.height = 1;
        const x = c.getContext('2d');
        for (let i = 0; i < 256; i++) {
            x.fillStyle = `rgb(${i},${i},${i})`; x.fillRect(i, 0, 1, 1);
        }
        const data = x.getImageData(0, 0, 256, 1).data;
        let changed = 0, maxDelta = 0, badAlpha = 0;
        for (let i = 0; i < data.length; i++) {
            const expected = i % 4 === 3 ? 255 : Math.floor(i / 4);
            if (data[i] !== expected) changed++;
            if (i % 4 === 3 && data[i] !== 255) badAlpha++;
            maxDelta = Math.max(maxDelta, Math.abs(data[i] - expected));
        }
        return {changed, maxDelta, badAlpha};
    }""")
    assert result["badAlpha"] == 0, result
    if normalized:
        assert result["changed"] == 0, "Normalized Canvas flag is not active"
    else:
        assert result["changed"] > 0 and result["maxDelta"] == 1, (
            "Default protected Canvas is not active", result)
    return {"mode": "normalized" if normalized else "protected", **result}

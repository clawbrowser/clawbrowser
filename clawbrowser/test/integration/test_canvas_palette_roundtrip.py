"""Separate detectable pixel protection from unstable/lossy PNG round trips."""

import json

import pytest

from conftest import _launch_browser


@pytest.mark.asyncio
@pytest.mark.parametrize("mode", ["native", "protected", "normalized"])
async def test_palette_png_roundtrip(mode, record_property):
    protected = mode != "native"
    if mode == "normalized":
        import sys
        if sys.platform != "linux":
            pytest.skip("normalized canvas experiment is Linux-only")
    async with _launch_browser(
        fixture_name="valid_fingerprint.json" if protected else None,
        backend_mode="mock" if protected else "vanilla",
        skip_verify=True,
        extra_browser_args=("--clawbrowser-experimental-normalized-canvas",)
            if mode == "normalized" else (),
    ) as (page, _):
        result = await page.evaluate("""async () => {
            const colors = [[255,0,0],[0,255,0],[0,0,255],[255,255,0],
                [255,0,255],[0,255,255],[1,1,1],[254,254,254],
                [0,0,0],[51,51,51],[102,102,102],[153,153,153],
                [204,204,204],[255,255,255]];
            const tile = 5, width = tile * colors.length, height = tile;
            const make = () => Object.assign(document.createElement('canvas'),
                {width, height});
            const source = make(), ctx = source.getContext('2d');
            colors.forEach((c, i) => {
                ctx.fillStyle = `rgb(${c.join(',')})`;
                ctx.fillRect(i * tile, 0, tile, height);
            });
            const read = c => Array.from(c.getContext('2d')
                .getImageData(0, 0, width, height).data);
            const first = read(source), url = source.toDataURL();
            const image = new Image(); image.src = url; await image.decode();
            const target = make(); target.getContext('2d').drawImage(image, 0, 0);
            const roundtrip = read(target), again = read(source);
            let changed = 0, maxDelta = 0, badAlpha = 0;
            const changedByTile = colors.map(() => 0);
            first.forEach((value, i) => {
                const pixel = Math.floor(i / 4), channel = i % 4;
                const stripe = Math.floor((pixel % width) / tile);
                const expected = channel === 3 ? 255 : colors[stripe][channel];
                if (value !== expected) { changed++; changedByTile[stripe]++; }
                if (channel === 3 && value !== 255) badAlpha++;
                maxDelta = Math.max(maxDelta, Math.abs(value - expected));
            });
            return {changed, changedByTile, maxDelta, badAlpha,
                stableReadback: first.every((v,i) => v === again[i]),
                stableExport: url === source.toDataURL(),
                roundtripMismatches: first.filter((v,i) => v !== roundtrip[i]).length,
                exportSignatureLength: source.toDataURL.toString().length,
                nativeExport: /native code/.test(source.toDataURL.toString())};
        }""")
        record_property("palette_observation", json.dumps(result, sort_keys=True))
        assert result["nativeExport"], result
        assert result["stableReadback"] and result["stableExport"], result
        assert result["roundtripMismatches"] == 0, result
        assert result["badAlpha"] == 0, result
        if mode == "protected":
            assert result["changed"] > 0, result
            assert result["maxDelta"] == 1, result
        else:
            assert result["changed"] == 0, result

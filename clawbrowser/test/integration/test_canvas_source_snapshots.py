"""Canvas protection must precede GPU upload, not just CPU readback."""

import pytest
import sys

from conftest import _launch_browser


@pytest.mark.asyncio
async def test_canvas_source_snapshots_protect_gpu_upload(browser_with_fingerprint):
    page, _ = browser_with_fingerprint
    await _check_source_snapshots(page, noisy=True)


@pytest.mark.asyncio
async def test_normalized_canvas_source_snapshots():
    if sys.platform != "linux":
        pytest.skip("normalized canvas experiment is Linux-only")
    async with _launch_browser(
        fixture_name="valid_fingerprint.json", backend_mode="mock", skip_verify=True,
        extra_browser_args=("--clawbrowser-experimental-normalized-canvas",),
    ) as (page, _):
        await _check_source_snapshots(page, noisy=False)


async def _check_source_snapshots(page, noisy):
    result = await page.evaluate("""async () => {
        const n = 64;
        const canvas = document.createElement('canvas');
        canvas.width = canvas.height = n;
        const offscreen = new OffscreenCanvas(n, n);
        for (const source of [canvas, offscreen]) {
            const ctx = source.getContext('2d');
            ctx.fillStyle = 'rgb(100,150,200)';
            ctx.fillRect(0, 0, n, n);
        }
        const expected = Array.from(canvas.getContext('2d')
            .getImageData(0, 0, n, n).data);
        const bitmap = await createImageBitmap(canvas);
        const offscreenBitmap = await createImageBitmap(offscreen);
        const inputs = {canvas, bitmap, offscreen, offscreenBitmap};
        const observations = [];
        function upload(source, version, name) {
            const target = document.createElement('canvas');
            target.width = target.height = n;
            const gl = target.getContext(version);
            if (!gl) throw new Error(version + ' unavailable');
            const texture = gl.createTexture();
            gl.bindTexture(gl.TEXTURE_2D, texture);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
            gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA,
                gl.UNSIGNED_BYTE, source);
            const fb = gl.createFramebuffer();
            gl.bindFramebuffer(gl.FRAMEBUFFER, fb);
            gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0,
                gl.TEXTURE_2D, texture, 0);
            const status = gl.checkFramebufferStatus(gl.FRAMEBUFFER);
            const bytes = new Uint8Array(n * n * 4);
            gl.readPixels(0, 0, n, n, gl.RGBA, gl.UNSIGNED_BYTE, bytes);
            observations.push({name, version, status, error: gl.getError(),
                mismatches: Array.from(bytes).filter((v, i) => v !== expected[i]).length,
                badAlpha: Array.from(bytes).filter((v, i) => i % 4 === 3 && v !== 255).length});
            gl.deleteFramebuffer(fb); gl.deleteTexture(texture);
        }
        for (const version of ['webgl', 'webgl2']) {
            for (const [name, input] of Object.entries(inputs)) upload(input, version, name);
        }
        const transferred = offscreen.transferToImageBitmap();
        for (const version of ['webgl', 'webgl2']) upload(transferred, version, 'transferred');
        // bitmaprenderer owns the transferred backing, rather than drawing it
        // through Canvas2D. Its snapshots must retain the same protection.
        const ownership = [];
        for (const kind of ['dom', 'offscreen']) {
            const target = kind === 'dom' ? document.createElement('canvas') : new OffscreenCanvas(n,n);
            target.width = target.height = n;
            const renderer = target.getContext('bitmaprenderer');
            if (!renderer) throw new Error(kind + ' bitmaprenderer unavailable');
            const image = await createImageBitmap(canvas);
            renderer.transferFromImageBitmap(image);
            ownership.push(image.width === 0 && image.height === 0);
            for (const version of ['webgl', 'webgl2']) upload(target, version, kind + '-bitmaprenderer');
            renderer.transferFromImageBitmap(null);
        }
        for (const image of [bitmap, offscreenBitmap, transferred]) image.close();
        const after = canvas.getContext('2d').getImageData(0, 0, n, n).data;
        return {observations, ownership,
            protectedBytes: expected.filter((v, i) => v !== [100,150,200,255][i % 4]).length,
            liveUnchanged: expected.every((v, i) => v === after[i])};
    }""")
    if noisy:
        assert result['protectedBytes'] > 0, 'control must demonstrate active canvas noise'
    else:
        assert result['protectedBytes'] == 0, result
    assert result['liveUnchanged'], 'snapshot must not mutate the canvas backing'
    assert result['ownership'] == [True, True], 'bitmap transfer must consume its input'
    assert len(result['observations']) == 14
    for observation in result['observations']:
        assert observation['status'] == 36053, observation  # FRAMEBUFFER_COMPLETE
        assert observation['error'] == 0, observation
        assert observation['badAlpha'] == 0, observation
        assert observation['mismatches'] == 0, observation

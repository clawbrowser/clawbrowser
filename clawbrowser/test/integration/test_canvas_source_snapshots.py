"""Canvas protection must precede GPU upload, not just CPU readback."""

import pytest


@pytest.mark.asyncio
async def test_canvas_source_snapshots_protect_gpu_upload(browser_with_fingerprint):
    page, _ = browser_with_fingerprint
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
        for (const image of [bitmap, offscreenBitmap, transferred]) image.close();
        const after = canvas.getContext('2d').getImageData(0, 0, n, n).data;
        return {observations,
            protectedBytes: expected.filter((v, i) => v !== [100,150,200,255][i % 4]).length,
            liveUnchanged: expected.every((v, i) => v === after[i])};
    }""")
    assert result['protectedBytes'] > 0, 'control must demonstrate active canvas noise'
    assert result['liveUnchanged'], 'snapshot must not mutate the canvas backing'
    assert len(result['observations']) == 10
    for observation in result['observations']:
        assert observation['status'] == 36053, observation  # FRAMEBUFFER_COMPLETE
        assert observation['error'] == 0, observation
        assert observation['badAlpha'] == 0, observation
        assert observation['mismatches'] == 0, observation

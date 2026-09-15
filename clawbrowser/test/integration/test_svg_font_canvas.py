"""SVG image text must not reopen host-font or CPU-dependent Canvas paths."""
import hashlib
import json
import os
import sys

import pytest

from conftest import _launch_browser_with_details
from test_isolated_canvas_control import canvas_control_files


@pytest.mark.asyncio
@pytest.mark.parametrize('canvas_mode', ['override'])
async def test_svg_text_canvas_normalization(tmp_path, record_property, canvas_mode):
    environments = [{}]
    if sys.platform == 'linux':
        configs = [os.environ.get('CLAWBROWSER_TEST_FONTCONFIG_' + suffix)
                   for suffix in ('A', 'B')]
        if not all(configs):
            pytest.skip('requires two Linux host-font controls')
        environments = [{'FONTCONFIG_FILE': config} for config in configs]
    path, mock = canvas_control_files(tmp_path, canvas_mode)
    observations = []
    for environment in environments:
        for args in ((), ('--disable-skia-runtime-opts',)):
            async with _launch_browser_with_details(
                fixture_name=str(path), backend_mode='mock', skip_verify=True,
                headless=False, extra_env=environment, extra_browser_args=args,
                fingerprints_fixture_path=mock,
            ) as launch:
                result = await launch['page'].evaluate(r'''async () => {
                    const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="256" height="160">
                    <defs><linearGradient id="g"><stop stop-color="#234567"/>
                    <stop offset="1" stop-color="#ecdabc"/></linearGradient></defs>
                    <rect width="256" height="160" fill="url(#g)"/>
                    <g fill="#173452" font-size="17.5">
                    <text x="4.25" y="25.5" font-family="serif">Latin 0123 Привет</text>
                    <text x="4.25" y="53.5" font-family="sans-serif">العربية 漢字 🧭</text>
                    <text x="4.25" y="82.5" font-family="monospace">Mono ffi AV 012345</text>
                    <text x="4.25" y="111.5" font-family="Arimo">Arimo AV ffi xyz</text>
                    <text x="4.25" y="140.5" font-family="MissingHostFont, serif">Fallback Ω Ж 漢</text>
                    </g></svg>`;
                    const url = URL.createObjectURL(new Blob([svg], {type:'image/svg+xml'}));
                    const img = new Image();
                    const blank = new Image();
                    const blankUrl = URL.createObjectURL(new Blob([
                        svg.replace(/<g fill=[\s\S]*<\/g>/, '')
                    ], {type:'image/svg+xml'}));
                    try {
                        img.src = url;
                        blank.src = blankUrl;
                        await Promise.all([img.decode(), blank.decode()]);
                        const outputs = {};
                        for (const kind of ['dom', 'offscreen']) {
                            const canvas = kind === 'dom' ? document.createElement('canvas') :
                                new OffscreenCanvas(256, 160);
                            canvas.width = 256; canvas.height = 160;
                            const ctx = canvas.getContext('2d');
                            ctx.drawImage(img, 0, 0);
                            const pixels = Array.from(ctx.getImageData(0,0,256,160).data);
                            ctx.clearRect(0,0,256,160);
                            ctx.drawImage(blank,0,0);
                            const noText = ctx.getImageData(0,0,256,160).data;
                            outputs[kind] = {pixels,
                                textDifferences: pixels.filter((v,i) => v !== noText[i]).length};
                        }
                        return outputs;
                    } finally {
                        URL.revokeObjectURL(url); URL.revokeObjectURL(blankUrl);
                    }
                }''')
                assert set(result) == {'dom', 'offscreen'}
                hashes = {}
                row_hashes = {}
                for kind, output in result.items():
                    pixels = output['pixels']
                    assert len(pixels) == 256 * 160 * 4
                    assert len(set(pixels)) > 64, 'SVG must render a nonuniform image'
                    assert output['textDifferences'] > 500, 'SVG text must actually render'
                    hashes[kind] = hashlib.sha256(bytes(pixels)).hexdigest()
                    row_hashes[kind] = [hashlib.sha256(bytes(pixels[y*1024:(y+1)*1024])).hexdigest()
                                        for y in range(160)]
                observations.append({'font_control': environments.index(environment),
                                     'args': args, 'hashes': hashes,
                                     'row_hashes': row_hashes,
                                     'text_differences': {k: v['textDifferences'] for k, v in result.items()}})
    record_property('svg_font_canvas', json.dumps({
        'host_platform': sys.platform, 'observations': observations,
    }))
    assert len({digest for row in observations for digest in row['hashes'].values()}) == 1, observations

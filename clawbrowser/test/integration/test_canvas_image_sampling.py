"""Cross-host observations for fractional bitmap sampling, without font inputs."""
import hashlib
import json
import base64
import os
import zlib

import pytest

from conftest import _launch_browser_with_details
from test_isolated_canvas_control import canvas_control_files


@pytest.mark.asyncio
@pytest.mark.parametrize('canvas_mode', ['native', 'override'])
async def test_fractional_bitmap_sampling(tmp_path, record_property, canvas_mode):
    path, mock = canvas_control_files(tmp_path, canvas_mode)
    observations = []
    for args in ((), ('--disable-skia-runtime-opts',)):
        async with _launch_browser_with_details(
            fixture_name=str(path), backend_mode='mock', skip_verify=True,
            headless=False, extra_browser_args=args,
            fingerprints_fixture_path=mock,
        ) as launch:
            result = await launch['page'].evaluate(r'''async () => {
                const source = document.createElement('canvas');
                source.width = source.height = 109;
                const s = source.getContext('2d');
                const input = s.createImageData(109,109);
                let seed = 20270915;
                for (let i=0; i<input.data.length; i++) {
                    seed ^= seed << 13; seed ^= seed >>> 17; seed ^= seed << 5;
                    input.data[i] = seed & 255;
                }
                s.putImageData(input,0,0);
                const img = new Image();
                img.src = source.toDataURL();
                await img.decode();
                const output = {source: Array.from(s.getImageData(0,0,109,109).data)};
                for (const [sourceKind, picture] of [['png',img],['canvas',source]]) {
                const unity = document.createElement('canvas');
                unity.width=unity.height=109;
                unity.getContext('2d').drawImage(picture,0,0);
                output[sourceKind+'/unity']=Array.from(unity.getContext('2d').getImageData(0,0,109,109).data);
                for (const quality of ['low','medium','high']) {
                    for (const kind of ['dom','offscreen']) {
                        const c = kind === 'dom' ? document.createElement('canvas') :
                            new OffscreenCanvas(256,160);
                        c.width=256; c.height=160;
                        const ctx=c.getContext('2d');
                        ctx.imageSmoothingEnabled=true;
                        ctx.imageSmoothingQuality=quality;
                        ctx.fillStyle='#cadbec'; ctx.fillRect(0,0,256,160);
                        for (const [x,y,w,h] of [[4.25,7.5,17.5,17.5],
                            [37.5,4.25,32.25,29.75],[75.75,3.5,107.5,111.25],
                            [10.5,50.75,53.25,87.5]]) {
                            ctx.drawImage(picture,x,y,w,h);
                        }
                        output[sourceKind+'/'+quality+'/'+kind]=Array.from(ctx.getImageData(0,0,256,160).data);
                    }
                }
                }
                return output;
            }''')
            assert len(result) == 15
            for key, pixels in result.items():
                assert len(pixels) == (109*109 if key == 'source' or key.endswith('/unity') else 256*160)*4
                assert len(set(pixels)) > 128
            observations.append({'args': args, 'hashes': {
                key: hashlib.sha256(bytes(pixels)).hexdigest() for key,pixels in result.items()
            }})
            if not args and os.environ.get('CLAWBROWSER_TEST_PIXEL_DIAGNOSTICS') == '1':
                # Only synthetic fixture pixels, never page/profile/user content.
                record_property('bitmap_sampling_pixels', json.dumps({
                    key: base64.b64encode(zlib.compress(bytes(result[key]))).decode('ascii')
                    for key in ('png/low/dom', 'canvas/low/dom')
                }))
    record_property('bitmap_sampling_matrix', json.dumps({'observations': observations}))
    for source in ('png','canvas'):
        for quality in ('low','medium','high'):
            assert len({v for row in observations for k,v in row['hashes'].items()
                        if k.startswith(source+'/'+quality+'/')}) == 1, (source, quality, observations)
    assert len({row['hashes']['source'] for row in observations}) == 1

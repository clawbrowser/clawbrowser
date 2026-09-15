"""Isolate glyph coverage from gradients, blend modes and text placement."""
import base64
import hashlib
import json
import os
import zlib

import pytest

from conftest import FIXTURE_DIR, _launch_browser_with_details
from test_isolated_canvas_control import canvas_control_files


@pytest.mark.asyncio
@pytest.mark.parametrize('mode', ['native','override'])
async def test_font_mask_controls(tmp_path, record_property, mode):
    fixture, mock = canvas_control_files(tmp_path, mode)
    seed = json.loads(fixture.read_text())['response']['fingerprint']['canvas_seed']
    async with _launch_browser_with_details(
        fixture_name=str(fixture), backend_mode='mock', skip_verify=True,
        headless=False, fingerprints_fixture_path=mock,
    ) as launch:
        rows = await launch['page'].evaluate('''() => {
            const rows=[];
            for(const textRendering of ['auto','geometricPrecision']) {
            for(const x of [3,3.25]) { for(const y of [24,24.75]) {
                const canvas=document.createElement('canvas');canvas.width=192;canvas.height=64;
                const c=canvas.getContext('2d',{willReadFrequently:true});
                c.fillStyle='white';c.fillRect(0,0,192,64);
                c.textRendering=textRendering;c.font='17px Arimo';c.fillStyle='black';
                c.fillText('Latin 0123 Привет',x,y);
                const pixels=Array.from(c.getImageData(0,0,192,64).data);
                const again=c.getImageData(0,0,192,64).data;
                rows.push({key:`${textRendering}/x${x}/y${y}`,pixels,
                    stable:pixels.every((v,i)=>v===again[i])});
            }}}
            return rows;
        }''')
    assert len(rows) == 8
    for row in rows:
        assert row['stable'] and len(row['pixels']) == 192 * 64 * 4
        assert set(row['pixels'][3::4]) == {255}
        assert min(row['pixels'][::4]) < 30, 'Drawing must contain text ink'
    hashes = {row['key']:hashlib.sha256(bytes(row['pixels'])).hexdigest() for row in rows}
    record_property('font_mask_hashes', json.dumps(hashes))
    record_property('text_composite_control', 'source-over')
    if os.environ.get('CLAWBROWSER_QA_FONT_PIXELS') == '1':
        record_property('font_pixel_controls', json.dumps({
            'width':192,'height':64,'seed':seed,'canvas_policy':mode,'rgba_zlib_base64':{
                row['key']:base64.b64encode(zlib.compress(bytes(row['pixels']))).decode()
                for row in rows}}))
    if mode == 'override':
        reference = json.loads((FIXTURE_DIR / 'font_mask_reference.json').read_text())
        assert reference['canvas_seed'] == seed
        assert (reference['width'], reference['height']) == (192, 64)
        assert hashes == reference['hashes'], 'Protected text differs from pinned cross-host reference'

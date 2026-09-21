"""Protected text must not reveal opaque/LCD or CPU/readback surface choices."""
import hashlib
import json

import pytest

from conftest import FIXTURE_DIR, _launch_browser_with_details
from test_isolated_canvas_control import canvas_control_files


@pytest.mark.asyncio
async def test_protected_canvas_text_surface_props(tmp_path, record_property):
    fixture, mock = canvas_control_files(tmp_path, 'override')
    async with _launch_browser_with_details(
        fixture_name=str(fixture), backend_mode='mock', skip_verify=True,
        headless=False, fingerprints_fixture_path=mock,
    ) as launch:
        rows = await launch['page'].evaluate('''async () => {
            function draw(canvas,alpha,willReadFrequently) {
                canvas.width=192;canvas.height=64;
                const c=canvas.getContext('2d',{alpha,willReadFrequently});
                c.fillStyle='white';c.fillRect(0,0,192,64);
                c.font='17px Arimo';c.fillStyle='black';
                c.fillText('Latin 0123 Привет',3.25,24);
                return {alpha:c.getContextAttributes().alpha,
                    pixels:Array.from(c.getImageData(0,0,192,64).data)};
            }
            const rows=[];
            for(const alpha of [true,false]) { for(const frequent of [false,true]) {
                for(const kind of ['dom','offscreen','worker']) {
                    let result;
                    if(kind!=='worker') {
                        result=draw(kind==='dom'?document.createElement('canvas'):
                            new OffscreenCanvas(192,64),alpha,frequent);
                    } else {
                        const url=URL.createObjectURL(new Blob([
                            'onmessage=e=>postMessage(('+draw.toString()+')(new OffscreenCanvas(192,64),e.data.alpha,e.data.frequent))'
                        ],{type:'text/javascript'}));
                        const worker=new Worker(url);let timer;
                        try {
                            result=await new Promise((resolve,reject)=>{
                                timer=setTimeout(()=>reject(new Error('worker timeout')),10000);
                                worker.onmessage=e=>resolve(e.data);
                                worker.onerror=()=>reject(new Error('worker failed'));
                                worker.postMessage({alpha,frequent});
                            });
                        } finally { clearTimeout(timer);worker.terminate();URL.revokeObjectURL(url); }
                    }
                    rows.push({kind,requestedAlpha:alpha,frequent,...result});
                }
            }}
            return rows;
        }''')
    expected = json.loads((FIXTURE_DIR / 'font_mask_reference.json').read_text())['hashes']['auto/x3.25/y24']
    observations = []
    for row in rows:
        assert row['alpha'] == row['requestedAlpha']
        assert len(row['pixels']) == 192 * 64 * 4
        assert set(row['pixels'][3::4]) == {255}
        observations.append({k:row[k] for k in ('kind','requestedAlpha','frequent')} |
                            {'hash':hashlib.sha256(bytes(row['pixels'])).hexdigest()})
    record_property('text_surface_props', json.dumps(observations))
    assert len(observations) == 12
    assert all(row['hash'] == expected for row in observations), observations

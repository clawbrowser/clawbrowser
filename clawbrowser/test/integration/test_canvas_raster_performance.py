"""Opt-in comparative raster timing; not an absolute performance release gate."""
import json
import os
import statistics

import pytest

from conftest import _launch_browser_with_details
from test_isolated_canvas_control import canvas_control_files


@pytest.mark.asyncio
@pytest.mark.parametrize('mode', ['native', 'override'])
async def test_canvas_raster_timing(tmp_path, record_property, mode):
    if os.environ.get('CLAWBROWSER_QA_RASTER_TIMING') != '1':
        pytest.skip('opt-in comparative timing only')
    fixture, mock = canvas_control_files(tmp_path, mode)
    async with _launch_browser_with_details(
        fixture_name=str(fixture), backend_mode='mock', skip_verify=True,
        headless=False, fingerprints_fixture_path=mock,
    ) as launch:
        result = await launch['page'].evaluate('''() => {
            const canvas=document.createElement('canvas');canvas.width=256;canvas.height=128;
            const c=canvas.getContext('2d',{willReadFrequently:true});
            const gradient=c.createLinearGradient(.3,.7,250.5,120.25);
            gradient.addColorStop(0,'color(display-p3 .9 .2 .1 / .7)');
            gradient.addColorStop(1,'color(display-p3 .1 .8 .6 / .9)');
            const draw=()=>{
                c.globalCompositeOperation='source-over';c.fillStyle=gradient;c.fillRect(0,0,256,128);
                c.globalCompositeOperation='screen';c.fillStyle='rgba(79,131,233,.51)';
                for(let i=0;i<24;i++){
                    c.beginPath();c.ellipse(30+i*7.13,61.25,29.75,17.125,.2,0,Math.PI*2);c.fill();
                }
                return c.getImageData(0,0,256,128).data.length;
            };
            for(let i=0;i<10;i++)draw();
            const samples=[];let length;
            for(let round=0;round<7;round++){
                const start=performance.now();for(let i=0;i<30;i++)length=draw();
                samples.push((performance.now()-start)/30);
            }
            return {samples,length};
        }''')
    assert result['length'] == 256 * 128 * 4
    assert len(result['samples']) == 7 and all(x > 0 for x in result['samples'])
    record_property('raster_timing', json.dumps({'mode':mode, 'samples_ms':result['samples'],
                                                'median_ms':statistics.median(result['samples'])}))

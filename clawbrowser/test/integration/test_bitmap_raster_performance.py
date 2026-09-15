"""Opt-in relative bitmap source-over timing; not a release threshold."""
import json
import os
import statistics

import pytest

from conftest import _launch_browser_with_details
from test_isolated_canvas_control import canvas_control_files


@pytest.mark.asyncio
@pytest.mark.parametrize('mode', ['native', 'override'])
async def test_bitmap_source_over_timing(tmp_path, record_property, mode):
    if os.environ.get('CLAWBROWSER_QA_BITMAP_TIMING') != '1':
        pytest.skip('opt-in comparative bitmap timing only')
    baseline = os.environ.get('CLAWBROWSER_QA_RASTER_BASELINE') == '1'
    color_type = os.environ.get('CLAWBROWSER_QA_BITMAP_COLOR_TYPE', 'unorm8')
    assert color_type in ('unorm8', 'float16')
    fixture, mock = canvas_control_files(tmp_path, mode)
    async with _launch_browser_with_details(
        fixture_name=str(fixture), backend_mode='mock', skip_verify=True,
        headless=False, fingerprints_fixture_path=mock,
        extra_browser_args=['--disable-skia-runtime-opts'] if baseline else [],
    ) as launch:
        result = await launch['page'].evaluate('''async (colorType) => {
            const src=document.createElement('canvas');src.width=src.height=109;
            const sc=src.getContext('2d'),data=sc.createImageData(109,109);
            let seed=20270915;
            for(let i=0;i<data.data.length;i++){
                seed^=seed<<13;seed^=seed>>>17;seed^=seed<<5;data.data[i]=seed&255;
            }
            sc.putImageData(data,0,0);
            const img=new Image();img.src=src.toDataURL();await img.decode();
            const canvas=document.createElement('canvas');canvas.width=256;canvas.height=160;
            const c=canvas.getContext('2d',{willReadFrequently:true,colorType});
            c.imageSmoothingEnabled=true;c.imageSmoothingQuality='low';
            const draw=()=>{
                c.fillStyle='#cadbec';c.fillRect(0,0,256,160);
                for(const [x,y,w,h] of [[4.25,7.5,17.5,17.5],[37.5,4.25,32.25,29.75],
                    [75.75,3.5,107.5,111.25],[10.5,50.75,53.25,87.5]])c.drawImage(img,x,y,w,h);
                return c.getImageData(0,0,256,160).data.length;
            };
            for(let i=0;i<20;i++)draw();
            const samples=[];let length;
            for(let round=0;round<7;round++){
                const start=performance.now();for(let i=0;i<1000;i++)length=draw();
                samples.push((performance.now()-start)/1000);
            }
            return {samples,length,colorType:c.getContextAttributes().colorType};
        }''', color_type)
    assert result['colorType'] == color_type
    assert result['length'] == 256*160*4
    assert len(result['samples']) == 7 and all(x > 0 for x in result['samples'])
    record_property('bitmap_raster_timing', json.dumps({
        'mode': mode, 'baseline_cpu': baseline, 'iterations_per_sample': 1000,
        'color_type': color_type,
        'samples_ms': result['samples'], 'median_ms': statistics.median(result['samples']),
    }))

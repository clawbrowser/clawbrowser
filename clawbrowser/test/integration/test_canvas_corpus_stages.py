"""Locate the first cross-host divergence in the fixed text/shape recipe."""
import hashlib
import json

import pytest

from conftest import _launch_browser_with_details
from test_isolated_canvas_control import canvas_control_files


@pytest.mark.asyncio
@pytest.mark.parametrize('mode', ['override'])
async def test_canvas_corpus_stages(tmp_path, record_property, mode):
    fixture, mock = canvas_control_files(tmp_path, mode)
    seed = json.loads(fixture.read_text())['response']['fingerprint']['canvas_seed']
    async with _launch_browser_with_details(
        fixture_name=str(fixture), backend_mode='mock', skip_verify=True,
        headless=False, fingerprints_fixture_path=mock,
    ) as launch:
        rows = await launch['page'].evaluate('''() => {
            const rows=[];
            for (const textRendering of ['auto','geometricPrecision']) {
            for (const colorType of ['unorm8','float16']) {
            for (const willReadFrequently of [false,true]) {
                const canvas=document.createElement('canvas');canvas.width=192;canvas.height=128;
                const c=canvas.getContext('2d',{colorType,willReadFrequently});
                c.textRendering=textRendering;
                const capture=stage=>{
                    const pixels=Array.from(c.getImageData(0,0,192,128).data);
                    const again=c.getImageData(0,0,192,128).data;
                    const sample=stage==='latin'?'Latin 0123 Привет':'العربية 漢字 🧭';
                    const measured=c.measureText(sample);
                    const metrics=Object.fromEntries(['width','actualBoundingBoxLeft',
                        'actualBoundingBoxRight','actualBoundingBoxAscent','actualBoundingBoxDescent',
                        'fontBoundingBoxAscent','fontBoundingBoxDescent'].map(k=>[k,measured[k]]));
                    rows.push({stage,colorType,willReadFrequently,textRendering,pixels,
                        metrics,
                        actualType:c.getContextAttributes().colorType,
                        stable:pixels.every((v,i)=>v===again[i])});
                };
                const gradient=c.createLinearGradient(.25,.75,180.5,110.25);
                gradient.addColorStop(0,'#183654');gradient.addColorStop(1,'#dbaf67');
                c.fillStyle=gradient;c.fillRect(0,0,192,128);capture('gradient');
                c.save();c.translate(11.125,9.375);c.rotate(.137);
                c.fillStyle='rgba(210,32,89,.71)';
                c.beginPath();c.moveTo(3.2,11.7);
                c.bezierCurveTo(97.2,-20.8,154.9,130.1,171.4,68.6);
                c.lineTo(18.8,92.2);c.closePath();c.fill();c.restore();capture('curve');
                c.globalCompositeOperation='multiply';
                c.font='17px Arimo';c.fillStyle='#395ac7';
                c.fillText('Latin 0123 Привет',3.25,43.75);capture('latin');
                c.font='16px sans-serif';c.fillText('العربية 漢字 🧭',4.5,78.25);capture('fallback');
                c.globalCompositeOperation='source-over';
                c.shadowColor='rgba(20,90,180,.4)';c.shadowBlur=3.5;
                c.strokeStyle='#65deab';c.lineWidth=1.25;
                c.beginPath();c.ellipse(91.25,86.75,33.5,17.25,.23,0,Math.PI*2);
                c.stroke();capture('shadow');
            }}}
            return rows;
        }''')
    observations, groups = [], {}
    for row in rows:
        assert row['actualType'] == row['colorType']
        assert row['stable']
        assert len(row['pixels']) == 192 * 128 * 4
        assert set(row['pixels'][3::4]) == {255}
        digest = hashlib.sha256(bytes(row['pixels'])).hexdigest()
        groups.setdefault((row['stage'], row['colorType'], row['textRendering']), set()).add(digest)
        observations.append({k: row[k] for k in ('stage','colorType','willReadFrequently','textRendering')} | {'hash':digest})
    record_property('raster_primitive_matrix', json.dumps({'seed':seed,'observations':observations}))
    record_property('text_metrics', json.dumps([
        {k: row[k] for k in ('stage','colorType','willReadFrequently','textRendering','metrics')}
        for row in rows if row['stage'] in ('latin','fallback')]))
    assert len(observations) == 40
    assert all(len(hashes) == 1 for hashes in groups.values()), observations
    # Closed-catalog text uses linear advances, not host hinting preferences.
    # Keep native-policy behavior out of this contract.
    advances = {}
    for row in rows:
        if row['stage'] in ('latin', 'fallback'):
            advances.setdefault(row['stage'], set()).add(row['metrics']['width'])
    assert all(len(widths) == 1 for widths in advances.values()), advances

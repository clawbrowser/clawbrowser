"""Export small, font-free raster controls for cross-architecture comparison."""
import hashlib
import json

import pytest

from conftest import _launch_browser_with_details
from test_isolated_canvas_control import canvas_control_files


@pytest.mark.asyncio
@pytest.mark.parametrize('mode', ['override'])
async def test_canvas_raster_primitives(tmp_path, record_property, mode):
    fixture, mock = canvas_control_files(tmp_path, mode)
    seed = json.loads(fixture.read_text())['response']['fingerprint']['canvas_seed']
    async with _launch_browser_with_details(
        fixture_name=str(fixture), backend_mode='mock', skip_verify=True,
        headless=False, fingerprints_fixture_path=mock,
    ) as launch:
        values = await launch['page'].evaluate('''() => {
            const rows=[];
            for (const primitive of ['solid','gradient','ellipse','composite',
                                     'wide-gamut-gradient','gradient-screen-ellipse',
                                     'gradient-screen-aligned-rect','gradient-screen-fractional-rect']) {
                for (const colorType of ['unorm8','float16']) {
                    for (const willReadFrequently of [false,true]) {
                        const canvas=document.createElement('canvas');
                        canvas.width=83;canvas.height=57;
                        const c=canvas.getContext('2d',{colorType,willReadFrequently});
                        c.fillStyle='rgb(41,113,197)';c.fillRect(0,0,83,57);
                        if (primitive.includes('gradient')) {
                            const g=c.createLinearGradient(.3,.7,78.5,51.25);
                            const wide=primitive!=='gradient';
                            g.addColorStop(0,wide?'color(display-p3 .9 .2 .1 / .7)':'rgba(221,63,19,.7)');
                            g.addColorStop(1,wide?'color(display-p3 .1 .8 .6 / .9)':'rgba(13,201,143,.9)');
                            c.fillStyle=g;c.fillRect(0,0,83,57);
                            if (primitive.startsWith('gradient-screen-')) {
                                c.globalCompositeOperation='screen';
                                c.fillStyle='rgba(79,131,233,.51)';
                                if (primitive.endsWith('aligned-rect')) {
                                    c.fillRect(10,10,50,30);
                                } else if (primitive.endsWith('fractional-rect')) {
                                    c.fillRect(10.25,10.75,50.5,30.25);
                                } else {
                                    c.beginPath();c.ellipse(39.5,27.25,29.75,17.125,.2,0,Math.PI*2);c.fill();
                                }
                            }
                        } else if (primitive==='ellipse') {
                            c.fillStyle='rgba(79,131,233,.51)';
                            c.beginPath();c.ellipse(39.5,27.25,29.75,17.125,.2,0,Math.PI*2);c.fill();
                        } else if (primitive==='composite') {
                            c.globalCompositeOperation='screen';
                            c.fillStyle='rgba(79,131,233,.51)';c.fillRect(0,0,83,57);
                        }
                        const pixels=Array.from(c.getImageData(0,0,83,57).data);
                        const again=c.getImageData(0,0,83,57).data;
                        rows.push({primitive,colorType,willReadFrequently,
                            actualType:c.getContextAttributes().colorType,pixels,
                            stable:pixels.every((v,i)=>v===again[i])});
                    }
                }
            }
            return rows;
        }''')
    observations, groups = [], {}
    for row in values:
        assert row['actualType'] == row['colorType']
        assert row['stable']
        assert len(row['pixels']) == 83 * 57 * 4
        assert set(row['pixels'][3::4]) == {255}
        digest = hashlib.sha256(bytes(row['pixels'])).hexdigest()
        groups.setdefault((row['primitive'], row['colorType']), set()).add(digest)
        observations.append({k: row[k] for k in ('primitive','colorType','willReadFrequently')} | {'hash':digest})
    record_property('raster_primitive_matrix', json.dumps({'seed':seed,'observations':observations}))
    assert len(observations) == 32
    assert all(len(hashes) == 1 for hashes in groups.values()), observations

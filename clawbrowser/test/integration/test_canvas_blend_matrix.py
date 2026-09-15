"""Font-free blend coverage across backing formats, contexts and CPU paths."""
import hashlib
import json

import pytest

from conftest import _launch_browser_with_details
from test_isolated_canvas_control import canvas_control_files


@pytest.mark.asyncio
@pytest.mark.parametrize('mode', ['override'])
async def test_canvas_blend_matrix(tmp_path, record_property, mode):
    fixture, mock = canvas_control_files(tmp_path, mode)
    observations, groups = [], {}
    for args in ((), ('--disable-skia-runtime-opts',)):
        async with _launch_browser_with_details(
            fixture_name=str(fixture), backend_mode='mock', skip_verify=True,
            headless=False, extra_browser_args=args,
            fingerprints_fixture_path=mock,
        ) as launch:
            rows = await launch['page'].evaluate(r'''() => {
                const rows=[];
                const operations=['source-over','source-in','source-out','source-atop',
                    'destination-over','destination-in','destination-out','destination-atop',
                    'lighter','copy','xor','multiply','screen','overlay','darken','lighten',
                    'color-dodge','color-burn','hard-light','soft-light','difference',
                    'exclusion','hue','saturation','color','luminosity'];
                for (const operation of operations) {
                for (const colorType of ['unorm8','float16']) {
                for (const kind of ['dom','offscreen']) {
                for (const frequent of [false,true]) {
                    const canvas=kind==='dom'?document.createElement('canvas'):new OffscreenCanvas(83,57);
                    canvas.width=83;canvas.height=57;
                    const c=canvas.getContext('2d',{colorType,willReadFrequently:frequent});
                    const gradient=c.createLinearGradient(.375,1.25,79.625,53.75);
                    gradient.addColorStop(0,'rgba(217,31,79,.43)');
                    gradient.addColorStop(.37,'rgba(21,173,233,.81)');
                    gradient.addColorStop(1,'rgba(137,219,43,.67)');
                    c.fillStyle=gradient;c.fillRect(2.25,3.75,76.5,48.125);
                    c.globalCompositeOperation=operation;
                    if(c.globalCompositeOperation!==operation)throw Error('unsupported blend '+operation);
                    c.fillStyle='rgba(53,129,211,.57)';
                    c.beginPath();c.ellipse(37.25,29.625,27.125,19.375,.27,0,Math.PI*2);c.fill();
                    const pixels=Array.from(c.getImageData(0,0,83,57).data);
                    const again=c.getImageData(0,0,83,57).data;
                    rows.push({operation,colorType,kind,frequent,
                        actualType:c.getContextAttributes().colorType,pixels,
                        stable:pixels.every((v,i)=>v===again[i])});
                }}}}
                return rows;
            }''')
        assert len(rows) == 208
        for row in rows:
            assert row['actualType'] == row['colorType']
            assert row['stable']
            pixels = row['pixels']
            assert len(pixels) == 83 * 57 * 4
            # Reject blank/constant fixtures, including a no-op blend setup.
            assert len(set(pixels[3::4])) > 8
            digest = hashlib.sha256(bytes(pixels)).hexdigest()
            groups.setdefault((row['operation'], row['colorType']), set()).add(digest)
            observations.append({k: row[k] for k in ('operation','colorType','kind','frequent')}
                                | {'args': args, 'hash': digest})
    record_property('blend_matrix', json.dumps({'observations': observations}))
    assert len(observations) == 416
    # DOM/Offscreen, readback hints and CPU dispatch must agree per format.
    assert all(len(hashes) == 1 for hashes in groups.values()), observations
    for color_type in ('unorm8', 'float16'):
        assert len({next(iter(v)) for (op, fmt), v in groups.items()
                    if fmt == color_type}) >= 24

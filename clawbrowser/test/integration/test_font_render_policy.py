"""Keep native font rendering distinct from closed-catalog normalization."""
import json

import pytest

from conftest import _launch_browser_with_details
from test_isolated_canvas_control import canvas_control_files


@pytest.mark.asyncio
@pytest.mark.parametrize('font_policy', ['native', 'native_or_allowlist'])
async def test_font_render_policy(tmp_path, record_property, font_policy):
    fixture, mock = canvas_control_files(tmp_path, 'override')
    for path in (fixture, mock):
        data = json.loads(path.read_text())
        data['response']['fingerprint']['surface_policy']['fonts']['mode'] = font_policy
        path.write_text(json.dumps(data))
    async with _launch_browser_with_details(
        fixture_name=str(fixture), backend_mode='mock', skip_verify=True,
        headless=False, fingerprints_fixture_path=mock,
    ) as launch:
        result = await launch['page'].evaluate('''() => {
            const c=document.createElement('canvas').getContext('2d');
            const rows=[];
            for(const textRendering of ['auto','geometricPrecision']) {
                c.textRendering=textRendering;c.font='17px Arimo';
                const m=c.measureText('Latin 0123 Привет');
                rows.push({textRendering,width:m.width,left:m.actualBoundingBoxLeft,
                    right:m.actualBoundingBoxRight,ascent:m.actualBoundingBoxAscent,
                    descent:m.actualBoundingBoxDescent});
            }
            return rows;
        }''')
    assert len(result) == 2 and all(row['width'] > 0 for row in result)
    record_property('font_render_metrics', json.dumps({'policy':font_policy,'rows':result}))
    if font_policy != 'native':
        assert result[0]['width'] == result[1]['width'], result
        # Outline metrics for the pinned Arimo-Regular.ttf catalog asset and
        # this fixed 17px recipe, independently recorded on managed macOS.
        # Integer mask bounds must not replace these on another host OS.
        expected = {'width':140.814453125, 'left':-1.39453125,
                    'right':140.52392578125, 'ascent':12.318359375,
                    'descent':3.52783203125}
        for row in result:
            assert {key:row[key] for key in expected} == pytest.approx(expected, abs=1e-6)

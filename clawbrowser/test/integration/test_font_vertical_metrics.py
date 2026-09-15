"""Observe vertical font metrics independently of painted glyph bounds."""
import json

import pytest

from conftest import _launch_browser_with_details
from test_isolated_canvas_control import canvas_control_files


@pytest.mark.asyncio
@pytest.mark.parametrize('font_policy', ['native', 'native_or_allowlist'])
async def test_font_vertical_metrics(tmp_path, record_property, font_policy):
    fixture, mock = canvas_control_files(tmp_path, 'override')
    for path in (fixture, mock):
        data = json.loads(path.read_text())
        data['response']['fingerprint']['surface_policy']['fonts']['mode'] = font_policy
        path.write_text(json.dumps(data))
    async with _launch_browser_with_details(
        fixture_name=str(fixture), backend_mode='mock', skip_verify=True,
        headless=False, fingerprints_fixture_path=mock,
    ) as launch:
        rows = await launch['page'].evaluate('''() => {
            const c=document.createElement('canvas').getContext('2d'), rows=[];
            for (const family of ['Arimo','Tinos','Cousine','sans-serif','serif','monospace']) {
            for (const size of [2,8,12,16,17,24,31.5]) {
            for (const textRendering of ['auto','geometricPrecision']) {
                c.textRendering=textRendering;c.font=`${size}px ${family}`;
                const m=c.measureText('Latin 0123 Привет العربية 漢字');
                rows.push({family,size,textRendering,
                    ascent:m.fontBoundingBoxAscent,descent:m.fontBoundingBoxDescent});
            }}}
            return rows;
        }''')
    record_property('font_vertical_metrics', json.dumps({'policy':font_policy,'rows':rows}))
    assert len(rows) == 84
    assert all(row['ascent'] > 0 and row['descent'] >= 0 for row in rows)

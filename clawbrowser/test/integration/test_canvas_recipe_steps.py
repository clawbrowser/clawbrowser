"""Opt-in localization of runtime-optimization dependent raster differences."""
import hashlib
import json
import os

import pytest

from conftest import _launch_browser_with_details
from test_isolated_canvas_control import CANVAS_CONTEXT_RECIPE, canvas_control_files


@pytest.mark.asyncio
async def test_canvas_recipe_steps(tmp_path, record_property):
    if os.environ.get('CLAWBROWSER_QA_CANVAS_RECIPE_STEPS') != '1':
        pytest.skip('opt-in per-draw diagnostic')
    fixture, mock = canvas_control_files(tmp_path, 'override')
    controls = []
    for args in ([], ['--disable-skia-runtime-opts']):
        async with _launch_browser_with_details(
            fixture_name=str(fixture), backend_mode='mock', skip_verify=True,
            headless=False, extra_browser_args=args, fingerprints_fixture_path=mock,
        ) as launch:
            rows = await launch['page'].evaluate('''() => {
            ''' + CANVAS_CONTEXT_RECIPE + '''
                const rows=[];
                for(const seed of [7,65537]) {
                    render(document.createElement('canvas'),true,seed,
                        (step,pixels)=>rows.push({seed,step,pixels}));
                }
                return rows;
            }''')
        assert len(rows) == 34
        controls.append({'args':args, 'rows':rows})
    observations=[]
    for a,b in zip(controls[0]['rows'],controls[1]['rows']):
        assert (a['seed'],a['step']) == (b['seed'],b['step'])
        assert len(a['pixels']) == len(b['pixels']) == 192 * 128 * 4
        delta=[abs(x-y) for x,y in zip(a['pixels'],b['pixels'])]
        observations.append({'seed':a['seed'],'step':a['step'],
            'hashes':[hashlib.sha256(bytes(r['pixels'])).hexdigest() for r in (a,b)],
            'changed_channels':sum(d != 0 for d in delta),'max_delta':max(delta),
            'changed_alpha':sum(d != 0 for d in delta[3::4])})
    record_property('canvas_recipe_steps', json.dumps(observations))
    assert all(row['changed_channels'] == 0 for row in observations), observations

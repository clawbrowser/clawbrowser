"""A local variable face in a nonzero TTC slot must retain its variation axes."""
import json
import sys

import pytest

from conftest import _launch_browser_with_details


@pytest.mark.asyncio
@pytest.mark.parametrize('region', ['JP', 'SC', 'KR', 'TC', 'HK'])
async def test_local_cjk_collection_preserves_weight_axis(record_property, region):
    if sys.platform not in ('linux', 'darwin', 'win32'):
        pytest.skip('desktop closed catalog')
    async with _launch_browser_with_details(
            fixture_name='valid_fingerprint.json', backend_mode='mock',
            skip_verify=True, headless=False) as launch:
        result = await launch['page'].evaluate('''async requestedFamily => {
          const face = new FontFace('CollectionProbe', 'local(' + JSON.stringify(requestedFamily) + ')',
                                    {weight: '100 900'});
          await face.load(); document.fonts.add(face);
          const ctx = document.createElement('canvas').getContext('2d');
          const rows = {};
          for (const family of ['CollectionProbe', requestedFamily]) {
            rows[family] = {};
            for (const weight of [100, 900]) {
              ctx.font = weight + ' 40px "' + family + '"';
              rows[family][weight] = ['Hamburgefontsiv0123', '漢字中文'].map(text => {
                const m = ctx.measureText(text);
                return [m.width, m.actualBoundingBoxLeft, m.actualBoundingBoxRight,
                        m.actualBoundingBoxAscent, m.actualBoundingBoxDescent];
              });
            }
          }
          document.fonts.delete(face);
          return {status: face.status, rows};
        }''', 'Noto Sans CJK ' + region)
    record_property('local_collection_variations', json.dumps(result))
    assert result['status'] == 'loaded'
    direct = result['rows']['Noto Sans CJK ' + region]
    local = result['rows']['CollectionProbe']
    assert direct['100'] != direct['900'], 'direct catalog variable control must change'
    assert local['100'] != local['900'], 'local TTC lookup silently ignored the weight axis'

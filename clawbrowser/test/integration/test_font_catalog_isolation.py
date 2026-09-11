"""Release gate: a managed identity must not inherit the host font catalog.

Set CLAWBROWSER_TEST_FONTCONFIG_A/B to isolated Fontconfig XML files using
different real catalogs. Do not point both variables to the same catalog.
This optional environment-dependent test currently exposes the unresolved
generic/character fallback gap; a skip is not a privacy acceptance pass.
"""

import os
from pathlib import Path
import sys

import pytest

from conftest import _launch_browser_with_details


@pytest.mark.asyncio
async def test_text_metrics_are_independent_of_host_font_catalog():
    paths = [os.environ.get('CLAWBROWSER_TEST_FONTCONFIG_' + suffix)
             for suffix in ('A', 'B')]
    if sys.platform != 'linux' or not all(paths):
        pytest.skip('requires Linux and two isolated host Fontconfig catalogs')
    configs = [Path(value).resolve() for value in paths]
    assert configs[0] != configs[1], 'independent catalog controls required'
    assert all(path.is_file() for path in configs)
    assert configs[0].read_bytes() != configs[1].read_bytes()
    results = []
    for config in configs:
        async with _launch_browser_with_details(
                fixture_name='valid_fingerprint.json', backend_mode='mock',
                skip_verify=True, headless=False,
                extra_env={'FONTCONFIG_FILE': str(config)}) as launch:
            results.append(await launch['page'].evaluate("""() => {
                const ctx = document.createElement('canvas').getContext('2d');
                const rows = [];
                for (const family of ['serif', 'sans-serif', 'monospace',
                                      'Arial', 'Times New Roman']) {
                    const generic = ['serif', 'sans-serif', 'monospace'].includes(family);
                    ctx.font = '32px ' + (generic ? family : '"' + family + '"');
                    for (const text of ['mmmmWWWW0123456789', 'Привет мир',
                                        'العربية', '漢字']) {
                        const m = ctx.measureText(text);
                        rows.push({family, text, width: m.width,
                            left: m.actualBoundingBoxLeft,
                            right: m.actualBoundingBoxRight,
                            ascent: m.actualBoundingBoxAscent});
                    }
                }
                return rows;
            }"""))
    assert len(results[0]) == len(results[1]) == 20
    changed = [{'catalogA': a, 'catalogB': b}
               for a, b in zip(*results) if a != b]
    assert not changed, f'Host font catalog changed managed text metrics: {changed}'

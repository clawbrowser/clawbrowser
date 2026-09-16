"""Managed system fonts must use catalog sans, not host desktop preferences."""
import sys

import pytest

from conftest import _launch_browser_with_details


@pytest.mark.asyncio
async def test_managed_system_ui_is_catalog_sans():
    if sys.platform not in ('linux', 'darwin', 'win32'):
        pytest.skip('desktop font catalog gate')
    async with _launch_browser_with_details(
            fixture_name='valid_fingerprint.json', backend_mode='mock',
            skip_verify=True, headless=False) as launch:
        rows = await launch['page'].evaluate('''() => {
          const ctx = document.createElement('canvas').getContext('2d');
          const result = {};
          for (const family of ['system-ui', 'Arimo', 'Tinos']) {
            ctx.font = '32px ' + family;
            result[family] = ['mmmmWWWW0123456789', 'Hamburgefontsiv', 'Привет мир'].map(text => {
              const m = ctx.measureText(text);
              return [m.width, m.actualBoundingBoxLeft, m.actualBoundingBoxRight,
                      m.actualBoundingBoxAscent, m.actualBoundingBoxDescent];
            });
          }
          return result;
        }''')
    assert rows['Arimo'] != rows['Tinos'], 'negative serif control must differ'
    assert rows['system-ui'] == rows['Arimo'], rows


@pytest.mark.asyncio
async def test_managed_system_font_keywords_do_not_expose_host_menu_metrics():
    if sys.platform not in ('linux', 'darwin', 'win32'):
        pytest.skip('desktop font catalog gate')
    async with _launch_browser_with_details(
            fixture_name='valid_fingerprint.json', backend_mode='mock',
            skip_verify=True, headless=False) as launch:
        rows = await launch['page'].evaluate('''() => {
          const rows = [];
          for (const keyword of ['caption', 'icon', 'menu', 'message-box',
                                 'small-caption', 'status-bar']) {
            const node = document.createElement('span');
            node.textContent = 'Menu caption 0123';
            node.style.font = keyword;
            document.body.append(node);
            const style = getComputedStyle(node);
            rows.push({keyword, accepted: CSS.supports('font', keyword),
                       family: style.fontFamily, size: style.fontSize});
            node.remove();
          }
          return rows;
        }''')
    assert len(rows) == 6
    assert all(row['accepted'] for row in rows), rows
    assert all(row['family'] == 'Arial' for row in rows), rows
    # Match the default provider and this fixture's default CSS font size.
    assert all(row['size'] == '16px' for row in rows), rows

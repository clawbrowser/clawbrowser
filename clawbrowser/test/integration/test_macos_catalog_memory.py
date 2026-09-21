"""Opt-in bounded cache-growth probe, not total memory or leak acceptance."""
import json
import os
import subprocess
import sys

import pytest

from conftest import _launch_browser_with_details

pytestmark = pytest.mark.skipif(
    sys.platform != "darwin" or os.environ.get("CLAWBROWSER_TEST_MEMORY") != "1",
    reason="requires explicit macOS process-memory experiment")


@pytest.mark.asyncio
async def test_repeated_catalog_workload_has_bounded_rss(record_property):
    async with _launch_browser_with_details(
        fixture_name="valid_fingerprint.json", backend_mode="mock",
        skip_verify=True, headless=False,
    ) as launch:
        page = launch["page"]
        browser_cdp = await page.context.browser.new_browser_cdp_session()
        page_cdp = await page.context.new_cdp_session(page)
        try:
            await page.set_content("<canvas width=600 height=120></canvas>")
            workload = """cycles => {
                const c=document.querySelector('canvas').getContext('2d');
                const families=['Arimo','Tinos','Cousine','DejaVu Sans',
                    'Noto Sans CJK JP','Noto Sans CJK SC','Noto Color Emoji'];
                let measured=0;
                for(let i=0;i<cycles;i++) for(const family of families) {
                    for(const weight of [100,400,900]) {
                        c.clearRect(0,0,600,120);
                        c.font=weight+' 32px "'+family+'"';
                        c.fillText('Latin 漢字 مرحبا 👩‍💻',2,50);
                        measured+=c.measureText('Latin 漢字 مرحبا 👩‍💻').width;
                    }
                }
                return measured;
            }"""
            await page.evaluate(workload, 5)
            processes = await browser_cdp.send("SystemInfo.getProcessInfo")
            pids = sorted(int(p["id"]) for p in processes["processInfo"]
                          if p["type"] == "renderer")
            assert pids
            observations = []
            for batch in range(6):
                if batch:
                    assert await page.evaluate(workload, 20) > 0
                await page_cdp.send("HeapProfiler.collectGarbage")
                current = await browser_cdp.send("SystemInfo.getProcessInfo")
                assert sorted(int(p["id"]) for p in current["processInfo"]
                              if p["type"] == "renderer") == pids
                rows = subprocess.check_output(
                    ["ps", "-o", "rss=", "-p", ",".join(map(str, pids))],
                    text=True, timeout=10).split()
                assert len(rows) == len(pids)
                observations.append(sum(map(int, rows)) * 1024)
            record_property("catalog_rss_probe", json.dumps({
                "renderer_count": len(pids), "rss_bytes": observations,
                "scope": "same-process fixed working set after warmup; RSS includes shared pages"}))
            # A generous regression guard, not an established product budget.
            assert max(observations[1:]) - observations[0] < 64 * 1024 * 1024, observations
        finally:
            await page_cdp.detach()
            await browser_cdp.detach()

"""Opt-in ingestion probe, not proof of closed system-font fallback.

CLAWBROWSER_TEST_FONT_ASSET_ROOT points at Chromium's pinned test_fonts folder.
Exercise the existing web-font decoder with the same bundled emoji bytes that
CoreText cannot instantiate, without registering/installing any host font.
"""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import os
from pathlib import Path
import sys
import threading

import pytest

from conftest import _launch_browser_with_details


@pytest.mark.asyncio
async def test_macos_bundled_emoji_fontations_ingestion(record_property):
    asset_root = os.environ.get("CLAWBROWSER_TEST_FONT_ASSET_ROOT")
    if sys.platform != "darwin" or not asset_root:
        pytest.skip("requires macOS and explicit pinned font asset directory")
    font_bytes = (Path(asset_root) / "NotoColorEmoji.ttf").read_bytes()

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path != "/emoji.ttf":
                self.send_error(404)
                return
            self.send_response(200)
            self.send_header("Content-Type", "font/ttf")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Content-Length", str(len(font_bytes)))
            self.end_headers()
            self.wfile.write(font_bytes)

        def log_message(self, *_):
            pass

    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        async with _launch_browser_with_details(
            fixture_name="valid_fingerprint.json", backend_mode="mock",
            skip_verify=True, headless=False,
        ) as launch:
            page = launch["page"]
            await page.evaluate("""async url => {
                const face = new FontFace('CatalogEmojiProbe', `url(${url})`);
                await face.load(); document.fonts.add(face);
                const span=document.createElement('span'); span.id='emoji';
                span.style.font='48px CatalogEmojiProbe'; span.textContent='😀';
                document.body.append(span); await document.fonts.ready;
            }""", f"http://127.0.0.1:{server.server_port}/emoji.ttf")
            cdp = await page.context.new_cdp_session(page)
            await cdp.send("DOM.enable")
            await cdp.send("CSS.enable")
            root = await cdp.send("DOM.getDocument")
            node = await cdp.send("DOM.querySelector", {"nodeId": root["root"]["nodeId"], "selector": "#emoji"})
            fonts = (await cdp.send("CSS.getPlatformFontsForNode", node))["fonts"]
            record_property("emoji_ingestion_fonts", str(fonts))
            assert fonts and all(f["isCustomFont"] and f["familyName"] == "Noto Color Emoji" for f in fonts), fonts
            assert sum(f["glyphCount"] for f in fonts) > 0
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)

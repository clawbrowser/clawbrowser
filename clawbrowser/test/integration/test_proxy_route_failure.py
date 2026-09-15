"""Controlled HTTP routing: proxy failure must never retry the origin directly."""
from contextlib import contextmanager
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import threading
from urllib.request import ProxyHandler, build_opener

import pytest
from playwright.async_api import Error

from conftest import _launch_browser_with_details


@contextmanager
def endpoint(body):
    hits = []

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            hits.append(self.path)
            self.send_response(200)
            self.send_header('Content-Type', 'text/html')
            self.send_header('Content-Length', str(len(body)))
            self.send_header('Connection', 'close')
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, *_):
            pass

    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        yield server, hits
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)


@pytest.mark.asyncio
async def test_proxy_disconnect_does_not_retry_direct(record_property):
    with endpoint(b'origin-control') as (origin, direct), endpoint(b'proxy-control') as (proxy, proxied):
        port = origin.server_port
        # Prove that the direct destination exists; no inherited HTTP proxy.
        opener = build_opener(ProxyHandler({}))
        assert opener.open(f'http://127.0.0.1:{port}/control', timeout=5).read() == b'origin-control'
        direct.clear()
        config = {'scheme': 'http', 'host': '127.0.0.1', 'port': proxy.server_port}
        async with _launch_browser_with_details(
            fixture_name='valid_fingerprint.json', backend_mode='mock',
            skip_verify=True, headless=False, proxy_config=config,
            extra_browser_args=('--host-resolver-rules=MAP proxy-route.invalid 127.0.0.1',),
        ) as launch:
            page = launch['page']
            target = f'http://proxy-route.invalid:{port}'
            await page.goto(target + '/before', wait_until='load')
            assert await page.locator('body').inner_text() == 'proxy-control'
            assert any('/before' in hit for hit in proxied)
            assert direct == []
            proxy.shutdown()
            proxy.server_close()
            with pytest.raises(Error) as failure:
                await page.goto(target + '/after', wait_until='load', timeout=15000)
            assert 'ERR_PROXY_CONNECTION_FAILED' in str(failure.value)
            assert direct == [], 'Browser contacted origin after proxy stopped'
            record_property('proxy_disconnect', json.dumps({
                'proxy_worked_before': True, 'error': 'ERR_PROXY_CONNECTION_FAILED',
                'direct_origin_hits_after_control': len(direct)}))


@pytest.mark.asyncio
@pytest.mark.parametrize("host", ["127.0.0.1", "localhost", "[::1]", "169.254.1.2", "[fe80::1234]"])
async def test_managed_proxy_does_not_bypass_loopback(record_property, host):
    with endpoint(b'origin-control') as (origin, direct), endpoint(b'proxy-control') as (proxy, proxied):
        config = {'scheme': 'http', 'host': '127.0.0.1', 'port': proxy.server_port}
        async with _launch_browser_with_details(
            fixture_name='valid_fingerprint.json', backend_mode='mock',
            skip_verify=True, headless=False, proxy_config=config,
        ) as launch:
            await launch['page'].goto(f'http://{host}:{origin.server_port}/loopback-probe')
            actual = await launch['page'].locator('body').inner_text()
            record_property('loopback_route', json.dumps({
                'destination_host': host,
                'direct_origin_hits': len(direct),
                'proxy_saw_probe': any('/loopback-probe' in hit for hit in proxied)}))
            assert actual == 'proxy-control', 'Implicit loopback bypass reached origin directly'
            assert any('/loopback-probe' in hit for hit in proxied)
            assert direct == []

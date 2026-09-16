"""Opt-in regression for direct DoH probes with a required profile proxy.

Only temporary test Local State is seeded. No host resolver settings change.
The receiver observes TLS ClientHello, not a successful DNS exchange.
"""
import json
import os
import socketserver

import pytest

import conftest
from canvas_network_mode import assert_canvas_network_mode, canvas_network_args
from conftest import _launch_browser_with_details
from test_proxy_route_failure import endpoint
from test_proxy_tls_hostname import serving


@pytest.mark.asyncio
@pytest.mark.skipif(os.environ.get('CLAWBROWSER_DOH_DIAGNOSTIC') != '1',
                    reason='Opt-in background DoH probe investigation')
@pytest.mark.parametrize('proxied', [False, True])
@pytest.mark.parametrize('mode', ['automatic', 'secure'])
async def test_background_doh_probe_obeys_profile_proxy(proxied, mode, monkeypatch, record_property):
    hello, connections = [], []

    class Doh(socketserver.BaseRequestHandler):
        def handle(self):
            connections.append(True)
            self.request.settimeout(3)
            try:
                prefix = b''
                while len(prefix) < 5:
                    chunk = self.request.recv(5 - len(prefix))
                    if not chunk:
                        break
                    prefix += chunk
                if len(prefix) == 5 and prefix[0] == 22 and prefix[1] == 3:
                    hello.append(True)
            except OSError:
                pass

    with serving(Doh) as doh_port, endpoint(b'proxy-page-control') as (proxy, hits):
        original = conftest._seed_config

        def seed(config_dir, base_url, api_key=None):
            original(config_dir, base_url, api_key)
            # With no API key, the vanilla control uses the isolated Auth
            # user-data directory (not Browser/Default).
            directory = config_dir / 'Browser' / ('test_profile' if proxied else 'Auth')
            directory.mkdir(parents=True, exist_ok=True)
            (directory / 'Local State').write_text(json.dumps({
                'dns_over_https': {'mode': mode,
                    'templates': f'https://127.0.0.1:{doh_port}/dns-query'}}))

        monkeypatch.setattr(conftest, '_seed_config', seed)
        async with _launch_browser_with_details(
            fixture_name='valid_fingerprint.json' if proxied else None,
            backend_mode='mock' if proxied else 'vanilla',
            skip_verify=True, headless=False,
            proxy_config={'scheme':'http','host':'127.0.0.1','port':proxy.server_port} if proxied else None,
            extra_browser_args=canvas_network_args() if proxied else ('--no-proxy-server',),
        ) as launch:
            page = launch['page']
            await page.goto(f'http://127.0.0.1:{proxy.server_port}/control')
            assert await page.locator('body').inner_text() == 'proxy-page-control'
            assert (f'http://127.0.0.1:{proxy.server_port}/control'
                    if proxied else '/control') in hits
            if proxied:
                record_property('canvas_mode', json.dumps(await assert_canvas_network_mode(page)))
            await page.wait_for_timeout(8000)
            record_property('doh_probe', json.dumps({'proxied':proxied,
                'mode':mode,'direct_tcp_connections':len(connections),
                'direct_tls_client_hello':len(hello),'window_seconds':8}))
            state = launch['config_dir'] / 'Browser' / ('test_profile' if proxied else 'Auth') / 'Local State'
            assert json.loads(state.read_text())['dns_over_https']['mode'] == mode
            if proxied:
                assert not connections, 'DoH probe connected directly despite a profile proxy'
            else:
                assert hello, 'No live unproxied browser DoH control; test is inconclusive'

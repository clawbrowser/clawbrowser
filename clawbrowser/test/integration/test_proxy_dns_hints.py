"""Opt-in stimuli for an operator-observed DNS prefetch/preconnect gate.

Pytest success alone does not accept DNS safety: the capture runner must observe
both unproxied browser controls and zero proxied queries, with zero capture loss.
"""
import json
import os
import uuid

import pytest

from conftest import _launch_browser_with_details
from canvas_network_mode import assert_canvas_network_mode, canvas_network_args
from test_proxy_hostname_routing import TARGET_HOST, socks_endpoint
from test_proxy_route_failure import endpoint


@pytest.mark.asyncio
@pytest.mark.skipif(os.environ.get('CLAWBROWSER_DNS_HINT_CAPTURE') != '1',
                    reason='Requires concurrent DNS capture and browser controls')
@pytest.mark.parametrize('kind', ['direct', 'http', 'socks5', 'socks5-auth'])
async def test_dns_hint_capture_stimulus(kind, record_property):
    observation_seconds = int(os.environ.get('CLAWBROWSER_DNS_HINT_SECONDS', '4'))
    assert 4 <= observation_seconds <= 30
    manager = endpoint(b'proxy-name-control') if kind in ('direct', 'http') else socks_endpoint(kind == 'socks5-auth')
    with manager as state:
        server = state[0]
        config = {'scheme': 'http' if kind == 'http' else 'socks5',
                  'host': '127.0.0.1', 'port': server.server_address[1]}
        if kind == 'socks5-auth':
            config.update(username='qa-user', password='qa-password')
        direct = kind == 'direct'
        async with _launch_browser_with_details(
            fixture_name=None if direct else 'valid_fingerprint.json',
            backend_mode='vanilla' if direct else 'mock',
            skip_verify=True, headless=False,
            proxy_config=None if direct else config,
            extra_browser_args=('--no-proxy-server',) if direct else canvas_network_args(),
        ) as launch:
            page = launch['page']
            url = f'http://127.0.0.1:{server.server_address[1]}/' if direct else f'http://{TARGET_HOST}/'
            await page.goto(url)
            assert await page.locator('body').inner_text() == 'proxy-name-control'
            if not direct:
                record_property('canvas_mode', json.dumps(await assert_canvas_network_mode(page)))
            token = uuid.uuid4().hex
            hints = [{'rel': rel, 'host': f'qa-hint-{kind}-{short}-{token}.example.com'}
                     for rel, short in [('dns-prefetch', 'dns'), ('preconnect', 'connect')]]
            count = await page.evaluate('''hints => {
                const meta = document.createElement('meta');
                meta.httpEquiv = 'x-dns-prefetch-control'; meta.content = 'on';
                document.head.append(meta);
                for (const hint of hints) {
                    const link = document.createElement('link');
                    link.rel = hint.rel; link.href = 'http://' + hint.host;
                    document.head.append(link);
                }
                return document.querySelectorAll('link').length;
            }''', hints)
            assert count == 2
            await page.wait_for_timeout(observation_seconds * 1000)
            record_property('dns_hints', json.dumps({'kind': kind, 'hints': hints,
                'observation_seconds': observation_seconds, 'requires_external_capture_acceptance': True}))

"""Origin names must reach the proxy without requiring local resolution.

This is a resolver-dependency regression, not a packet-level DNS leak audit.
The proxy endpoint is an IP literal; proxy-host bootstrap DNS is out of scope.
"""
from contextlib import contextmanager
import json
import socket
import socketserver
import threading
import uuid

import pytest

from conftest import _launch_browser_with_details
from test_proxy_route_failure import endpoint

TARGET_HOST = f'qa-dns-route-{uuid.uuid4().hex}.invalid'


@contextmanager
def socks_endpoint(authenticated):
    connections = []
    errors = []

    class Handler(socketserver.StreamRequestHandler):
        def handle(self):
            self.connection.settimeout(5)

            def read(size):
                value = self.rfile.read(size)
                if len(value) != size:
                    raise EOFError('Incomplete SOCKS request')
                return value

            try:
                version, count = read(2)
                assert version == 5
                methods = read(count)
                method = 2 if authenticated else 0
                assert method in methods
                self.wfile.write(bytes([5, method]))
                if authenticated:
                    assert read(1) == b'\x01'
                    assert read(read(1)[0]) == b'qa-user'
                    assert read(read(1)[0]) == b'qa-password'
                    self.wfile.write(b'\x01\x00')
                version, command, reserved, address_type = read(4)
                assert (version, command, reserved) == (5, 1, 0)
                if address_type == 3:
                    host = read(read(1)[0]).decode('ascii')
                elif address_type == 1:
                    host = socket.inet_ntop(socket.AF_INET, read(4))
                elif address_type == 4:
                    host = socket.inet_ntop(socket.AF_INET6, read(16))
                else:
                    raise AssertionError(f'Invalid SOCKS address type {address_type}')
                port = int.from_bytes(read(2), 'big')
                connections.append((host, port))
                self.wfile.write(b'\x05\x00\x00\x01\x7f\x00\x00\x01\x00\x50')
                request = self.rfile.readline(8192)
                if not request:
                    return
                # Only the controlled HTTP destination gets a response.
                if host != TARGET_HOST or port != 80:
                    return
                while self.rfile.readline(8192) not in (b'\r\n', b''):
                    pass
                body = b'proxy-name-control'
                self.wfile.write(
                    b'HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n'
                    + f'Content-Length: {len(body)}\r\n'.encode('ascii')
                    + b'Connection: close\r\n\r\n' + body)
            except (EOFError, ConnectionError, TimeoutError):
                # Speculative sockets may be closed without sending a request.
                pass
            except Exception as error:
                errors.append(str(error))

    class Server(socketserver.ThreadingTCPServer):
        daemon_threads = True

    server = Server(('127.0.0.1', 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        yield server, connections, errors
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)


@pytest.mark.asyncio
@pytest.mark.parametrize('kind', ['http', 'socks5', 'socks5-auth'])
@pytest.mark.parametrize('resolution', ['forced_not_found', 'system_resolver'])
async def test_origin_hostname_forwarded_to_proxy(kind, resolution, record_property):
    host = TARGET_HOST
    manager = endpoint(b'proxy-name-control') if kind == 'http' else socks_endpoint(kind == 'socks5-auth')
    with manager as state:
        server, requests = state[:2]
        config = {'scheme': 'http' if kind == 'http' else 'socks5',
                  'host': '127.0.0.1', 'port': server.server_address[1]}
        if kind == 'socks5-auth':
            config.update(username='qa-user', password='qa-password')
        async with _launch_browser_with_details(
            fixture_name='valid_fingerprint.json', backend_mode='mock',
            skip_verify=True, headless=False, proxy_config=config,
            extra_browser_args=(f'--host-resolver-rules=MAP {host} ~NOTFOUND',)
            if resolution == 'forced_not_found' else (),
        ) as launch:
            await launch['page'].goto(f'http://{host}/name-control', timeout=15000)
            assert await launch['page'].locator('body').inner_text() == 'proxy-name-control'
            if kind == 'http':
                assert f'http://{host}/name-control' in requests
            else:
                assert (host, 80) in requests
                assert not state[2], state[2]
            record_property('origin_hostname_route', json.dumps({
                'proxy': kind, 'hostname_forwarded': True,
                'origin_local_resolution': resolution,
                'scope': 'resolver dependency, not DNS packet capture'}))

"""Controlled TLS routing via CONNECT/SOCKS, not public-CA acceptance.

Only the generated certificate's SPKI is exempted from browser trust checks.
The independent Python TLS control validates that certificate and hostname.
"""
import base64
from contextlib import contextmanager
import hashlib
import json
import select
import shutil
import socket
import socketserver
import ssl
import subprocess
import threading
import uuid

import pytest

from canvas_network_mode import assert_canvas_network_mode, canvas_network_args
from conftest import _launch_browser_with_details


@contextmanager
def serving(handler):
    class Server(socketserver.ThreadingTCPServer):
        daemon_threads = True
    server = Server(('127.0.0.1', 0), handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        yield server.server_address[1]
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)


def http_response(body):
    return (b'HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n' +
            f'Content-Length: {len(body)}\r\n'.encode() +
            b'Connection: close\r\n\r\n' + body)


@pytest.mark.asyncio
@pytest.mark.parametrize('kind', ['http', 'socks5', 'socks5-auth'])
@pytest.mark.parametrize('resolution', ['system_resolver', 'forced_not_found'])
async def test_https_origin_hostname_reaches_proxy(kind, resolution, tmp_path, record_property):
    if not shutil.which('openssl'):
        pytest.skip('openssl is required for the temporary TLS certificate')
    # Use an ordinary DNS suffix, not .invalid which a resolver may reject
    # locally without issuing a packet regardless of proxy configuration.
    host = f'qa-tls-route-{uuid.uuid4().hex}.example.com'
    cert, key = tmp_path / 'certificate.pem', tmp_path / 'key.pem'
    subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
                    '-keyout', str(key), '-out', str(cert), '-days', '1',
                    '-subj', f'/CN={host}', '-addext', f'subjectAltName=DNS:{host}'],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    key.chmod(0o600)
    pub = subprocess.check_output(['openssl', 'x509', '-in', str(cert), '-pubkey', '-noout'])
    der = subprocess.check_output(['openssl', 'pkey', '-pubin', '-outform', 'DER'], input=pub)
    spki = base64.b64encode(hashlib.sha256(der).digest()).decode()
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(cert, key)
    hits, routes, errors = [], [], []

    class Origin(socketserver.BaseRequestHandler):
        def handle(self):
            self.request.settimeout(5)
            try:
                with context.wrap_socket(self.request, server_side=True) as conn:
                    with conn.makefile('rb') as reader:
                        request = reader.readline(8192)
                        if not request:
                            return
                        while reader.readline(8192) not in (b'\r\n', b''):
                            pass
                        hits.append((request.decode('ascii').split()[1], conn.version()))
                        conn.sendall(http_response(b'tls-proxy-control'))
            except (OSError, ValueError):
                pass  # Speculative sockets may close without an HTTP request.

    with serving(Origin) as origin_port:
        # A live independently verified TLS response is mandatory, not just
        # an empty browser error or absent DNS packets.
        control_context = ssl.create_default_context(cafile=str(cert))
        with socket.create_connection(('127.0.0.1', origin_port), timeout=5) as raw:
            with control_context.wrap_socket(raw, server_hostname=host) as conn:
                conn.sendall(f'GET /tls-control HTTP/1.1\r\nHost: {host}\r\n\r\n'.encode())
                response = b''
                while chunk := conn.recv(4096):
                    response += chunk
                assert response.endswith(b'tls-proxy-control')

        class Proxy(socketserver.StreamRequestHandler):
            def handle(self):
                self.connection.settimeout(5)

                def read(size):
                    data = self.rfile.read(size)
                    if len(data) != size:
                        raise EOFError()
                    return data

                try:
                    if kind == 'http':
                        line = self.rfile.readline(8192).decode('ascii')
                        if not line:
                            return
                        while self.rfile.readline(8192) not in (b'\r\n', b''):
                            pass
                        method, authority, _ = line.split()
                        if method != 'CONNECT':
                            self.wfile.write(http_response(b'proxy-bootstrap'))
                            return
                        target, port = authority.rsplit(':', 1)
                        port = int(port)
                        success = b'HTTP/1.1 200 Connection Established\r\n\r\n'
                    else:
                        version, count = read(2)
                        assert version == 5
                        methods = read(count)
                        method = 2 if kind == 'socks5-auth' else 0
                        assert method in methods
                        self.wfile.write(bytes([5, method]))
                        if method == 2:
                            assert read(1) == b'\x01'
                            assert read(read(1)[0]) == b'qa-user'
                            assert read(read(1)[0]) == b'qa-password'
                            self.wfile.write(b'\x01\x00')
                        version, command, reserved, address_type = read(4)
                        assert (version, command, reserved) == (5, 1, 0)
                        if address_type == 3:
                            target = read(read(1)[0]).decode('ascii')
                        elif address_type in (1, 4):
                            target = socket.inet_ntop(socket.AF_INET if address_type == 1 else socket.AF_INET6,
                                                      read(4 if address_type == 1 else 16))
                        else:
                            raise AssertionError('Invalid SOCKS address type')
                        port = int.from_bytes(read(2), 'big')
                        success = b'\x05\x00\x00\x01\x7f\x00\x00\x01\x00\x50'
                    if (target, port) != (host, origin_port):
                        return  # Never forward unrelated traffic.
                    routes.append((target, port))
                    with socket.create_connection(('127.0.0.1', origin_port), timeout=5) as upstream:
                        self.wfile.write(success)
                        while True:
                            ready, _, _ = select.select([self.connection, upstream], [], [], 5)
                            if not ready:
                                return
                            for source in ready:
                                data = source.recv(65536)
                                if not data:
                                    return
                                (upstream if source is self.connection else self.connection).sendall(data)
                except (OSError, EOFError):
                    pass
                except Exception as error:
                    errors.append(str(error))

        with serving(Proxy) as proxy_port:
            proxy = {'scheme': 'http' if kind == 'http' else 'socks5',
                     'host': '127.0.0.1', 'port': proxy_port}
            if kind == 'socks5-auth':
                proxy.update(username='qa-user', password='qa-password')
            args = (*canvas_network_args(), f'--ignore-certificate-errors-spki-list={spki}')
            if resolution == 'forced_not_found':
                args += (f'--host-resolver-rules=MAP {host} ~NOTFOUND',)
            async with _launch_browser_with_details(
                fixture_name='valid_fingerprint.json', backend_mode='mock',
                skip_verify=True, headless=False, proxy_config=proxy,
                extra_browser_args=args,
            ) as launch:
                page = launch['page']
                await page.goto(f'https://{host}:{origin_port}/browser-tls', timeout=15000)
                assert await page.locator('body').inner_text() == 'tls-proxy-control'
                assert (host, origin_port) in routes
                assert any(path == '/browser-tls' and tls in ('TLSv1.2', 'TLSv1.3') for path, tls in hits)
                assert not errors, errors
                record_property('tls_hostname_route', json.dumps({
                    'proxy': kind, 'resolution': resolution, 'hostname_forwarded': True,
                    'independent_tls_control': True, 'browser_tls_response': True,
                    'browser_certificate_policy': 'temporary test SPKI exception',
                    'canvas': await assert_canvas_network_mode(page)}))

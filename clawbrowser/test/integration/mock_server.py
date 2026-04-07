#!/usr/bin/env python3
"""Local mock backend for clawbrowser browser integration tests."""

import argparse
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

WORKSPACE_ROOT = Path(__file__).resolve().parents[3]
FINGERPRINTS_FIXTURE = WORKSPACE_ROOT / "api/mocks/fingerprints.json"
PROXY_FIXTURE = WORKSPACE_ROOT / "api/mocks/proxy.json"
TEST_API_KEY = "test_api_key_123"


def _load_fixture(path: Path):
    with path.open(encoding="utf-8") as handle:
        return json.load(handle)


class MockHandler(BaseHTTPRequestHandler):
    server_version = "ClawbrowserMock/1.0"

    def do_OPTIONS(self):
        if self.path not in {"/v1/fingerprints/generate", "/v1/proxy/verify"}:
            self.send_error(404)
            return

        self.send_response(204)
        self._write_cors_headers()
        self.end_headers()

    def do_GET(self):
        if self.path == "/__healthz":
            self.send_response(200)
            self._write_cors_headers()
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(b'{"ok":true}\n')
            return

        if self.path == "/__headers":
            self._write_json(
                200,
                {
                    "headers": {
                        key: value for key, value in self.headers.items()
                    }
                },
            )
            return

        if self.path == "/__blank":
            blank_html = b"<!DOCTYPE html><html><body></body></html>\n"
            self.send_response(200)
            self._write_cors_headers()
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(blank_html)))
            self.end_headers()
            self.wfile.write(blank_html)
            return

        if self.path == "/__verify" and self.server.verify_html_path is not None:
            html_bytes = self.server.verify_html_path.read_bytes()
            self.send_response(200)
            self._write_cors_headers()
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(html_bytes)))
            self.end_headers()
            self.wfile.write(html_bytes)
            return

        self.send_error(404)

    def do_POST(self):
        route_map = {
            "/v1/fingerprints/generate": self.server.fingerprints_fixture,
            "/v1/proxy/verify": self.server.proxy_fixture,
        }
        fixture_path = route_map.get(self.path)
        if fixture_path is None:
            self.send_error(404)
            return

        auth_header = self.headers.get("Authorization")
        if auth_header != f"Bearer {TEST_API_KEY}":
            self._write_json(
                401,
                {"code": "invalid_api_key", "message": "invalid API key"},
            )
            return

        content_length = int(self.headers.get("Content-Length", "0"))
        raw_body = self.rfile.read(content_length)
        try:
            body = json.loads(raw_body.decode("utf-8"))
        except (json.JSONDecodeError, UnicodeDecodeError):
            self._write_json(
                400,
                {"code": "invalid_json", "message": "request body was not valid JSON"},
            )
            return

        fixture = _load_fixture(fixture_path)
        if body != fixture["request"]:
            self._write_json(
                400,
                {
                    "code": "request_mismatch",
                    "message": "request body did not match mock fixture",
                    "expected": fixture["request"],
                    "actual": body,
                },
            )
            return

        self._write_json(200, fixture["response"])

    def log_message(self, fmt, *args):
        print(fmt % args, flush=True)

    def _write_json(self, status_code: int, payload):
        encoded = json.dumps(payload).encode("utf-8")
        self.send_response(status_code)
        self._write_cors_headers()
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)

    def _write_cors_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header(
            "Access-Control-Allow-Headers",
            "Authorization, Content-Type",
        )
        self.send_header(
            "Access-Control-Allow-Methods",
            "GET, POST, OPTIONS",
        )


class MockHTTPServer(ThreadingHTTPServer):
    def __init__(
        self,
        server_address,
        handler_class,
        *,
        fingerprints_fixture,
        proxy_fixture,
        verify_html_path,
    ):
        super().__init__(server_address, handler_class)
        self.fingerprints_fixture = fingerprints_fixture
        self.proxy_fixture = proxy_fixture
        self.verify_html_path = verify_html_path


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8787)
    parser.add_argument("--once", action="store_true")
    parser.add_argument(
        "--fingerprints-fixture",
        default=str(FINGERPRINTS_FIXTURE),
    )
    parser.add_argument(
        "--proxy-fixture",
        default=str(PROXY_FIXTURE),
    )
    parser.add_argument("--verify-html")
    return parser.parse_args()


def main():
    args = parse_args()
    server = MockHTTPServer(
        (args.host, args.port),
        MockHandler,
        fingerprints_fixture=Path(args.fingerprints_fixture),
        proxy_fixture=Path(args.proxy_fixture),
        verify_html_path=Path(args.verify_html) if args.verify_html else None,
    )

    if args.once:
        server.handle_request()
        return

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()

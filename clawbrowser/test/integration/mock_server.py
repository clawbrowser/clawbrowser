#!/usr/bin/env python3
"""Local mock backend for clawbrowser browser integration tests."""

import argparse
import json
import http.client
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit

WORKSPACE_ROOT = Path(__file__).resolve().parents[3]
FINGERPRINTS_FIXTURE = WORKSPACE_ROOT / "api/mocks/fingerprints.json"
PROXY_FIXTURE = WORKSPACE_ROOT / "api/mocks/proxy.json"
TEST_API_KEY = "test_api_key_123"
TOP_UP_BYTES = 1024 * 1024 * 1024
DASHBOARD_URL = "https://app.clawbrowser.ai/dashboard"

# Mutable proxy-traffic state so top-up is observable across requests.
# New accounts start with a single GiB; the mock impersonates an internal
# account so the top-up route stays exercisable. Flip TOP_UP_AVAILABLE to model
# an external account, which the backend answers with 403 and which must ask
# for traffic in Discord instead.
TOP_UP_AVAILABLE = True
REQUEST_URL = "https://discord.gg/CK62brtKhe"
TRAFFIC = {
    "used_bytes": 0,
    "limit_bytes": TOP_UP_BYTES,
}


def _load_fixture(path: Path):
    with path.open(encoding="utf-8") as handle:
        return json.load(handle)


def _traffic_stats():
    used = TRAFFIC["used_bytes"]
    limit = TRAFFIC["limit_bytes"]
    remaining = max(limit - used, 0)
    percent = (used / limit * 100) if limit else 0.0

    if remaining <= 0:
        state = "exhausted"
    elif percent >= 80:
        state = "near_limit"
    else:
        state = "ok"

    return {
        "limited": True,
        "used_bytes": used,
        "limit_bytes": limit,
        "remaining_bytes": remaining,
        "percent_used": round(percent, 2),
        "state": state,
        "top_up_available": TOP_UP_AVAILABLE,
        "top_up_bytes": TOP_UP_BYTES if TOP_UP_AVAILABLE else 0,
        "dashboard_url": DASHBOARD_URL,
        "request_url": REQUEST_URL,
    }


def _request_matches_fixture(path: str, actual: dict, expected: dict) -> bool:
    if path != "/v1/fingerprints/generate":
        return actual == expected
    return all(actual.get(key) == value for key, value in expected.items())


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
        # Act as a bounded fixture HTTP proxy, never as an Internet proxy.
        # Surface tests need genuine localhost origins (secure-context APIs),
        # but the browser must reach them through its configured proxy.
        if self.path.startswith("http://") or self.path.startswith("https://"):
            target = urlsplit(self.path)
            if target.scheme != "http" or target.hostname not in {"127.0.0.1", "localhost"}:
                self.send_error(403, "Only local HTTP fixture destinations are permitted")
                return
            path = target.path or "/"
            if target.query:
                path += "?" + target.query
            connection = http.client.HTTPConnection("127.0.0.1", target.port or 80, timeout=5)
            try:
                headers = {k: v for k, v in self.headers.items()
                           if k.lower() not in {"proxy-authorization", "proxy-connection", "connection"}}
                connection.request("GET", path, headers=headers)
                response = connection.getresponse()
                body = response.read()
                self.send_response(response.status)
                for key, value in response.getheaders():
                    if key.lower() not in {"connection", "transfer-encoding", "content-length"}:
                        self.send_header(key, value)
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
            except OSError:
                self.send_error(502)
            finally:
                connection.close()
            return

        if self.path == "/v1/auth/api-key/validate":
            if not self._require_api_key():
                return
            self._write_json(
                200,
                {
                    "valid": True,
                    "key_id": "mock_key_id",
                    "owner_id": "mock_owner_id",
                },
            )
            return

        if self.path == "/v1/proxy/traffic":
            if not self._require_api_key():
                return
            self._write_json(200, _traffic_stats())
            return

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

        if self.path == "/__identity-worker.js":
            source = b"""
self.addEventListener('install', event => event.waitUntil(self.skipWaiting()));
self.addEventListener('activate', event => event.waitUntil(self.clients.claim()));
self.addEventListener('message', event => event.waitUntil((async () => {
  const result = {
    ua: navigator.userAgent, platform: navigator.platform,
    languages: [...navigator.languages], cores: navigator.hardwareConcurrency,
    memory: navigator.deviceMemory,
    timezone: Intl.DateTimeFormat().resolvedOptions().timeZone,
    offset: new Date().getTimezoneOffset(),
    hints: navigator.userAgentData ? await navigator.userAgentData.getHighEntropyValues(
      ['fullVersionList', 'platformVersion', 'architecture', 'bitness']) : null,
  };
  event.ports[0].postMessage(result);
})()));
"""
            self.send_response(200)
            self.send_header("Content-Type", "text/javascript")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(source)))
            self.end_headers()
            self.wfile.write(source)
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
        if self.path == "/v1/telemetry/tool":
            self._drain_body()
            self.send_response(204)
            self._write_cors_headers()
            self.end_headers()
            return

        if self.path == "/v1/usage/proxy-traffic/top-up":
            if not self._require_api_key():
                return
            self._drain_body()
            if not TOP_UP_AVAILABLE:
                self._write_json(
                    403,
                    {
                        "code": "proxy_traffic_top_up_forbidden",
                        "message": (
                            "Adding proxy traffic from the app is limited to internal "
                            "accounts. Request more traffic in the Clawbrowser Discord "
                            "and the team will add it."
                        ),
                    },
                )
                return
            TRAFFIC["limit_bytes"] += TOP_UP_BYTES
            self._write_json(200, _traffic_stats())
            return

        route_map = {
            "/v1/fingerprints/generate": self.server.fingerprints_fixture,
            "/v1/proxy/verify": self.server.proxy_fixture,
        }
        fixture_path = route_map.get(self.path)
        if fixture_path is None:
            self.send_error(404)
            return

        if not self._require_api_key():
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
        if not _request_matches_fixture(self.path, body, fixture["request"]):
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

    def _require_api_key(self) -> bool:
        auth_header = self.headers.get("Authorization")
        if auth_header == f"Bearer {TEST_API_KEY}":
            return True
        self._write_json(
            401,
            {"code": "invalid_api_key", "message": "invalid API key"},
        )
        return False

    def _drain_body(self):
        content_length = int(self.headers.get("Content-Length", "0"))
        if content_length:
            self.rfile.read(content_length)

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
    parser.add_argument("--traffic-used-bytes", type=int)
    parser.add_argument("--traffic-limit-bytes", type=int)
    return parser.parse_args()


def main():
    args = parse_args()
    if args.traffic_used_bytes is not None:
        TRAFFIC["used_bytes"] = args.traffic_used_bytes
    if args.traffic_limit_bytes is not None:
        TRAFFIC["limit_bytes"] = args.traffic_limit_bytes
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

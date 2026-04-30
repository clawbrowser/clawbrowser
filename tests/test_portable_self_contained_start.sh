#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${ROOT_DIR}/bin/clawbrowser"

fail() {
  printf 'FAIL: %s\n' "$*" >&2
  exit 1
}

skip() {
  printf 'ok - %s (skipped: %s)\n' "$1" "$2"
}

assert_contains() {
  local haystack="$1"
  local needle="$2"
  if [[ "${haystack}" != *"${needle}"* ]]; then
    fail "expected output to contain: ${needle}"
  fi
}

portable_platform_dir() {
  case "$(uname -m)" in
    x86_64|amd64) printf '%s\n' linux-amd64-glibc ;;
    arm64|aarch64) printf '%s\n' linux-arm64-glibc ;;
    *) return 1 ;;
  esac
}

portable_loader_filename() {
  case "$(uname -m)" in
    x86_64|amd64) printf '%s\n' ld-linux-x86-64.so.2 ;;
    arm64|aarch64) printf '%s\n' ld-linux-aarch64.so.1 ;;
    *) return 1 ;;
  esac
}

portable_arch_dir() {
  case "$(uname -m)" in
    x86_64|amd64) printf '%s\n' x86_64-linux-gnu ;;
    arm64|aarch64) printf '%s\n' aarch64-linux-gnu ;;
    *) return 1 ;;
  esac
}

make_fake_runtime() {
  local runtime_dir="$1"
  local arch_dir loader_name loader_path
  arch_dir="$(portable_arch_dir)" || fail "unsupported architecture for fake runtime"
  loader_name="$(portable_loader_filename)" || fail "unsupported architecture for fake runtime loader"
  loader_path="${runtime_dir}/lib/${arch_dir}/${loader_name}"
  mkdir -p \
    "${runtime_dir}/bin" \
    "${runtime_dir}/lib/${arch_dir}" \
    "${runtime_dir}/lib/usr-${arch_dir}" \
    "${runtime_dir}/clawbrowser" \
    "${runtime_dir}/share/X11/xkb"

  cat >"${runtime_dir}/manifest.json" <<'JSON'
{
  "entrypoints": {
    "browser": "clawbrowser/clawbrowser.real",
    "xkbcomp": "bin/xkbcomp",
    "xvfb": "bin/Xvfb"
  },
  "kind": "clawbrowser-portable",
  "version": "test"
}
JSON

  cat >"${runtime_dir}/bin/Xvfb" <<'PY'
#!/usr/bin/env python3
import os
import signal
import socket
import sys
import time

display = sys.argv[1].lstrip(":")
socket_dir = "/tmp/.X11-unix"
socket_path = os.path.join(socket_dir, f"X{display}")
os.makedirs(socket_dir, exist_ok=True)
try:
    os.unlink(socket_path)
except FileNotFoundError:
    pass

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.bind(socket_path)
sock.listen(1)

def shutdown(signum, frame):
    try:
        sock.close()
    finally:
        try:
            os.unlink(socket_path)
        except FileNotFoundError:
            pass
    raise SystemExit(0)

signal.signal(signal.SIGTERM, shutdown)
signal.signal(signal.SIGINT, shutdown)
while True:
    time.sleep(1)
PY

  cat >"${runtime_dir}/bin/xkbcomp" <<'SH'
#!/usr/bin/env bash
exit 0
SH

  cat >"${loader_path}" <<'SH'
#!/usr/bin/env bash
if [[ "${1:-}" == "--library-path" ]]; then
  shift 2
fi
export PORTABLE_RUNTIME_LOADER_USED=1
exec "$@"
SH

  cat >"${runtime_dir}/clawbrowser/clawbrowser.real" <<'PY'
#!/usr/bin/env python3
import json
import os
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer

chrome_wrapper = os.environ.get("CHROME_WRAPPER", "")
if not chrome_wrapper or not os.path.exists(chrome_wrapper):
    raise SystemExit("portable browser wrapper was not exposed via CHROME_WRAPPER")
if os.environ.get("PORTABLE_RUNTIME_LOADER_USED") != "1":
    raise SystemExit("portable browser did not launch through the runtime loader")

if "--list" in sys.argv[1:]:
    print(json.dumps([]))
    raise SystemExit(0)

port = None
for arg in sys.argv[1:]:
    if arg.startswith("--remote-debugging-port="):
        port = int(arg.split("=", 1)[1])
        break

if port is None:
    raise SystemExit("missing --remote-debugging-port")

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/json/version":
            body = json.dumps({
                "Browser": "Clawbrowser/Test",
                "webSocketDebuggerUrl": f"ws://127.0.0.1:{port}/devtools/browser/test",
            }).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        self.send_response(404)
        self.end_headers()

    def log_message(self, fmt, *args):
        return

HTTPServer(("127.0.0.1", port), Handler).serve_forever()
PY

  chmod +x \
    "${loader_path}" \
    "${runtime_dir}/bin/Xvfb" \
    "${runtime_dir}/bin/xkbcomp" \
    "${runtime_dir}/clawbrowser/clawbrowser.real"
}

test_missing_runtime_error() {
  if [[ "$(uname -s)" != "Linux" ]]; then
    skip "missing portable runtime error" "Linux-only"
    return 0
  fi

  local tmp output code
  tmp="$(mktemp -d)"
  set +e
  output="$(
    env -i \
      PATH="${PATH}" \
      HOME=/root \
      XDG_CONFIG_HOME="${tmp}/config" \
      XDG_CACHE_HOME="${tmp}/cache" \
      XDG_DATA_HOME="${tmp}/data" \
      CLAWBROWSER_API_KEY=test-key \
      "${BIN}" start --self-contained --session missing-runtime -- clawbrowser://verify/ 2>&1
  )"
  code=$?
  set -e
  rm -rf "${tmp}"

  if [[ ${code} -eq 0 ]]; then
    fail "missing portable runtime should fail"
  fi
  assert_contains "${output}" "portable runtime not found; set CLAWBROWSER_PORTABLE_LOCAL_DIR or run clawctl install"
}

test_stop_missing_self_contained_no_docker() {
  if [[ "$(uname -s)" != "Linux" ]]; then
    skip "missing self-contained stop avoids Docker" "Linux-only"
    return 0
  fi

  local tmp docker_log output
  tmp="$(mktemp -d)"
  docker_log="${tmp}/docker-called.log"
  mkdir -p "${tmp}/bin"

  cat >"${tmp}/bin/docker" <<SH
#!/usr/bin/env bash
printf 'docker invoked: %s\n' "\$*" >> "${docker_log}"
exit 64
SH
  chmod +x "${tmp}/bin/docker"

  output="$(
    env -i \
      PATH="${tmp}/bin:${PATH}" \
      HOME=/root \
      XDG_CONFIG_HOME="${tmp}/config" \
      XDG_CACHE_HOME="${tmp}/cache" \
      XDG_DATA_HOME="${tmp}/data" \
      CLAWBROWSER_DOCKER_BIN="${tmp}/bin/docker" \
      "${BIN}" stop --self-contained --session missing-self-contained
  )"

  if [[ -n "${output}" ]]; then
    rm -rf "${tmp}"
    fail "missing self-contained stop returned unexpected output: ${output}"
  fi
  if [[ -e "${docker_log}" ]]; then
    output="$(cat "${docker_log}")"
    rm -rf "${tmp}"
    fail "missing self-contained stop invoked Docker: ${output}"
  fi

  rm -rf "${tmp}"
  printf 'ok - missing self-contained stop avoids Docker\n'
}

test_self_contained_start_no_docker() {
  if [[ "$(uname -s)" != "Linux" ]]; then
    skip "self-contained portable startup" "Linux-only"
    return 0
  fi

  local platform tmp opt_root runtime_parent runtime_dir docker_log endpoint endpoint_again payload session
  platform="$(portable_platform_dir)" || {
    skip "self-contained portable startup" "unsupported architecture"
    return 0
  }

  tmp="$(mktemp -d)"
  opt_root="/opt/clawbrowser/test-${$}"
  if ! mkdir -p "${opt_root}" 2>/dev/null; then
    rm -rf "${tmp}"
    skip "self-contained portable startup" "/opt/clawbrowser is not writable"
    return 0
  fi

  session="portable-self-contained-${$}"
  runtime_parent="${tmp}/runtime"
  runtime_dir="${runtime_parent}/${platform}"
  docker_log="${tmp}/docker-called.log"
  mkdir -p "${tmp}/bin" "${runtime_dir}"
  make_fake_runtime "${runtime_dir}"

  cat >"${tmp}/bin/docker" <<SH
#!/usr/bin/env bash
printf 'docker invoked: %s\n' "\$*" >> "${docker_log}"
exit 64
SH
  chmod +x "${tmp}/bin/docker"

  cleanup() {
    if [[ -n "${session:-}" && -n "${opt_root:-}" && -n "${runtime_parent:-}" ]]; then
      HOME=/root \
      XDG_CONFIG_HOME="${opt_root}/config" \
      XDG_CACHE_HOME="${opt_root}/cache" \
      XDG_DATA_HOME="${opt_root}/data" \
      CLAWBROWSER_PORTABLE_LOCAL_DIR="${runtime_parent}" \
      "${BIN}" stop --session "${session}" >/dev/null 2>&1 || true
    fi
    [[ -n "${tmp:-}" ]] && rm -rf "${tmp}"
    [[ -n "${opt_root:-}" ]] && rm -rf "${opt_root}"
    true
  }
  trap cleanup RETURN EXIT

  endpoint="$(
    env \
      PATH="${tmp}/bin:${PATH}" \
      HOME=/root \
      XDG_CONFIG_HOME="${opt_root}/config" \
      XDG_CACHE_HOME="${opt_root}/cache" \
      XDG_DATA_HOME="${opt_root}/data" \
      CLAWBROWSER_CONFIG_DIR=/root/should-not-win/config \
      CLAWBROWSER_CACHE_DIR=/root/should-not-win/cache \
      CLAWBROWSER_DATA_DIR=/root/should-not-win/data \
      CLAWBROWSER_DOCKER_BIN="${tmp}/bin/docker" \
      CLAWBROWSER_PORTABLE_LOCAL_DIR="${runtime_parent}" \
      "${BIN}" start \
        --self-contained \
        --session "${session}" \
        --config-dir "${opt_root}/config/clawbrowser" \
        --cache-dir "${opt_root}/cache/clawbrowser" \
        --data-dir "${opt_root}/data/clawbrowser" \
        --state-dir "${opt_root}/state/clawbrowser" \
        --session-dir "${opt_root}/cache/clawbrowser/sessions" \
        --runtime-root "${opt_root}/cache/clawbrowser/runtime" \
        -- clawbrowser://verify/
  )"

  endpoint_again="$(
    env \
      HOME=/root \
      XDG_CONFIG_HOME="${opt_root}/config" \
      XDG_CACHE_HOME="${opt_root}/cache" \
      XDG_DATA_HOME="${opt_root}/data" \
      "${BIN}" endpoint \
        --session "${session}" \
        --state-dir "${opt_root}/state/clawbrowser" \
        --session-dir "${opt_root}/cache/clawbrowser/sessions"
  )"

  if [[ "${endpoint_again}" != "${endpoint}" ]]; then
    fail "endpoint command returned ${endpoint_again}, want ${endpoint}"
  fi
  if [[ -e "${docker_log}" ]]; then
    fail "portable self-contained startup invoked Docker"
  fi
  if [[ -e /root/should-not-win ]]; then
    fail "CLI path flags did not override CLAWBROWSER_* /root paths"
  fi

  payload="$(python3 - "${endpoint}/json/version" <<'PY'
import json
import sys
import urllib.request

with urllib.request.urlopen(sys.argv[1], timeout=2) as response:
    payload = json.load(response)
print(json.dumps(payload, sort_keys=True))
PY
)"
  assert_contains "${payload}" '"Browser": "Clawbrowser/Test"'
  printf 'ok - self-contained portable startup\n'
}

test_self_contained_list_uses_loader_no_docker() {
  if [[ "$(uname -s)" != "Linux" ]]; then
    skip "self-contained portable list" "Linux-only"
    return 0
  fi

  local platform tmp opt_root runtime_parent runtime_dir docker_log output session
  platform="$(portable_platform_dir)" || {
    skip "self-contained portable list" "unsupported architecture"
    return 0
  }

  tmp="$(mktemp -d)"
  opt_root="${tmp}/opt"
  session="portable-list-${$}"
  runtime_parent="${tmp}/runtime"
  runtime_dir="${runtime_parent}/${platform}"
  docker_log="${tmp}/docker-called.log"
  mkdir -p "${tmp}/bin" "${runtime_dir}" "${opt_root}"
  make_fake_runtime "${runtime_dir}"

  cat >"${tmp}/bin/docker" <<SH
#!/usr/bin/env bash
printf 'docker invoked: %s\n' "\$*" >> "${docker_log}"
exit 64
SH
  chmod +x "${tmp}/bin/docker"

  cleanup() {
    [[ -n "${tmp:-}" ]] && rm -rf "${tmp}"
    true
  }
  trap cleanup RETURN EXIT

  output="$(
    env \
      PATH="${tmp}/bin:${PATH}" \
      HOME=/root \
      XDG_CONFIG_HOME="${opt_root}/config" \
      XDG_CACHE_HOME="${opt_root}/cache" \
      XDG_DATA_HOME="${opt_root}/data" \
      CLAWBROWSER_DOCKER_BIN="${tmp}/bin/docker" \
      CLAWBROWSER_PORTABLE_LOCAL_DIR="${runtime_parent}" \
      "${BIN}" list \
        --self-contained \
        --session "${session}" \
        --config-dir "${opt_root}/config/clawbrowser" \
        --cache-dir "${opt_root}/cache/clawbrowser" \
        --data-dir "${opt_root}/data/clawbrowser" \
        --state-dir "${opt_root}/state/clawbrowser" \
        --session-dir "${opt_root}/cache/clawbrowser/sessions"
  )"

  if [[ "${output}" != "[]" ]]; then
    fail "portable list returned ${output}, want []"
  fi
  if [[ -e "${docker_log}" ]]; then
    fail "portable self-contained list invoked Docker"
  fi

  printf 'ok - self-contained portable list\n'
}

test_missing_runtime_error
test_stop_missing_self_contained_no_docker
test_self_contained_start_no_docker
test_self_contained_list_uses_loader_no_docker

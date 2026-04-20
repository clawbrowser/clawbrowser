#!/usr/bin/env bash
set -euo pipefail

log() {
  printf '[clawbrowser-docker] %s\n' "$*"
}

die() {
  printf '[clawbrowser-docker] ERROR: %s\n' "$*" >&2
  exit 1
}

wait_for_x_socket() {
  local display_number="${DISPLAY#:}"
  local socket_path="/tmp/.X11-unix/X${display_number}"
  local attempt

  for attempt in $(seq 1 40); do
    if [[ -S "${socket_path}" ]]; then
      return 0
    fi
    sleep 0.25
  done

  die "Xvfb did not create ${socket_path}"
}

terminate_children() {
  if [[ -n "${APP_PID:-}" ]] && kill -0 "${APP_PID}" 2>/dev/null; then
    kill "${APP_PID}" 2>/dev/null || true
  fi
  if [[ -n "${SOCAT_PID:-}" ]] && kill -0 "${SOCAT_PID}" 2>/dev/null; then
    kill "${SOCAT_PID}" 2>/dev/null || true
  fi
  if [[ -n "${XVFB_PID:-}" ]] && kill -0 "${XVFB_PID}" 2>/dev/null; then
    kill "${XVFB_PID}" 2>/dev/null || true
  fi
}

cleanup() {
  terminate_children

  if [[ -n "${APP_PID:-}" ]]; then
    wait "${APP_PID}" 2>/dev/null || true
  fi
  if [[ -n "${SOCAT_PID:-}" ]]; then
    wait "${SOCAT_PID}" 2>/dev/null || true
  fi
  if [[ -n "${XVFB_PID:-}" ]]; then
    wait "${XVFB_PID}" 2>/dev/null || true
  fi
}

extract_switch_value() {
  local switch_name="$1"
  shift
  local expect_value=0
  local arg

  for arg in "$@"; do
    if ((expect_value == 1)); then
      printf '%s\n' "${arg}"
      return 0
    fi

    case "${arg}" in
      "${switch_name}="*)
        printf '%s\n' "${arg#*=}"
        return 0
        ;;
      "${switch_name}")
        expect_value=1
        ;;
    esac
  done

  return 1
}

has_arg() {
  local needle="$1"
  shift
  local arg
  for arg in "$@"; do
    if [[ "${arg}" == "${needle}" ]]; then
      return 0
    fi
  done
  return 1
}

trap terminate_children INT TERM
trap cleanup EXIT

mkdir -p "${XDG_RUNTIME_DIR}"
chmod 700 "${XDG_RUNTIME_DIR}"

default_browser="${CLAWBROWSER_BROWSER_BINARY:-/opt/clawbrowser/clawbrowser.real}"

default_args=(
  "${default_browser}"
  "--no-first-run"
  "--no-default-browser-check"
  "--disable-dev-shm-usage"
  "--window-size=${CLAWBROWSER_WINDOW_SIZE:-1920,1080}"
)

if [[ "${CLAWBROWSER_NO_SANDBOX:-1}" == "1" ]] && ! has_arg "--no-sandbox" "$@"; then
  default_args+=("--no-sandbox")
fi

if (($# == 0)); then
  set -- "${default_args[@]}"
elif [[ "$1" == -* ]]; then
  set -- "${default_args[@]}" "$@"
fi

if [[ "$1" == "${default_browser}" && ! -x "${default_browser}" ]]; then
  die "Browser binary is not executable: ${default_browser}"
fi

cdp_port="$(extract_switch_value "--remote-debugging-port" "$@" || true)"
cdp_address="$(extract_switch_value "--remote-debugging-address" "$@" || true)"
if [[ -z "${cdp_address}" ]]; then
  cdp_address="127.0.0.1"
fi

log "Starting Xvfb on ${DISPLAY} with screen ${XVFB_WHD}"
Xvfb "${DISPLAY}" -screen 0 "${XVFB_WHD}" -nolisten tcp -noreset +extension RANDR &
XVFB_PID=$!

wait_for_x_socket

if [[ -n "${cdp_port}" && "${cdp_port}" != "0" && "${cdp_address}" != "127.0.0.1" && "${cdp_address}" != "localhost" ]]; then
  container_ip="$(hostname -i | awk '{print $1}')"
  if [[ -n "${container_ip}" ]]; then
    log "Forwarding CDP ${container_ip}:${cdp_port} -> 127.0.0.1:${cdp_port}"
    socat "TCP-LISTEN:${cdp_port},bind=${container_ip},reuseaddr,fork" "TCP:127.0.0.1:${cdp_port}" &
    SOCAT_PID=$!
  fi
fi

log "Launching command: $*"
dbus-run-session -- "$@" &
APP_PID=$!
wait "${APP_PID}"

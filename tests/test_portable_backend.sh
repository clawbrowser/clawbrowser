#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${ROOT_DIR}/bin/clawbrowser"
SOURCE_FILE="${ROOT_DIR}/bin/clawbrowser"

fail() {
  printf 'FAIL: %s\n' "$*" >&2
  exit 1
}

assert_contains() {
  local haystack="$1"
  local needle="$2"
  if [[ "${haystack}" != *"${needle}"* ]]; then
    fail "expected output to contain: ${needle}"
  fi
}

run_test() {
  local name="$1"
  shift
  printf 'ok - %s\n' "${name}"
  "$@"
}

test_help_mentions_portable_backend() {
  local output
  output="$("${BIN}" help)"
  assert_contains "${output}" "--backend MODE"
  assert_contains "${output}" "auto|app|portable|docker"
}

test_unknown_backend_errors() {
  local output
  set +e
  output="$("${BIN}" ensure-runtime --backend invalid 2>&1)"
  local code=$?
  set -e
  if [[ ${code} -eq 0 ]]; then
    fail "invalid backend should fail"
  fi
  assert_contains "${output}" "Unknown backend"
}

test_docker_backend_reports_image() {
  local output
  output="$("${BIN}" ensure-runtime --backend docker)"
  assert_contains "${output}" "clawbrowser"
}

test_portable_backend_contract_on_host() {
  local output
  local os_name
  os_name="$(uname -s)"
  set +e
  output="$("${BIN}" ensure-runtime --backend portable 2>&1)"
  local code=$?
  set -e

  if [[ "${os_name}" != "Linux" ]]; then
    if [[ ${code} -eq 0 ]]; then
      fail "portable backend should fail on non-Linux hosts"
    fi
    assert_contains "${output}" "PORTABLE_UNSUPPORTED_PLATFORM"
  fi
}

test_source_contract_for_portable_backend_selection() {
  grep -Fq 'if [[ "${BACKEND}" == "portable" ]]; then' "${SOURCE_FILE}" || fail "portable backend explicit selection missing"
  grep -Fq 'if portable_platform_supported; then' "${SOURCE_FILE}" || fail "portable platform gate missing"
  grep -Fq 'if [[ "${CLAWBROWSER_ALLOW_DOCKER_BACKEND}" == "1" ]]; then' "${SOURCE_FILE}" || fail "docker fallback gate missing"
  grep -Fq 'x86_64|amd64|arm64|aarch64)' "${SOURCE_FILE}" || fail "portable amd64/arm64 platform gate missing"
}

test_source_contract_for_arch_aware_portable_artifacts() {
  grep -Fq -- 'portable_host_arch' "${SOURCE_FILE}" || fail "portable host arch helper missing"
  grep -Fq -- 'clawbrowser-portable-linux-%s-glibc' "${SOURCE_FILE}" || fail "portable artifact basename should be arch-aware"
  grep -Fq -- 'clawbrowser-portable-xvfb-linux-%s-glibc' "${SOURCE_FILE}" || fail "portable Xvfb artifact fallback missing"
  grep -Fq -- 'linux-%s-glibc' "${SOURCE_FILE}" || fail "portable platform dir should be arch-aware"
}

test_source_contract_for_headless_rejection() {
  grep -Fq -- '--headless|--headless=*|--headless=new|--headless=old)' "${SOURCE_FILE}" || fail "portable headless argument matcher missing"
  grep -Fq -- 'portable backend does not allow Chromium headless mode.' "${SOURCE_FILE}" || fail "portable headless rejection message missing"
}

test_source_contract_for_offline_runtime_short_circuit() {
  grep -Fq -- 'CLAWBROWSER_PORTABLE_LOCAL_DIR' "${SOURCE_FILE}" || fail "portable local runtime env knob missing"
  grep -Fq -- 'find_portable_runtime_local' "${SOURCE_FILE}" || fail "portable local runtime discovery helper missing"
  grep -Fq -- 'find_portable_runtime_in_root "${PORTABLE_RUNTIME_ROOT}"' "${SOURCE_FILE}" || fail "portable runtime cache-root lookup missing"
  grep -Fq -- 'data_runtime_root="${DATA_ROOT}/runtime"' "${SOURCE_FILE}" || fail "portable runtime data-root fallback missing"
  grep -Fq -- 'Using local portable runtime' "${SOURCE_FILE}" || fail "portable local runtime short-circuit log missing"
}

test_source_contract_for_xvfb_xkbdir_flag() {
  grep -Fq -- '-xkbdir "${runtime_dir}/share/X11/xkb"' "${SOURCE_FILE}" || fail "portable Xvfb missing -xkbdir"
}

test_source_contract_for_xkbcomp_binding() {
  grep -Fq -- 'portable_xvfb_entrypoint' "${SOURCE_FILE}" || fail "portable Xvfb entrypoint helper missing"
  grep -Fq -- 'patch_portable_xvfb_binary' "${SOURCE_FILE}" || fail "portable Xvfb self-contained patch helper missing"
  grep -Fq -- 'portable_loader_path' "${SOURCE_FILE}" || fail "portable runtime loader helper missing"
  grep -Fq -- 'wrapped_xkbcomp=' "${SOURCE_FILE}" || fail "portable xkbcomp wrapper path missing"
  grep -Fq -- 'prepare_x11_socket_dir' "${SOURCE_FILE}" || fail "portable Xvfb should prepare /tmp/.X11-unix for non-root containers"
  grep -Fq -- 'Using self-contained portable Xvfb/xkbcomp wrappers' "${SOURCE_FILE}" || fail "portable self-contained Xvfb wrapper log missing"
}

test_source_contract_for_browser_loader_binding() {
  grep -Fq -- 'portable_browser_library_path' "${SOURCE_FILE}" || fail "portable browser library path helper missing"
  grep -Fq -- 'portable_prepared_browser_entrypoint' "${SOURCE_FILE}" || fail "portable browser prepared entrypoint helper missing"
  grep -Fq -- 'patch_portable_browser_binary' "${SOURCE_FILE}" || fail "portable browser interpreter patch helper missing"
  grep -Fq -- 'portable_browser_loader_link' "${SOURCE_FILE}" || fail "portable browser loader symlink helper missing"
  grep -Fq -- 'portable_browser_wrapper' "${SOURCE_FILE}" || fail "portable browser wrapper helper missing"
  grep -Fq -- 'PORTABLE_BROWSER_WRAPPER_USED=1 LD_LIBRARY_PATH="${portable_browser_ld_library_path}" exec "${browser_exec}"' "${SOURCE_FILE}" || fail "portable browser wrapper should execute patched browser with isolated library path"
  grep -Fq -- 'PORTABLE_BROWSER_WRAPPER_USED=1 exec "${portable_loader}" --library-path "${portable_browser_ld_library_path}" "${browser_exec}"' "${SOURCE_FILE}" || fail "portable browser wrapper should use bundled loader for non-ELF test entrypoints"
  grep -Fq -- 'CHROME_WRAPPER="${browser_wrapper}"' "${SOURCE_FILE}" || fail "portable browser wrapper env missing"
  grep -Fq -- 'env -u LD_LIBRARY_PATH' "${SOURCE_FILE}" || fail "portable browser launch should clear inherited LD_LIBRARY_PATH"
  grep -Fq -- '"${browser_wrapper}"' "${SOURCE_FILE}" || fail "portable start should launch through prepared browser wrapper"
  grep -Fq -- '--disable-gpu' "${SOURCE_FILE}" || fail "portable start should disable GPU under Xvfb"
  grep -Fq -- '--disable-background-networking' "${SOURCE_FILE}" || fail "portable start should disable background networking in restricted containers"
  grep -Fq -- 'GSETTINGS_BACKEND="${GSETTINGS_BACKEND:-memory}"' "${SOURCE_FILE}" || fail "portable start should avoid host GSettings dependency"
  if grep -Fq -- 'Portable browser reported CDP readiness' "${SOURCE_FILE}"; then
    fail "portable start must not treat browser log lines as CDP readiness"
  fi
}

test_source_contract_for_self_contained_flag() {
  grep -Fq -- '--self-contained|--standalone)' "${SOURCE_FILE}" || fail "self-contained option parser missing"
  grep -Fq -- 'SELF_CONTAINED_REQUESTED=1' "${SOURCE_FILE}" || fail "self-contained flag should win over backend parse order"
  grep -Fq -- 'Alias for --backend portable on Linux' "${SOURCE_FILE}" || fail "self-contained help text missing"
}

test_source_contract_for_restricted_paths() {
  grep -Fq -- '/root|/root/|/root/.)' "${SOURCE_FILE}" || fail "HOME=/root guard missing"
  grep -Fq -- '--config-dir)' "${SOURCE_FILE}" || fail "config-dir CLI override missing"
  grep -Fq -- '--cache-dir)' "${SOURCE_FILE}" || fail "cache-dir CLI override missing"
  grep -Fq -- '--data-dir)' "${SOURCE_FILE}" || fail "data-dir CLI override missing"
  grep -Fq -- '--runtime-root|--portable-runtime-root)' "${SOURCE_FILE}" || fail "runtime-root CLI override missing"
}

test_source_contract_for_missing_runtime_error() {
  grep -Fq -- 'portable runtime not found; set CLAWBROWSER_PORTABLE_LOCAL_DIR or run clawctl install' "${SOURCE_FILE}" || fail "portable runtime not-found guidance missing"
  grep -Fq -- 'PORTABLE_ALLOW_DOWNLOAD' "${SOURCE_FILE}" || fail "portable start should not silently download unless explicitly ensuring runtime"
}

run_test "help mentions portable backend" test_help_mentions_portable_backend
run_test "unknown backend errors" test_unknown_backend_errors
run_test "docker backend reports image" test_docker_backend_reports_image
run_test "portable backend contract on host" test_portable_backend_contract_on_host
run_test "source contract for portable backend selection" test_source_contract_for_portable_backend_selection
run_test "source contract for arch-aware portable artifacts" test_source_contract_for_arch_aware_portable_artifacts
run_test "source contract for headless rejection" test_source_contract_for_headless_rejection
run_test "source contract for offline runtime short-circuit" test_source_contract_for_offline_runtime_short_circuit
run_test "source contract for Xvfb xkbdir flag" test_source_contract_for_xvfb_xkbdir_flag
run_test "source contract for xkbcomp binding" test_source_contract_for_xkbcomp_binding
run_test "source contract for browser loader binding" test_source_contract_for_browser_loader_binding
run_test "source contract for self-contained flag" test_source_contract_for_self_contained_flag
run_test "source contract for restricted paths" test_source_contract_for_restricted_paths
run_test "source contract for missing runtime error" test_source_contract_for_missing_runtime_error

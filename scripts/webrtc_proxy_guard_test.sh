#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
proxy_config="${repo_root}/clawbrowser/proxy/proxy_config.cc"
startup="${repo_root}/clawbrowser/startup.cc"
webrtc_patch="${repo_root}/clawbrowser/patches/017-webrtc-leak-prevention.patch"
verify_js="${repo_root}/clawbrowser/verify/resources/verify.js"
verify_source="${repo_root}/clawbrowser/verify/verify_page.cc"
webrtc_probe="${repo_root}/clawbrowser/test/integration/webrtc_probe.py"
real_proxy_test="${repo_root}/clawbrowser/test/integration/test_real_socks5_proxy.py"

grep -q -- '--webrtc-ip-handling-policy=disable_non_proxied_udp' "${proxy_config}"
grep -q -- '--force-webrtc-ip-handling-policy=disable_non_proxied_udp' "${proxy_config}"
grep -q 'GetProxyCommandLineFlags' "${startup}"
grep -q '^# Pinned against Chromium revision 28a7a6c409e03c701d3474ef9e3b1f0be6249039 (151.0.7922.109)$' "${webrtc_patch}"
grep -q 'port_config.enable_nonproxied_udp = false' "${webrtc_patch}"
grep -q 'web_configuration.type = webrtc::PeerConnectionInterface::kRelay' "${webrtc_patch}"
grep -q 'sdp.find(" typ relay") == std::string::npos' "${webrtc_patch}"
grep -q 'OnIceCandidateError' "${webrtc_patch}"
grep -q 'redact_local_address' "${webrtc_patch}"
grep -q 'static_cast<uint16_t>(redact_local_address ? 0 : port)' "${webrtc_patch}"
grep -q 'new RTCPeerConnection' "${verify_js}"
grep -q 'webrtc.iceCandidates' "${verify_js}"
grep -q 'onicecandidateerror' "${verify_js}"
grep -q 'hostCandidate' "${verify_js}"
grep -q 'dataset.managedProxyPrivacy' "${verify_js}"
grep -q 'GetSwitchValueASCII("webrtc-ip-handling-policy")' "${verify_source}"
grep -q 'GetSwitchValueASCII("force-webrtc-ip-handling-policy")' \
  "${verify_source}"
grep -q 'return 2;' "${verify_source}"
grep -q 'onicecandidateerror' "${webrtc_probe}"
grep -q 'hostCandidate' "${webrtc_probe}"
grep -q 'require_relay=True' "${real_proxy_test}"
grep -q 'proxy_config=profile_proxy_from_url(proxy_url)' "${real_proxy_test}"
if grep -q 'CLAWBROWSER_DEV_PROXY_URL' "${real_proxy_test}"; then
  printf 'Release proxy tests must use the normal profile, not a dev-only override\n' >&2
  exit 1
fi

if grep -Eq 'stun\.l\.google\.com|stun\.cloudflare\.com' "${verify_js}"; then
  printf 'Built-in verifier must not contact a public STUN service\n' >&2
  exit 1
fi

node "${repo_root}/scripts/verify_proxy_render_test.js"
bash "${repo_root}/scripts/verify_page_assets_test.sh"
python3 -m py_compile \
  "${repo_root}/clawbrowser/test/integration/test_proxy.py" \
  "${repo_root}/clawbrowser/test/integration/test_real_socks5_proxy.py" \
  "${repo_root}/clawbrowser/test/integration/webrtc_probe.py"

PYTHONPATH="${repo_root}" python3 - <<'PY'
from clawbrowser.test.integration.webrtc_probe import assert_relay_only


def observation(errors):
    return [{"complete": True, "candidates": [], "errors": errors}]


assert_relay_only(observation([
    {"address": "0.0.0.0", "hostCandidate": "[::]:9"},
]))

for exposed_error in (
    {"address": "192.0.2.8"},
    {"hostCandidate": "192.0.2.8:50000"},
):
    try:
        assert_relay_only(observation([exposed_error]))
    except AssertionError:
        continue
    raise AssertionError("ICE candidate error address was not rejected")
PY

printf 'PASS\n'

#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
proxy_config="${repo_root}/clawbrowser/proxy/proxy_config.cc"
startup="${repo_root}/clawbrowser/startup.cc"
webrtc_patch="${repo_root}/clawbrowser/patches/017-webrtc-leak-prevention.patch"
verify_js="${repo_root}/clawbrowser/verify/resources/verify.js"
real_proxy_test="${repo_root}/clawbrowser/test/integration/test_real_socks5_proxy.py"

grep -q -- '--webrtc-ip-handling-policy=disable_non_proxied_udp' "${proxy_config}"
grep -q -- '--force-webrtc-ip-handling-policy=disable_non_proxied_udp' "${proxy_config}"
grep -q 'GetProxyCommandLineFlags' "${startup}"
grep -q 'port_config.enable_nonproxied_udp = false' "${webrtc_patch}"
grep -q 'web_configuration.type = webrtc::PeerConnectionInterface::kRelay' "${webrtc_patch}"
grep -q 'sdp.find(" typ relay") == std::string::npos' "${webrtc_patch}"
grep -q 'new RTCPeerConnection' "${verify_js}"
grep -q 'webrtc.iceCandidates' "${verify_js}"
grep -q 'require_relay=True' "${real_proxy_test}"

node "${repo_root}/scripts/verify_proxy_render_test.js"
bash "${repo_root}/scripts/verify_page_assets_test.sh"
python3 -m py_compile \
  "${repo_root}/clawbrowser/test/integration/test_proxy.py" \
  "${repo_root}/clawbrowser/test/integration/test_real_socks5_proxy.py" \
  "${repo_root}/clawbrowser/test/integration/webrtc_probe.py"

printf 'PASS\n'

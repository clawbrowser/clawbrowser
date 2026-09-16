"""Opt-in, real TURN/TLS echo through an operator-provided proxy.

Provide CLAWBROWSER_QA_TURN_TLS_CONFIG (RTC iceServer JSON) and
CLAWBROWSER_QA_HTTP_PROXY_CONFIG / CLAWBROWSER_QA_SOCKS5_PROXY_CONFIG
(the normal profile proxy JSON). These are private file paths, not credentials.
The TURN service must use a publicly trusted certificate. No certificate or
sandbox bypass is used. This is a controlled identity test, not OAuth E2E.
"""

import json
import os
from pathlib import Path

import pytest

from conftest import _launch_browser_with_details
from canvas_network_mode import canvas_network_args, assert_canvas_network_mode


RELAY_ECHO = r"""async turn => {
    // Deliberately do not request iceTransportPolicy:'relay': the product
    // policy, rather than the test page, must suppress direct candidates.
    const peers = [new RTCPeerConnection({iceServers:[turn]}),
                   new RTCPeerConnection({iceServers:[turn]})];
    const [a, b] = peers;
    const candidateTypes = [];
    let timer;
    const complete = async () => {
        for (const pc of peers) pc.onicecandidate = e => {
            if (e.candidate) candidateTypes.push(e.candidate.type);
        };
        const gather = pc => new Promise(resolve => {
            if (pc.iceGatheringState === 'complete') return resolve();
            pc.addEventListener('icegatheringstatechange', () => {
                if (pc.iceGatheringState === 'complete') resolve();
            });
        });
        const dc = a.createDataChannel('qa-relay');
        const opened = new Promise(resolve => dc.onopen = resolve);
        const echoed = new Promise(resolve => dc.onmessage = e => resolve(e.data));
        b.ondatachannel = e => e.channel.onmessage = m => e.channel.send(m.data);
        await a.setLocalDescription(await a.createOffer()); await gather(a);
        await b.setRemoteDescription(a.localDescription);
        await b.setLocalDescription(await b.createAnswer()); await gather(b);
        await a.setRemoteDescription(b.localDescription);
        await opened;
        dc.send('clawbrowser-relay-proof');
        const echo = (await echoed) === 'clawbrowser-relay-proof';
        const deadline = performance.now() + 5000;
        let pairs;
        do {
            pairs = [];
            for (const pc of peers) {
                const stats = await pc.getStats();
                for (const transport of stats.values()) {
                    if (transport.type !== 'transport' || !transport.selectedCandidatePairId) continue;
                    const pair = stats.get(transport.selectedCandidatePairId);
                    const local = stats.get(pair.localCandidateId);
                    const remote = stats.get(pair.remoteCandidateId);
                    pairs.push({state:pair.state, sent:pair.bytesSent,
                        received:pair.bytesReceived, localType:local.candidateType,
                        remoteType:remote.candidateType, protocol:local.relayProtocol});
                }
            }
            if (pairs.length === 2 && pairs.every(p => p.state === 'succeeded')) break;
            await new Promise(resolve => setTimeout(resolve, 100));
        } while (performance.now() < deadline);
        // No SDP, candidate addresses, credentials, or TURN URL in artifacts.
        return {echo, candidateTypes, pairs};
    };
    try {
        return await Promise.race([complete(), new Promise((_, reject) => {
            timer = setTimeout(() => reject(new Error('TURN/TLS echo timeout')), 60000);
        })]);
    } finally {
        clearTimeout(timer);
        for (const pc of peers) pc.close();
    }
}"""


def _private_config(variable):
    path = os.environ.get(variable)
    if not path:
        pytest.skip(f"requires {variable}")
    try:
        value = json.loads(Path(path).read_text())
    except (OSError, ValueError):
        pytest.fail(f"Cannot read valid JSON from {variable}", pytrace=False)
    if not isinstance(value, dict):
        pytest.fail(f"{variable} must contain a JSON object", pytrace=False)
    return value


@pytest.mark.asyncio
@pytest.mark.parametrize("scheme", ["http", "socks5"])
async def test_real_turn_tls_echo(scheme, record_property):
    turn = _private_config("CLAWBROWSER_QA_TURN_TLS_CONFIG")
    proxy = _private_config(f"CLAWBROWSER_QA_{scheme.upper()}_PROXY_CONFIG")
    urls = turn.get("urls")
    urls = [urls] if isinstance(urls, str) else urls
    if not isinstance(urls, list) or not urls or not all(
        isinstance(url, str) and url.startswith("turns:") for url in urls
    ):
        pytest.fail("TURN config must specify only turns: URLs", pytrace=False)
    if proxy.get("scheme") != scheme:
        pytest.fail("Proxy config scheme mismatch", pytrace=False)
    async with _launch_browser_with_details(
        fixture_name="valid_fingerprint.json", backend_mode="mock",
        skip_verify=True, proxy_config=proxy, headless=False,
        extra_browser_args=canvas_network_args(),
    ) as launch:
        # A remote proxy cannot reach this machine's localhost mock page, and
        # managed profiles intentionally have no implicit loopback bypass.
        # Use an explicit in-memory probe document, never a network error page.
        await launch["page"].goto("data:text/html,<title>TURN relay probe</title>")
        assert await launch["page"].title() == "TURN relay probe"
        record_property("canvas_mode_canary", json.dumps(
            await assert_canvas_network_mode(launch["page"]), sort_keys=True))
        result = await launch["page"].evaluate(RELAY_ECHO, turn)
    record_property("turn_tls_observation", json.dumps(result, sort_keys=True))
    assert result["echo"], result
    assert result["candidateTypes"] and set(result["candidateTypes"]) == {"relay"}, result
    assert len(result["pairs"]) == 2, result
    for pair in result["pairs"]:
        assert pair["state"] == "succeeded", result
        assert pair["localType"] == pair["remoteType"] == "relay", result
        assert pair["protocol"] == "tls", result
        assert pair["sent"] > 0 and pair["received"] > 0, result

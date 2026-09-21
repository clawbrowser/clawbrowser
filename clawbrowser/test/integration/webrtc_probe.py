"""Reusable WebRTC ICE observations for browser integration tests."""

WEBRTC_PROBE_SCRIPT = r"""async iceServerSets => {
    const gather = iceServers => new Promise((resolve, reject) => {
        const pc = new RTCPeerConnection({iceServers});
        const candidates = [];
        const errors = [];
        let resolved = false;
        let timeoutId;
        const finish = async complete => {
            if (resolved) return;
            resolved = true;
            clearTimeout(timeoutId);
            const sdp = pc.localDescription?.sdp || '';
            for (const line of sdp.split(/\r?\n/)) {
                if (/^a=candidate:/i.test(line)) {
                    candidates.push({
                        candidate: line.slice(2),
                        source: 'sdp',
                    });
                }
            }
            let statsTimeoutId;
            try {
                const stats = await Promise.race([
                    pc.getStats(),
                    new Promise((_, reject) => {
                        statsTimeoutId = setTimeout(
                            () => reject(new Error('getStats timed out')),
                            2000,
                        );
                    }),
                ]);
                stats.forEach(report => {
                    if (report.type !== 'local-candidate') return;
                    candidates.push({
                        type: report.candidateType || '',
                        address: report.address || report.ip || '',
                        relatedAddress: report.relatedAddress || '',
                        source: 'stats',
                    });
                });
            } catch (error) {
                complete = false;
            } finally {
                clearTimeout(statsTimeoutId);
            }
            pc.close();
            resolve({complete, candidates, errors});
        };
        pc.onicecandidate = event => {
            if (event.candidate) {
                candidates.push({
                    candidate: event.candidate.candidate || '',
                    type: event.candidate.type || '',
                    address: event.candidate.address || '',
                    relatedAddress: event.candidate.relatedAddress || '',
                    source: 'event',
                });
            } else if (pc.iceGatheringState === 'complete') {
                void finish(true);
            }
        };
        pc.onicecandidateerror = event => {
            errors.push({
                address: event.address || '',
                hostCandidate: event.hostCandidate || '',
                port: event.port || 0,
            });
        };
        pc.onicegatheringstatechange = () => {
            if (pc.iceGatheringState === 'complete') {
                void finish(true);
            }
        };
        pc.createDataChannel('clawbrowser-webrtc-test');
        pc.createOffer()
            .then(offer => pc.setLocalDescription(offer))
            .catch(error => {
                clearTimeout(timeoutId);
                pc.close();
                reject(error);
            });
        timeoutId = setTimeout(() => void finish(false), 7000);
    });

    return Promise.all(iceServerSets.map(gather));
}"""


def _is_unspecified_address(address):
    normalized = str(address or "").strip().lower()
    if normalized in ("", "0.0.0.0", "::", "[::]"):
        return True
    if normalized.startswith("0.0.0.0:"):
        return normalized.removeprefix("0.0.0.0:").isdigit()
    if normalized.startswith("[::]:"):
        return normalized.removeprefix("[::]:").isdigit()
    if normalized.startswith(":::"):
        return normalized.removeprefix(":::").isdigit()
    return False


async def collect_webrtc_observations(page, ice_server_sets):
    """Gather ICE candidates and error addresses from every exposed surface."""
    return await page.evaluate(WEBRTC_PROBE_SCRIPT, ice_server_sets)


def _candidate_type(candidate):
    candidate_type = candidate.get("type", "").lower()
    if candidate_type:
        return candidate_type
    parts = candidate.get("candidate", "").lower().split()
    if "typ" in parts and parts.index("typ") + 1 < len(parts):
        return parts[parts.index("typ") + 1]
    return "unknown"


def _related_address(candidate):
    related_address = candidate.get("relatedAddress", "").lower()
    if related_address:
        return related_address
    parts = candidate.get("candidate", "").lower().split()
    if "raddr" in parts and parts.index("raddr") + 1 < len(parts):
        return parts[parts.index("raddr") + 1]
    return ""


def assert_relay_only(observations, *, require_relay=False):
    """Reject incomplete gathering and every observable non-relay address."""
    for observation in observations:
        for error in observation.get("errors", []):
            for field in ("address", "hostCandidate"):
                address = error.get(field, "")
                assert _is_unspecified_address(address), (
                    f"ICE candidate error exposed {field}"
                )

    assert all(item["complete"] for item in observations), (
        "ICE gathering did not complete"
    )

    relay_count = 0
    for observation in observations:
        for candidate in observation["candidates"]:
            candidate_type = _candidate_type(candidate)
            assert candidate_type == "relay", (
                f"Non-relay candidate found: {candidate}"
            )
            relay_count += 1

            related_address = _related_address(candidate)
            assert related_address in ("", "0.0.0.0", "::", "[::]"), (
                f"Related address exposed: {candidate}"
            )

    if require_relay:
        assert relay_count > 0, "TURN control produced no relay candidate"

# Linux patch 038 regression evidence

Status: scoped regression and network gates passed; not overall release ready.

Candidate includes patch 038's protected Canvas2D software raster selection.
Tests use the committed PR snapshot `eec1088` and a freshly extracted archive,
headful under Xvfb with the browser sandbox enabled. The previous a984314
artifact is preserved, not overwritten.

- ELF SHA-256: `9f8c6bd6b014874477b7a2c69d9d72c99478a7171d38e9e9d24def9fe5d80b33`.
- Archive SHA-256: `0f5545d4747ec482ae17fb6cc4c9aef82d94a717232c4963186c396104ae5b8c`.
- C++: 215 passed.
- Full integration with two independent Fontconfig controls: 113 passed,
  8 skipped in 253.62 seconds. External opt-in gates are not counted as passed
  by this result. An initial collection/import error was corrected by running
  `python -m pytest`; the initial failed report is retained.
- Separate trusted TURN/TLS tests: HTTP and SOCKS5 both passed in 15.95 seconds.
  Each exchanged an actual DataChannel echo, selected two succeeded relay/relay
  pairs with positive byte counters and TLS relay protocol, and emitted only
  relay candidates. No certificate bypass or page-level relay-only request.
- Independent IPv4 and IPv6 STUN controls produced four captured packets.
  The subsequent HTTP/SOCKS5 browser phase produced zero packets to the same
  controlled UDP endpoint and no direct candidates, with zero kernel drops.
  This is endpoint-scoped direct-STUN evidence, not proof of all possible egress.

The first positive-control capture was stopped before libpcap dispatched its
buffer (eight filter receptions, zero saved packets). It was correctly rejected.
The successful repeat used immediate capture mode and a drain interval; both
the failed and passing captures are retained privately. Temporary relay services
were stopped. No production deploy, publication or merge was performed.

Sanitized evidence files: `patch038-full-rerun.xml`, `patch038-cpp.xml`,
`patch038-turn-tls.xml`, `patch038-stun-pcap.json`. Raw packet captures and proxy
configuration stay private. This does not close PixelScan font/canvas warnings,
macOS CoreText fallback isolation, Windows/Linux ARM acceptance, or the full
managed desktop identity/update journey.

# Managed proxy implicit-bypass regression

This is partial candidate evidence, not release approval.

The current branch already stops managed startup after fingerprint API,
save/load, missing required proxy, invalid proxy and conflicting native proxy
configuration failures. An older checkout still contains vanilla fallback;
its behavior must not be used as evidence about this branch's binary.

A new live-origin regression found a separate gap: Chromium's implicit
localhost/link-local bypass remained enabled. On the macOS patch038 baseline,
the local origin received a browser request directly despite a configured
HTTP proxy. A successful-proxy-then-disconnect control did not retry directly.

Both proxy flag generators now emit `--proxy-bypass-list=<-loopback>`.
This removes implicit destination bypasses; it does not prohibit connecting
to a local proxy transport such as the SOCKS bridge.

## Evidence

- Baseline route tests: 1 pass, 1 failure (localhost bypass).
- macOS candidate staged `20260915-104649`: both route tests pass (5.04s).
- Candidate C++ proxy/startup selection: 25 tests pass.
- First full macOS run: 80 pass, 26 fail, 17 skip. Many fixtures had depended
  on implicit bypass to reach local pages while using `proxy.example.com`.
  This run is retained, not counted as acceptance.
- Mock mode now substitutes an actual local HTTP fixture proxy for that
  example host. The proxy permits only localhost/127.0.0.1 HTTP destinations;
  it rejects external and TLS destinations. Explicit non-example proxy
  configurations remain unchanged. No browser bypass exception is added.
- Routed full run: 102 pass, 4 fail, 17 skip (219.64s). Two failures are the
  previously recorded native canvas CPU/GPU equality controls. The other two
  were mock proxy-verification request expectations still naming the example
  host. After updating those expectations, the focused verifier and expanded
  route run passed all 8 tests (22.01s). A final all-tests-in-one run has not
  been claimed.
- Local WebRTC contract/verify asset checks pass. CI only checks the contract
  and test syntax here; it does not build Chromium or run these live tests.

Expanded routing cases cover IPv4/IPv6 loopback and link-local URL
destinations. These prove requests reach the proxy; only the 127.0.0.1
disconnect test includes an independently reachable direct-origin control.
They are not a general packet-capture proof or a substitute for live TURN,
SOCKS and product lifecycle checks on the final archive.

Linux incremental build and broad compatibility validation remain pending.
PixelScan, macOS portable fallback fonts and product-level release gates
remain open independently of this change.

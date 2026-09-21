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

## Linux candidate

Siso incremental build succeeded in 2m53s. Packaged and freshly extracted:

- ELF SHA256: `0bf06ff97602e1c2a4ce4177df9a7b1fee6b8eb9ee8b821ad6b0566dc0273d7e`.
- Archive SHA256: `668c56b5402816f07afb7cae411a382000f8726fe21b007f8f12bb2876f34e7a`.
- All 215 C++ tests pass as the unprivileged builder user. An initial root run
  failed the directory-not-writable negative test; root is not a valid user
  for that permission control, and the failed report is retained separately.
- Actual HTTP and SOCKS5 TURN/TLS echo: 2 pass (17.76s), with successful relay
  pairs and bidirectional bytes. The probe uses an explicit in-memory document,
  not a localhost network-error page behind the real remote proxy.
- Independent IPv4/IPv6 STUN positive control: 4 captured packets, zero drops.
  Browser phase through HTTP and SOCKS5: zero UDP packets to the controlled
  port, zero candidates, zero capture drops. This is endpoint-scoped proof,
  not a claim about every possible traffic destination.
- Bounded TURN/STUN services stopped after the checks. Browser sandbox remains
  enabled, with an exact QA executable-path AppArmor userns profile.

Full headful Linux run: **119 passed, 8 skipped, 263.10s** using both isolated
font controls. Final full macOS run: **108 passed, 2 failed, 17 skipped,
236.04s**. The two failures are the same native canvas equality controls
recorded before this proxy change, not protected-mode failures. Opt-in live
network checks are reported separately above rather than counted as full-suite
passes. The final TURN test has a probe-document-only overlay on the f435ef2
test snapshot; it does not change the candidate binary.

PixelScan, macOS portable fallback fonts and product-level release gates
remain open independently of this change.

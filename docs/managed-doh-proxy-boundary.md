# Managed proxy boundary and built-in DoH

## Reproduction on Linux candidate 065

Chromium `net/dns/dns_http_attempt.cc` sets `LOAD_BYPASS_PROXY` for built-in
DNS-over-HTTPS requests, including availability probes. A working fixed proxy
for ordinary page requests does not prevent those direct connections.

The headful, sandboxed, non-root regression seeds **only a temporary test
profile's Local State** with automatic DoH and a localhost HTTPS receiver.
It does not alter the system resolver or a real user's preferences. The receiver
counts TLS ClientHello records and closes the socket: it does not complete a
DNS exchange. The ordinary page must also load through the controlled proxy.

On 065 both an unproxied browser control and the fingerprint/proxy case made
**12 direct TLS connections in the eight-second observation window**. The
control passed and the managed-proxy assertion failed, as required to detect
the problem. Evidence: `doh-probe-065-control-fixed.xml` (1 pass, 1 fail, 19.84s).
The first exploratory control used the wrong test user-data directory and is
not a valid negative/positive comparison. Its result is retained separately.

This demonstrates a proxy bypass when built-in DoH is configured. It is not
evidence that a public DNS provider received the QA server's IP during this
test; the receiver was local. Prior destination-name DNS, HTTP/HTTPS outage,
STUN and TURN tests remain valid for their stated scopes, but did not cover
background DoH probing.

## Fix

Patch `061-managed-proxy-disable-direct-doh.patch` makes the effective stub
resolver configuration turn DoH off when the browser command line expresses
`--fingerprint` together with `--proxy-server`, or explicit
`--clawbrowser-require-proxy` intent. This also takes precedence
over explicit secure-DNS preferences/policy; such preferences cannot override
the mandatory proxy boundary. It runs on configuration refresh as well as
initial configuration and does not rewrite the stored preferences.

Unproxied sessions without explicit require-proxy intent retain Chromium's DoH
behavior. Ordinary destination names still go to the proxy. Proxy-host bootstrap
resolution remains a separate concern; this patch does not claim to eliminate
all operating-system DNS or all browser background networking.

The first 066 implementation checked fingerprint intent alone. It blocked
managed probes but also blocked the unproxied Auth control, because startup
adds an implicit fingerprint flag there. That candidate is **not accepted**.
The corrected condition additionally requires a fixed proxy for implicit
fingerprint intent, while explicit require-proxy intent always enforces the
boundary. The corrected 067 build and live-control result are below.
macOS/Windows runtime acceptance is separate.

The opt-in test is `test_doh_proxy_diagnostic.py`, enabled by
`CLAWBROWSER_DOH_DIAGNOSTIC=1`. A successful control must observe direct TLS;
absence of traffic in both cases is not acceptance.

## Corrected Linux candidate 067

Runtime source commit: `3b39222`. Incremental build: two steps in 3m08.03s.
ELF SHA256: `47218b672d1377cdff5c6638b2126ad153ef734a5764efba99fb7507641c1de9`.
QA archive SHA256: `c257d43826bef3d1cbdad826ef8faf1d67fa139953abd90903e265bd02446267`.
The archive was extracted to a new path and tested as non-root with sandbox
enabled. It was not published as a release or deployed to production.

The four automatic/secure × unproxied/proxied cases passed in **40.07s**:

| DoH mode | Unproxied TLS control | Managed proxy TLS connections |
| --- | ---: | ---: |
| Automatic | 12 | 0 |
| Secure | 240 | 0 |

Counts cover eight seconds per case and are not throughput benchmarks; the
receiver deliberately closes handshakes, causing retries. Both stored mode
preferences remained unchanged. The ordinary page's absolute proxy request
was also asserted, preventing a direct page load from satisfying the gate.
Evidence: `doh-modes-067.xml`.

The regression has additionally been strengthened to count **all accepted TCP
connections**, including those that never send a full TLS record, and to check
the active Canvas policy. The normalized-policy run passed all four cases in
**39.652s**: unproxied automatic/secure controls produced 12/244 TCP connections
and TLS ClientHellos, while both managed cases produced **zero TCP connections**.
The live Canvas canaries reported normalized mode with zero changed pixels.
Evidence: `doh-modes-067-normalized.xml`.

The full headful, non-root, sandboxed Linux integration run completed:
**207 passed, 25 skipped, zero failures/errors in 595.65s**
(`full-suite-067.xml`). It includes the strengthened protected-policy DoH
cases: automatic control 12 TCP/12 TLS; secure control 254 TCP/253 TLS;
both managed cases zero TCP/TLS. The protected Canvas canaries changed 368
RGB channel values by at most one, with no alpha changes.

The 25 skips are not passes: eight macOS-only cases, five opt-in timing/draw
diagnostics, four DNS-hint cases requiring an external capture, seven
real-service/proxy/TURN cases requiring separate configuration, and one
native-Canvas-only case. Dedicated controlled STUN/TURN evidence below covers
its own stated scope; it does not convert unrelated integration skips to passes.
Two investigation-only modules (navigation Client Hints and bitmaprenderer
snapshot diagnostics) were explicitly excluded from this regression invocation.

Fresh 067 STUN and TURN/TLS checks passed under both Canvas policies: live STUN
controls produced four packets versus zero browser packets; live direct TURN
controls produced seven packets versus zero direct browser packets. Both
proxy schemes completed TLS relay-to-relay DataChannel echoes, with certificate
validation enabled and zero capture drops. This preserves the earlier network
functionality while closing the newly reproduced DoH bypass.

Both controlled nextctl startup/outage gates also passed (37.912s protected,
38.103s experimental normalized). These exercise fingerprint/API failures,
missing proxy host/port, an unsupported proxy scheme, and proxy outage without
direct fallback. They use the gate's headless/no-sandbox/test-certificate
configuration, so are not presented as headful or trusted-certificate evidence.
Reports: `doh-boundary-067-{protected,experimental}-outage.jsonl`.

PR contract/portability CI is green for `087cd3b`, but does not run a complete
macOS or Windows browser. The QA archive is not the published runtime used by
nextctl's public-artifact gate. Real-site screenshots from candidate 065 also
must not be relabelled as 067 results. PixelScan's separate font inconsistency
and physical-platform acceptance remain open.

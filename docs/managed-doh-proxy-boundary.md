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
`--fingerprint` or `--clawbrowser-require-proxy` intent. This also takes precedence
over explicit secure-DNS preferences/policy; such preferences cannot override
the mandatory proxy boundary. It runs on configuration refresh as well as
initial configuration and does not rewrite the stored preferences.

Vanilla sessions without either managed-intent switch retain Chromium's DoH
behavior. Ordinary destination names still go to the proxy. Proxy-host bootstrap
resolution remains a separate concern; this patch does not claim to eliminate
all operating-system DNS or all browser background networking.

At the initial fix commit the new Linux binary is still being linked. Do not
mark this scenario accepted until the same live-control test passes on the
new extracted artifact. macOS/Windows runtime acceptance is separate.

The opt-in test is `test_doh_proxy_diagnostic.py`, enabled by
`CLAWBROWSER_DOH_DIAGNOSTIC=1`. A successful control must observe direct TLS;
absence of traffic in both cases is not acceptance.

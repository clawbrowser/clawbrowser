# PixelScan WebRTC: empty candidates classified as a leak

## Observed artifact and result

Sandboxed Linux ClawBrowser ELF:
`5f7fb6e5686c78c775f58f2fdbbdc8202e5b67b5f46cf5b17e8bd2b04d393bb8`.
The existing managed QA profile passes 35 built-in checks. Its HTTP egress
address differs from the QA host. No stock Chromium was used for this test.

On the normal [WebRTC checker](https://pixelscan.net/webrtc-check), clicking
Start Check produces a red leak warning. The summary displays the same proxy
IP as the ordinary fingerprint page. All three connection panels show no local
or external addresses. The original red screenshot is retained, not presented
as a green checker result.

## Source and network diagnosis

The page loaded [this checker chunk](https://pixelscan.net/150.59771b0c527663a4.js).
Its success boolean is true only when the HTTP response's public IP occurs
in a STUN or TURN external-address list. Empty lists therefore set success to
false; the UI maps that value to its red warning. This also means this boolean
alone does not distinguish absent candidates from a revealed nonmatching IP.

A separate read-only network observation captured the normal `/wr` request:

| Source | Local IPv4 | Local IPv6 | External IPv4 | External IPv6 |
| --- | --- | --- | --- | --- |
| Default ICE | 0 | 0 | 0 | 0 |
| STUN | 0 | 0 | 0 | 0 |
| TURN | 0 | 0 | 0 | 0 |

The successful HTTP 200 response is an `ok/value` envelope; after decoding it,
the public IP matches the previously displayed proxy address and is not the
QA server address. Early diagnostic output did not unwrap that envelope and
must not be interpreted as a missing or changed IP. The final parser asserts
the envelope and all three request-source keys before reporting observations.
No raw SDP, proxy credentials or authentication tokens are included in evidence.

For this run, the red warning is **not evidence of the host IP leaking**:
the site's submitted ICE lists are empty. This is narrower than claiming all
WebRTC routes are safe. Separate controlled packet tests on the same artifact
find zero direct STUN packets and zero direct TURN/TLS connections, while real
TLS relay echo works through both HTTP and SOCKS5 proxies. See the
[artifact acceptance report](cross-platform-canvas-2026-09-15.md).

The fingerprint checker's independent font and Canvas masking warnings remain
open. No PixelScan-specific product exception, disabled Canvas protection,
changed classifier input or forged green result was used. This report has not
been sent to PixelScan and does not grant merge or release approval.

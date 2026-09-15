# PixelScan WebRTC: empty candidates classified as a leak

## Observed artifact and result

Latest confirmation on September 15 uses extracted Linux ClawBrowser ELF
`c0670393e68f9df237c9b552483abbc61397608dcc98a86ff057a3cc520ce173`
(patch 049). The managed profile passes 35/35 built-in checks. One ordinary
Start Check with passive request/response observation returns HTTP 200, the
same red warning, and zero entries in each of the 12 candidate arrays below.
The response public IP is present, matches the fingerprint page, and is not
the QA server IP. The screenshot and sanitized JSON are retained as
`webrtc-exact-srcover-049.png` and `webrtc-exact-srcover-049.json` in the local
`clawbrowser-linux-c067039` evidence set. Neither the classifier nor its inputs
were modified. Current controlled STUN/TURN results are in the linked artifact
report. The earlier investigation below is retained with its original hash.

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

# Managed launch capability regression

A real `nbc start` against the isolated QA backend exposed a defect that
component-level network acceptance had missed. Authentication succeeded using
the pre-existing QA integration identity in a separate OS account, but the CLI
refused the session because the runtime did not attest privacy capability 2.
The user's saved builder account was not changed.

`ManagedProxyPrivacyCapabilityForCommandLine` rejected any `proxy-bypass-list`
switch. Startup now correctly adds the subtractive `<-loopback>` rule to close
Chromium's implicit loopback/link-local exceptions. Consequently, valid managed
launches attested capability 0 and were stopped by the CLI. This was a false
rejection, not evidence of direct fallback.

Commit `d68176f` requires the exact subtractive rule. Missing, empty, wildcard,
local-host and mixed exception lists remain rejected, as do conflicting proxy
and WebRTC switches. No CLI capability gate was disabled or lowered.

Evidence:

- On the previous Linux `0631a21` archive, the new live WebUI regression failed
  with `0 != 2` in 1.88 seconds (`capability-bypass-browser-before.xml`).
- On the rebuilt macOS candidate `20260915-150759`, all seven routing and live
  capability tests passed in 20.11 seconds (`mac-capability-bypass-fixed.xml`).
  Launcher SHA256: `945d653b9470f2465381e246d944afc02b16facc51f371e964275167db13c4df`.
- The pure capability unit cases belong to `clawbrowser_browser_unittests`,
  not `clawbrowser_unittests`. A zero-matching-test invocation is explicitly
  not evidence of success. Building the browser-unit target required thousands
  of additional targets and was stopped; those added unit cases are not yet
  claimed as executed. The real-browser regression above did execute.

The next managed attempt exposed a second defect in nextctl: final browser
argument filtering discarded the lifecycle layer's own `--clawbrowser-require-proxy`.
PR nextbrowser-oss/nextctl#26 commit `f1ef79f` restores the canonical flag once,
after filtering duplicates and user variants. The final-argv regression failed
for Linux/macOS/Windows before the fix, then passed. CLI, launcher, session and
path packages passed; all four CLI cross-platform CI builds passed.

With both fixes, normal managed startup against QA port 18081 succeeded and
`nbc verify` passed all 35 checks. Authentication used the isolated integration
account; desktop OAuth is still not covered. The rebuilt Linux archive passed
120 integration tests with 16 skips in 282.54 seconds. macOS passed 119 with
15 skips and the same two native-canvas failures in 275.78 seconds.

Linux ELF SHA256: `63a0b953f0c8fb27b60f9dd627f111f4ddecad438eeb5a97c33407b4c7d80924`.
Archive SHA256: `b6277fa3d8b18a382a057940c8f67daa32794059d9d35811fb96fb4248344d3c`.
The extracted binary hash matches. Sandbox and the capability gate stayed on.

PixelScan's fingerprint page was reached through this managed profile and
still reports **inconsistent / Masking detected**. Its displayed location was
United States / Stanley, with “No proxy detected” and “No automated behavior
detected.” Those website labels are not proof that there is no configured
proxy or that every fingerprint surface is correct. Screenshot:
`pixelscan-capability-fixed.png`. The browser-version tile displayed
151.0.7922.76 while the build version is 151.0.7922.109; the profile/version
mapping needs a separate audit before attributing the tile warning.

The separate PixelScan WebRTC page reports a potential leak for `24.88.27.72`.
Its ICE/STUN/TURN detail panels show no local or external candidate addresses.
The same profile's ordinary PixelScan IP page independently displays exactly
`24.88.27.72` (Stanley, US). This is not the QA server address `151.115.167.3`.
Thus the red label in this run does not demonstrate exposure of the server IP;
it must not be reported as a green website result either. The source of that
site label and the broader fingerprint inconsistency still need analysis.
Screenshots: `pixelscan-webrtc-capability-after-wait.png` and
`pixelscan-http-ip-capability.png`.

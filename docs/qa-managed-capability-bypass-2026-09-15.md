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

Linux rebuild, fresh archive acceptance and managed PixelScan remain separate
gates. A passed fixture or isolated-service test does not prove desktop OAuth.

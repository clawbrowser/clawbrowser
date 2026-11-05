# ClawBrowser manual release checklist

Copy the evidence table below into a release-specific record before testing.
Use `PASS`, `FAIL`, `BLOCKED`, `PENDING`, or `N/A` with a reason. An unchecked
item is pending. A successful build, cross-compile, fixture test, or check of
an older browser does not establish that the candidate works on its target OS.

This checklist covers the native ClawBrowser runtime and its CLI integration.
NextBrowser's Electron app, APIs, and admin panels have separate artifacts;
record their versions and checks separately when they ship together.

## Release identity and scope — required

- [ ] Record the release version, exact browser project commit, Chromium version
  and revision from [clawbrowser_pin.conf](../scripts/clawbrowser_pin.conf),
  build configuration, toolchain versions, OS, architecture, and CI run links.
- [ ] List selected PRs and their inclusion in the release commit. Record
  exclusions explicitly. Do not include an unrelated branch merely because
  it exists or because its CI passed.
- [ ] Record the matching clawctl and, if applicable, nbc versions and source
  revisions. Pin these candidates during QA so an updater cannot silently
  replace a binary with the stable release.
- [ ] Draft release notes describing user-visible behavior, fixes, known
  limitations, supported platforms, upgrade requirements, and rollback steps.
  Keep credentials and customer identifiers out of public evidence.

## Build and package matrix — required for every shipped target

| Target | Package to verify | Required execution environment |
| --- | --- | --- |
| Windows amd64 | `clawbrowser-win-amd64.zip`, including its actual `setup.exe` payload when present | Clean supported Windows VM/host |
| macOS arm64 | Native `Clawbrowser.app` and each archive/installer actually distributed | ARM Mac with a GUI desktop context |
| Linux x64 | Native archive; Docker image and portable archive when distributed | Matching x64 host; separately test the container and no-display portable path |
| Linux arm64 | Native archive; Docker image and portable archive when distributed | Matching ARM64 host; a cross-build alone is insufficient |

Builder archives can have names such as `clawbrowser-prod-...` or use `x64`
where an installer expects `amd64`. Record the final download names and test
them against the matching CLI's resolver. Do not rename assets by assumption.
The portable payload is distinct from both the browser archive and the
standalone clawctl bootstrap archive. See [INSTALL.md](../INSTALL.md).

- [ ] Build from the recorded source with the production argument writer in
  [build_remote_prod_artifacts.sh](../scripts/build_remote_prod_artifacts.sh)
  or [build_windows_prod_clawbrowser.ps1](../scripts/build_windows_prod_clawbrowser.ps1).
  Archive effective `args.gn` and source/toolchain metadata. Document any
  intentional difference; do not weaken runtime protections to fit a runner.
- [ ] Run native core and browser tests on the target architecture. Save full
  result files, failure counts, skips, and logs. Explain every required skip.
- [ ] Inspect package layout, executable architecture/version, bundled fonts,
  resources, loader/shared libraries, and runtime dependency availability.
- [ ] Start the extracted or installed payload itself. Source-tree execution
  does not replace a check of the distributed package.
- [ ] For Docker, exercise the default entrypoint/display setup and the
  intended persistent config mount. For portable Linux, execute the shipped
  wrapper and bundled loader/Xvfb as an unprivileged user without a display.
- [ ] Scan final distributed payloads with the approved scanner and retain its
  engine/version, scan time, and results. Resolve findings before promotion.

## Signing and integrity — required for the intended distribution channel

- [ ] On macOS, verify the final native app's expected signing identity,
  bundle identifier, entitlements, nested code, and hardened-runtime settings.
  Ad-hoc signing is candidate evidence only, not Developer ID notarization.
- [ ] Verify successful notarization, ticket stapling, and Gatekeeper
  assessment for the actual macOS distribution format. Test a newly downloaded
  artifact with normal quarantine behavior; do not bypass security prompts.
- [ ] When an Electron app ships with this release, verify its DMG and updater
  ZIP independently. A signed Electron package does not prove the separate
  native ClawBrowser runtime is signed, tested, or notarized.
- [ ] On Windows, verify the expected publisher/signature and install behavior
  required by the channel. Record an unsigned candidate explicitly.
- [ ] Compute SHA256 after final signing, notarization, and packaging. Check
  every distribution asset against its checksum manifest. Validate updater
  metadata, versions, sizes, hashes, and architecture for each consumer.
- [ ] Keep signing keys on approved signing infrastructure. Public evidence
  must contain verification results, not keychain credentials or private keys.

## Clean installation, upgrade, and removal — required

- [ ] On a disposable clean host, follow the supported standalone clawctl
  install flow using the exact candidate assets. Save the resolved versions,
  asset names, diagnostics, and final installed binary hashes.
- [ ] Check installation locations and permissions, initial launch, restart,
  and actionable failure messages. Confirm cancellation/failure leaves a
  usable prior installation or a clear recoverable state.
- [ ] Upgrade from the previous supported release using copied test profiles.
  Preserve profile identity, proxy selection, configuration, and user data.
  Verify that the running process uses the intended new executable.
- [ ] Exercise the shipped runtime-update journey, including active-profile
  handling. An already-current version check does not test an actual update.
- [ ] When shipping the desktop app, test per-user/all-users modes as supported,
  app-to-CLI IPC, and OAuth callback registration through install and upgrade.
  On uninstall remove only the callback owned by that installation; preserve
  another installation's handler and follow the documented data-retention rule.
- [ ] Use disposable hosts for installers and integration setup that can alter
  protocol handlers, agent configuration, or other user-level registrations.

## Profile, proxy, and CLI/MCP behavior — required

- [ ] Create, start, stop, and reopen a test profile with the candidate runtime.
  Confirm the intended browser, proxy, and saved fingerprint are retained.
- [ ] Verify a manual local proxy, including authentication where supported,
  from inside the actual browser. Check HTTP, HTTPS, WebSocket, and secure
  WebSocket traffic using controlled endpoints and recorded request counts.
- [ ] Test bad credentials, proxy refusal/outage, failed upstream requests,
  and startup-check failure. Confirm no direct fallback using a separate
  positive control proving the direct-traffic detector can observe traffic.
- [ ] For local proxy geography changes, cover a matching exit, canonical
  timezone aliases, provider fallback/outage, mismatching geography, and
  legacy profiles without required geography. Record the browser's actual
  verification result and fail-closed behavior, not just command-line flags.
- [ ] Run the matching CLI's version/start/stop/verify and MCP initialize,
  tool listing, and representative browser operations. Test missing/invalid
  credentials and confirm useful errors without leaking secrets.
- [ ] When Multilogin integration is included, launch an approved real test
  profile through the candidate CLI. Mock launcher tests do not establish
  real profile startup or the vendor account's API entitlement.
- [ ] Check diagnostics for failed launches and correlation with the relevant
  telemetry when configured. Retain sanitized logs; do not publish tokens,
  proxy credentials, full customer profiles, or private conversation content.

## Feature-specific checks — required when included

- [ ] Record regression checks for each shipped fix, including its original
  failing case, before/after evidence, and the exact artifact tested.
- [ ] Run the applicable fingerprint, font, canvas, network, and client-hints
  acceptance checks. Explain environment limits; do not claim that an
  unrelated detector result establishes all-platform privacy correctness.
- [ ] For remote Live viewing, use the authenticated backend relay. Verify
  different hosts with direct host-to-host traffic blocked, including reconnect
  and backend outage. Local CDP capture is not a substitute for this path.
- [ ] For Helpdesk, test approved real inbound and outbound sources, threading,
  deduplication, restart recovery, near-real-time delivery, and product/user
  isolation. Fixture providers and SQL tests remain separately labelled.
- [ ] For the built-in agent, test the advertised model provider with working
  credentials, tool calls, browser operations, errors, and cancellation.

## Publication, rollback, and post-release checks — required, in order

- [ ] Review all required results for each intended target. Record a release
  decision and unresolved limitations. `BLOCKED` and `PENDING` are not passes.
- [ ] Confirm authorization to publish the specified versions and targets.
  Candidate build authorization alone does not authorize stable promotion.
- [ ] Preserve the previous verified binaries, checksums, updater metadata,
  and copied configuration/profile backup. Define the rollback owner and
  trigger; test binary/config compatibility before downgrading profile data.
- [ ] After approval, publish only verified final assets and notes. Account
  for release-triggered Docker/portable workflows before publishing a stable
  GitHub release; their effects are part of the rollout.
- [ ] Download the published assets independently, recheck hashes/signatures,
  and exercise clean install and upgrade through the public resolver/updater.
  Record resulting versions, links, and sanitized launch/verification evidence.
- [ ] Monitor startup, proxy, update, and support failures against the previous
  release. If rollback is needed, restore approved distribution pointers and
  known-good assets; recheck actual consumer downloads afterward.

## Optional checks

- [ ] Additional physical hardware/GPU coverage beyond the required matrix.
- [ ] Extended soak and performance comparisons beyond agreed release budgets.
- [ ] Additional third-party compatibility or detector checks, explicitly
  scoped to their tested environment and without site-specific bypasses.

An optional check becomes required when it is an acceptance criterion for a
selected change. Record the reason for every `N/A` decision.

## Evidence record template

| Field | Value |
| --- | --- |
| Release/date and decision owner | |
| Project SHA / Chromium version and revision | |
| CLI/app/API/admin versions and SHAs, where applicable | |
| Selected PRs / explicit exclusions | |
| Target OS/architecture / host image and toolchains | |
| Final asset name / SHA256 / updater metadata | |
| Installation source and previous version for upgrade | |
| Test identity and environment; secrets omitted | |
| Check / status / observed result / evidence link | One row per check and target |
| Required failures, skips, blockers, and owners | |
| Publication decision and authorization reference | |
| Rollback assets, trigger, and owner | |
| Published download and post-release verification | |

Keep results tied to the tested source and artifact hash. A later rebuild or
changed runtime invalidates affected artifact checks. Documentation-only
follow-ups should identify the original runtime source rather than relabel
an existing binary as compiled from the documentation commit.

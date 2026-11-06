# Release QA snapshot — 2026-11-05

Updated 2026-11-06 for the candidate dated 2026-11-05.

Decision: **HOLD — candidate preparation only.** Native browser execution,
required live integrations, and corporate macOS signing remain unverified.
This record applies [the manual checklist](release-checklist.md) to the
current candidates. It is not a release announcement or publication approval.
Statuses are a dated snapshot; follow the linked runs for later results.

## Candidate identity

| Component | Candidate | Source |
| --- | --- | --- |
| Native ClawBrowser | 1.0.12 | `8eeb132817152c4f0bf5fa72968f39cb226b53f3`; build-only MIDL locale fix in release PR #60 |
| Chromium base | 151.0.7922.109 | `28a7a6c409e03c701d3474ef9e3b1f0be6249039` |
| NextBrowser desktop | 0.2.73-rc.2 | `f94e9c23b383e66161800a2941e539417b767c4b` |
| nbc | 1.2.40-rc.2 | `3cade3b8dd0d96cca5b0c8c8c56989d408205777`; Windows fixture corrections merged into release branch only |
| clawctl | 1.3.6-rc.2 | `36f885b4b25e23aa3f471edda6a6fb6b2dc8d83a`; test/CI corrections merged into release branch only |

This documentation follow-up does not change the native source being built.
The current native release source includes a build-only follow-up to the
earlier `85ede59` candidates. No earlier artifact is relabeled as this source.
NodeMaven traffic-accounting/migration changes are excluded from this release.

## Results and remaining gates

| Check | Target/scope | Status | Evidence or remaining work |
| --- | --- | --- | --- |
| Desktop candidate packaging | Windows amd64, macOS arm64 | PASS | [Exact-source app CI](https://github.com/nextbrowser-oss/nextbrowser-app/actions/runs/42040285553) |
| Desktop renderer tests | Both app platforms | PASS | 1159 tests per OS in the app CI above |
| Desktop Node tests | Windows / macOS | PASS | Windows 618 passed, 5 skipped; macOS 623 passed; zero failures. Skips remain visible in CI. |
| Desktop TypeScript, packaged launch and production modal blur | Both app platforms | PASS | App CI above; this does not test the separate native runtime |
| Actual installed Windows app and final rebuilt candidate CLI IPC | Per-user install on a disposable VM | PASS | Exact final CLI source above; release evidence records real renderer IPC, isolated storage, zero renderer errors, update and owned/foreign OAuth handler cleanup |
| Windows app install/update/uninstall | All-users mode | PASS | [Installer QA](https://github.com/nextbrowser-oss/nextbrowser-app/actions/runs/42047329643): owned OAuth handler removed on uninstall, preserved on update, foreign handler preserved |
| Windows app signature | Distributed installer | PENDING | Current candidate is unsigned; distribution signing decision remains open |
| macOS app signature/notarization | Actual distribution artifacts | BLOCKED | Candidate is ad-hoc signed. Corporate signing/notarization has not been demonstrated. |
| Native compilation and C++ tests | Linux x64 and ARM64 | BLOCKED | [Standard x64-hosted builders](https://github.com/clawbrowser/clawbrowser/actions/runs/42031130414) both stopped at the six-hour hosted execution limit; no native runtime pass. A persistent builder or verified resumable build is needed. The separate ARM-host experiment failed on x64-only host tools after gperf build/check passed. |
| Native compilation and C++ tests | Windows amd64 | PENDING | [Earlier hosted builder](https://github.com/clawbrowser/clawbrowser/actions/runs/42033783430) stopped at six hours. Local core test binary compiled and all 242 tests passed. Production browser compilation resumed at the current source after the MIDL build fix; package/runtime acceptance outstanding. |
| Native core C++ tests | Local Windows amd64 | PASS | 242 passed, zero failures/errors/disabled. Executed at 85ede59; current source differs only in the MIDL build patch, its regression fixtures, documentation and CI invocation. Browser runtime implementation unchanged. |
| Windows MIDL reproducibility | Actual Russian-locale host | PASS | [Release-only PR #60](https://github.com/clawbrowser/clawbrowser/pull/60): original action now passes full unchanged output comparison with SDK 26100.7705 and 26100.9457; five actual-typelib regression checks and four CI checks passed. Binary baselines and runtime code unchanged. |
| Native compilation and C++ tests | macOS arm64 | BLOCKED | Both [Intel cross-builder](https://github.com/clawbrowser/clawbrowser/actions/runs/42037844724) and [native ARM builder](https://github.com/clawbrowser/clawbrowser/actions/runs/42054605490) on the earlier source exceeded the six-hour hosted limit. Persistent build capacity/access is required. |
| Native Mac signing/notarization | ClawBrowser distribution | BLOCKED | Corporate signing access and final distribution validation required; ad-hoc build integrity alone is insufficient |
| Native package startup and dependencies | Every intended native target | PENDING | Run the actual extracted/installed candidate on each target OS and architecture |
| Native clean installation and upgrade | Every intended native target | PENDING | Include copied existing profiles, identity/proxy preservation, restart, and actual update flow |
| Native proxy outage/no direct fallback | New native candidate | PENDING | Independent exact-artifact consumers prepared; old-browser canary is not a pass for this source |
| Native local-proxy geography | New native candidate | PENDING | Matching, timezone alias, fallback, authenticated proxy, mismatch, provider outage, and missing geography cases prepared |
| Linux Docker and portable runtime | x64, ARM64 distribution paths | PENDING | Build from successful exact native artifacts, then execute default entrypoint and shipped portable wrapper/loader |
| Full CLI tests and vet | Linux release sources | PASS | nbc 1520 passed / 6 skipped; clawctl 979 passed / 3 skipped; zero failures, vet passed; both final source candidates rebuilt for four targets |
| Actual final packaged CLI executables and MCP/auth | Native Windows amd64, macOS ARM64, Linux amd64 and ARM64 hosts | PASS | [NBC artifacts](https://github.com/nextbrowser-oss/nextctl/actions/runs/42081068769), [Clawctl artifacts](https://github.com/clawbrowser/clawctl_internal/actions/runs/42082006759): eight exact executables, 36 supported MCP/auth sessions; versions, source metadata and binary hashes matched. Loopback API fixtures, no real accounts or browsers. |
| Full Windows nbc suite | Final release tree | PASS | 1500 passed, 26 skipped, zero failures; vet and Go temporary-binary cleanup passed. Tested tree 2c6d5d9 equals final merge 3cade3b; runtime code unchanged from c69c128. Original 16 failures and intermediate cleanup failure remain archived. |
| Full Windows clawctl suite | Final release Go/test sources | PASS | 959 passed, 23 skipped, zero failures; vet and Go cleanup exit 0. Final 36f885b differs from Windows-tested 4625be2 only in CI YAML; final tree equals Linux-tested 925b7c7. Runtime unchanged from baaff8d; original 12 failures retained. |
| Actual rebuilt Windows clawctl binary | Isolated HOME/USERPROFILE and loopback auth fixture | PASS | Final artifact hash/source/version checked; version command makes the expected auth request without changing the binary or writing updater cache. No real account/profile used. |
| Real Multilogin launch | Approved vendor test profile via candidate CLI | BLOCKED | API-enabled test workspace/profile access required; mock launcher checks are separate evidence |
| Helpdesk regression | UI/API fixtures and backend database CI | PASS | Source-level fixture evidence only; does not prove live provider delivery |
| Helpdesk real inbound/outbound | Approved Discord, GitHub and Gmail sources | BLOCKED | Integration provisioning, configuration, and real threading/recovery checks required |
| Built-in agent live provider/tool flow | Approved model credentials | PENDING | Complete actual model-to-browser tool flow; mocked/unit checks alone are insufficient |
| Checksums and updater metadata | Built app/CLI/service candidates | PASS | Current app/CLI archives and service archive metadata verified in the release evidence |
| Final native asset checksums, resolver names, signatures and scan | Every native distribution asset | PENDING | Requires final native artifacts; verify again after signing/packaging |
| Rollback rehearsal | Final versions and profile/config compatibility | PENDING | Preserve previous verified assets and rehearse supported rollback with copied data |
| Stable publication, Docker push, deployment | Intended release targets | PENDING | Not authorized or performed; required gates must be reviewed first |
| Public download/install/update and monitoring | Published release | PENDING | Perform only after approved publication; record actual downloads and installed versions |

## Evidence boundaries

- The desktop Electron package and native ClawBrowser are different binaries.
  Their successful builds and signatures are not interchangeable evidence.
- Tests of released ClawBrowser 1.0.11 established the earlier controlled proxy
  outage behavior, not correctness of the new C++ geography implementation.
- A controlled old-browser regression test exercises the local proxy fixture
  and observes the old remote-verification behavior. The new native seven-case
  test execution is still pending.
- Fixture Helpdesk delivery and database tests establish implemented behavior
  in that environment; actual external accounts and webhook configuration
  require a separate acceptance check.
- Windows fixture corrections use the supported explicit data directory,
  preserve isolated config, and check vendor-secret scrubbing in a portable
  child process. A Linux case-sensitive filesystem fixture is explicitly
  skipped on Windows; Unix sticky-bit tests execute on Unix. FFmpeg is installed
  for media tests. The final successful run also terminates only registered
  fake browser helpers whose actual image matches this suite's test executable.
- Complete Go suite success includes the recorded skips. Real-browser gates
  that require an explicit candidate, network installer smoke, and some
  platform-specific installer/self-update fixtures are not exercised by the
  ordinary Windows unit run. Actual native install/upgrade and proxy execution
  remain separate pending checks above.
- Packaged CLI protocol checks respect each product's startup contract. NBC
  used enabled auto-update and both direct and desktop MCP arguments; its
  server handshakes before returning missing/invalid-key tool errors. Clawctl
  used an explicitly disabled updater in its isolated test environment; it
  handshakes with a valid key and rejects missing/invalid keys before starting
  the stdio server. This does not establish Clawctl updater behavior during MCP
  startup, provider delivery, or a real browser launch. Candidate runtime code
  was unchanged for these checks.
- No stable tag, release upload, registry push, or production deployment is
  implied by the PASS rows. Update this record with final hashes and execution
  evidence before making a release decision.

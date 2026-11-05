# Release QA snapshot — 2026-11-05

Decision: **HOLD — candidate preparation only.** Native browser execution,
required live integrations, and corporate macOS signing remain unverified.
This record applies [the manual checklist](release-checklist.md) to the
current candidates. It is not a release announcement or publication approval.
Statuses are a dated snapshot; follow the linked runs for later results.

## Candidate identity

| Component | Candidate | Source |
| --- | --- | --- |
| Native ClawBrowser | 1.0.12 | `85ede59686a51e0828eadfaa6f0d137cad5efaea` |
| Chromium base | 151.0.7922.109 | `28a7a6c409e03c701d3474ef9e3b1f0be6249039` |
| NextBrowser desktop | 0.2.73-rc.2 | `f94e9c23b383e66161800a2941e539417b767c4b` |
| nbc | 1.2.40-rc.2 | Recorded separately in the CLI release evidence |
| clawctl | 1.3.6-rc.2 | Recorded separately in the CLI release evidence |

This documentation follow-up does not change the native source being built.
Do not relabel an existing binary with this document's later commit hash.
NodeMaven traffic-accounting/migration changes are excluded from this release.

## Results and remaining gates

| Check | Target/scope | Status | Evidence or remaining work |
| --- | --- | --- | --- |
| Desktop candidate packaging | Windows amd64, macOS arm64 | PASS | [Exact-source app CI](https://github.com/nextbrowser-oss/nextbrowser-app/actions/runs/42040285553) |
| Desktop renderer tests | Both app platforms | PASS | 1159 tests per OS in the app CI above |
| Desktop Node tests | Windows / macOS | PASS | Windows 618 passed, 5 skipped; macOS 623 passed; zero failures. Skips remain visible in CI. |
| Desktop TypeScript, packaged launch and production modal blur | Both app platforms | PASS | App CI above; this does not test the separate native runtime |
| Actual installed Windows app and candidate CLI IPC | Per-user install on a disposable VM | PASS | Release evidence records real renderer IPC, isolated storage, and zero renderer errors |
| Windows app install/update/uninstall | All-users mode | PASS | [Installer QA](https://github.com/nextbrowser-oss/nextbrowser-app/actions/runs/42047329643): owned OAuth handler removed on uninstall, preserved on update, foreign handler preserved |
| Windows app signature | Distributed installer | PENDING | Current candidate is unsigned; distribution signing decision remains open |
| macOS app signature/notarization | Actual distribution artifacts | BLOCKED | Candidate is ad-hoc signed. Corporate signing/notarization has not been demonstrated. |
| Native compilation and C++ tests | Linux x64 and ARM64 | PENDING | [Linux builders](https://github.com/clawbrowser/clawbrowser/actions/runs/42031130414); ARM cross-compilation must be followed by ARM execution |
| Native compilation and C++ tests | Windows amd64 | PENDING | [Windows builder](https://github.com/clawbrowser/clawbrowser/actions/runs/42033783430); local fallback also remains in progress |
| Native compilation and C++ tests | macOS arm64 | PENDING | [Intel cross-builder](https://github.com/clawbrowser/clawbrowser/actions/runs/42037844724) and [native ARM builder](https://github.com/clawbrowser/clawbrowser/actions/runs/42054605490) use the same native source |
| Native Mac signing/notarization | ClawBrowser distribution | BLOCKED | Corporate signing access and final distribution validation required; ad-hoc build integrity alone is insufficient |
| Native package startup and dependencies | Every intended native target | PENDING | Run the actual extracted/installed candidate on each target OS and architecture |
| Native clean installation and upgrade | Every intended native target | PENDING | Include copied existing profiles, identity/proxy preservation, restart, and actual update flow |
| Native proxy outage/no direct fallback | New native candidate | PENDING | Independent exact-artifact consumers prepared; old-browser canary is not a pass for this source |
| Native local-proxy geography | New native candidate | PENDING | Matching, timezone alias, fallback, authenticated proxy, mismatch, provider outage, and missing geography cases prepared |
| Linux Docker and portable runtime | x64, ARM64 distribution paths | PENDING | Build from successful exact native artifacts, then execute default entrypoint and shipped portable wrapper/loader |
| Full CLI tests and vet | Linux release source | PASS | Recorded in the separate CLI release evidence; cross-built archives do not replace Windows execution |
| Full Windows nbc suite | Current candidate source | PENDING | Clean Windows VM run started; historical baseline failures are retained until analyzed |
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
- No stable tag, release upload, registry push, or production deployment is
  implied by the PASS rows. Update this record with final hashes and execution
  evidence before making a release decision.

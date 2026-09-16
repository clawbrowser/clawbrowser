# Windows font isolation: implementation gap, not just an unrun test

Audit base: PR31 `f7c3cc4`, Chromium 151.0.7922.109 at
`28a7a6c409e03c701d3474ef9e3b1f0be6249039`.

The Linux/macOS closed font catalog must not be described as implemented on
Windows. This is a source-level finding; no current Windows executable has
been tested here and no claim of a reproduced Windows host-font leak is made.

## Evidence

- `clawbrowser/BUILD.gn`: catalog generation and the core catalog dependency
  are guarded by `is_linux || is_mac`. The catalog-backed typeface adapter is
  included only for `is_mac`.
- `clawbrowser/startup.cc`: catalog capability and cached-profile font-list
  reconciliation are guarded by `IS_LINUX || IS_MAC`; Windows resets the
  catalog capability.
- Patch 035 guards explicit local-name probing and fallback-list retries.
  It is not a replacement for the platform character-fallback implementation.
- Patch 039 replaces macOS family lookup, character fallback and last-resort
  fallback. It does not modify the Windows implementations.
- The pinned Windows `font_cache_skia_win.cc` still uses
  `GetFallbackFamilyNameFromHardcodedChoices()` and
  `GetDWriteFallbackFamily()`, which calls the default system font manager's
  `matchFamilyStyleCharacter()`. `CreateFontPlatformData()` similarly retains
  native lookup. Shared Skia last-resort fallback is a separate path to audit.
- `scripts/build_windows_prod_clawbrowser.ps1` does not stage
  `clawbrowser-fonts`. The setup archive packages `mini_installer.exe`, so
  adding a folder to the portable ZIP alone would not fix installed builds.
- Catalog provenance/dynamic-font tests currently skip Windows explicitly.
  The Windows raster portability CI job tests numerical code, not this path.

## Required implementation and acceptance

1. Extract the platform-neutral catalog family/character matching from the
   macOS adapter without regressing its alias/style and collection behavior.
2. Add a validated Windows catalog resource path with renderer sandbox-safe
   access. Missing/corrupt assets must fail closed for managed profiles.
   Do not globally register fonts or disable the sandbox to make tests pass.
3. Connect Windows family lookup, character fallback and last-resort fallback
   to that catalog in managed mode. Preserve downloaded web fonts and native
   behavior when protection is intentionally not enabled.
4. Generate the same pinned assets for Windows and include them in both the
   portable package and actual mini-installer payload. Validate installed
   paths and manifest/font hashes, not just build-output files.
5. Enable applicable Windows integration tests only after the implementation
   exists. Add native-positive controls, two genuinely different host font
   inventories, explicit-local-name probes, multilingual fallback, DOM/Worker
   add/delete/clear and fresh/retained canvas tests.
6. Run the installed Windows ClawBrowser with its sandbox enabled and compare
   protected observations to Linux/macOS. Skips and source checks cannot
   satisfy this acceptance gate.

No Windows privacy-completion or all-platform merge recommendation is justified
until this gap is implemented and the actual binary acceptance passes.

## Shared-policy preparation

The macOS adapter now delegates family/character selection to platform-neutral
`MatchManagedCatalogFamily` / `MatchManagedCatalogCharacter`. The filesystem
loader remains platform-specific. Alias/style mappings and fallback order were
moved without changes; Windows support is not enabled by this refactor.

The real Chromium `clawbrowser_font_typeface_unittests` target rebuilt on the
Linux QA host (3 build steps, 9.35s). All three tests passed, including checked
catalog assets, generic mappings, exact full/PostScript aliases, named-style
precedence, emoji priority, invalid code points and an empty-catalog negative
control. Report: `shared-font-policy-054.xml`; the pinned-catalog case took
101ms. This does not replace recompiling/testing the macOS adapter or a Windows
binary. The last accepted browser artifact remains worker-font-053.

## Preload and byte-integrity preparation

`LoadCatalogTypefaces` validates and decodes a single owned snapshot. A new
real-font unit test deletes its own temporary catalog after loading and proves
the returned face can still resolve a glyph and expose its owned stream. Wrong
manifest hashes are rejected. The four C++ tests passed on Linux (102ms for
the full pinned catalog), report `preloaded-font-bytes-055.xml`.

A Windows adapter now resolves resources relative to the loaded module, not
the launcher, and has explicit pre-sandbox initialization with no lazy disk
lookup or system-font fallback. GN can generate the pinned Windows catalog.
**This adapter is not yet hooked into renderer startup or Windows FontCache,
and has not been compiled on Windows.** Installer staging is still pending.
The pinned RendererMain loads the fingerprint before `InitializeSkia()`;
Windows sandbox engagement occurs later. Integration must preload after Skia
initialization and before sandbox engagement, only for managed font mode.

The audit also found a platform-specific staging defect: Windows text-mode
newlines could make the stored manifest bytes differ from the hashed canonical
bytes. Manifest/config/marker/header output now uses exact UTF-8 byte writes;
reuse validation compares raw manifest and marker bytes. Two new regressions
fail on the previous implementation (simulated Windows newline translation
and a CRLF-corrupted reused manifest). All 9 build-catalog and 7 staging tests
pass locally. The Windows CI job now runs the build-catalog regressions on a
real Windows runner; this is staging evidence, not browser acceptance.

## Renderer and fallback wiring

Patch 053 connects managed Windows renderer initialization after Skia setup,
before sandbox engagement, and terminates initialization if catalog loading
fails. Family lookup, character fallback and last-resort fallback now have
catalog-only managed branches; native branches remain for unprotected mode.
The catalog hint and cached-profile normalization also include Windows.
The typeface target is a component with exported entry points so renderer
startup and Blink share the same preloaded state in component builds.

`git apply --check` passed on the pinned Chromium tree and the patch was applied
to the QA checkout. Linux rebuilt the affected renderer/Skia/core units and
the four typeface unit tests passed (80ms for the full catalog), report
`windows-component-policy-056.xml`. The browser link/regression is pending;
this does **not** compile or exercise Windows-only branches.

The preceding staging change passed all **9 tests on Windows CI**, run
`35081750192`, job `104747142355` (0.156s). Installer/portable packaging and
actual Windows acceptance remain required before shipping this wiring.

## Packaging integration

`windows_package.py` generates exact (non-wildcard) catalog entries in the
mini-installer's GENERAL section, all beneath `VersionDir` alongside the
loaded browser module. The Windows build script invokes this before building
the installer, validates catalog inputs before installer preparation, and
copies validated bytes into the portable package before archiving it.
Conflicting existing bytes are not overwritten. Missing/extra fonts, wrong
hashes/notices/manifest/marker, unsafe names and case-insensitive duplicate
font filenames are rejected.

Seven new packaging tests pass locally and are added to both Linux and Windows
CI. The helper also validated/copied all real QA catalog assets and generated
33 entries against a copy of the pinned Chromium `chrome.release`. This proves
the helper's real-input path, not execution of a Windows mini-installer or
verification of its embedded payload. In particular, staging an old existing
`mini_installer.exe` still requires independent installed-payload verification.

The Linux compatibility build completed (55 steps, 270.69s) and a fresh archive
was staged/extracted as `windows-wiring-056`. ELF SHA-256:
`155b4ad2c83d2a11337d3c21debba9604a7ea0fc75c4f05b632faf5b7d726c0a`.
Archive SHA-256:
`8ef2849aa98c5e3188573cb40b336fdeac3534a9724b483f99ec8e8ef2c0f285`.
The full sandboxed/headful Linux regression is running separately; no pass is
claimed yet. Windows compilation and installed-browser acceptance remain open.

The 056 full Linux regression completed: **140 passed, 21 skipped in 395.10s**,
with the same two diagnostic-module exclusions. All 714 comparable protected
observations match 053, and 614 match the saved Mac references. These references
are not a current macOS rebuild. Report: `full-suite-056.xml`.

Windows CI run `35085855856`, job `104760376172`, passed all 9 catalog-build
and 7 packaging tests (0.286s / 0.152s). This still does not execute an installer.

Production `-StageExistingArtifacts` is now explicitly rejected: adjacent
validated assets cannot certify the embedded payload of an old
`mini_installer.exe`. Builders must omit that option and run the installer
build step. Development portable reuse is unaffected. This conservative guard
can be relaxed only after an embedded-payload verifier is implemented; it is
not a claim that the verifier already exists. A Windows PowerShell regression
parses the complete build script and exercises the isolated guard without
running build/deploy entry points.

That guard passed on actual Windows CI (run `35087915478`, job
`104767041657`); all five checks were green. A follow-up source audit also
found that exact bundled full/PostScript aliases were still enabled only on
Linux/macOS. The allowlist now recognizes those same pinned aliases on Windows,
and the previously Linux-only unit regression is enabled on all three desktop
platforms. Eight font-policy tests passed in the Linux C++ target (5ms), report
`windows-alias-policy-057.xml`. Windows execution of that C++ test remains open;
this conditional change does not alter the accepted Linux 056 behavior.

## Windows font E2E entry point

Dynamic webfont, complex fallback, profile migration/local-name and glyph
coverage tests now include Windows. Test font data must come from the chosen
browser installation and be adjacent to its `chrome.dll`. Multiple installed
versions are rejected unless `CLAWBROWSER_TEST_RESOURCE_DIR` identifies the
tested version directory. The resolver has six unit tests, also wired into
Windows CI; loose font files or another installation cannot satisfy it.

On a Windows builder, after a **fresh** build/install, from this repository:

```powershell
$env:CLAWBROWSER_BINARY = 'C:\path\to\Clawbrowser\clawbrowser.exe'
$env:CLAWBROWSER_TEST_HEADFUL = '1'
# Only if multiple installed versions exist:
# $env:CLAWBROWSER_TEST_RESOURCE_DIR = 'C:\path\to\Clawbrowser\151.0.7922.109'
python -m pip install -r clawbrowser/test/integration/requirements.txt
python -m pytest -q -o junit_family=xunit1 --junitxml=windows-font-gate.xml `
  clawbrowser/test/integration/test_dynamic_font_fallback.py `
  clawbrowser/test/integration/test_complex_font_fallback.py `
  clawbrowser/test/integration/test_font_glyph_coverage.py `
  clawbrowser/test/integration/test_system_font_policy.py `
  clawbrowser/test/integration/test_local_collection_variations.py `
  clawbrowser/test/integration/test_font_descriptor_native.py `
  clawbrowser/test/integration/test_font_catalog_isolation.py::test_legacy_font_list_is_reconciled_before_child_launch `
  clawbrowser/test/integration/test_font_catalog_isolation.py::test_bundled_local_faces_resolve_full_and_postscript_names
if ($LASTEXITCODE -ne 0) { throw 'Windows font gate failed' }
python -c "import xml.etree.ElementTree as E; c=list(E.parse('windows-font-gate.xml').getroot().iter('testcase')); assert len(c)==41; assert all(not any(x.find(t) is not None for t in ('failure','error','skipped')) for x in c)"
if ($LASTEXITCODE -ne 0) { throw 'Missing, skipped or failed Windows font evidence' }
```

Do not add `--no-sandbox`. Run against portable **and installed** builds, record
binary/module/catalog hashes, and keep separate XML reports. This selected
41-test gate does not replace real Windows host-font-inventory differential
testing, full rendering/network regression, or desktop lifecycle acceptance.

Linux compatibility of the changed harness passed 10 tests in 14.39s (including
its Linux-only Fontconfig differential), plus the updated hash-checked glyph
coverage test in 0.75s, against 056. Reports:
`windows-enabled-font-harness-056.xml`, `windows-enabled-glyph-harness-056.xml`.
No Windows E2E execution is claimed.

## Windows strike normalization

The source audit found a second Windows-only gap: patch 042 modifies the
non-Windows `FontPlatformData::CreateSkFont`, whereas Windows has its own
implementation. Even with identical catalog bytes, the latter inherited host
ClearType/antialiasing choices, did not force linear metrics, and could retain
embedded bitmap strikes.

Patch 054 now applies the managed Linux Fontations strike settings only when
`ShouldFilterLocalFonts` is true. It preserves font identity, size, synthetic
style and transforms; `geometricPrecision` selects no hinting, otherwise normal
hinting. Native profiles retain their previous code path. The explicit Chromium
web-test subpixel-positioning override still runs after normalization.

`ApplyManagedFontRendering` is covered by two actual Skia C++ unit tests:
384 combinations of incoming strike preferences and a style-preservation test.
Both passed on the Linux QA build in 4ms after compiling the new test target
(15.94s). The first compilation caught a missing explicit `SkFontTypes.h`
include; it was corrected before the successful run. Report:
`windows-strike-policy-058.xml` (a unit-test report, **not a new browser build**).
The source-wiring contract also checks that the Windows helper is guarded and
matches the Linux settings. Patch application was checked against pinned
Chromium 151.0.7922.109 and then applied to the QA source tree.

No Windows binary was compiled or executed for this result. Native Windows
acceptance must still compare protected metrics/pixels with different host
smoothing preferences, exercise `geometricPrecision`, and run the installed and
portable gates above. The accepted Linux browser remains 056: this new runtime
call site is Windows-only and does not change Linux behavior.

## Embedded installer catalog gate

`Stage-WindowsSetupArchive` now validates the **copied `setup.exe`** before
archiving it. `windows_installer_payload.py` maps the PE with
`LOAD_LIBRARY_AS_DATAFILE` (no installer execution), reads the pinned full
`B7 / CHROME.PACKED.7Z` resource and checks its nested `chrome.7z` archive.
Only the full installer format is supported; missing/differential payloads
fail closed. The validator checks one versioned catalog adjacent to `chrome.dll`,
the installed launcher, exact catalog file inventory, sizes and all bytes
against the separately validated pinned font payload. Archive members are
streamed to verifier-owned numbered temporary files, never extracted to paths
provided by an archive. A successful report identifies the resource hash,
version, catalog and verified file count; it is not runtime acceptance.

The old-installer reuse prohibition remains in place. A new AST guard requires
the copied PE verification to run before ZIP creation. Eight regression tests
cover layout, missing/extra/corrupt assets, unsafe/duplicate paths, links and
encryption, actual nested 7z archives, and Windows PE resource access. On Linux
QA, six passed in 0.083s and the two Windows API cases were explicitly skipped.
Windows CI requires 7z and runs both PE cases against a test-owned copy of the
Python executable with a synthetic embedded resource; that copy is never
executed. A real ClawBrowser installer has **not** yet been produced or checked.

## System font call-chain audit

The remaining direct native fallback calls were traced, not treated as leaks
merely because they appear in source. Windows `GetDWriteFallbackFamily` and
`GetFallbackFamilyNameFromHardcodedChoices` are reached after the managed return
in `PlatformFallbackFontForCharacter`. The macOS CoreText fallback and native
family creation paths are likewise behind the managed catalog returns.
`CrashWithFontInfo::countFamilies` records crash diagnostics, not a successful
page-visible lookup; the unique-name availability path goes through the guarded
`GetFontPlatformData` call.

There was, however, an earlier Windows path: `GetFontPlatformData(system-ui)`
resolves `FontCache::SystemFontFamily()` **before** invoking the catalog matcher.
It previously returned the host menu font, often not in the pinned catalog.
Also, CSS `menu`, `small-caption` and `status-bar` obtained host font names and
heights from `LayoutThemeFontProvider` before any typeface lookup. Filtering
the eventual glyph lookup does not hide those computed CSS values.

Patch 055 routes managed `system-ui` directly to Arimo. Managed CSS system-font
keywords use the existing default GUI family and default-provider font size,
while native profiles retain the Windows preferences. The control-specific
size rules remain unchanged. This is a source-confirmed gap and a source fix;
it is **not** a Windows binary reproduction or Windows acceptance pass.

The two new desktop integration tests passed on sandboxed/headful Linux 056
in 2.33s (`system-font-policy-056-r2.xml`): system-ui metrics equal Arimo, with
Tinos as a differing negative control, and all six CSS system font keywords
resolve to the default provider's Arial/16px. An initial test incorrectly
assumed CSS Arial itself must equal Arimo; the managed allowlist does not make
that guarantee, so that unrelated assumption was removed. The system-ui/Arimo
and negative-control assertions remain strict. No Linux runtime change or
new Linux browser build was needed for this Windows-only patch.

The system-font additions brought the Windows acceptance selection to 12 tests
(the TTC and descriptor regressions below bring it to 41). It must be run on
a fresh Windows artifact and repeated with changed host menu font preferences
to prove independence; Linux execution alone cannot establish that result.

## Shared local TTC variation fix (patch 056)

`LocalFontFaceSource` cloned local fonts without passing their collection index.
For a nonzero TTC member, Fontations returned the original face and silently
ignored the requested variation axes. This was reproduced on Linux artifact
056: `local("Noto Sans CJK SC")` gave identical metrics at weights 100 and 900,
while the direct family control changed. The before report has one failing
test; it is not a passing baseline.

Patch 056 obtains the index from the already-loaded typeface stream only in
managed mode and passes it to the clone. It does not enumerate host fonts or
change the native-profile path. Linux artifact `local-collection-060` passes
all five JP/SC/KR/TC/HK variants (6.28s), then the full headful, sandboxed,
non-root suite: **147 passed, 21 skipped in 398.57s**. Reports are
`local-collection-before-056.xml`, `local-collection-after-060.xml`, and
`full-suite-060.xml`. Skips are not acceptance passes.

ELF SHA-256: `cceb32d337ff467dc28dec32a60cc68438ff409da52bde93b8eb92fc7d38cc9a`.
Archive SHA-256: `99eb28e788c3dc36e965d7ab5cfaf8251dada6568c4e6586cd260b47e5629395`.
This shared-code fix still needs the fresh macOS and Windows artifact gates;
the five tests are included in the Windows command above. It is not evidence
that PixelScan is green or that current builds pass on all platforms.

## Worker descriptor mutation (patch 057)

The extended TTC test changes a loaded face's `variationSettings` from weight
100 to 900 and back, comparing persistent/fresh canvas metrics against a newly
created control face. Document cases additionally compare DOM range widths.
Artifact 060 passed all document cases, but all five Worker cases retained
stale metrics in both persistent and newly created OffscreenCanvas contexts.
The control face changed, so this was not a missing-font fallback.

`FontFace::InvalidateFontFaceOnDescriptorUpdate` returned early without a
Document. Patch 057 resolves the worker's existing font selector instead and
uses the same cache removal/addition and invalidation path. No host-font lookup,
site exception, or privacy toggle is introduced. This corrects general worker
descriptor handling, including native profiles; it is not limited to TTC.

On extracted Linux artifact `worker-descriptor-061`, all **15 targeted tests
pass in 17.91s** with sandbox, display and non-root execution. Before:
`local-collection-worker-mutation-060.xml` (5 failed, 10 passed). After:
`local-collection-worker-mutation-061.xml` (15 passed). The full 061 regression
passed **157 tests with 21 skips in 407.82s**; all 714 comparable protected
rendering observations match 060. Fresh macOS/Windows browser acceptance remains
outstanding; skips do not count as passes.

ELF SHA-256: `309cba11707793eede6aae0e6849c9c94d70eebcee9ddc80293465a6510dd9ac`.
Archive SHA-256: `c5e5f17b4d65a12debc3bffc7062df54886d63ab3dfc8b84c4c1c79ac109e20f`.

## Loaded webfont size-adjust mutation (patch 058)

`setSizeAdjust` did not request descriptor invalidation at all. With verified
bundled Tinos bytes loaded as a webfont, changing 50% to 150% retained the old
width in document and worker canvases. Both managed and native **font policies**
failed; these are fingerprinted test profiles, not wholly unmanaged browsers.
An initial attempt without a fingerprint failed the mock startup contract and
is not evidence of a native rendering failure.

Patch 058 follows the existing variation setter: invalidate only after the
parsed value changes. On extracted, sandboxed/headful/non-root Linux artifact
`size-adjust-062`, all four cases pass in 5.11s, including restoration, unchanged
value, invalid input raising SyntaxError without changing metrics, and warm/
fresh canvas agreement. Reports: `native-font-policy-descriptor-061.xml` (four
actual failures) and `native-font-policy-descriptor-062.xml` (four passes).
The full 062 regression passed **161 tests with 21 skips in 416.46s**; all 714
comparable protected rendering observations match 061. All five CI checks passed.

ELF SHA-256: `95e6c99a69c615248e1360f26a2c66dcc765b39ea9958ec98eebc52d38abe33c`.
Archive SHA-256: `9f940f7b785b9f25530162e96f966975d5c04575efe12c8d66b08b76859ce100`.

## Vertical metric descriptor mutation (patch 059)

The adjacent ascent, descent and line-gap setters also lacked invalidation.
The expanded test measures canvas font bounding boxes and a two-line DOM block,
with fresh-face controls, unchanged/invalid inputs and restoration. Ascent and
descent are tested in document and worker contexts; line gap is tested in DOM
only because canvas TextMetrics do not expose line gap. Both native-font and
managed policies are covered.

On 062, ten new cases failed while all four size-adjust cases still passed
(`vertical-descriptor-062.xml`, 17.92s). Patch 059 applies change-sensitive
invalidation to the three setters. On extracted, sandboxed/headful/non-root
artifact `metric-overrides-063`, all **14 cases pass in 18.28s**
(`vertical-descriptor-063.xml`). Full 063 regression is pending at this commit.
This is rendering consistency evidence, not a PixelScan or all-platform pass.

ELF SHA-256: `7420c644d6388e39cc473ec615ca9f4ed664a9fc37ac374ddb91188a11816982`.
Archive SHA-256: `1562c7a5c3220d6eb7638f55c13e9f9c69eafd90bb1715b4054004100520d646`.

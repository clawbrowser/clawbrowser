# Chromium Base Update Guide

Clawbrowser is a Chromium fork. A **Chromium base update** (also "browser core
update", or just "core update") moves the fork onto a newer Chromium release.

This guide is written for someone who has never done one and knows nothing about
how the fork is layered on Chromium. It does assume you can already build the
browser.

Nothing here is automated. A human picks the version, resolves every patch
conflict, fixes the build breaks, runs the tests, and decides whether to ship.
**Plan for a full day at minimum, and be ready for two or three.** A cold build
after a milestone jump runs for hours on its own, and conflicts need real
reading, not pattern-matching.

Throughout, the worked example updates the fork to **151.0.7922.109**, whose
upstream commit is `28a7a6c409e03c701d3474ef9e3b1f0be6249039`. Substitute your
own target and revision everywhere those appear.

## How the fork is layered

Clawbrowser does *not* keep a forked Chromium tree. This repo holds three things
that get combined into a Chromium checkout at build time:

| Layer | Lives in | What it is |
| --- | --- | --- |
| **Pin** | `scripts/chromium_pin.conf`, `scripts/clawbrowser_pin.conf` | One exact upstream Chromium revision the fork is built against |
| **Patches** | `clawbrowser/patches/*.patch` | 31 unified diffs applied on top of that revision |
| **Overlay** | `clawbrowser/`, `api/openapi.yaml`, `branding/` | Whole files copied into `chromium/src`, not patched |

The build does, in order: sync `chromium/src` to the pinned revision → copy the
overlay in → apply all patches → `gn gen` → `autoninja`.

That model has one consequence worth internalising before you start: **a patch
is only ever validated against one exact Chromium revision.** Move the revision
and every patch's context lines may drift. Most drift is harmless (the code
moved down 12 lines). Some is not (the function was deleted and split in two).
Telling those apart is the actual work of a base update.

One patch does not target `chromium/src` at all:
`033-devtools-no-getter-preview.patch` targets `v8/src/inspector/value-mirror.cc`,
and `src/v8` is a separate DEPS repository. It applies from the `src` root like
the others, so applying it needs nothing special — but it is easy to miss when
you check "did everything apply?".

## Read these once, before your first base update

| Doc | What it gives you |
| --- | --- |
| [`docs/chromium-remote-build.md`](chromium-remote-build.md) | The official Chromium flow, `depot_tools`, pinning, ccache |
| [`docs/clawbrowser-remote-build.md`](clawbrowser-remote-build.md) | The same for the Clawbrowser variant, plus branding sync |
| [`docs/windows-agent-install.md`](windows-agent-install.md) | Windows install/runtime specifics |
| `scripts/chromium_remote.sh --help` | Every stage the pipeline runs, as a command |

---

## 0. Before you start

You need all of the following:

- Read access to this repo and a working `depot_tools` on `PATH`.
- A Chromium checkout you are allowed to destroy and re-sync
  (`gclient sync --force --reset` throws away local state on purpose).
- **~150 GB free disk** and several hours of uninterrupted build time. The
  checkout alone is ~100 GB; `out/CBProd` adds tens of GB more.
- A working `sccache` (the build scripts expect it and will start the server).
- An API key in `%LOCALAPPDATA%\Clawbrowser\config.json` if you intend to run
  the `clawctl` acceptance in step 9.

### 0.1 Do not resolve conflicts in the live checkout

Resolving a patch conflict is pure source merging. You do not need a synced,
buildable tree to do it — you need the *pristine* upstream file at the new
revision and the patch. Work in a scratch directory:

```bash
mkdir -p /tmp/rebase/006-screen-metrics
cd /c/src/chromium/src
git show 151.0.7922.109:third_party/blink/renderer/core/frame/screen.cc \
  > /tmp/rebase/006-screen-metrics/screen.cc
```

Reasons to keep it out of the live checkout:

- You will iterate. A scratch tree lets you throw the whole thing away.
- The live checkout is what you will later verify against, and you want that
  verification to be independent of your editing.
- `git apply` in the live tree will happily half-apply a patch and leave you
  reconciling a mess.

Step 5.1 gives a loop that does this extraction for all 31 patches at once.

### 0.2 Seed a profile for acceptance

Step 9 checks that a *used* profile survives the upgrade, so create the seed
deliberately, on the **currently released** build, before you replace anything:

```bash
clawctl start --profile seed --country DE --verify --json
```

Browse a little, leave tabs open, then stop the profile. You want real history
and a real on-disk profile to migrate, not an empty one.

---

## 1. Pick the target Chromium version

Take the latest **Stable Channel Update for Desktop** post from the
[Chrome releases blog](https://chromereleases.googleblog.com/search/label/Stable%20updates).

Reading the post: a sentence like "updated to 151.0.7922.75/.76 for Windows and
Mac" means the target is **151.0.7922.76** — the `.75/.76` split is Google's
staged rollout, so take the highest. (That is only an illustration of how to read
the blog; the worked example in the rest of this guide uses 151.0.7922.109.)

### 1.1 Get the revision SHA

Confirm the tag exists upstream, and get the commit it points at:

```bash
git ls-remote --tags https://chromium.googlesource.com/chromium/src \
  refs/tags/151.0.7922.109
```

```
28a7a6c409e03c701d3474ef9e3b1f0be6249039	refs/tags/151.0.7922.109
```

**Column 1 is the revision SHA.** That value is what goes into
`CHROMIUM_REVISION` in step 3, and into every `--revision src@...` from here on.
No output means the tag does not exist — check the version before going further.

### 1.2 Compare against where the fork sits today

The pin file carries two fields, and the label can be in either of two formats:

```bash
cat scripts/chromium_pin.conf
```

```
CHROMIUM_VERSION_LABEL="main@{#1609092}"          # a main-branch position
CHROMIUM_REVISION="25a94a5662bc9cce918f4626472e7edca1ba2b39"
```

- `main@{#1609092}` — a **main-branch commit position**. The fork was pinned to
  a snapshot of `main`, not to a release.
- `151.0.7922.109` — a **release tag**.

A main-position label can never string-match a release tag, so do not compare the
labels. Compare the versions they resolve to:

```bash
git -C /c/src/chromium/src show \
  25a94a5662bc9cce918f4626472e7edca1ba2b39:chrome/VERSION
```

That prints `MAJOR`/`MINOR`/`BUILD`/`PATCH` for the currently pinned revision —
compare that with your target. Stop if they are the same.

Part of doing this update is **normalising the pin to a release tag**: after
step 3 the label reads `151.0.7922.109`, and future updates can compare labels
directly.

---

## 2. Check the fingerprint service supports the target — do this first

**This is a hard blocker and it is invisible to every test in the repo.** Check
it before you spend a day rebasing patches.

The browser sends its real runtime version to the backend on every fingerprint
request (`ApplyRuntimeRequestHints` in `clawbrowser/startup.cc`). If the service
has no header profiles for that Chromium version, it returns **HTTP 500
`generation_failed`** and the browser **silently falls back to vanilla mode** —
no fingerprint, no spoofing, plain Chromium. It still starts. `clawctl` still
reports `ok`. The only visible signal is one warning line and this sentence on
`clawbrowser://verify/`:

> No expected values — not running in fingerprint mode

Check it directly, with your own API key:

```bash
curl -s -w '\n%{http_code}\n' -X POST \
  https://api.clawbrowser.ai/v1/fingerprints/generate \
  -H "Authorization: Bearer $CLAWBROWSER_API_KEY" \
  -H 'Content-Type: application/json' \
  -d '{"platform":"windows","browser":"chrome","country":"US","runtime_browser_version":"151.0.7922.109"}'
```

- **200** with a `fingerprint` object → the service supports it, continue.
- **500 `generation_failed`** → stop. The update can be prepared and merged, but
  it cannot ship until the backend adds profiles for that version. Raise it with
  whoever owns the service before doing the rest of the work.

The integration suite cannot catch this, because it runs against
`clawbrowser/test/integration/mock_server.py`, which serves static fixtures and
has no version gate. A completely green test run tells you nothing about whether
the real service accepts your new version.

---

## 3. Update the pins

Set both variables in both pin files. `CHROMIUM_REVISION` is the SHA from
step 1.1; `CHROMIUM_VERSION_LABEL` becomes the release tag:

```bash
sed -i \
  -e 's/^CHROMIUM_VERSION_LABEL=.*/CHROMIUM_VERSION_LABEL="151.0.7922.109"/' \
  -e 's/^CHROMIUM_REVISION=.*/CHROMIUM_REVISION="28a7a6c409e03c701d3474ef9e3b1f0be6249039"/' \
  scripts/chromium_pin.conf scripts/clawbrowser_pin.conf
```

Both build docs quote the pin in prose — the label, the full revision, the short
form, an example `--chromium-revision`, and an example `gclient sync` line. All
five need rewriting in each file:

```bash
sed -i \
  -e 's/^- label: `.*`$/- label: `151.0.7922.109`/' \
  -e 's/25a94a5662bc9cce918f4626472e7edca1ba2b39/28a7a6c409e03c701d3474ef9e3b1f0be6249039/g' \
  -e 's/`25a94a5662bc`/`28a7a6c409e0`/g' \
  docs/chromium-remote-build.md docs/clawbrowser-remote-build.md
```

Then the per-patch headers:

```bash
sed -i 's|^# Pinned against Chromium revision .*|# Pinned against Chromium revision 28a7a6c409e03c701d3474ef9e3b1f0be6249039 (151.0.7922.109)|' \
  clawbrowser/patches/*.patch
```

Verify no stale references survive anywhere:

```bash
grep -rn "25a94a5662bc\|1609092" scripts/ docs/ clawbrowser/patches/ || echo clean
```

Three patches (`027`, `029`, `030`) have never carried the pin header. Leave
them alone rather than adding one.

**The Windows build scripts need no edit.** They resolve the checkout path
dynamically and read `chrome/VERSION` at build time, so they follow the pin
automatically. Do not hardcode a version into them.

---

## 4. Sync Chromium

Release tags are not fetched by default. Fetch the one you need, then sync:

```bash
cd /c/src/chromium/src
git fetch origin +refs/tags/151.0.7922.109:refs/tags/151.0.7922.109
cd /c/src/chromium
gclient sync -D --force --reset \
  --revision "src@28a7a6c409e03c701d3474ef9e3b1f0be6249039"
```

Expect this to take a long time across a milestone jump — it pulls a large DEPS
and toolchain delta and runs every hook.

> **`gclient sync -D` deletes the overlay.** `clawbrowser/`, `api/`, and the
> branding assets are unversioned inside `chromium/src`, so `-D` removes them.
> This is expected: the build scripts re-sync the overlay from this repo on the
> next run. Do not "rescue" those directories — they are copies, and the repo is
> the source of truth.

Verify you landed where you meant to:

```bash
git -C /c/src/chromium/src rev-parse HEAD
cat /c/src/chromium/src/chrome/VERSION
```

---

## 5. Rebase the patches

This is the part that needs judgement. Everything else is mechanical.

### 5.1 Get the conflict inventory

Test-apply every patch against pristine upstream files, in a scratch tree, and
bucket the results. Do **not** touch the live checkout yet.

There is no script in the repo for this; you write it. Its shape:

```bash
#!/usr/bin/env bash
# Bucket every patch against a target revision, without touching the checkout.
TAG=151.0.7922.109
SRC=/c/src/chromium/src
REPO=/path/to/clawbrowser_core
OUT=/tmp/rebase

for p in "$REPO"/clawbrowser/patches/*.patch; do
  name=$(basename "$p")
  work="$OUT/${name%.patch}"
  rm -rf "$work"; mkdir -p "$work"

  # Extract each file the patch touches, at the target revision.
  for f in $(grep '^--- a/' "$p" | sed 's|^--- a/||'); do
    if git -C "$SRC" cat-file -e "$TAG:$f" 2>/dev/null; then
      mkdir -p "$work/$(dirname "$f")"
      git -C "$SRC" show "$TAG:$f" > "$work/$f"
    else
      echo "  $name: target absent upstream: $f"
    fi
  done

  # fuzz=0 on purpose: see 5.2.
  if (cd "$work" && patch -p1 --dry-run --forward --fuzz=0 -i "$p" \
        >"$work/.log" 2>&1); then
    if grep -q offset "$work/.log"; then echo "OFFSET   $name"
    else echo "CLEAN    $name"; fi
  else
    echo "CONFLICT $name"
  fi
done
```

Three buckets come out:

- **clean** — applies exactly.
- **offset** — applies, but the code moved. Harmless; the regenerated patch will
  carry the new line numbers.
- **conflict** — context no longer matches. Needs a human.

Run against the 148-era patch set with a 151 checkout, that loop buckets them
**3 clean / 15 offset / 13 conflicts**. Expect roughly a third of the patch set
to need real attention on a multi-milestone jump.

Bucketing each patch in isolation, as above, is the right way to plan the work.
It can differ by a patch or two from applying them in sequence into one tree,
where later patches see earlier patches' changes — so treat the conflict count
as a planning figure, not an invariant.

Files reported as "target absent upstream" are the important edge case: upstream
deleted or moved the file, which is always a semantic conflict even though no
hunk failed.

### 5.2 Never let the patch tool guess

Apply with **`--fuzz=0`**:

```bash
patch -p1 --forward --fuzz=0 --no-backup-if-mismatch -i "$PATCH"
```

Line-number *offsets* are fine — the code moved, the context still matches.
**Fuzz is not fine.** Fuzz means "ignore some context lines and place the hunk
anyway". It does not fail. It reports success and writes the change to the wrong
place.

A real example from the 148 → 151 update. Patch `006` inserts an early return
into `LocalDOMWindow::devicePixelRatio()`. Upstream had only added braces to an
`if`. At `--fuzz=3` the tool reported success — and put the insertion in a
completely different function, splitting a multi-line expression in
`LocalDOMWindow::screenY()`:

```cpp
    return static_cast<int>(
        lroundf(chrome_client.RootWindowRect(*frame).y() *
  if (const auto* fp = clawbrowser::FingerprintAccessor::Get())
    return fp->screen.pixel_ratio;
                chrome_client.GetScreenInfo(*frame).device_scale_factor));
```

That does not compile, and even if it had compiled, it would have been wrong.
Every hunk in that run was reported as "succeeded". **Fuzzy application is not a
time-saver on a security-relevant patch set — it is a way to introduce defects
that look like successful output.** Use `--fuzz=0` and resolve the `.rej` files
by hand.

### 5.3 Resolving a conflict

For each rejected hunk:

1. Read the `.rej` to see what the patch intends.
2. Read the *current* upstream code around the target.
3. Ask what upstream changed and why. A rename or a reformat is mechanical. A
   function being deleted and split in two is a semantic decision.
4. Apply the intent by hand to the pristine file.

The 11 conflicts fell into three kinds. One example of each:

**Mechanical.** `017` failed only because upstream fixed the indentation of a
`DCHECK` the patch was normalising. Re-anchor and move on.

**Structural.** `021`'s immersive-fullscreen check moved from `view->` to
`WindowFeatureController::From(view->browser())->`. Same intent, new call path.

**Semantic — stop and think.** `019` was the dangerous one. Upstream deleted
`ValidateUrl()` and split it into two functions in a new file:

- `ValidateLaunchUrlWebUnsafe()` — command line and OS intents
- `ValidateLaunchUrlWebSafe()` — redirects initiated by untrusted web content

The fork's `clawbrowser://` allowance had lived in the single old function. Put
it in the wrong half and web pages can navigate into `clawbrowser://auth`. The
correct answer was **WebUnsafe only**, confirmed by reading
`chrome/browser/ui/startup/startup_tab_provider.cc`, which routes a plain
command-line argument to WebUnsafe and only `google-chrome://`-stripped URLs to
WebSafe.

When a conflict is a security boundary, the resolution is not "make it apply".
It is "work out which side of the new boundary this belongs on, and write a test
that pins the answer".

### 5.4 Regenerate the patch

Once the pristine file carries the change, regenerate the diff rather than
hand-editing hunk headers:

```bash
cd /tmp/rebase/019-verify-page-registration
git init -q . && git add -A
git -c user.email=x@x -c user.name=x commit -qm base
# ... apply your resolution to the working files ...
git add -A
git diff --cached -U3 | grep -vE '^(index |new file mode|deleted file mode)' \
  > /path/to/repo/clawbrowser/patches/019-verify-page-registration.patch
```

Two things to preserve:

- The `# Pinned against Chromium revision ...` header line, if the patch had one.
- The `diff --git` lines. `scripts/clawbrowser_startup_patch_test.sh` asserts on
  them, so stripping them breaks a contract test.

If you *do* hand-edit a hunk body, the `@@` header line counts are now wrong and
so are the start lines of every later hunk in that file. Recompute them; do not
eyeball them.

### 5.5 Verify nothing was silently dropped

A regenerated patch that lost a target file will still apply cleanly. Compare
file counts against the committed version:

```bash
for p in clawbrowser/patches/*.patch; do
  old=$(git show "HEAD:$p" | grep -c '^--- a/')
  new=$(grep -c '^--- a/' "$p")
  [ "$old" = "$new" ] || echo "MISMATCH $p: $old -> $new"
done
```

The count is a tripwire, not a verdict: investigate every mismatch, and note that
`019` changed which files it targets — the `ValidateUrl` split moved two hunks to
a new file — without changing the count.

---

## 6. Apply, build, and verify

The Windows script does the whole sequence — overlay sync, branding, resource
IDs, patches, `gn gen`, build, staging:

```bash
powershell -NoProfile -ExecutionPolicy Bypass \
  -File scripts/build_windows_prod_clawbrowser.ps1 \
  -ChromiumSrc "C:\src\chromium\src" -DepotTools "C:\src\depot_tools" \
  -BuildProfile Prod -SkipBuild
```

Run it with **`-SkipBuild` first.** That applies every patch and runs `gn gen` in
about a minute, which catches bad `BUILD.gn` edits immediately instead of three
hours into a compile. A healthy run ends with something like
`Done. Made 31650 targets from 4850 files`.

Then confirm all 31 patches are actually in the tree. If a patch reverse-applies
cleanly, it is present:

```bash
cd /c/src/chromium/src
for p in /path/to/repo/clawbrowser/patches/*.patch; do
  git apply --reverse --check -p1 "$p" 2>/dev/null \
    || echo "NOT APPLIED: $(basename $p)"
done
```

This loop **does** cover `033`, the V8 patch: its paths are `v8/...` relative to
the `src` root, so running from `src` reaches into the `src/v8` sub-repository
correctly. A clean run really does mean 31 of 31. (The reason `033` still earns a
warning is that it is easy to forget when you apply patches by hand, or to skip
when a script only iterates files under `chromium/src`.)

Only then drop `-SkipBuild` and let it build.

### 6.1 The MIDL rebaseline

`gclient sync` resets `third_party/win_build_output/` to the pristine upstream
baselines, which then disagree with your locally installed Windows SDK. The build
fails in `chrome/windows_services/elevated_tracing_service` with:

```
midl.exe output different from files in gen/chrome/windows_services/elevated_tracing_service,
see C:\Users\<you>\AppData\Local\Temp\tmpXXXXXXXX
To rebaseline:
  copy /y C:\Users\<you>\AppData\Local\Temp\tmpXXXXXXXX\* ..\..\third_party\win_build_output\midl\chrome\windows_services\elevated_tracing_service\x64
```

There is **no rebaseline script in this repo** — you either follow the printed
instruction or write a loop. Either way, apply one safety rule:

> Accept the copy **only if the sole difference is the binary `.tlb` type
> library.** A `.tlb` differing is an SDK version stamp. Any generated `.h`,
> `.c`, or `_i.c` differing is a semantic change, and copying over it hides a
> real problem.

The loop shape, if you write one: build just the MIDL targets with `-k 0` so they
all report, parse the `copy /y SRC DEST` lines out of the log, compare each file
against its baseline, refuse the target if any non-`.tlb` file differs, copy the
rest, and repeat until the targets build clean. It normally converges in two
passes. Targets involved include
`chrome/windows_services/elevated_tracing_service:tracing_service_idl_idl_action`,
`chrome/elevation_service:elevation_service_idl_idl_action`, and the
`chrome/updater/app/server/win:*_idl_action` family.

This is not a patch problem and will happen on every clean sync.

---

## 7. Tests

Four layers, cheapest first.

**Contract tests** — patch text only, no build needed:

```bash
for t in scripts/*_test.sh; do echo "== $t"; bash "$t"; done
```

**Unit tests** — these are *not* built by the normal build, which only makes
`chrome`:

```bash
cd /c/src/chromium/src
autoninja -C out/CBProd clawbrowser_unittests
./out/CBProd/clawbrowser_unittests.exe
```

**Integration tests** — drive the built browser over CDP against a mock backend,
so no API key is needed:

```bash
cd /c/src/chromium/src
CLAWBROWSER_BINARY=/c/src/chromium/src/out/CBProd/clawbrowser.exe \
CLAWBROWSER_PROJECT_DIR=/path/to/repo \
python -m pytest clawbrowser/test/integration/ -q \
  --deselect clawbrowser/test/integration/test_real_backend.py \
  --deselect clawbrowser/test/integration/test_real_socks5_proxy.py
```

`CLAWBROWSER_BINARY` is mandatory on Windows — the auto-detection list in
`run_integration_tests.py` has no Windows path.

**Acceptance** — see step 9.

### Getting a real baseline instead of guessing

When a test fails, the question is always "did I break this, or was it already
broken?" Do not answer it by reasoning. Answer it by running the same suite
against the previously released build:

```bash
CLAWBROWSER_BINARY=/path/to/previous/clawbrowser.exe \
CLAWBROWSER_PROJECT_DIR=/path/to/repo \
python -m pytest clawbrowser/test/integration/ -q ...
```

Installed releases live under `%LOCALAPPDATA%\Clawbrowser\`. If pytest's `FAILED`
and `ERROR` sets are identical between old and new, you have introduced no
regression, and you can say so with evidence rather than confidence.

**Copying a build breaks its sandbox.** A copied or staged tree loses the
`ALL RESTRICTED APPLICATION PACKAGES` ACL that Chromium's sandbox requires, and
then *everything* fails for an unrelated reason — `Sandbox cannot access
executable ... Access is denied`, and `clawctl start` reports
`LAUNCH_FAILED / no page targets found`. Restore it before drawing any
conclusion:

```powershell
icacls "C:\path\to\build" /grant "*S-1-15-2-2:(OI)(CI)(RX)" /T /C /Q
```

This applies to staged artifacts under `chromium/artifacts/` too, not just to
copies you make by hand.

---

## 8. Fixing what the tests find

Expect two categories, and keep them in separate commits.

**Real breaks from the new Chromium.** 151 enabled `-Wshorten-64-to-32` for
Blink modules, so `ReserveInitialCapacity(fp.media_devices.size())` became a hard
error in two patches. When you hit one of these, grep the whole patch set for the
same pattern immediately rather than fixing one and rediscovering its twin an
hour later.

**Pre-existing breakage you are the first to see.** Most of the fork's tests have
only ever run on macOS, so the first Windows run surfaces years of accumulated
platform assumptions: `base::SetPosixFilePermissions` does not exist on Windows,
`FilePath::value()` is `std::wstring` there, and fixtures that pin
`"platform": "macos"` will not match what the browser actually sends.

These are worth fixing, but they are **not part of the base update**, so the
version bump stays revertable on its own.

Two judgement calls worth copying:

- When a test fails because production code is wrong, fix the code. A test that
  caught `group\profile` instead of `group/profile` on Windows found a real bug
  in profile-id round-tripping; the right fix was in `profile_manager.cc`, not in
  the assertion.
- When a test asserts behaviour that no longer exists, delete the assertion and
  say where the behaviour went. Do not resurrect dead code to make it green.

---

## 9. Acceptance with `clawctl`

Tests pass against a mock. Acceptance is what proves the thing works against the
real service.

```bash
clawctl proxy-traffic --json
clawctl start --profile accept-151 \
  --browser-bin "C:\src\chromium\artifacts\clawbrowser-prod-windows-x64\Clawbrowser\clawbrowser.exe" \
  --verify --json
```

Check `proxy-traffic` first; if `state` is `exhausted`, stop and top up rather
than looping on failures.

Then **open `clawbrowser://verify/` and read it.** This is the check that catches
what nothing else does. If it says:

> No expected values — not running in fingerprint mode

then the browser is running as vanilla Chromium with no fingerprint applied —
almost certainly the service rejecting your new version (step 2). A green test
suite and a successful build do not rule this out.

Also confirm your seeded profile from step 0.2 still opens, restores its session,
and keeps its history and downloads.

---

## 10. Commit

Keep the base update and any test repairs as separate commits. The version bump
should be revertable without dragging unrelated fixes with it.

Before committing, check you have not shipped a diff full of noise:

```bash
git diff --stat main..HEAD
git diff --ignore-cr-at-eol --stat main..HEAD
```

If those two disagree wildly, you have line-ending churn. Scripted whole-file
rewrites on Windows will silently convert an LF file to CRLF, which turns a
twelve-line change into a three-thousand-line diff. Convert the affected files
back before committing:

```bash
sed -i 's/\r$//' path/to/file
```

Note that `grep -c $'\r$'` is not a reliable way to detect this. Check the bytes:

```bash
git cat-file blob $(git rev-parse HEAD:path/to/file) | head -c 80 | od -c
```

Write the commit message for the person who will bisect to it in six months.
Record which patches needed semantic decisions and why, what you verified, and
any known-failing tests with evidence that they pre-date the update.

---

## 11. Traps the pipeline cannot see

Collected in one place, because each of these has cost real time:

| Trap | Symptom | Fix |
| --- | --- | --- |
| Service lacks the new version | Browser starts, everything green, no fingerprint | Check step 2 **before** doing the work |
| Fuzzy patching | Hunks report success, code lands in the wrong function | `--fuzz=0`, resolve `.rej` by hand |
| `gclient sync -D` | `clawbrowser/` and `api/` vanish from `chromium/src` | Expected; build scripts re-sync them |
| MIDL baselines | Build fails in `elevated_tracing_service` | Rebaseline against your SDK (step 6.1) |
| Copied build's ACL | `Sandbox cannot access executable`, `LAUNCH_FAILED` | `icacls ... *S-1-15-2-2:(OI)(CI)(RX) /T` |
| Line-ending churn | Diff is thousands of lines for a small change | `--ignore-cr-at-eol` to detect, `sed -i 's/\r$//'` to fix |
| Unit tests not built | "Tests pass" — but they were never compiled | `autoninja -C out/CBProd clawbrowser_unittests` |
| Mock has no version gate | Integration suite green while the real service 500s | Acceptance in step 9 is the only cover |
| `033` targets V8 | Missed if you apply patches by hand or iterate only `chromium/src` | Reverse-apply check from the `src` root covers it |

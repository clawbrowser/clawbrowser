# Chromium Remote Build Guide

This guide is for building the `clawbrowser` browser-component work inside a real Chromium checkout on a remote macOS machine.

It keeps the setup honest:

- **Official Chromium flow:** `depot_tools`, `fetch`/`gclient`, `gn`, `autoninja`
- **Project-specific overlay:** syncing this repo's `clawbrowser/` directory and `api/openapi.yaml` into `chromium/src`
- **Practical recommendations:** persistent `ccache`, git tuning for large repos, Spotlight exclusion
- **Reproducibility:** checkout and update pin Chromium to one exact repo-defined revision by default

## What Is Official

Chromium's official macOS documentation covers:

- full Xcode plus macOS SDK
- `depot_tools`
- `fetch chromium`
- `gn gen out/...`
- `autoninja -C out/... chrome`
- updating with `gclient sync`

Primary sources:

- [Chromium Mac build instructions](https://chromium.googlesource.com/chromium/src/+/main/docs/mac_build_instructions.md)
- [Chromium ccache on Mac](https://chromium.googlesource.com/chromium/src/+/main/docs/ccache_mac.md)

## What Is Project-Specific

This repo is **not** a standalone Chromium checkout. The code under [clawbrowser/BUILD.gn](/Users/nomionz/dev/clawbrowser/clawbrowser/BUILD.gn#L7) assumes it lives under `chromium/src/` as `//clawbrowser`.

For this project, the minimum overlay needed for build-related work is:

- `clawbrowser/` -> `chromium/src/clawbrowser/`
- `api/openapi.yaml` -> `chromium/src/api/openapi.yaml`

The helper script also patches Chromium's checked-in
`tools/gritsettings/resource_ids.spec` on the remote checkout so the
`clawbrowser/verify/clawbrowser_verify.grd` resource target can reserve unique
resource IDs. This means the Chromium checkout will be locally modified after
`sync-project`/`build` runs.

This repo's patch files are version-pinned unified diffs for the Chromium pin in
`scripts/chromium_pin.conf`. For browser integration work:

- `sync-project` copies the repo-owned `clawbrowser/` overlay into `chromium/src`
- `apply-patches` applies the repo-pinned `clawbrowser/patches/*.patch` set into the Chromium checkout
- `build --target chrome` then compiles the integrated browser from repo state

The standalone fast loop is still useful:

- `//clawbrowser:clawbrowser_unittests` remains the cheapest loop for shim-only work
- browser-level verification is only authoritative after `apply-patches` has been run on the pinned checkout

For standalone `//clawbrowser` targets, the helper script uses Chromium's existing top-level `root_extra_deps` GN argument to load `//clawbrowser` into the build graph without editing checked-in Chromium build files.

Exception: the verify-page GRIT target requires a `resource_ids.spec` entry, so
the helper script patches that file automatically during sync.

For actual browser integration, patch 020 still matters:

- `chrome/BUILD.gn` must reference `//clawbrowser` once the browser starts linking or depending on the shim library

## Repo-Local Helper Script

Use:

```bash
bash scripts/chromium_remote.sh
```

Available workflows:

- `machine-setup`
- `checkout`
- `update`
- `sync-project`
- `apply-patches`
- `gen`
- `build`
- `integration-setup`
- `integration-test`
- `ccache-stats`
- `ccache-size`
- `tune-git`

Run help:

```bash
bash scripts/chromium_remote.sh help
```

The repo also includes a convenience launcher for syncing the current repo over
SSH and starting a remote build under `nohup`:

```bash
bash scripts/launch_remote_chrome_build.sh --help
```

That helper currently has machine-specific defaults in the script itself, so if
you use different remote builders you should override them explicitly with flags
or environment variables instead of relying on the checked-in defaults.

## Default Layout

Defaults are:

- `~/opt/depot_tools`
- `~/work/chromium`
- `~/cache/ccache`
- build dir: `~/work/chromium/src/out/CBFast`
- pin file: `scripts/chromium_pin.conf`

These can be overridden with CLI flags or environment variables.

## Generic SSH Variables

When driving the remote machine from your local workstation, use shell
variables like these rather than hard-coding one specific box:

```bash
REMOTE_HOST="youruser@203.0.113.10"
SSH_KEY="$HOME/.ssh/your_builder_key"

REMOTE_REPO_DIR="/Users/youruser/dev/clawbrowser"
REMOTE_CHROMIUM_DIR="/Users/youruser/work/chromium"
REMOTE_BUILD_DIR="${REMOTE_CHROMIUM_DIR}/src/out/CBFast"
REMOTE_LOG_DIR="/Users/youruser/dev/clawbrowser-build-logs"
```

Replace:

- `youruser` with the SSH username on the remote Mac
- `203.0.113.10` with the remote machine IP or DNS name
- `your_builder_key` with the SSH private key you actually use
- the remote paths if that machine stores repos, Chromium, or build logs
  elsewhere

## Default Chromium Pin

The helper does not track Chromium `main` by default.

Instead, `checkout` and `update` load the repo-local pin from
`scripts/chromium_pin.conf` and sync `chromium/src` to that exact revision.

Current default pin:

- label: `main@{#1609092}`
- revision: `25a94a5662bc9cce918f4626472e7edca1ba2b39`

That revision matches the current Chromium patch manifests that already refer to
the short form `25a94a5662bc`.

If you intentionally want to test a different Chromium base for one run, pass an
explicit override:

```bash
bash scripts/chromium_remote.sh update \
  --chromium-revision 25a94a5662bc9cce918f4626472e7edca1ba2b39
```

## First-Time Remote Machine Setup

On the remote Mac:

1. Install full Xcode and accept the license.
2. Clone this project repo onto the remote machine.
3. Run:

```bash
bash scripts/chromium_remote.sh machine-setup
```

What the script does:

- checks for full Xcode and the macOS SDK
- installs the Xcode `MetalToolchain` component if `xcrun metal` cannot compile a probe shader
- clones or updates `depot_tools`
- installs `ccache` with Homebrew if needed
- creates the persistent ccache directory
- sets the ccache size

## Initial Chromium Checkout

```bash
bash scripts/chromium_remote.sh checkout
```

By default this:

```bash
fetch --no-history chromium
gclient sync -D --force --reset --revision "src@25a94a5662bc9cce918f4626472e7edca1ba2b39"
```

To keep the full Chromium history:

```bash
bash scripts/chromium_remote.sh checkout --with-history
```

## Updating an Existing Chromium Checkout

This resyncs the existing checkout back to the repo-pinned Chromium revision:

```bash
cd /path/to/your/project-repo
bash scripts/chromium_remote.sh update
```

Internally this runs:

- `gclient sync -D --force --reset --revision "src@<pinned revision>"`

The script refuses to do branch switching or repo updates if the Chromium repo is dirty.

## Two Supported Project Repo Modes

### Mode 1: Work directly on the remote machine

If your project repo clone on the remote machine already has the branch you want:

```bash
cd /path/to/your/project-repo
git switch browser-component-impl
bash scripts/chromium_remote.sh build --target //clawbrowser:clawbrowser_unittests
```

### Mode 2: Pull a branch from your fork

If you want the helper to clone/update a separate project repo checkout and use a branch from a fork:

```bash
bash scripts/chromium_remote.sh build \
  --project-repo-url git@github.com:you/clawbrowser.git \
  --project-branch browser-component-impl \
  --project-dir ~/work/clawbrowser-source \
  --target //clawbrowser:clawbrowser_unittests
```

## Sync And Build Over SSH From Your Local Machine

If you do not want to keep a full Chromium checkout on your local machine, the
supported pattern is:

1. keep this repo locally
2. `rsync` the repo to a remote Mac over SSH
3. run `chromium_remote.sh` on the remote machine
4. capture remote logs in a dedicated directory

### One-Time Remote Bootstrap

From your local machine:

```bash
REMOTE_HOST="youruser@203.0.113.10"
SSH_KEY="$HOME/.ssh/your_builder_key"
REMOTE_REPO_DIR="/Users/youruser/dev/clawbrowser"

ssh -i "${SSH_KEY}" "${REMOTE_HOST}" "mkdir -p '${REMOTE_REPO_DIR}'"

rsync -av --delete \
  -e "ssh -i ${SSH_KEY} -o IdentitiesOnly=yes" \
  /path/to/your/local/clawbrowser/ \
  "${REMOTE_HOST}:${REMOTE_REPO_DIR}/"

ssh -i "${SSH_KEY}" "${REMOTE_HOST}" "
  set -euo pipefail
  cd '${REMOTE_REPO_DIR}'
  bash scripts/chromium_remote.sh machine-setup
  bash scripts/chromium_remote.sh checkout
"
```

That leaves the remote machine ready to build against the repo-pinned Chromium
revision.

### Repeatable Sync + Build Flow

After the first checkout exists on the remote machine, your normal update loop
from local is:

```bash
REMOTE_HOST="youruser@203.0.113.10"
SSH_KEY="$HOME/.ssh/your_builder_key"
REMOTE_REPO_DIR="/Users/youruser/dev/clawbrowser"
REMOTE_LOG_DIR="/Users/youruser/dev/clawbrowser-build-logs"

rsync -av --delete \
  -e "ssh -i ${SSH_KEY} -o IdentitiesOnly=yes" \
  /path/to/your/local/clawbrowser/ \
  "${REMOTE_HOST}:${REMOTE_REPO_DIR}/"

ssh -i "${SSH_KEY}" "${REMOTE_HOST}" "
  set -euo pipefail
  mkdir -p '${REMOTE_LOG_DIR}'
  cd '${REMOTE_REPO_DIR}'

  timestamp=\$(date '+%Y%m%d-%H%M%S-%Z')
  bash scripts/chromium_remote.sh apply-patches \
    2>&1 | tee '${REMOTE_LOG_DIR}/apply-patches-\${timestamp}.log'
  bash scripts/chromium_remote.sh build --target chrome \
    2>&1 | tee '${REMOTE_LOG_DIR}/build-chrome-\${timestamp}.log'
"
```

To watch the latest build log:

```bash
ssh -i "${SSH_KEY}" "${REMOTE_HOST}" "
  latest_log=\$(ls -1t ${REMOTE_LOG_DIR}/build-chrome-*.log | head -n1)
  tail -f \"\${latest_log}\"
"
```

If you want a more exact log path per run, print `timestamp` to stdout and tail
that specific file instead of globbing.

### Launching The Convenience Script With Placeholders

If you prefer the checked-in helper, run it with explicit overrides:

```bash
REMOTE_HOST="youruser@203.0.113.10" \
SSH_KEY="$HOME/.ssh/your_builder_key" \
REMOTE_REPO_DIR="/Users/youruser/dev/clawbrowser" \
REMOTE_CHROMIUM_DIR="/Users/youruser/work/chromium" \
REMOTE_BUILD_DIR="/Users/youruser/work/chromium/src/out/CBFast" \
REMOTE_LOG_DIR="/Users/youruser/dev/clawbrowser-build-logs" \
bash scripts/launch_remote_chrome_build.sh --target chrome
```

That helper:

- syncs the current repo to the remote machine with `rsync`
- starts a remote `nohup` build
- prints the exact remote log path, metadata path, and generated remote script

Then inspect the remote logs with:

```bash
ssh -i "${SSH_KEY}" "${REMOTE_HOST}" \
  "tail -f '${REMOTE_LOG_DIR}/latest-chrome-build.log'"
```

## Fast Iteration Loop

This is the supported fast loop and it is for:

- `noise/prng`
- generated-type parsing
- profile loading
- startup orchestration
- API client
- other shim-only logic

It deliberately targets `//clawbrowser:clawbrowser_unittests`, which builds the standalone shim lane and avoids browser-only pieces like `verify_page.cc` and the verify-page GRIT resources.

Build the standalone test target:

```bash
bash scripts/chromium_remote.sh build --target //clawbrowser:clawbrowser_unittests
```

Then run the test binary manually from the Chromium checkout, for example:

```bash
~/work/chromium/src/out/CBFast/clawbrowser_unittests
~/work/chromium/src/out/CBFast/clawbrowser_unittests --gtest_filter='PrngTest.*'
```

This is the cheapest build loop and should be your default unless you changed Chromium source files outside `src/clawbrowser/`.

Do not treat verify-page checks or Playwright/CDP integration tests as authoritative until the version-pinned Chromium patch set under `clawbrowser/patches/` has been applied in the Chromium checkout.

## Slower Integration Loop

This is for:

- actual edits under `chrome/`
- `content/`
- Blink files
- browser/renderer/GPU integration points

Sync the repo overlay and apply the pinned Chromium diffs:

```bash
bash scripts/chromium_remote.sh apply-patches
```

The command is safe to rerun. It skips patch files that are already applied.

Build Chromium itself:

```bash
bash scripts/chromium_remote.sh build --target chrome
```

Run Chromium on macOS:

```bash
~/work/chromium/src/out/CBFast/Chromium.app/Contents/MacOS/Chromium \
  --use-mock-keychain \
  --disable-features=DialMediaRouteProvider
```

Those two runtime flags come from Chromium's macOS build instructions as a practical way to avoid repeated keychain and incoming-connection prompts in dev builds.

## Browser-Level Verification

Once the Chromium-side patches are real, the integration harness is meant to
launch the browser through the actual startup contract, not by injecting
`--clawbrowser-fp-path` directly.

Current harness behavior:

- the browser binary comes from `CLAWBROWSER_BINARY`, with a macOS-friendly
  fallback to `out/CBFast/Chromium.app/Contents/MacOS/Chromium`
- each test run gets a temporary `HOME`
- offline runs start the checked-in local mock API by default using
  `api/mocks/fingerprints.json` and `api/mocks/proxy.json`
- fingerprint-mode fixtures seed
  `~/.config/clawbrowser/Browser/fp_test/fingerprint.json`
- surface-only tests launch with `--fingerprint=fp_test --skip-verify`
- verify-page tests launch with `--fingerprint=fp_test` and attach over CDP to
  the startup-opened `clawbrowser://verify` tab
- an absurd manual smoke fixture is available at
  `clawbrowser/test/fixtures/absurd_fingerprint.json` with matching mock API
  data in `api/mocks/fingerprints_absurd.json`

Manual absurd-fixture smoke path:

1. seed `~/.config/clawbrowser/Browser/fp_test/fingerprint.json` from
   `clawbrowser/test/fixtures/absurd_fingerprint.json`
2. launch the built browser with `--fingerprint=fp_test --skip-verify`
3. confirm the obviously fake Chrome version, timezone, screen metrics,
   WebGL strings, media-device labels, and plugin name show up in both JS
   and the network-visible UA / UA-CH surfaces

Supported remote invocation:

```bash
bash scripts/chromium_remote.sh integration-setup
bash scripts/chromium_remote.sh integration-test
```

What this does:

- syncs the repo overlay into `chromium/src`
- creates or reuses a persistent off-repo venv at `~/.cache/clawbrowser/integration-venv`
- installs `clawbrowser/test/integration/requirements.txt`
- runs the Playwright/pytest suite against the built browser under `out/CBFast`
- uses the local mock API unless you explicitly set a real backend override

To point the verify flow at a real backend instead of the local mock server:

```bash
export CLAWBROWSER_API_BASE_URL=https://your-api.example.com
export CLAWBROWSER_API_KEY=your_real_api_key
```

Then rerun `bash scripts/chromium_remote.sh integration-test`.

If you need to invoke the runner manually after setup, the helper is equivalent to:

```bash
export CLAWBROWSER_BINARY=/Users/m1/work/chromium/src/out/CBFast/Chromium.app/Contents/MacOS/Chromium
cd /Users/m1/work/chromium/src
~/.cache/clawbrowser/integration-venv/bin/python3 \
  clawbrowser/test/integration/run_integration_tests.py
```

## Generated `args.gn`

The helper script writes an `args.gn` file for faster iteration with:

```gn
is_debug = false
is_component_build = true
symbol_level = 0
cc_wrapper = "env CCACHE_DIR=... CCACHE_SLOPPINESS=time_macros ccache"
root_extra_deps = [ "//clawbrowser" ]
```

The first three values match Chromium's documented "faster builds" recommendations for macOS:

- `is_debug = false`
- `is_component_build = true`
- `symbol_level = 0`

The `cc_wrapper` line is the practical ccache integration, using Chromium's documented `cc_wrapper` approach.

The `root_extra_deps` line is a practical Chromium-supported way to load the repo-local `//clawbrowser` targets into GN for standalone builds without patching checked-in Chromium build files.

## Persistent ccache

The helper uses a dedicated persistent cache directory, by default:

```text
~/cache/ccache
```

Useful commands:

```bash
bash scripts/chromium_remote.sh ccache-stats
bash scripts/chromium_remote.sh ccache-size --ccache-max-size 75G
```

## Practical macOS Performance Tips

These are safe and useful for large Chromium trees.

### Spotlight exclusion

This is recommended by Chromium's macOS build instructions because Spotlight indexing can burn CPU during large builds.

Official recommendation: exclude the Chromium checkout from Spotlight indexing in macOS settings.

Practical terminal alternative if you prefer managing it over SSH:

```bash
sudo mdutil -i off ~/work/chromium
```

Use that only if you are comfortable changing indexing behavior on the remote Mac.

### Git untracked cache

Chromium's macOS build instructions recommend testing and then enabling untracked cache when supported.

The helper can apply repo-local git tuning to the Chromium checkout:

```bash
bash scripts/chromium_remote.sh tune-git
```

### Git fsmonitor

Chromium's macOS build instructions recommend enabling repo-local `core.fsmonitor` on large repositories when the Git version is new enough.

The helper enables it automatically in `tune-git` when Git is at least `2.43.0`.

## End-to-End Honesty

This setup is enough to:

- prepare the remote Mac for Chromium development
- keep a real Chromium checkout pinned and reproducible
- sync this project's overlay into `chromium/src`
- replay the repo-owned Chromium patch set deterministically
- generate `args.gn`
- build either `chrome` or a custom target
- provision the Python integration-test environment from repo state
- run the browser-level Playwright/CDP verification suite against the built browser

That means the repo now owns the full replay path:

- `//clawbrowser:clawbrowser_unittests` remains the cheap shim-only loop
- `apply-patches` plus `build --target chrome` is the authoritative integration build
- `integration-test` is the authoritative browser-level verification step

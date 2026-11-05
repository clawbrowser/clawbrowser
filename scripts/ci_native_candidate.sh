#!/usr/bin/env bash
# Compile the exact release source without registry pushes, tags or deployments.
set -euo pipefail
project=$(realpath "$1")
checkout=$(realpath -m "$2")
artifacts=$(realpath -m "$3")
expected_source=$4
target_cpu=$5
[[ "$expected_source" =~ ^[0-9a-f]{40}$ ]]
[[ "$target_cpu" == x64 || "$target_cpu" == arm64 ]]
[[ "$(git -C "$project" rev-parse HEAD)" == "$expected_source" ]]
[[ "$checkout" == "$RUNNER_TEMP/"* ]]
mkdir -p "$artifacts" "$checkout"
exec > >(tee "$artifacts/native-build.log") 2>&1
export DEPOT_TOOLS_UPDATE=0 DEPOT_TOOLS_WIN_TOOLCHAIN=0
source "$project/scripts/clawbrowser_pin.conf"
helpers="$RUNNER_TEMP/native-package-functions.sh"
: > "$helpers"
for name in write_prod_args require_icon_asset write_linux_wrapper copy_linux_runtime overlay_linux_bundle_icons prune_linux_runtime_noise assert_linux_runtime_clean stage_linux_runtime_bundle; do
  awk -v name="$name" '$0 == name "() {" { inside=1 } inside { print } inside && /^}/ { exit }' "$project/scripts/build_remote_prod_artifacts.sh" >> "$helpers"
done
source "$helpers"
for name in write_prod_args require_icon_asset write_linux_wrapper copy_linux_runtime overlay_linux_bundle_icons prune_linux_runtime_noise assert_linux_runtime_clean stage_linux_runtime_bundle; do declare -F "$name" >/dev/null; done
# No persistent cache is available on this one-shot runner; avoid duplicating
# every object file into a compiler cache on the same constrained disk.
resolve_ccache_bin() { return 1; }
die() { printf '%s\n' "$*" >&2; exit 1; }
append_summary() { printf '%s\n' "$*"; }
repo_dir="$project"
icon_assets_dir="$project/branding/icons/app/side-bite"
depot="$RUNNER_TEMP/depot_tools"
git clone --depth=1 https://chromium.googlesource.com/chromium/tools/depot_tools.git "$depot"
export PATH="$depot:$PATH"
printf 'Workflow: %s\nSource: %s\nChromium: %s\nDepot tools: %s\n' "$GITHUB_SHA" "$expected_source" "$CHROMIUM_REVISION" "$(git -C "$depot" rev-parse HEAD)"
python3 - "$checkout/.gclient" <<'PY'
import pathlib, sys
pathlib.Path(sys.argv[1]).write_text("solutions = [{'name': 'src', 'url': 'https://chromium.googlesource.com/chromium/src.git', 'managed': False, 'custom_deps': {}, 'custom_vars': {}}]\ntarget_os = ['linux']\n")
PY
cd "$checkout"
gclient sync --no-history --nohooks --jobs 4 --revision "src@$CHROMIUM_REVISION"
[[ "$(git -C src rev-parse HEAD)" == "$CHROMIUM_REVISION" ]]
sudo src/build/install-build-deps.sh --no-prompt --no-arm --no-chromeos-fonts --no-syms
sudo apt-get install -y --no-install-recommends librsvg2-bin rsync xvfb xauth
gclient runhooks
bash "$project/scripts/clawbrowser_remote.sh" apply-patches --chromium-dir "$checkout" --project-dir "$project" --depot-tools-dir "$depot"

# The production argument writer/staging functions were validated before sync.
build="$checkout/src/out/CBProd"
write_prod_args "$build" "$target_cpu"
cd "$checkout/src"
cp "$build/args.gn" "$artifacts/args.gn"
gn gen "$build"
# One platform per 4-core/16-GB hosted runner. Keep the production GN options;
# Chromium computes its own link-memory pools. No ThinLTO/feature downgrade.
autoninja -C "$build" -j 4 chrome chrome_sandbox clawbrowser_unittests clawbrowser_browser_unittests
xvfb-run -a "$build/clawbrowser_unittests" --gtest_output="json:$artifacts/native-core-tests.json"
xvfb-run -a "$build/clawbrowser_browser_unittests" --gtest_output="json:$artifacts/native-browser-tests.json"
bundle="$artifacts/clawbrowser-linux-$target_cpu"
stage_linux_runtime_bundle "$build" "$bundle"
config=$(mktemp -d "$RUNNER_TEMP/native-startup-XXXXXX")
CLAWBROWSER_CONFIG_DIR="$config" timeout 30 "$bundle/clawbrowser.real" --list > "$artifacts/startup-hook.txt"
python3 - "$artifacts/startup-hook.txt" <<'PY'
import json, pathlib, sys
assert json.loads(pathlib.Path(sys.argv[1]).read_text()) == []
PY
export NATIVE_SOURCE_SHA="$expected_source" NATIVE_TARGET_CPU="$target_cpu" CHROMIUM_REVISION CHROMIUM_VERSION_LABEL
export NATIVE_DEPOT_SHA="$(git -C "$depot" rev-parse HEAD)"
python3 - "$artifacts/source.json" <<'PY'
import json, os, pathlib, sys
data={k:os.environ[k] for k in ['NATIVE_SOURCE_SHA','NATIVE_TARGET_CPU','CHROMIUM_REVISION','CHROMIUM_VERSION_LABEL','NATIVE_DEPOT_SHA','GITHUB_SHA']}
data.update(version='1.0.12',build_profile='production',source_tests_passed=True,startup_hook_passed=True,published=False)
pathlib.Path(sys.argv[1]).write_text(json.dumps(data,indent=2)+'\n')
PY
tar -C "$artifacts" -czf "$artifacts/clawbrowser-linux-$target_cpu.tar.gz" "clawbrowser-linux-$target_cpu"
cd "$artifacts"
sha256sum "clawbrowser-linux-$target_cpu.tar.gz" source.json args.gn native-core-tests.json native-browser-tests.json startup-hook.txt > SHA256SUMS

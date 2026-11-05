#!/usr/bin/env bash
set -euo pipefail
project=$(cd "$1" && pwd -P)
checkout=$2
artifacts=$3
expected_source=$4
[[ "$RUNNER_ENVIRONMENT" == github-hosted && "$(uname -s)" == Darwin ]]
[[ "$checkout" == "$RUNNER_TEMP/"* && "$artifacts" == "$GITHUB_WORKSPACE/"* ]]
[[ "$expected_source" =~ ^[0-9a-f]{40}$ && "$(git -C "$project" rev-parse HEAD)" == "$expected_source" ]]
mkdir -p "$checkout" "$artifacts"
exec > >(tee "$artifacts/native-build.log") 2>&1

# Only this disposable GitHub-hosted VM is touched. Preserve the selected Xcode
# and remove named inactive Xcode apps only until enough build space is free.
python3 - "$artifacts/host-capacity.json" <<'PY'
import json, os, pathlib, platform, re, shutil, subprocess, sys
assert os.environ['RUNNER_ENVIRONMENT']=='github-hosted' and platform.system()=='Darwin'
apps=pathlib.Path('/Applications').resolve()
developer=pathlib.Path(subprocess.check_output(['xcode-select','-p'],text=True).strip()).resolve()
active=next(p for p in developer.parents if p.suffix=='.app' and p.parent==apps)
before=shutil.disk_usage(os.getcwd()).free
removed=[]
for app in sorted(apps.glob('Xcode_*.app')):
    if shutil.disk_usage(os.getcwd()).free>=100*2**30:break
    if app.is_symlink() or not app.is_dir() or not re.fullmatch(r'Xcode_[0-9.]+\.app',app.name):continue
    target=app.resolve()
    assert target.parent==apps and target==app
    if target==active or developer.is_relative_to(target):continue
    print('Removing inactive hosted-image SDK:',target.name,flush=True)
    subprocess.run(['sudo','/bin/rm','-rf','--',str(target)],check=True)
    removed.append(target.name)
free=shutil.disk_usage(os.getcwd()).free
pathlib.Path(sys.argv[1]).write_text(json.dumps({'active_xcode':active.name,'removed_inactive_xcodes':removed,'initial_free_gib':round(before/2**30,2),'build_free_gib':round(free/2**30,2),'corporate_keychain_used':False},indent=2)+'\n')
assert free>=80*2**30,'Hosted Mac still lacks space for the shallow native candidate build'
PY

extract() {
  python3 - "$1" "$2" <<'PY'
import pathlib,re,sys
text=pathlib.Path(sys.argv[1]).read_text()
headers=list(re.finditer(r'(?m)^([A-Za-z_][A-Za-z_0-9]*)\(\) \{\s*$',text))
matches=[i for i,h in enumerate(headers) if h.group(1)==sys.argv[2]]
assert len(matches)==1,sys.argv[2]
i=matches[0]
print(text[headers[i].start():headers[i+1].start() if i+1<len(headers) else len(text)])
PY
}
helpers="$RUNNER_TEMP/native-macos-functions.sh"
: > "$helpers"
for name in command_exists require_command ensure_full_xcode xcode_metal_toolchain_works ensure_xcode_metal_toolchain; do extract "$project/scripts/clawbrowser_remote.sh" "$name" >> "$helpers"; done
for name in write_prod_args require_icon_asset rewrite_macos_bundle_metadata overlay_macos_app_icons copy_macos_runtime_dylibs sign_macos_app_bundle; do extract "$project/scripts/build_remote_prod_artifacts.sh" "$name" >> "$helpers"; done
bash -n "$helpers"
source "$helpers"
log() { printf '%s\n' "$*"; }
die() { printf '%s\n' "$*" >&2; exit 1; }
append_summary() { printf '%s\n' "$*"; }
resolve_ccache_bin() { return 1; }
icon_assets_dir="$project/branding/icons/app/side-bite"
clawbrowser_bundle_version=1.0.12
ensure_full_xcode

export DEPOT_TOOLS_UPDATE=0 DEPOT_TOOLS_WIN_TOOLCHAIN=0
source "$project/scripts/clawbrowser_pin.conf"
depot="$RUNNER_TEMP/depot_tools"
git clone --depth=1 https://chromium.googlesource.com/chromium/tools/depot_tools.git "$depot"
export PATH="$depot:$PATH"
python3 - "$checkout/.gclient" <<'PY'
import pathlib,sys
pathlib.Path(sys.argv[1]).write_text("solutions = [{'name': 'src', 'url': 'https://chromium.googlesource.com/chromium/src.git', 'managed': False, 'custom_deps': {}, 'custom_vars': {}}]\ntarget_os = ['mac']\n")
PY
cd "$checkout"
gclient sync --no-history --nohooks --jobs 3 --revision "src@$CHROMIUM_REVISION"
[[ "$(git -C src rev-parse HEAD)" == "$CHROMIUM_REVISION" ]]
gclient runhooks
bash "$project/scripts/clawbrowser_remote.sh" apply-patches --chromium-dir "$checkout" --project-dir "$project" --depot-tools-dir "$depot"
build="$checkout/src/out/CBProd"
write_prod_args "$build" arm64
cp "$build/args.gn" "$artifacts/args.gn"
cd "$checkout/src"
gn gen "$build"
autoninja -C "$build" -j 3 chrome clawbrowser_unittests clawbrowser_browser_unittests
"$build/clawbrowser_unittests" --gtest_output="json:$artifacts/native-core-tests.json"
"$build/clawbrowser_browser_unittests" --gtest_output="json:$artifacts/native-browser-tests.json"
app="$artifacts/stage/Clawbrowser.app"
mkdir -p "$app"
rsync -a "$build/Chromium.app/" "$app/"
mv "$app/Contents/MacOS/Chromium" "$app/Contents/MacOS/Clawbrowser"
copy_macos_runtime_dylibs "$build" "$app"
rewrite_macos_bundle_metadata "$app"
overlay_macos_app_icons "$app"
sign_macos_app_bundle "$app"
codesign --verify --deep --strict "$app"
export NATIVE_SOURCE_SHA="$expected_source" CHROMIUM_REVISION CHROMIUM_VERSION_LABEL
export NATIVE_DEPOT_SHA="$(git -C "$depot" rev-parse HEAD)"
python3 - "$app/Contents/MacOS/Clawbrowser" "$artifacts/source.json" <<'PY'
import json,os,pathlib,subprocess,sys,tempfile
with tempfile.TemporaryDirectory(prefix='native-startup-') as config:
    result=subprocess.run([sys.argv[1],'--list'],env={**os.environ,'CLAWBROWSER_CONFIG_DIR':config},capture_output=True,text=True,timeout=30,check=True)
    assert json.loads(result.stdout)==[]
data={k:os.environ[k] for k in ['NATIVE_SOURCE_SHA','CHROMIUM_REVISION','CHROMIUM_VERSION_LABEL','NATIVE_DEPOT_SHA','GITHUB_SHA']}
data.update(version='1.0.12',target_cpu='arm64',build_profile='production',source_tests_passed=True,startup_hook_passed=True,signing='ad-hoc candidate',production_ready=False,published=False)
pathlib.Path(sys.argv[2]).write_text(json.dumps(data,indent=2)+'\n')
PY
COPYFILE_DISABLE=1 tar -C "$artifacts/stage" -czf "$artifacts/clawbrowser-macos-arm64.tar.gz" Clawbrowser.app
cd "$artifacts"
shasum -a 256 clawbrowser-macos-arm64.tar.gz source.json args.gn native-core-tests.json native-browser-tests.json > SHA256SUMS

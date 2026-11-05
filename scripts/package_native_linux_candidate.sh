#!/usr/bin/env bash
# Package an exact native CI artifact; no tags, registry writes or publication.
set -euo pipefail
project=$(realpath "$1")
output=$(realpath -m "$2")
native_run=$3
source_sha=$4
cpu=$5
[[ "$RUNNER_ENVIRONMENT" == github-hosted && "$output" == "$GITHUB_WORKSPACE/"* ]]
[[ "$source_sha" =~ ^[0-9a-f]{40}$ && "$native_run" =~ ^[0-9]+$ ]]
[[ "$(git -C "$project" rev-parse HEAD)" == "$source_sha" ]]
case "$cpu:$(uname -m)" in
  x64:x86_64) arch=amd64 ;;
  arm64:aarch64|arm64:arm64) arch=arm64 ;;
  *) echo 'Packaging requires a matching native host'; exit 1 ;;
esac
mkdir -p "$output"
exec > >(tee "$output/packaging.log") 2>&1
download="$RUNNER_TEMP/native-download"
gh run download "$native_run" --repo clawbrowser/clawbrowser --name "native-linux-$cpu-$source_sha" --dir "$download"
python3 - "$download" "$source_sha" "$cpu" <<'PY'
import hashlib,json,pathlib,sys
root=pathlib.Path(sys.argv[1]).resolve()
source=json.loads((root/'source.json').read_text())
assert source['NATIVE_SOURCE_SHA']==sys.argv[2] and source['NATIVE_TARGET_CPU']==sys.argv[3]
assert source['source_tests_compiled'] is True and source['published'] is False
for line in (root/'SHA256SUMS').read_text().splitlines():
    expected,relative=line.split(maxsplit=1)
    file=(root/relative.lstrip('*')).resolve()
    assert file.is_relative_to(root) and file.is_file()
    with file.open('rb') as stream: assert hashlib.file_digest(stream,'sha256').hexdigest()==expected
PY
context="$RUNNER_TEMP/native-docker-context"
mkdir -p "$context/clawbrowser-dist-$arch"
python3 - "$download/clawbrowser-linux-$cpu.tar.gz" "$context/clawbrowser-dist-$arch" "$cpu" <<'PY'
import pathlib,sys,tarfile,tempfile,shutil
destination=pathlib.Path(sys.argv[2])
with tempfile.TemporaryDirectory(dir=destination.parent) as temporary:
    with tarfile.open(sys.argv[1]) as stream: stream.extractall(temporary,filter='data')
    source=pathlib.Path(temporary)/('clawbrowser-linux-'+sys.argv[3])
    assert source.is_dir() and list(pathlib.Path(temporary).iterdir())==[source]
    shutil.copytree(source,destination,dirs_exist_ok=True,symlinks=True)
PY
cp "$project/Dockerfile" "$project/docker-entrypoint.sh" "$context/"
image="clawbrowser-native-candidate:1.0.12-$arch"
docker build --platform "linux/$arch" --label "org.opencontainers.image.revision=$source_sha" --label org.opencontainers.image.version=1.0.12 --tag "$image" "$context"
docker image inspect "$image" > "$output/image-metadata.json"
docker run --rm --env CLAWBROWSER_CONFIG_DIR=/tmp/candidate-empty-config --entrypoint /opt/clawbrowser/clawbrowser.real "$image" --list > "$output/image-startup.json"
python3 - "$output/image-metadata.json" "$output/image-startup.json" "$arch" "$source_sha" <<'PY'
import json,pathlib,sys
image=json.loads(pathlib.Path(sys.argv[1]).read_text())[0]
assert image['Architecture']==sys.argv[3] and image['Os']=='linux'
assert image['Config']['Labels']['org.opencontainers.image.revision']==sys.argv[4]
assert json.loads(pathlib.Path(sys.argv[2]).read_text())==[]
PY
docker image save --output "$output/clawbrowser-docker-linux-$arch.tar" "$image"
IMAGE_REF="$image" VERSION=1.0.12 OUT_DIR="$output" PLATFORM="linux/$arch" bash "$project/scripts/build_portable_xvfb_from_runtime_image.sh"
python3 - "$output" "$context/clawbrowser-dist-$arch/clawbrowser.real" "$arch" "$source_sha" "$native_run" <<'PY'
import hashlib,json,pathlib,sys,tarfile
root=pathlib.Path(sys.argv[1]);arch=sys.argv[3]
prefix='linux-'+arch+'-glibc'
base='clawbrowser-portable-'+prefix
manifest=json.loads((root/(base+'.manifest.json')).read_text())
assert manifest['version']=='1.0.12' and manifest['platform']==prefix
with tarfile.open(root/(base+'.tar.gz')) as stream:
    for path in ['bin/Xvfb','bin/xkbcomp','clawbrowser/clawbrowser.real','manifest.json']:
        assert stream.getmember(prefix+'/'+path).isfile()
    with stream.extractfile(prefix+'/clawbrowser/clawbrowser.real') as packaged,open(sys.argv[2],'rb') as native:
        assert hashlib.file_digest(packaged,'sha256').hexdigest()==hashlib.file_digest(native,'sha256').hexdigest()
(root/'candidate-provenance.json').write_text(json.dumps({'version':'1.0.12','native_source':sys.argv[4],'native_run_id':int(sys.argv[5]),'architecture':arch,'image_started':True,'portable_payload_verified':True,'published':False,'production_ready':False,'runtime_canary':'separate native consumer gate'},indent=2)+'\n')
PY
cd "$output"
sha256sum --check "clawbrowser-portable-linux-$arch-glibc.tar.gz.sha256"
find . -maxdepth 1 -type f ! -name SHA256SUMS ! -name packaging.log -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS

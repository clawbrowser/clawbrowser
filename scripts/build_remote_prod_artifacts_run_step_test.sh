#!/usr/bin/env bash
# Exercise real runner functions without a remote machine or Chromium build.
set -euo pipefail
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
target="${TARGET:-${script_dir}/build_remote_prod_artifacts.sh}"
work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT
extract() { awk -v name="$1" '$0 == name "() {" { inside=1 } inside { print } inside && /^}/ { exit }' "$target"; }
for function in run_step cleanup_on_exit cleanup_stage_dirs record_artifacts; do
  extract "$function" >> "$work/functions.sh"
done
cat > "$work/harness.sh" <<'HARNESS'
set -euo pipefail
work=$1
scenario=$2
source "$3"
artifact_count=0
artifact_1="" artifact_2="" artifact_3="" artifact_4=""
cleanup_dir_1="" cleanup_dir_2="" cleanup_dir_3=""
artifact_manifest_path="$work/manifest"
append_summary() { echo "$*" >> "$work/summary"; }
write_status() { echo "$1 $2 $3 count=$artifact_count" > "$work/status"; }
release_build_locks() { : > "$work/locks-released"; }
trap cleanup_on_exit EXIT
package() {
  artifact_count=2
  artifact_1="$work/one.tar.gz" artifact_2="$work/two.tar.gz"
  cleanup_dir_1="$work/stage-one" cleanup_dir_2="$work/stage-two"
  mkdir "$cleanup_dir_1" "$cleanup_dir_2"
  case "$scenario" in
    middle) false ;;
    explicit) exit 7 ;;
    early_success) exit 0 ;;
    pipeline) false | cat ;;
    missing) echo "$UNDEFINED_RUNNER_TEST_VARIABLE" ;;
    handled)
      set +e
      false
      code=$?
      set -e
      [[ "$code" == 1 ]]
      ;;
  esac
  echo reached-end >> "$work/summary"
}
run_step package package
[[ "$artifact_count" == 2 ]]
record_artifacts
write_status succeeded finished 0
echo finished >> "$work/summary"
HARNESS
for scenario in ${SCENARIOS:-success handled middle explicit early_success pipeline missing}; do
  dir="$work/$scenario"; mkdir "$dir"
  set +e
  bash "$work/harness.sh" "$dir" "$scenario" "$work/functions.sh" > "$dir/output" 2>&1
  code=$?
  set -e
  [[ -f "$dir/locks-released" && ! -e "$dir/stage-one" && ! -e "$dir/stage-two" ]] || {
    echo "FAIL $scenario: cleanup or state propagation"; cat "$dir/output"; exit 1;
  }
  case "$scenario" in
    success|handled)
      [[ "$code" == 0 ]] || { cat "$dir/output"; exit 1; }
      grep -qx 'succeeded finished 0 count=2' "$dir/status"
      printf '%s\n' "$dir/one.tar.gz" "$dir/two.tar.gz" > "$dir/expected"
      cmp "$dir/manifest" "$dir/expected"
      ;;
    *)
      [[ "$code" != 0 ]]
      [[ "$scenario" != explicit || "$code" == 7 ]]
      grep -qx "failed package $code count=2" "$dir/status"
      if grep -qE 'reached-end|finished|OK package' "$dir/summary"; then
        echo "FAIL $scenario: execution continued after failure"; exit 1
      fi
      ;;
  esac
  echo "PASS $scenario"
done

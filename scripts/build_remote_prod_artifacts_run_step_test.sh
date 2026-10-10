#!/usr/bin/env bash
# Behavioural check: a command failing in the middle of a step must fail the
# step even when later commands in the same step succeed.
set -euo pipefail
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
run_step_src="$(awk '/^run_step\(\) \{/,/^\}/' "${TARGET:-${script_dir}/build_remote_prod_artifacts.sh}" | head -n 200)"
[[ -n "${run_step_src}" ]] || { echo "FAIL: run_step not found"; exit 1; }
work="$(mktemp -d)"; trap 'rm -rf "${work}"' EXIT
cat > "${work}/harness.sh" <<HARNESS
set -euo pipefail
append_summary() { :; }
write_status() { echo "status=\$1 step=\$2 exit=\$3" > "${work}/status"; }
${run_step_src}
middle_failure() { false; echo "after the failure"; true; }
run_step middle middle_failure
echo "not reached"
HARNESS
set +e
out="$(bash "${work}/harness.sh" 2>&1)"; code=$?
set -e
if [[ "${code}" -eq 0 || "${out}" == *"not reached"* || "${out}" == *"after the failure"* ]]; then
  echo "FAIL: a failure inside a step was swallowed (exit=${code}): ${out}"; exit 1
fi
grep -q "status=failed step=middle" "${work}/status" || { echo "FAIL: status not recorded"; exit 1; }
echo PASS

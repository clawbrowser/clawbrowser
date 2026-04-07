#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

REMOTE_HOST="${REMOTE_HOST:-m1@62.210.166.239}"
SSH_KEY="${SSH_KEY:-${HOME}/.ssh/id_ed25519_macmini}"
REMOTE_REPO_DIR="${REMOTE_REPO_DIR:-/Users/m1/dev/clawbrowser}"
REMOTE_CHROMIUM_DIR="${REMOTE_CHROMIUM_DIR:-/Users/m1/work/chromium}"
REMOTE_BUILD_DIR="${REMOTE_BUILD_DIR:-${REMOTE_CHROMIUM_DIR}/src/out/CBFast}"
REMOTE_LOG_DIR="${REMOTE_LOG_DIR:-/Users/m1/dev/clawbrowser-build-logs}"
INTEGRATION_VENV_DIR="${INTEGRATION_VENV_DIR:-/Users/m1/.cache/clawbrowser/integration-venv}"
INTEGRATION_FILTER="${INTEGRATION_FILTER:-user_agent or timezone or verify_page or proxy}"
SKIP_RSYNC=0

log() {
  printf '[overnight-validation] %s\n' "$*"
}

die() {
  printf '[overnight-validation] ERROR: %s\n' "$*" >&2
  exit 1
}

usage() {
  cat <<EOF
Usage:
  scripts/launch_remote_overnight_validation.sh [options]

This script:
1. rsyncs the current repo to ${REMOTE_HOST}:${REMOTE_REPO_DIR}
2. starts one remote nohup validation job
3. writes one log per step under ${REMOTE_LOG_DIR}/overnight-<timestamp>/

Remote steps:
  01. sync-project
  02. apply-patches
  03. build chrome
  04. build //clawbrowser:clawbrowser_unittests
  05. build //clawbrowser:clawbrowser_browser_unittests
  06. integration-setup
  07. integration tests with pytest filter

Artifacts:
  summary.txt   - step-by-step pass/fail summary
  status.env    - machine-readable status
  nohup.log     - outer runner stdout/stderr
  01-*.log ...  - one log file per step

Options:
  --skip-rsync                    Do not sync the repo before launching
  --integration-filter EXPR       Default: ${INTEGRATION_FILTER}
  --remote-host USER@HOST         Default: ${REMOTE_HOST}
  --ssh-key PATH                  Default: ${SSH_KEY}
  --remote-repo-dir PATH          Default: ${REMOTE_REPO_DIR}
  --remote-chromium-dir PATH      Default: ${REMOTE_CHROMIUM_DIR}
  --remote-build-dir PATH         Default: ${REMOTE_BUILD_DIR}
  --remote-log-dir PATH           Default: ${REMOTE_LOG_DIR}
  --integration-venv-dir PATH     Default: ${INTEGRATION_VENV_DIR}
  --help                          Show this help
EOF
}

assert_no_spaces() {
  local path="$1"
  local label="$2"
  if [[ "${path}" == *" "* ]]; then
    die "${label} must not contain spaces: ${path}"
  fi
}

quote() {
  printf '%q' "$1"
}

ssh_cmd() {
  ssh -i "${SSH_KEY}" -o IdentitiesOnly=yes "${REMOTE_HOST}" "$@"
}

rsync_repo() {
  log "Syncing ${REPO_ROOT} to ${REMOTE_HOST}:${REMOTE_REPO_DIR}"
  rsync -av --delete \
    -e "ssh -i ${SSH_KEY} -o IdentitiesOnly=yes" \
    "${REPO_ROOT}/" \
    "${REMOTE_HOST}:${REMOTE_REPO_DIR}/"
}

parse_args() {
  while (($# > 0)); do
    case "$1" in
      --skip-rsync)
        SKIP_RSYNC=1
        shift
        ;;
      --integration-filter)
        INTEGRATION_FILTER="$2"
        shift 2
        ;;
      --remote-host)
        REMOTE_HOST="$2"
        shift 2
        ;;
      --ssh-key)
        SSH_KEY="$2"
        shift 2
        ;;
      --remote-repo-dir)
        REMOTE_REPO_DIR="$2"
        shift 2
        ;;
      --remote-chromium-dir)
        REMOTE_CHROMIUM_DIR="$2"
        shift 2
        ;;
      --remote-build-dir)
        REMOTE_BUILD_DIR="$2"
        shift 2
        ;;
      --remote-log-dir)
        REMOTE_LOG_DIR="$2"
        shift 2
        ;;
      --integration-venv-dir)
        INTEGRATION_VENV_DIR="$2"
        shift 2
        ;;
      --help|-h)
        usage
        exit 0
        ;;
      *)
        die "Unknown option: $1"
        ;;
    esac
  done
}

build_remote_runner_script() {
  cat <<EOF
#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(quote "${REMOTE_REPO_DIR}")
chromium_dir=$(quote "${REMOTE_CHROMIUM_DIR}")
build_dir=$(quote "${REMOTE_BUILD_DIR}")
run_dir="\$1"
integration_venv_dir=$(quote "${INTEGRATION_VENV_DIR}")
integration_filter=$(quote "${INTEGRATION_FILTER}")

summary_file="\${run_dir}/summary.txt"
status_file="\${run_dir}/status.env"
runner_log="\${run_dir}/runner.log"

timestamp_now() {
  date '+%Y-%m-%d %H:%M:%S %Z'
}

write_status() {
  local state="\$1"
  local step="\${2:-}"
  local exit_code="\${3:-0}"
  cat >"\${status_file}" <<STATUS
state=\${state}
step=\${step}
exit_code=\${exit_code}
updated_at=\$(timestamp_now)
run_dir=\${run_dir}
STATUS
}

append_summary() {
  printf '%s\n' "\$*" | tee -a "\${summary_file}" >>"\${runner_log}"
}

run_step() {
  local number="\$1"
  local name="\$2"
  shift 2

  local log_file="\${run_dir}/\${number}-\${name}.log"
  append_summary "[\$(timestamp_now)] START \${number} \${name} -> \${log_file}"
  write_status "running" "\${number}-\${name}" 0

  if "\$@" >"\${log_file}" 2>&1; then
    append_summary "[\$(timestamp_now)] PASS  \${number} \${name}"
    return 0
  else
    local rc=\$?
    append_summary "[\$(timestamp_now)] FAIL  \${number} \${name} rc=\${rc}"
    write_status "failed" "\${number}-\${name}" "\${rc}"
    exit "\${rc}"
  fi
}

mkdir -p "\${run_dir}"
: >"\${summary_file}"
: >"\${runner_log}"
write_status "running" "boot" 0

append_summary "Overnight validation started at \$(timestamp_now)"
append_summary "repo_dir=\${repo_dir}"
append_summary "chromium_dir=\${chromium_dir}"
append_summary "build_dir=\${build_dir}"
append_summary "integration_filter=\${integration_filter}"

cd "\${repo_dir}"

run_step 01 sync-project \
  bash scripts/chromium_remote.sh sync-project \
    --chromium-dir "\${chromium_dir}" \
    --project-dir "\${repo_dir}"

run_step 02 apply-patches \
  bash scripts/chromium_remote.sh apply-patches \
    --chromium-dir "\${chromium_dir}" \
    --project-dir "\${repo_dir}"

run_step 03 build-chrome \
  bash scripts/chromium_remote.sh build \
    --chromium-dir "\${chromium_dir}" \
    --build-dir "\${build_dir}" \
    --project-dir "\${repo_dir}" \
    --target chrome

run_step 04 build-clawbrowser_unittests \
  bash scripts/chromium_remote.sh build \
    --chromium-dir "\${chromium_dir}" \
    --build-dir "\${build_dir}" \
    --project-dir "\${repo_dir}" \
    --target //clawbrowser:clawbrowser_unittests

run_step 05 build-clawbrowser_browser_unittests \
  bash scripts/chromium_remote.sh build \
    --chromium-dir "\${chromium_dir}" \
    --build-dir "\${build_dir}" \
    --project-dir "\${repo_dir}" \
    --target //clawbrowser:clawbrowser_browser_unittests

run_step 06 integration-setup \
  bash scripts/chromium_remote.sh integration-setup \
    --chromium-dir "\${chromium_dir}" \
    --project-dir "\${repo_dir}" \
    --integration-venv-dir "\${integration_venv_dir}"

run_step 07 integration-tests \
  env \
    CLAWBROWSER_BINARY="\${build_dir}/Chromium.app/Contents/MacOS/Chromium" \
    CLAWBROWSER_PROJECT_DIR="\${repo_dir}" \
    "\${integration_venv_dir}/bin/python3" \
    clawbrowser/test/integration/run_integration_tests.py \
    -k "\${integration_filter}"

append_summary "Overnight validation finished at \$(timestamp_now)"
write_status "passed" "done" 0
EOF
}

launch_remote_job() {
  local remote_output
  local runner_body
  local q_repo_dir q_chromium_dir q_build_dir q_log_dir q_venv_dir q_filter

  runner_body="$(build_remote_runner_script)"
  q_repo_dir="$(quote "${REMOTE_REPO_DIR}")"
  q_chromium_dir="$(quote "${REMOTE_CHROMIUM_DIR}")"
  q_build_dir="$(quote "${REMOTE_BUILD_DIR}")"
  q_log_dir="$(quote "${REMOTE_LOG_DIR}")"
  q_venv_dir="$(quote "${INTEGRATION_VENV_DIR}")"
  q_filter="$(quote "${INTEGRATION_FILTER}")"

  remote_output="$(
    ssh_cmd "bash -s" <<EOF
set -euo pipefail

repo_dir=${q_repo_dir}
chromium_dir=${q_chromium_dir}
build_dir=${q_build_dir}
log_root=${q_log_dir}
integration_venv_dir=${q_venv_dir}
integration_filter=${q_filter}

mkdir -p "\${log_root}"

timestamp="\$(date '+%Y%m%d-%H%M%S-%Z')"
run_dir="\${log_root}/overnight-\${timestamp}"
runner_path="\${run_dir}/run.sh"
meta_path="\${run_dir}/launch.env"
latest_link="\${log_root}/latest-overnight"

mkdir -p "\${run_dir}"

cat >"\${runner_path}" <<'RUNNER'
${runner_body}
RUNNER
chmod +x "\${runner_path}"

cat >"\${meta_path}" <<META
timestamp=\${timestamp}
remote_host=\$(hostname)
repo_dir=\${repo_dir}
chromium_dir=\${chromium_dir}
build_dir=\${build_dir}
integration_venv_dir=\${integration_venv_dir}
integration_filter=\${integration_filter}
run_dir=\${run_dir}
runner_path=\${runner_path}
META

nohup "\${runner_path}" "\${run_dir}" >"\${run_dir}/nohup.log" 2>&1 < /dev/null &
pid="\$!"
printf 'pid=%s\n' "\${pid}" >>"\${meta_path}"
ln -sfn "\${run_dir}" "\${latest_link}"

sleep 1
if ! kill -0 "\${pid}" 2>/dev/null; then
  echo "FAILED_TO_START=1"
  echo "RUN_DIR=\${run_dir}"
  tail -n 50 "\${run_dir}/nohup.log" || true
  exit 1
fi

printf 'RUN_DIR=%s\n' "\${run_dir}"
printf 'PID=%s\n' "\${pid}"
printf 'SUMMARY=%s\n' "\${run_dir}/summary.txt"
printf 'STATUS=%s\n' "\${run_dir}/status.env"
printf 'NOHUP=%s\n' "\${run_dir}/nohup.log"
printf 'LATEST=%s\n' "\${latest_link}"
EOF
  )"

  printf '%s\n' "${remote_output}"
}

main() {
  parse_args "$@"

  [[ -f "${SSH_KEY}" ]] || die "SSH key not found: ${SSH_KEY}"

  assert_no_spaces "${REMOTE_REPO_DIR}" "Remote repo path"
  assert_no_spaces "${REMOTE_CHROMIUM_DIR}" "Remote chromium path"
  assert_no_spaces "${REMOTE_BUILD_DIR}" "Remote build path"
  assert_no_spaces "${REMOTE_LOG_DIR}" "Remote log path"
  assert_no_spaces "${INTEGRATION_VENV_DIR}" "Integration venv path"

  if ((SKIP_RSYNC == 0)); then
    rsync_repo
  else
    log "Skipping rsync"
  fi

  log "Launching remote overnight validation under nohup"
  launch_remote_job
}

main "$@"

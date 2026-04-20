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
TARGET="${TARGET:-chrome}"
SKIP_RSYNC=0

log() {
  printf '[remote-clawbrowser-build] %s\n' "$*"
}

die() {
  printf '[remote-clawbrowser-build] ERROR: %s\n' "$*" >&2
  exit 1
}

usage() {
  cat <<EOF
Usage:
  scripts/launch_remote_clawbrowser_build.sh [options]

This script:
1. rsyncs the current repo to ${REMOTE_HOST}:${REMOTE_REPO_DIR}
2. starts a remote launchd-backed build that runs:
   - bash scripts/clawbrowser_remote.sh apply-patches
   - bash scripts/clawbrowser_remote.sh build --target ${TARGET}
3. stores timestamped logs, metadata, and the exact remote command under:
   ${REMOTE_LOG_DIR}

Options:
  --skip-rsync               Do not sync the repo before launching the build
  --target NAME              Build target. Default: chrome
  --remote-host USER@HOST    Default: ${REMOTE_HOST}
  --ssh-key PATH             Default: ${SSH_KEY}
  --remote-repo-dir PATH     Default: ${REMOTE_REPO_DIR}
  --remote-chromium-dir PATH Default: ${REMOTE_CHROMIUM_DIR}
  --remote-build-dir PATH    Default: ${REMOTE_BUILD_DIR}
  --remote-log-dir PATH      Default: ${REMOTE_LOG_DIR}
  --help                     Show this help
EOF
}

assert_no_spaces() {
  local path="$1"
  local label="$2"
  if [[ "${path}" == *" "* ]]; then
    die "${label} must not contain spaces: ${path}"
  fi
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
      --target)
        TARGET="$2"
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

launch_remote_build() {
  local remote_output
  local q_repo_dir q_chromium_dir q_build_dir q_log_dir q_target

  q_repo_dir="$(printf '%q' "${REMOTE_REPO_DIR}")"
  q_chromium_dir="$(printf '%q' "${REMOTE_CHROMIUM_DIR}")"
  q_build_dir="$(printf '%q' "${REMOTE_BUILD_DIR}")"
  q_log_dir="$(printf '%q' "${REMOTE_LOG_DIR}")"
  q_target="$(printf '%q' "${TARGET}")"

  remote_output="$(
    ssh_cmd "bash -s" <<EOF
set -euo pipefail

repo_dir=${q_repo_dir}
chromium_dir=${q_chromium_dir}
build_dir=${q_build_dir}
log_dir=${q_log_dir}
target=${q_target}

mkdir -p "\${log_dir}"

timestamp="\$(date '+%Y%m%d-%H%M%S-%Z')"
label="dev.clawbrowser.clawbrowser-build.\${timestamp}.\$\$"
log_path="\${log_dir}/clawbrowser-build-\${timestamp}.log"
meta_path="\${log_dir}/clawbrowser-build-\${timestamp}.meta"
script_path="\${log_dir}/clawbrowser-build-\${timestamp}.sh"
latest_log="\${log_dir}/latest-clawbrowser-build.log"
latest_meta="\${log_dir}/latest-clawbrowser-build.meta"
latest_script="\${log_dir}/latest-clawbrowser-build.sh"

cat >"\${script_path}" <<'SCRIPT'
#!/usr/bin/env bash
set -euo pipefail

cd '${REMOTE_REPO_DIR}'
echo \"[remote-build] started \$(date '+%Y-%m-%d %H:%M:%S %Z')\"
bash scripts/clawbrowser_remote.sh apply-patches --chromium-dir '${REMOTE_CHROMIUM_DIR}' --project-dir '${REMOTE_REPO_DIR}'
bash scripts/clawbrowser_remote.sh build --target '${TARGET}' --chromium-dir '${REMOTE_CHROMIUM_DIR}' --build-dir '${REMOTE_BUILD_DIR}' --project-dir '${REMOTE_REPO_DIR}'
echo \"[remote-build] finished \$(date '+%Y-%m-%d %H:%M:%S %Z')\"
SCRIPT

chmod +x "\${script_path}"

launchctl submit -l "\${label}" -- /bin/sh -c '
"\$1" >"\$2" 2>&1
exit \$?
' sh "\${script_path}" "\${log_path}"

pid=""
launchctl_dump=""
service_registered=0
for attempt in 1 2 3; do
  launchctl_dump="\$(launchctl print "gui/\$(id -u)/\${label}" 2>/dev/null || true)"
  if [[ -n "\${launchctl_dump}" ]]; then
    service_registered=1
  fi
  pid="\$(printf '%s\n' "\${launchctl_dump}" | sed -n 's/^[[:space:]]*pid = \([0-9][0-9]*\)$/\1/p' | head -n 1)"
  if [[ -n "\${pid}" || -f "\${log_path}" ]]; then
    break
  fi
  sleep 1
done

cat >"\${meta_path}" <<META
timestamp=\${timestamp}
remote_host=\$(hostname)
label=\${label}
pid=\${pid}
repo_dir=\${repo_dir}
chromium_dir=\${chromium_dir}
build_dir=\${build_dir}
target=\${target}
log_path=\${log_path}
script_path=\${script_path}
META

ln -sfn "\${log_path}" "\${latest_log}"
ln -sfn "\${meta_path}" "\${latest_meta}"
ln -sfn "\${script_path}" "\${latest_script}"

if (( service_registered == 0 )) && [[ ! -f "\${log_path}" ]]; then
  echo "BUILD_FAILED_TO_START=1"
  echo "LABEL=\${label}"
  echo "LOG_PATH=\${log_path}"
  tail -n 50 "\${log_path}" || true
  exit 1
fi

printf 'TIMESTAMP=%s\n' "\${timestamp}"
printf 'LABEL=%s\n' "\${label}"
printf 'PID=%s\n' "\${pid}"
printf 'LOG_PATH=%s\n' "\${log_path}"
printf 'META_PATH=%s\n' "\${meta_path}"
printf 'SCRIPT_PATH=%s\n' "\${script_path}"
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

  if ((SKIP_RSYNC == 0)); then
    rsync_repo
  else
    log "Skipping rsync"
  fi

  log "Launching remote ${TARGET} build under launchd"
  launch_remote_build
}

main "$@"

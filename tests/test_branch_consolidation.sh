#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
  printf 'FAIL: %s\n' "$*" >&2
  exit 1
}

cleanup_subject="Clarify portable no-monitor install flow"

subjects="$(git -C "${ROOT_DIR}" log --format=%s --max-count=200)"
if ! grep -Fxq "${cleanup_subject}" <<<"${subjects}"; then
  fail "project-restructure-release-cleanup cleanup commit is not present in HEAD history"
fi

if git -C "${ROOT_DIR}" rev-parse --verify -q origin/main >/dev/null; then
  git -C "${ROOT_DIR}" merge-base --is-ancestor origin/main HEAD ||
    fail "HEAD is not based on current origin/main"
fi

if git -C "${ROOT_DIR}" rev-parse --verify -q origin/fix/portable-self-contained-handoff-release >/dev/null; then
  git -C "${ROOT_DIR}" merge-base --is-ancestor origin/fix/portable-self-contained-handoff-release HEAD ||
    fail "superseded portable handoff branch is not contained in HEAD"
else
  printf 'skip - superseded release branch ref is not available locally\n'
fi

printf 'ok - release branch consolidation\n'

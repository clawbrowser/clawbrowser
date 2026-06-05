#!/usr/bin/env bash
# check_generated_fresh.sh — Verify that browser_schema.json and
# fingerprint_types.{h,cc} are in sync.
#
# Usage:
#   ./clawbrowser/schemas/check_generated_fresh.sh [--update]
#
# With --update: regenerate the canonical schema artifact and generated files.
# Without --update: verify the generated files match the checked-in schema
# artifact and, when PyYAML is available, verify the checked-in schema artifact
# still matches api/openapi.yaml.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
OPENAPI="${REPO_ROOT}/api/openapi.yaml"
SCHEMA_ARTIFACT="${SCRIPT_DIR}/browser_schema.json"
HEADER_PATH="${REPO_ROOT}/clawbrowser/generated/fingerprint_types.h"
SOURCE_PATH="${REPO_ROOT}/clawbrowser/generated/fingerprint_types.cc"
GENERATOR="${SCRIPT_DIR}/generate_browser_types.py"
EXTRACTOR="${SCRIPT_DIR}/extract_schemas.py"

if ! command -v python3 >/dev/null 2>&1; then
  echo "ERROR: python3 is required" >&2
  exit 1
fi

if [[ "${1:-}" == "--update" ]]; then
  python3 "${EXTRACTOR}" --openapi "${OPENAPI}" --output "${SCHEMA_ARTIFACT}"
  python3 "${GENERATOR}" \
    --schema "${SCHEMA_ARTIFACT}" \
    --header "${HEADER_PATH}" \
    --source "${SOURCE_PATH}"
  echo "Updated ${SCHEMA_ARTIFACT} and generated browser types"
  exit 0
fi

if [[ ! -f "${SCHEMA_ARTIFACT}" ]]; then
  echo "ERROR: ${SCHEMA_ARTIFACT} not found. Run with --update to initialize." >&2
  exit 1
fi

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TMP_DIR}"' EXIT

python3 "${GENERATOR}" \
  --check \
  --schema "${SCHEMA_ARTIFACT}" \
  --header "${HEADER_PATH}" \
  --source "${SOURCE_PATH}"

if grep -nE 'for \(const auto& \[key, item\] : \*' "${SOURCE_PATH}" >&2; then
  echo "ERROR: generated DictValue string-map loops must not bind iterator pairs by reference" >&2
  exit 1
fi

if python3 -c "import yaml" >/dev/null 2>&1; then
  python3 "${EXTRACTOR}" --openapi "${OPENAPI}" --output "${TMP_DIR}/browser_schema.json" >/dev/null
  if ! cmp -s "${SCHEMA_ARTIFACT}" "${TMP_DIR}/browser_schema.json"; then
    echo "STALE: ${SCHEMA_ARTIFACT} does not match ${OPENAPI}" >&2
    echo "Run: clawbrowser/schemas/check_generated_fresh.sh --update" >&2
    exit 1
  fi
else
  echo "WARNING: PyYAML not installed; skipped OpenAPI-to-schema freshness check" >&2
fi

echo "OK: browser schema artifact and generated types are fresh"

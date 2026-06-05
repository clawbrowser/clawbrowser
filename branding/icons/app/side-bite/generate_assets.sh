#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"
SOURCE_SVG="${REPO_ROOT}/clawbrowser/resources/side_bite.svg"
TEMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/side-bite.XXXXXX")"
ICONSET_DIR="${TEMP_DIR}/side-bite.iconset"
mkdir -p "${ICONSET_DIR}"

cleanup() {
  rm -rf "${TEMP_DIR}"
}

trap cleanup EXIT

render_png() {
  local size="$1"
  local out_path="$2"

  if ! command -v rsvg-convert >/dev/null 2>&1; then
    printf 'need rsvg-convert to render deterministic Side Bite assets: %s\n' "${out_path}" >&2
    return 1
  fi

  rsvg-convert -w "${size}" -h "${size}" "${SOURCE_SVG}" -o "${out_path}"
}

render_png 16 "${SCRIPT_DIR}/product_logo_16.png"
render_png 22 "${SCRIPT_DIR}/product_logo_22.png"
render_png 24 "${SCRIPT_DIR}/product_logo_24.png"
render_png 32 "${SCRIPT_DIR}/product_logo_32.png"
render_png 48 "${SCRIPT_DIR}/product_logo_48.png"
render_png 64 "${SCRIPT_DIR}/product_logo_64.png"
render_png 128 "${SCRIPT_DIR}/product_logo_128.png"
render_png 256 "${SCRIPT_DIR}/product_logo_256.png"
render_png 512 "${SCRIPT_DIR}/product_logo_512.png"
render_png 1024 "${SCRIPT_DIR}/product_logo_1024.png"

cp "${SCRIPT_DIR}/product_logo_16.png" "${ICONSET_DIR}/icon_16x16.png"
cp "${SCRIPT_DIR}/product_logo_32.png" "${ICONSET_DIR}/icon_16x16@2x.png"
cp "${SCRIPT_DIR}/product_logo_32.png" "${ICONSET_DIR}/icon_32x32.png"
cp "${SCRIPT_DIR}/product_logo_64.png" "${ICONSET_DIR}/icon_32x32@2x.png"
cp "${SCRIPT_DIR}/product_logo_128.png" "${ICONSET_DIR}/icon_128x128.png"
cp "${SCRIPT_DIR}/product_logo_256.png" "${ICONSET_DIR}/icon_128x128@2x.png"
cp "${SCRIPT_DIR}/product_logo_256.png" "${ICONSET_DIR}/icon_256x256.png"
cp "${SCRIPT_DIR}/product_logo_512.png" "${ICONSET_DIR}/icon_256x256@2x.png"
cp "${SCRIPT_DIR}/product_logo_512.png" "${ICONSET_DIR}/icon_512x512.png"
cp "${SCRIPT_DIR}/product_logo_1024.png" "${ICONSET_DIR}/icon_512x512@2x.png"

write_icns_fallback() {
  python3 - "${SCRIPT_DIR}" <<'PY'
import pathlib
import struct
import sys

script_dir = pathlib.Path(sys.argv[1])
entries = [
    ("icp4", script_dir / "product_logo_16.png"),
    ("icp5", script_dir / "product_logo_32.png"),
    ("icp6", script_dir / "product_logo_64.png"),
    ("ic07", script_dir / "product_logo_128.png"),
    ("ic08", script_dir / "product_logo_256.png"),
    ("ic09", script_dir / "product_logo_512.png"),
    ("ic10", script_dir / "product_logo_1024.png"),
]

chunks = []
for type_code, path in entries:
    data = path.read_bytes()
    chunks.append(type_code.encode("ascii") + struct.pack(">I", len(data) + 8) + data)

payload = b"".join(chunks)
(script_dir / "app.icns").write_bytes(b"icns" + struct.pack(">I", len(payload) + 8) + payload)
PY
}

if command -v iconutil >/dev/null 2>&1 &&
   iconutil -c icns "${ICONSET_DIR}" -o "${SCRIPT_DIR}/app.icns" 2>/dev/null; then
  exit 0
fi

if xcrun -f iconutil >/dev/null 2>&1 &&
   xcrun iconutil -c icns "${ICONSET_DIR}" -o "${SCRIPT_DIR}/app.icns" 2>/dev/null; then
  exit 0
fi

write_icns_fallback

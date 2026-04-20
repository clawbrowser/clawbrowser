#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
SIDE_BITE_SVG="${REPO_ROOT}/clawbrowser/resources/side_bite.svg"
ICON_DIR="${REPO_ROOT}/branding/icons/app/side-bite"
ARTIFACT_TAR="${REPO_ROOT}/site-bite-artifacts.tar.gz"
AUTH_HTML="${REPO_ROOT}/clawbrowser/auth/resources/auth.html"
VERIFY_HTML="${REPO_ROOT}/clawbrowser/verify/resources/verify.html"

fail() {
  printf 'FAIL: %s\n' "$*" >&2
  exit 1
}

assert_contains() {
  local file="$1"
  local needle="$2"
  grep -Fq -- "${needle}" "${file}" || \
    fail "expected ${file} to contain: ${needle}"
}

assert_not_contains() {
  local file="$1"
  local needle="$2"
  if grep -Fq -- "${needle}" "${file}"; then
    fail "did not expect ${file} to contain: ${needle}"
  fi
}

assert_tar_contains() {
  local tarball="$1"
  local path="$2"
  tar -tzf "${tarball}" | grep -Fxq -- "${path}" || \
    fail "expected ${tarball} to contain: ${path}"
}

assert_tar_contains_text() {
  local tarball="$1"
  local path="$2"
  local needle="$3"
  tar -xOf "${tarball}" "${path}" | grep -Fq -- "${needle}" || \
    fail "expected ${tarball}:${path} to contain: ${needle}"
}

assert_contains "${SIDE_BITE_SVG}" '<linearGradient id="tileFill"'
assert_contains "${SIDE_BITE_SVG}" '<linearGradient id="tileStroke"'
assert_contains "${SIDE_BITE_SVG}" '<radialGradient id="circleFill"'
assert_contains "${SIDE_BITE_SVG}" '<rect x="33.5" y="33.5" width="189" height="189" rx="46.5" stroke="url(#tileStroke)" stroke-width="3"/>'
assert_contains "${SIDE_BITE_SVG}" '<circle cx="160" cy="128" r="34" fill="url(#circleFill)" stroke="#282828" stroke-width="2" filter="url(#circleInset)"/>'
assert_not_contains "${SIDE_BITE_SVG}" '<mask id="clawbrowser-side-bite"'
assert_not_contains "${SIDE_BITE_SVG}" 'mask="url(#clawbrowser-side-bite)"'
assert_not_contains "${SIDE_BITE_SVG}" '#6B3C72'
assert_not_contains "${SIDE_BITE_SVG}" '#18001E'

for asset in app.icns \
             product_logo_16.png product_logo_22.png product_logo_24.png \
             product_logo_32.png product_logo_48.png product_logo_64.png \
             product_logo_128.png product_logo_256.png product_logo_512.png \
             product_logo_1024.png; do
  [[ -f "${ICON_DIR}/${asset}" ]] || fail "missing generated icon asset: ${ICON_DIR}/${asset}"
done

assert_contains "${AUTH_HTML}" '<link rel="icon" type="image/svg+xml" href="side-bite.svg">'
assert_contains "${VERIFY_HTML}" '<link rel="icon" type="image/svg+xml" href="side-bite.svg">'

assert_tar_contains "${ARTIFACT_TAR}" 'clawbrowser/resources/side_bite.svg'
assert_tar_contains_text "${ARTIFACT_TAR}" 'clawbrowser/resources/side_bite.svg' '<linearGradient id="tileFill"'
assert_tar_contains "${ARTIFACT_TAR}" 'branding/icons/app/side-bite/app.icns'
for asset in product_logo_16.png product_logo_22.png product_logo_24.png \
             product_logo_32.png product_logo_48.png product_logo_64.png \
             product_logo_128.png product_logo_256.png product_logo_512.png \
             product_logo_1024.png; do
  assert_tar_contains "${ARTIFACT_TAR}" "branding/icons/app/side-bite/${asset}"
done

printf 'PASS\n'

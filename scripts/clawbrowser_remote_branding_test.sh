#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
REMOTE_SCRIPT="${REPO_ROOT}/scripts/clawbrowser_remote.sh"

fail() {
  printf 'FAIL: %s\n' "$*" >&2
  exit 1
}

assert_exists() {
  local path="$1"
  [[ -e "${path}" ]] || fail "expected path to exist: ${path}"
}

load_remote_functions() {
  local temp_script
  temp_script="$(mktemp)"
  trap 'rm -f "${temp_script}"' RETURN
  sed '/^main "\$@"$/d' "${REMOTE_SCRIPT}" >"${temp_script}"
  # shellcheck disable=SC1090
  source "${temp_script}"
}

run_branding_sync() {
  local tmp_dir
  tmp_dir="$(mktemp -d)"
  trap 'rm -rf "${tmp_dir}"' RETURN

  local project_dir="${tmp_dir}/project"
  local chromium_dir="${tmp_dir}/chromium"
  local icon_dir="${project_dir}/branding/icons/app/side-bite"
  local side_bite_svg="${project_dir}/clawbrowser/resources/side_bite.svg"
  local compile_record="${tmp_dir}/compile_car_args.txt"

  mkdir -p "${icon_dir}" \
           "$(dirname "${side_bite_svg}")" \
           "${chromium_dir}/src/tools/mac/icons" \
           "${chromium_dir}/src/chrome/app/theme/chromium/mac" \
           "${chromium_dir}/src/chrome/app/theme/default_100_percent/chromium"

  printf '<svg xmlns="http://www.w3.org/2000/svg"/>\n' >"${side_bite_svg}"

  for icon in app.icns \
              product_logo_16.png product_logo_22.png product_logo_24.png \
              product_logo_32.png product_logo_48.png product_logo_64.png \
              product_logo_128.png product_logo_256.png product_logo_512.png \
              product_logo_1024.png; do
    printf 'old %s\n' "${icon}" >"${icon_dir}/${icon}"
  done

  cat >"${icon_dir}/generate_assets.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
touch "${script_dir}/generate.called"
for icon in app.icns \
            product_logo_16.png product_logo_22.png product_logo_24.png \
            product_logo_32.png product_logo_48.png product_logo_64.png \
            product_logo_128.png product_logo_256.png product_logo_512.png \
            product_logo_1024.png; do
  printf 'generated %s\n' "${icon}" >"${script_dir}/${icon}"
done
EOF
  chmod +x "${icon_dir}/generate_assets.sh"

  cat >"${chromium_dir}/src/tools/mac/icons/compile_car.py" <<'PY'
import pathlib
import sys

record = pathlib.Path(sys.argv[2])
record.write_text("\n".join(sys.argv[1:]) + "\n", encoding="utf-8")
PY

  (
    PROJECT_DIR="${project_dir}"
    CHROMIUM_DIR="${chromium_dir}"
    load_remote_functions
    sync_chromium_branding_assets "${compile_record}"
  )

  assert_exists "${chromium_dir}/src/chrome/app/theme/chromium/mac/app.icns"
  assert_exists "${chromium_dir}/src/chrome/app/theme/chromium/product_logo_256.png"
  assert_exists "${chromium_dir}/src/chrome/app/theme/default_100_percent/chromium/product_logo_32.png"
  assert_exists "${chromium_dir}/src/chrome/app/theme/chromium/mac/Assets.xcassets/Contents.json"
  assert_exists "${chromium_dir}/src/chrome/app/theme/chromium/mac/Assets.xcassets/AppIcon.appiconset/Contents.json"
  assert_exists "${chromium_dir}/src/chrome/app/theme/chromium/mac/Assets.xcassets/AppIcon.appiconset/appicon_1024.png"
  assert_exists "${chromium_dir}/src/chrome/app/theme/chromium/mac/Assets.xcassets/Icon.iconset/icon_256x256.png"
  assert_exists "${compile_record}"
  assert_exists "${icon_dir}/generate.called"

  grep -Fq 'chrome/app/theme/chromium/mac/Assets.xcassets' "${compile_record}" || \
    fail "expected compile_car.py to receive the Chromium asset catalog path"
  grep -Fq 'generated product_logo_256.png' \
    "${chromium_dir}/src/chrome/app/theme/chromium/product_logo_256.png" || \
    fail "expected Chromium branding sync to use regenerated product_logo_256.png"
}

run_branding_sync

printf 'PASS\n'

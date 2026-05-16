#!/usr/bin/env bash

set -euo pipefail

usage() {
  cat <<'EOF'
Usage: scripts/sign-and-notarize.sh [options]

Sign macOS build artifacts, build a signed installer package, notarize it, and staple it.

Options:
  --build                     Run cmake configure + build before signing.
  --build-preset NAME         CMake preset to configure/build. Default: macos-make
  --build-dir PATH            Build directory containing the out/ folder.
                              Default: build/<build-preset>
  --application-identity NAME Developer ID Application identity.
  --installer-identity NAME   Developer ID Installer identity.
  --keychain PATH             Keychain to search for signing identities.
  --apple-id VALUE            Apple ID for notarytool.
  --team-id VALUE             Apple Developer Team ID.
  --password VALUE            App-specific password for notarytool.
  --output PATH               Final .pkg path.
  --skip-notarize             Stop after building the signed .pkg.
  -h, --help                  Show this help.

Environment fallbacks:
  MACOS_DEV_ID_APPLICATION
  MACOS_DEV_ID_INSTALLER
  MACOS_SIGNING_KEYCHAIN
  MACOS_NOTARY_APPLE_ID
  MACOS_NOTARY_TEAM_ID
  MACOS_NOTARY_APP_PASSWORD
EOF
}

die() {
  echo "error: $*" >&2
  exit 1
}

require_option_value() {
  local option_name="$1"
  local option_value="${2-}"
  [[ -n "${option_value}" ]] || die "missing value for ${option_name}"
  printf '%s\n' "${option_value}"
}

warn_gatekeeper_assessment() {
  local assessment_type="$1"
  local target="$2"

  set +e
  local output
  output="$(spctl --assess --type "${assessment_type}" --verbose=4 "${target}" 2>&1)"
  local status=$?
  set -e

  if [[ ${status} -eq 0 ]]; then
    echo "${output}"
    return 0
  fi

  echo "${output}"
  echo "warning: Gatekeeper rejected ${target} before notarization; continuing" >&2
}

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_requested=0
build_preset="macos-make"
build_dir=""
application_identity="${MACOS_DEV_ID_APPLICATION:-}"
installer_identity="${MACOS_DEV_ID_INSTALLER:-}"
signing_keychain="${MACOS_SIGNING_KEYCHAIN:-}"
apple_id="${MACOS_NOTARY_APPLE_ID:-}"
team_id="${MACOS_NOTARY_TEAM_ID:-}"
password="${MACOS_NOTARY_APP_PASSWORD:-}"
output_path=""
skip_notarize=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build)
      build_requested=1
      shift
      ;;
    --build-preset)
      build_preset="$(require_option_value "$1" "${2-}")"
      shift 2
      ;;
    --build-dir)
      build_dir="$(require_option_value "$1" "${2-}")"
      shift 2
      ;;
    --application-identity)
      application_identity="$(require_option_value "$1" "${2-}")"
      shift 2
      ;;
    --installer-identity)
      installer_identity="$(require_option_value "$1" "${2-}")"
      shift 2
      ;;
    --keychain)
      signing_keychain="$(require_option_value "$1" "${2-}")"
      shift 2
      ;;
    --apple-id)
      apple_id="$(require_option_value "$1" "${2-}")"
      shift 2
      ;;
    --team-id)
      team_id="$(require_option_value "$1" "${2-}")"
      shift 2
      ;;
    --password)
      password="$(require_option_value "$1" "${2-}")"
      shift 2
      ;;
    --output)
      output_path="$(require_option_value "$1" "${2-}")"
      shift 2
      ;;
    --skip-notarize)
      skip_notarize=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      die "unknown argument: $1"
      ;;
  esac
done

if [[ -z "${build_dir}" ]]; then
  build_dir="${repo_root}/build/${build_preset}"
elif [[ "${build_dir}" != /* ]]; then
  build_dir="${repo_root}/${build_dir}"
fi

out_dir="${build_dir}/out"
dist_dir="${build_dir}/dist"
mkdir -p "${dist_dir}"

[[ -n "${application_identity}" ]] || die "Developer ID Application identity is required"
[[ -n "${installer_identity}" ]] || die "Developer ID Installer identity is required"

if [[ "${build_requested}" -eq 1 ]]; then
  cmake --preset "${build_preset}"
  cmake --build --preset "${build_preset}"
fi

[[ -d "${out_dir}" ]] || die "build output directory not found: ${out_dir}"

entitlements="${repo_root}/RVRSE/installer/macos-entitlements.plist"
[[ -f "${entitlements}" ]] || die "entitlements file not found: ${entitlements}"

sign_bundle() {
  local target="$1"
  local use_entitlements="$2"
  local cmd=(
    codesign
    --force
    --sign "${application_identity}"
    --timestamp
    --options runtime
    --deep
  )

  if [[ -n "${signing_keychain}" ]]; then
    cmd+=(--keychain "${signing_keychain}")
  fi

  if [[ "${use_entitlements}" == "yes" ]]; then
    cmd+=(--entitlements "${entitlements}")
  fi

  cmd+=("${target}")

  echo "signing ${target}"
  xattr -cr "${target}"
  xcrun "${cmd[@]}"
  xcrun codesign --verify --deep --strict --verbose=2 "${target}"
}

[[ -e "${out_dir}/RVRSE.vst3" ]] && sign_bundle "${out_dir}/RVRSE.vst3" "no"
[[ -e "${out_dir}/RVRSE.component" ]] && sign_bundle "${out_dir}/RVRSE.component" "no"
[[ -e "${out_dir}/RVRSE.clap" ]] && sign_bundle "${out_dir}/RVRSE.clap" "no"

if [[ -e "${out_dir}/RVRSE.app" ]]; then
  sign_bundle "${out_dir}/RVRSE.app" "yes"
  warn_gatekeeper_assessment execute "${out_dir}/RVRSE.app"
fi

package_cmd=(
  "${repo_root}/scripts/build-macos-installer.sh"
  --build-dir "${build_dir}"
  --installer-identity "${installer_identity}"
)

if [[ -n "${signing_keychain}" ]]; then
  package_cmd+=(--keychain "${signing_keychain}")
fi

if [[ -n "${output_path}" ]]; then
  package_cmd+=(--output "${output_path}")
fi

"${package_cmd[@]}"

if [[ -z "${output_path}" ]]; then
  version="$(
    awk -F'"' '/#define PLUG_VERSION_STR/ { print $2; exit }' \
      "${repo_root}/RVRSE/config.h"
  )"
  output_path="${dist_dir}/RVRSE-${version}-macOS.pkg"
elif [[ "${output_path}" != /* ]]; then
  output_path="${repo_root}/${output_path}"
fi

warn_gatekeeper_assessment install "${output_path}"

if [[ "${skip_notarize}" -eq 1 ]]; then
  echo "skipped notarization: ${output_path}"
  exit 0
fi

[[ -n "${apple_id}" ]] || die "Apple ID is required for notarization"
[[ -n "${team_id}" ]] || die "Team ID is required for notarization"
[[ -n "${password}" ]] || die "App-specific password is required for notarization"

submit_log="${dist_dir}/notarytool-submit.json"
detail_log="${dist_dir}/notarytool-log.json"

set +e
xcrun notarytool submit "${output_path}" \
  --apple-id "${apple_id}" \
  --password "${password}" \
  --team-id "${team_id}" \
  --wait \
  --output-format json | tee "${submit_log}"
submit_status=${PIPESTATUS[0]}
set -e

submission_id="$(
  python3 - "${submit_log}" <<'PY'
import json
import pathlib
import sys

try:
    data = json.loads(pathlib.Path(sys.argv[1]).read_text())
except Exception:
    print("")
else:
    print(data.get("id", ""))
PY
)"

if [[ -n "${submission_id}" ]]; then
  set +e
  xcrun notarytool log "${submission_id}" "${detail_log}" \
    --apple-id "${apple_id}" \
    --password "${password}" \
    --team-id "${team_id}"
  log_status=$?
  set -e

  if [[ ${log_status} -ne 0 ]]; then
    echo "warning: failed to fetch notarization detail log for submission ${submission_id}" >&2
  fi
fi

if [[ ${submit_status} -ne 0 ]]; then
  if [[ -n "${submission_id}" ]]; then
    die "notarization failed; see ${submit_log} and ${detail_log}"
  fi
  die "notarization failed before a submission id was returned; see ${submit_log}"
fi

[[ -n "${submission_id}" ]] || die "notarytool did not return a submission id"

xcrun stapler staple -v "${output_path}"
xcrun stapler validate -v "${output_path}"
spctl --assess --type install --verbose=4 "${output_path}"

echo "notarized installer ready: ${output_path}"
echo "submission log: ${submit_log}"
echo "detail log: ${detail_log}"

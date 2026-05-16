#!/usr/bin/env bash

set -euo pipefail

usage() {
  cat <<'EOF'
Usage: scripts/build-macos-installer.sh [options]

Build a macOS .pkg installer from the current build outputs.

Options:
  --build-dir PATH           Build directory containing the out/ folder.
                             Default: build/macos-make
  --output PATH              Final .pkg path.
                             Default: <build-dir>/dist/RVRSE-<version>-macOS.pkg
  --installer-identity NAME  Developer ID Installer identity to sign the final .pkg.
  --keychain PATH            Keychain to search for the installer identity.
  --examples-dir PATH        Optional source directory whose contents will be
                             bundled into /Library/Application Support/RVRSE/Examples
  -h, --help                 Show this help.

Environment fallbacks:
  MACOS_DEV_ID_INSTALLER
  MACOS_SIGNING_KEYCHAIN
  RVRSE_INSTALLER_EXAMPLES_DIR
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

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${repo_root}/build/macos-make"
output_path=""
installer_identity="${MACOS_DEV_ID_INSTALLER:-}"
signing_keychain="${MACOS_SIGNING_KEYCHAIN:-}"
examples_dir="${RVRSE_INSTALLER_EXAMPLES_DIR:-${repo_root}/RVRSE/installer/examples}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      build_dir="$(require_option_value "$1" "${2-}")"
      shift 2
      ;;
    --output)
      output_path="$(require_option_value "$1" "${2-}")"
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
    --examples-dir)
      examples_dir="$(require_option_value "$1" "${2-}")"
      shift 2
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

if [[ "${build_dir}" != /* ]]; then
  build_dir="${repo_root}/${build_dir}"
fi

out_dir="${build_dir}/out"
[[ -d "${out_dir}" ]] || die "build output directory not found: ${out_dir}"

version="$(
  awk -F'"' '/#define PLUG_VERSION_STR/ { print $2; exit }' \
    "${repo_root}/RVRSE/config.h"
)"
[[ -n "${version}" ]] || die "could not determine version from RVRSE/config.h"

if [[ -z "${output_path}" ]]; then
  output_path="${build_dir}/dist/RVRSE-${version}-macOS.pkg"
fi
if [[ "${output_path}" != /* ]]; then
  output_path="${repo_root}/${output_path}"
fi

dist_dir="$(dirname "${output_path}")"
mkdir -p "${dist_dir}"

work_dir="$(mktemp -d "${TMPDIR:-/tmp}/rvrse-installer.XXXXXX")"
trap 'rm -rf "${work_dir}"' EXIT

payload_root="${work_dir}/payload-root"
package_path="${work_dir}/RVRSE-payload.pkg"
distribution_path="${work_dir}/distribution.xml"

mkdir -p "${payload_root}"

included_any=0

stage_bundle() {
  local bundle_name="$1"
  local install_root="$2"
  local source_path="${out_dir}/${bundle_name}"

  if [[ ! -e "${source_path}" ]]; then
    return
  fi

  mkdir -p "${payload_root}${install_root}"
  cp -R -L "${source_path}" "${payload_root}${install_root}/"
  included_any=1
  echo "staged ${bundle_name} -> ${install_root}"
}

stage_bundle "RVRSE.vst3" "/Library/Audio/Plug-Ins/VST3"
stage_bundle "RVRSE.component" "/Library/Audio/Plug-Ins/Components"
stage_bundle "RVRSE.clap" "/Library/Audio/Plug-Ins/CLAP"
stage_bundle "RVRSE.app" "/Applications"

if [[ -d "${examples_dir}" ]] && find "${examples_dir}" -mindepth 1 -print -quit >/dev/null 2>&1; then
  mkdir -p "${payload_root}/Library/Application Support/RVRSE/Examples"
  cp -R -L "${examples_dir}/." "${payload_root}/Library/Application Support/RVRSE/Examples/"
  included_any=1
  echo "staged example payload -> /Library/Application Support/RVRSE/Examples"
fi

[[ "${included_any}" -eq 1 ]] || die "no installable macOS artifacts were found in ${out_dir}"

pkgbuild_args=(
  pkgbuild
  --root "${payload_root}"
  --identifier "com.samufl.rvrse.pkg.payload"
  --version "${version}"
  "${package_path}"
)

xcrun "${pkgbuild_args[@]}"

license_xml=""
readme_xml=""
welcome_xml=""
background_xml=""

if [[ -f "${repo_root}/RVRSE/installer/license.rtf" ]]; then
  license_xml='    <license file="license.rtf" mime-type="application/rtf"/>'
fi
if [[ -f "${repo_root}/RVRSE/installer/readme-mac.rtf" ]]; then
  readme_xml='    <readme file="readme-mac.rtf" mime-type="application/rtf"/>'
fi
if [[ -f "${repo_root}/RVRSE/installer/intro.rtf" ]]; then
  welcome_xml='    <welcome file="intro.rtf" mime-type="application/rtf"/>'
fi
if [[ -f "${repo_root}/RVRSE/installer/RVRSE-installer-bg.png" ]]; then
  background_xml='    <background file="RVRSE-installer-bg.png" alignment="topleft" scaling="none"/>'
fi

cat > "${distribution_path}" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>RVRSE ${version}</title>
${license_xml}
${readme_xml}
${welcome_xml}
${background_xml}
    <options customize="never" require-scripts="false"/>
    <domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>
    <choices-outline>
        <line choice="default"/>
    </choices-outline>
    <choice id="default" title="RVRSE">
        <pkg-ref id="com.samufl.rvrse.pkg.payload"/>
    </choice>
    <pkg-ref id="com.samufl.rvrse.pkg.payload" version="${version}" onConclusion="none">$(basename "${package_path}")</pkg-ref>
</installer-gui-script>
EOF

rm -f "${output_path}"

productbuild_args=(
  productbuild
  --distribution "${distribution_path}"
  --package-path "${work_dir}"
  --resources "${repo_root}/RVRSE/installer"
)

if [[ -n "${installer_identity}" ]]; then
  productbuild_args+=(--sign "${installer_identity}" --timestamp)
fi

if [[ -n "${signing_keychain}" ]]; then
  productbuild_args+=(--keychain "${signing_keychain}")
fi

productbuild_args+=("${output_path}")

xcrun "${productbuild_args[@]}"

echo "created installer: ${output_path}"

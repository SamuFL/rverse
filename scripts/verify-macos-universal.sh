#!/usr/bin/env bash

set -euo pipefail

usage() {
  cat <<'EOF'
Usage: scripts/verify-macos-universal.sh --build-dir PATH [--package PATH]

Verify that RVRSE macOS build outputs contain arm64 and x86_64 slices.
When --package is provided, expand the installer and verify its payload too.
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
build_dir=""
package_path=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      build_dir="$(require_option_value "$1" "${2-}")"
      shift 2
      ;;
    --package)
      package_path="$(require_option_value "$1" "${2-}")"
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

[[ -n "${build_dir}" ]] || die "--build-dir is required"
if [[ "${build_dir}" != /* ]]; then
  build_dir="${repo_root}/${build_dir}"
fi
[[ -d "${build_dir}" ]] || die "build directory not found: ${build_dir}"

if [[ -n "${package_path}" && "${package_path}" != /* ]]; then
  package_path="${repo_root}/${package_path}"
fi

assert_universal() {
  local binary_path="$1"
  [[ -f "${binary_path}" ]] || die "Mach-O binary not found: ${binary_path}"

  local architectures
  architectures="$(lipo -archs "${binary_path}")"
  for required_architecture in arm64 x86_64; do
    [[ " ${architectures} " == *" ${required_architecture} "* ]] ||
      die "${binary_path} is missing ${required_architecture}; found: ${architectures}"
  done

  local x86_minimum
  local arm_minimum
  x86_minimum="$(vtool -arch x86_64 -show-build "${binary_path}" | awk '/minos/ { print $2; exit }')"
  arm_minimum="$(vtool -arch arm64 -show-build "${binary_path}" | awk '/minos/ { print $2; exit }')"
  [[ "${x86_minimum}" == "10.15" ]] ||
    die "${binary_path} has unexpected x86_64 minimum macOS ${x86_minimum}"
  [[ "${arm_minimum}" == "11.0" ]] ||
    die "${binary_path} has unexpected arm64 minimum macOS ${arm_minimum}"

  echo "universal (${architectures}; x86_64 >= 10.15, arm64 >= 11.0): ${binary_path}"
}

assert_universal "${build_dir}/out/RVRSE.vst3/Contents/MacOS/RVRSE"
assert_universal "${build_dir}/out/RVRSE.component/Contents/MacOS/RVRSE"
assert_universal "${build_dir}/out/RVRSE.clap/Contents/MacOS/RVRSE"
assert_universal "${build_dir}/out/RVRSE.app/Contents/MacOS/RVRSE"
assert_universal "${build_dir}/tests/rvrse_tests"

if [[ -z "${package_path}" ]]; then
  exit 0
fi

[[ -f "${package_path}" ]] || die "installer package not found: ${package_path}"
expanded_parent="$(mktemp -d "${TMPDIR:-/tmp}/rvrse-package.XXXXXX")"
expanded_package="${expanded_parent}/expanded"
trap 'rm -rf "${expanded_parent}"' EXIT
pkgutil --expand-full "${package_path}" "${expanded_package}"

assert_single_payload_binary() {
  local suffix="$1"
  local matches=()
  local match

  while IFS= read -r -d '' match; do
    matches+=("${match}")
  done < <(find "${expanded_package}" -type f -path "*${suffix}" -print0)

  [[ ${#matches[@]} -eq 1 ]] ||
    die "expected one package payload binary ending in ${suffix}; found ${#matches[@]}"
  assert_universal "${matches[0]}"
}

assert_single_payload_binary "/Library/Audio/Plug-Ins/VST3/RVRSE.vst3/Contents/MacOS/RVRSE"
assert_single_payload_binary "/Library/Audio/Plug-Ins/Components/RVRSE.component/Contents/MacOS/RVRSE"
assert_single_payload_binary "/Library/Audio/Plug-Ins/CLAP/RVRSE.clap/Contents/MacOS/RVRSE"
assert_single_payload_binary "/Applications/RVRSE.app/Contents/MacOS/RVRSE"

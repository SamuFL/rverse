#!/usr/bin/env bash

set -euo pipefail

usage() {
  cat <<'EOF'
Usage: scripts/validate-macos-au.sh [--architecture native|x86_64]

Validate the RVRSE Audio Unit, retrying while macOS registers the component.
EOF
}

die() {
  echo "error: $*" >&2
  exit 1
}

architecture="native"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --architecture)
      [[ $# -ge 2 ]] || die "missing value for $1"
      architecture="$2"
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

case "${architecture}" in
  native)
    auval_command=(/usr/bin/auval)
    ;;
  x86_64)
    auval_command=(arch -x86_64 /usr/bin/auval)
    ;;
  *)
    die "unsupported architecture: ${architecture}"
    ;;
esac

maximum_attempts=10
retry_delay_seconds=2

for ((attempt = 1; attempt <= maximum_attempts; attempt++)); do
  set +e
  output="$("${auval_command[@]}" -v aumu 5SpI SmFL 2>&1)"
  status=$?
  set -e
  printf '%s\n' "${output}"

  if [[ ${status} -eq 0 ]]; then
    exit 0
  fi

  if [[ "${output}" != *"didn't find the component"* ]]; then
    exit "${status}"
  fi

  if [[ ${attempt} -eq ${maximum_attempts} ]]; then
    die "Audio Unit was not discovered after ${maximum_attempts} attempts"
  fi

  echo "Audio Unit not registered yet; retrying in ${retry_delay_seconds}s (${attempt}/${maximum_attempts})"
  sleep "${retry_delay_seconds}"
done

#!/usr/bin/env python3
"""Sync the canonical version from RVRSE/config.h to all satellite files.

Reads PLUG_VERSION_STR and PLUG_VERSION_HEX from config.h and propagates
them to plist files, the Inno Setup installer script, and CMakeLists.txt.

Usage:
    python3 scripts/sync-version.py          # apply updates
    python3 scripts/sync-version.py --check  # exit 1 if anything is out of sync
"""

import argparse
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
CONFIG_H = REPO_ROOT / "RVRSE" / "config.h"
CMAKELISTS = REPO_ROOT / "RVRSE" / "CMakeLists.txt"
ISS_FILE = REPO_ROOT / "RVRSE" / "installer" / "RVRSE.iss"
PLIST_DIR = REPO_ROOT / "RVRSE" / "resources"


def read_config_version() -> str:
    """Extract PLUG_VERSION_STR from config.h."""
    text = CONFIG_H.read_text()
    m = re.search(r'#define\s+PLUG_VERSION_STR\s+"([^"]+)"', text)
    if not m:
        sys.exit("ERROR: PLUG_VERSION_STR not found in config.h")
    return m.group(1)


def expected_hex(version: str) -> str:
    """Convert 'M.N.P' to '0xMMMMNNPP' hex string matching iPlug2 convention."""
    parts = version.split(".")
    if len(parts) != 3:
        sys.exit(f"ERROR: version '{version}' is not in M.N.P format")
    major, minor, patch = (int(p) for p in parts)
    val = (major << 16) | (minor << 8) | patch
    return f"0x{val:08x}"


def check_config_hex(version: str) -> None:
    """Verify PLUG_VERSION_HEX matches PLUG_VERSION_STR.

    A mismatch is a hard error because this script does not rewrite config.h,
    so continuing would leave the canonical source of truth out of sync while
    other satellite files may have been updated.
    """
    text = CONFIG_H.read_text()
    m = re.search(r"#define\s+PLUG_VERSION_HEX\s+(0x[0-9a-fA-F]+)", text)
    if not m:
        sys.exit("ERROR: config.h: PLUG_VERSION_HEX not found")
    actual = m.group(1).lower()
    want = expected_hex(version)
    if actual != want:
        sys.exit(f"ERROR: config.h: PLUG_VERSION_HEX is {actual}, expected {want}")


def sync_plists(version: str, check_only: bool) -> list[str]:
    """Update CFBundleShortVersionString and CFBundleVersion in all plists."""
    diffs = []
    plist_files = sorted(PLIST_DIR.glob("*.plist"))
    # Regex matches the <string>...</string> line after a version key
    ver_pattern = re.compile(
        r"(<key>CFBundle(?:ShortVersionString|Version)</key>\s*\n\s*<string>)"
        r"([^<]+)"
        r"(</string>)"
    )
    for plist in plist_files:
        text = plist.read_text()
        matches = ver_pattern.findall(text)
        for _prefix, val, _suffix in matches:
            if val.strip() != version:
                rel = plist.relative_to(REPO_ROOT)
                diffs.append(f"{rel}: '{val.strip()}' != '{version}'")
        if not check_only:
            new_text = ver_pattern.sub(rf"\g<1>{version}\3", text)
            if new_text != text:
                plist.write_text(new_text)
    return diffs


def sync_iss(version: str, check_only: bool) -> list[str]:
    """Update AppVersion and VersionInfoVersion in RVRSE.iss."""
    if not ISS_FILE.exists():
        return []
    diffs = []
    text = ISS_FILE.read_text()
    for key in ("AppVersion", "VersionInfoVersion"):
        pattern = re.compile(rf"^({key}=)(.+)$", re.MULTILINE)
        m = pattern.search(text)
        if m and m.group(2) != version:
            rel = ISS_FILE.relative_to(REPO_ROOT)
            diffs.append(f"{rel}: {key}={m.group(2)}, expected {version}")
            if not check_only:
                text = pattern.sub(rf"\g<1>{version}", text)
    if not check_only:
        ISS_FILE.write_text(text)
    return diffs


def sync_cmake(version: str, check_only: bool) -> list[str]:
    """Update project(RVRSE VERSION x.y.z) in CMakeLists.txt."""
    diffs = []
    text = CMAKELISTS.read_text()
    pattern = re.compile(r"(project\s*\(\s*RVRSE\s+VERSION\s+)([\d.]+)(\s*\))")
    m = pattern.search(text)
    if m and m.group(2) != version:
        rel = CMAKELISTS.relative_to(REPO_ROOT)
        diffs.append(f"{rel}: VERSION {m.group(2)}, expected {version}")
        if not check_only:
            text = pattern.sub(rf"\g<1>{version}\3", text)
            CMAKELISTS.write_text(text)
    return diffs


def main():
    parser = argparse.ArgumentParser(description="Sync version from config.h")
    parser.add_argument(
        "--check",
        action="store_true",
        help="Check mode: exit 1 if files are out of sync, don't modify anything",
    )
    args = parser.parse_args()

    version = read_config_version()
    print(f"Canonical version: {version}")

    check_config_hex(version)

    all_diffs: list[str] = []
    all_diffs.extend(sync_plists(version, args.check))
    all_diffs.extend(sync_iss(version, args.check))
    all_diffs.extend(sync_cmake(version, args.check))

    if all_diffs:
        label = "Out of sync" if args.check else "Fixed"
        for d in all_diffs:
            print(f"  {label}: {d}")
        if args.check:
            print(f"\n{len(all_diffs)} file(s) out of sync. Run: python3 scripts/sync-version.py")
            sys.exit(1)
        else:
            print(f"\nUpdated {len(all_diffs)} value(s).")
    else:
        print("All files in sync.")


if __name__ == "__main__":
    main()

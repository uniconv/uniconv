#!/usr/bin/env python3
"""
Release the uniconv core CLI.

Bumps the version in CMakeLists.txt, commits, tags, and pushes to trigger
the CI release workflow (.github/workflows/release.yml).

Usage:
    python release.py patch              # 0.1.0 -> 0.1.1
    python release.py minor              # 0.1.0 -> 0.2.0
    python release.py major              # 0.1.0 -> 1.0.0
    python release.py 2.0.0              # explicit version
    python release.py patch --dry-run    # show what would happen
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
CMAKELISTS = SCRIPT_DIR / "CMakeLists.txt"


def die(msg: str):
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def git(*args: str) -> str:
    result = subprocess.run(
        ["git", "-C", str(SCRIPT_DIR), *args],
        capture_output=True, text=True
    )
    if result.returncode != 0:
        die(f"git {' '.join(args)} failed:\n{result.stderr.strip()}")
    return result.stdout.strip()


def get_current_version() -> str:
    text = CMAKELISTS.read_text()
    m = re.search(r"project\(uniconv VERSION (\d+\.\d+\.\d+)", text)
    if not m:
        die(f"Could not extract version from {CMAKELISTS}")
    return m.group(1)


def bump_version(current: str, bump: str) -> str:
    major, minor, patch = (int(x) for x in current.split("."))
    if bump == "major":
        return f"{major + 1}.0.0"
    elif bump == "minor":
        return f"{major}.{minor + 1}.0"
    elif bump == "patch":
        return f"{major}.{minor}.{patch + 1}"
    else:
        die(f"Invalid bump type: {bump}")


def validate_version(version: str):
    if not re.match(r"^\d+\.\d+\.\d+$", version):
        die(f"Invalid version format: {version} (expected X.Y.Z)")


def is_version(s: str) -> bool:
    return bool(re.match(r"^\d+\.\d+\.\d+$", s))


def main():
    parser = argparse.ArgumentParser(description="Release the uniconv core CLI")
    parser.add_argument("bump", help="patch, minor, major, or explicit X.Y.Z")
    parser.add_argument("--dry-run", action="store_true", help="Show what would happen")
    args = parser.parse_args()

    if not CMAKELISTS.is_file():
        die(f"CMakeLists.txt not found: {CMAKELISTS}")

    current_version = get_current_version()

    if is_version(args.bump):
        new_version = args.bump
    else:
        new_version = bump_version(current_version, args.bump)

    validate_version(new_version)
    if new_version == current_version:
        die(f"New version is the same as current ({current_version})")

    tag = f"v{new_version}"
    dry_run = args.dry_run

    print("=== uniconv release ===")
    print(f"  Current version: {current_version}")
    print(f"  New version:     {new_version}")
    print(f"  Tag:             {tag}")
    print()

    # --- Step 1: Update CMakeLists.txt ---
    print("--- Step 1: Update CMakeLists.txt ---")
    if dry_run:
        print(f"  [dry-run] Would update VERSION {current_version} -> {new_version} in CMakeLists.txt")
    else:
        text = CMAKELISTS.read_text()
        text = text.replace(
            f"project(uniconv VERSION {current_version}",
            f"project(uniconv VERSION {new_version}"
        )
        CMAKELISTS.write_text(text)
        print(f"  Updated VERSION to {new_version}")
    print()

    # --- Step 2: Commit ---
    print("--- Step 2: Commit ---")
    if dry_run:
        print(f"  [dry-run] Would commit: chore: bump version to {tag}")
    else:
        git("add", "CMakeLists.txt")
        git("commit", "-m", f"chore: bump version to {tag}")
        print("  Committed.")
    print()

    # --- Step 3: Tag ---
    print("--- Step 3: Tag ---")
    if dry_run:
        print(f"  [dry-run] Would create annotated tag: {tag}")
    else:
        git("tag", "-a", tag, "-m", f"Release {tag}")
        print(f"  Tagged: {tag}")
    print()

    # --- Step 4: Push ---
    print("--- Step 4: Push ---")
    if dry_run:
        print(f"  [dry-run] Would push commit and tag to origin")
    else:
        git("push", "origin", "HEAD", tag)
        print("  Pushed commit and tag.")
    print()

    print(f"=== Done: uniconv {tag} released ===")


if __name__ == "__main__":
    main()

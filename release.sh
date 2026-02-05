#!/bin/bash
set -euo pipefail

# Release the uniconv core CLI.
#
# Bumps the version in CMakeLists.txt, commits, tags, and pushes to trigger
# the CI release workflow (.github/workflows/release.yml).
#
# Usage:
#   ./release.sh patch              # 0.1.0 → 0.1.1
#   ./release.sh minor              # 0.1.0 → 0.2.0
#   ./release.sh major              # 0.1.0 → 1.0.0
#   ./release.sh 2.0.0              # explicit version
#   ./release.sh patch --dry-run    # show what would happen

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CMAKELISTS="$SCRIPT_DIR/CMakeLists.txt"
DRY_RUN=false

# --- Helpers ---

die() { echo "ERROR: $*" >&2; exit 1; }

usage() {
    echo "Usage: $0 <patch|minor|major|X.Y.Z> [--dry-run]"
    exit 1
}

# Extract current version from CMakeLists.txt
get_current_version() {
    local ver
    ver=$(sed -n 's/^project(uniconv VERSION \([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\).*/\1/p' "$CMAKELISTS")
    [[ -n "$ver" ]] || die "Could not extract version from $CMAKELISTS"
    echo "$ver"
}

# Compute the next version given a bump type
bump_version() {
    local current="$1" bump="$2"
    local major minor patch
    IFS='.' read -r major minor patch <<< "$current"

    case "$bump" in
        major) echo "$((major + 1)).0.0" ;;
        minor) echo "${major}.$((minor + 1)).0" ;;
        patch) echo "${major}.${minor}.$((patch + 1))" ;;
        *) die "Invalid bump type: $bump" ;;
    esac
}

# Validate that a string looks like a semver version
validate_version() {
    [[ "$1" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || die "Invalid version format: $1 (expected X.Y.Z)"
}

# --- Parse arguments ---

[[ $# -ge 1 ]] || usage

BUMP_OR_VERSION="$1"
shift
while [[ $# -gt 0 ]]; do
    case "$1" in
        --dry-run) DRY_RUN=true ;;
        *) die "Unknown option: $1" ;;
    esac
    shift
done

# --- Determine versions ---

[[ -f "$CMAKELISTS" ]] || die "CMakeLists.txt not found: $CMAKELISTS"

CURRENT_VERSION=$(get_current_version)

if [[ "$BUMP_OR_VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    NEW_VERSION="$BUMP_OR_VERSION"
else
    NEW_VERSION=$(bump_version "$CURRENT_VERSION" "$BUMP_OR_VERSION")
fi

validate_version "$NEW_VERSION"

[[ "$NEW_VERSION" != "$CURRENT_VERSION" ]] || die "New version is the same as current ($CURRENT_VERSION)"

TAG="v${NEW_VERSION}"

echo "=== uniconv release ==="
echo "  Current version: $CURRENT_VERSION"
echo "  New version:     $NEW_VERSION"
echo "  Tag:             $TAG"
echo ""

# --- Step 1: Update CMakeLists.txt ---

echo "--- Step 1: Update CMakeLists.txt ---"
if $DRY_RUN; then
    echo "  [dry-run] Would update VERSION $CURRENT_VERSION → $NEW_VERSION in CMakeLists.txt"
else
    if [[ "$OSTYPE" == "darwin"* ]]; then
        sed -i '' "s/project(uniconv VERSION $CURRENT_VERSION/project(uniconv VERSION $NEW_VERSION/" "$CMAKELISTS"
    else
        sed -i "s/project(uniconv VERSION $CURRENT_VERSION/project(uniconv VERSION $NEW_VERSION/" "$CMAKELISTS"
    fi
    echo "  Updated VERSION to $NEW_VERSION"
fi
echo ""

# --- Step 2: Commit ---

echo "--- Step 2: Commit ---"
if $DRY_RUN; then
    echo "  [dry-run] Would commit: chore: bump version to $TAG"
else
    git -C "$SCRIPT_DIR" add CMakeLists.txt
    git -C "$SCRIPT_DIR" commit -m "chore: bump version to $TAG"
    echo "  Committed."
fi
echo ""

# --- Step 3: Tag ---

echo "--- Step 3: Tag ---"
if $DRY_RUN; then
    echo "  [dry-run] Would create annotated tag: $TAG"
else
    git -C "$SCRIPT_DIR" tag -a "$TAG" -m "Release $TAG"
    echo "  Tagged: $TAG"
fi
echo ""

# --- Step 4: Push ---

echo "--- Step 4: Push ---"
if $DRY_RUN; then
    echo "  [dry-run] Would push commit and tag to origin"
else
    git -C "$SCRIPT_DIR" push origin HEAD "$TAG"
    echo "  Pushed commit and tag."
fi
echo ""

echo "=== Done: uniconv $TAG released ==="

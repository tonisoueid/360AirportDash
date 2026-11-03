#!/usr/bin/env bash
# ----------------------------------------------------------------------------
# Rename the template's application identity after "Use this template".
#
#   scripts/rename-project.sh NewName [lowercase-id]
#
# NewName        project/target name, e.g. FleetConsole or 360Console
#                (letters, digits, _ and -; may start with a digit — the
#                extraction-origin app itself was a digit-leading target)
# lowercase-id   optional; defaults to NewName lowercased. Used for the
#                .desktop file, icon name, Wayland app id and config dir.
#
# Replaces the two identity tokens across every tracked text file:
#   360AirportDash  -> NewName        (CMake project/target, window title,
#                                         log source, packaging artifacts)
#   360airportdash  -> lowercase-id   (.desktop file + Icon=, StartupWMClass,
#                                         desktopFileName, Docker image names)
# then renames packaging/linux/<id>.desktop to match. Re-run any time; it is
# idempotent per name pair.
#
# With --no-verify it skips the build check. Otherwise it rebuilds and runs
# an offscreen --grab smoke test so a broken rename never lands silently.
# ----------------------------------------------------------------------------
set -euo pipefail
cd "$(dirname "$0")/.."

OLD_PC=360AirportDash
OLD_LC=360airportdash

NEW_PC="${1:?usage: scripts/rename-project.sh NewName [lowercase-id]}"
VERIFY=1
if [ "${1:-}" = "--no-verify" ]; then VERIFY=0; shift; NEW_PC="${1:?usage: scripts/rename-project.sh NewName [lowercase-id]}"; fi
if [ "${2:-}" = "--no-verify" ]; then VERIFY=0; fi
NEW_LC="${2:-}"

if ! [[ "$NEW_PC" =~ ^[A-Za-z0-9][A-Za-z0-9_-]*$ ]]; then
    echo "error: NewName may contain only letters/digits/_/- (and may start with a digit)" >&2
    exit 1
fi
if [ -z "$NEW_LC" ]; then
    NEW_LC=$(printf '%s' "$NEW_PC" | tr '[:upper:]' '[:lower:]')
fi
if [ "$NEW_PC" = "$OLD_PC" ] && [ "$NEW_LC" = "$OLD_LC" ]; then
    echo "already named $NEW_PC — nothing to do"
    exit 0
fi

# tracked files; fall back to a text-file walk in a fresh, uncommitted clone
mapfile -t FILES < <(git ls-files 2>/dev/null | sort -u || true)
if [ ${#FILES[@]} -eq 0 ]; then
    mapfile -t FILES < <(find . -type f \
        -not -path './.git/*' -not -path './build/*' -not -path './dist/*' \
        -not -name '*.png' -not -name '*.zip' -not -name '*.tar.gz')
fi

count=0
for f in "${FILES[@]}"; do
    [ -f "$f" ] || continue
    if grep -q -e "$OLD_PC" -e "$OLD_LC" -- "$f"; then
        sed -i -e "s/$OLD_PC/$NEW_PC/g" -e "s/$OLD_LC/$NEW_LC/g" -- "$f"
        count=$((count + 1))
        echo "  renamed: $f"
    fi
done

if [ -f "packaging/linux/$OLD_LC.desktop" ]; then
    mv "packaging/linux/$OLD_LC.desktop" "packaging/linux/$NEW_LC.desktop"
    echo "  renamed: packaging/linux/$OLD_LC.desktop -> $NEW_LC.desktop"
fi

echo "identity: $OLD_PC/$OLD_LC -> $NEW_PC/$NEW_LC ($count files)"

if [ "$VERIFY" = 1 ]; then
    echo "verifying: fresh build + offscreen grab…"
    cmake -B build -DCMAKE_BUILD_TYPE=Release >/dev/null
    cmake --build build -j >/dev/null
    QT_QPA_PLATFORM=offscreen timeout 60 "./build/$NEW_PC" --offline --grab /tmp/rename-verify.png
    [ -s /tmp/rename-verify.png ] && echo "verify OK (frame rendered)"
fi

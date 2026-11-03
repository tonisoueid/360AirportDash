#!/usr/bin/env bash
# ----------------------------------------------------------------------------
# Render the app icon set from the app's own painter (src/AppIcon.h) with a
# host Qt 6 (Gui) + CMake + Ninja harness.
#
#   packaging/common/gen-icons.sh OUTDIR
#
# Writes OUTDIR/icon-{16,24,32,48,64,128,256,512}.png and OUTDIR/icon.ico
# (multi-size, PNG-compressed entries: 16/24/32/48/64/128/256).
# ----------------------------------------------------------------------------
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT=$(realpath -m "${1:?usage: gen-icons.sh OUTDIR}")
mkdir -p "$OUT"
H=$(mktemp -d)
trap 'rm -rf "$H"' EXIT
cat > "$H/main.cpp" <<'EOF'
#include "AppIcon.h"
#include <QGuiApplication>
int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    for (int px : {16, 24, 32, 48, 64, 128, 256, 512})
        if (!AppIcon::pixmap(px).save(QString("%1/icon-%2.png").arg(argv[1], QString::number(px))))
            return 1;
    return 0;
}
EOF
cat > "$H/CMakeLists.txt" <<EOF
cmake_minimum_required(VERSION 3.21)
project(iconharness LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
find_package(Qt6 REQUIRED COMPONENTS Gui)
add_executable(iconharness main.cpp)
target_include_directories(iconharness PRIVATE "$ROOT/src")
target_link_libraries(iconharness PRIVATE Qt6::Gui)
EOF
cmake -S "$H" -B "$H/b" -GNinja >/dev/null
cmake --build "$H/b" >/dev/null
QT_QPA_PLATFORM=offscreen "$H/b/iconharness" "$OUT"
python3 - "$OUT" <<'EOF'
import struct, sys
d = sys.argv[1]
sizes = [16, 24, 32, 48, 64, 128, 256]
pngs = [open(f'{d}/icon-{s}.png', 'rb').read() for s in sizes]
head = struct.pack('<HHH', 0, 1, len(sizes))
off = 6 + 16 * len(sizes)
entries = b''
for s, png in zip(sizes, pngs):
    entries += struct.pack('<BBBBHHII', s % 256, s % 256, 0, 0, 1, 32, len(png), off)
    off += len(png)
open(f'{d}/icon.ico', 'wb').write(head + entries + b''.join(pngs))
EOF
echo "   icon set ready in $OUT"

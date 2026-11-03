#!/usr/bin/env bash
# ----------------------------------------------------------------------------
# Cross-build DashboardTemplate.exe for Windows 10/11 x64 — from Linux, via Docker.
#
#   ./packaging/windows/build-win64.sh
#
# Uses Fedora's prebuilt mingw-w64 Qt 6 packages (same 6.10.x series as the
# Linux build). Linux moc/rcc host tools run natively — no Wine needed at
# build time. Produces:
#
#   dist/DashboardTemplate-0.1.0-win64/      runnable folder (exe + Qt DLLs + plugins)
#   dist/DashboardTemplate-0.1.0-win64.zip   the shippable artifact (~40 MB)
#
# DLL deployment is a fixpoint pass: every non-system DLL import of the exe,
# Qt libs and plugins is copied from the Fedora sysroot until the closure
# resolves. Verified by Wine smoke test (see "Verifying" below).
#
# Verifying without a Windows box (optional):
#   docker run --rm -v $PWD/dist:/dist:ro -v /tmp/wt:/t ubuntu:26.04 bash -c '
#     apt-get update -qq && apt-get install -y -qq wine64 file ca-certificates
#     export WINEPREFIX=/t/wp WINEDEBUG=-all
#     cp -r /dist/DashboardTemplate-0.1.0-win64 /t/app && cd /t/app && wineboot --init
#     # fresh prefix + system CAs => schannel TLS works once a feed is wired:
#     wine DashboardTemplate.exe --grab Z:\\t\\frame.png'
# ----------------------------------------------------------------------------
set -euo pipefail
cd "$(dirname "$0")/../.."

VER=0.1.0
IMAGE=dashboardtemplate-win-cross:latest

# Taskbar/Explorer icon: rendered on the host from the app's own AppIcon painter.
ICONS=$(mktemp -d)
trap 'rm -rf "$ICONS"' EXIT
packaging/common/gen-icons.sh "$ICONS"

docker build -q -t "$IMAGE" - <<'EOF'
FROM fedora:43
RUN dnf -y --setopt=install_weak_deps=False install \
      cmake ninja-build zip mingw64-gcc-c++ mingw64-qt6-qtbase \
 && dnf clean all
EOF

docker run --rm \
  -v "$PWD:/src:ro" \
  -v "$PWD/dist:/out" \
  -v "$ICONS:/icons:ro" \
  "$IMAGE" bash -euxo pipefail -c '
SYS=/usr/x86_64-w64-mingw32/sys-root/mingw
D=/out/DashboardTemplate-'"$VER"'-win64

cmake -S /src -B /tmp/b -GNinja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=/usr/share/mingw/toolchain-mingw64.cmake \
      -DAPP_ICON_ICO=/icons/icon.ico
cmake --build /tmp/b

rm -rf "$D"
mkdir -p "$D"/{platforms,tls,networkinformation,styles}
cp /tmp/b/DashboardTemplate.exe "$D/"
cp "$SYS"/bin/Qt6{Core,Gui,Network,Widgets}.dll "$D/"
cp "$SYS"/lib/qt6/plugins/platforms/qwindows.dll           "$D/platforms/"
cp "$SYS"/lib/qt6/plugins/tls/*.dll                        "$D/tls/"
cp "$SYS"/lib/qt6/plugins/networkinformation/*.dll         "$D/networkinformation/"
cp "$SYS"/lib/qt6/plugins/styles/qmodernwindowsstyle.dll   "$D/styles/"

# Windows system DLLs (case-insensitive); everything else must ship in $D.
is_system() { local d="${1%.*}"; case "${d,,}" in
  kernel32|user32|gdi32|shell32|ole32|oleaut32|comdlg32|comctl32|advapi32|\
  ws2_32|wsock32|crypt32|secur32|bcrypt|ncrypt|userenv|version|winmm|imm32|\
  msvcrt|ntdll|uxtheme|dwmapi|shcore|shlwapi|setupapi|dnsapi|iphlpapi|psapi|\
  rpcrt4|wintrust|dbghelp|winhttp|authz|netapi32|mpr|wtsapi32|d3d9|d3d10|\
  d3d11|d3d12|dxgi|opengl32|gdiplus|dwrite|api-ms-*) return 0 ;;
  *) return 1 ;; esac; }

# Fixpoint: copy any missing non-system dependency from the sysroot.
for _ in 1 2 3 4; do
  miss=0
  for f in "$D"/*.dll "$D"/{platforms,tls,networkinformation,styles}/*.dll "$D"/*.exe; do
    for dll in $(x86_64-w64-mingw32-objdump -p "$f" | sed -n "s/\s*DLL Name: //p"); do
      if ! is_system "$dll" && [ ! -f "$D/$dll" ]; then
        cp "$SYS/bin/$dll" "$D/"   # fails loudly if truly unresolvable
        miss=1
      fi
    done
  done
  [ "$miss" = 0 ] && break
done

# Final gate: fail the build if any non-system DLL is still unresolved.
for f in "$D"/*.dll "$D"/{platforms,tls,networkinformation,styles}/*.dll "$D"/*.exe; do
  for dll in $(x86_64-w64-mingw32-objdump -p "$f" | sed -n "s/\s*DLL Name: //p"); do
    is_system "$dll" || [ -f "$D/$dll" ] || { echo "UNRESOLVED: $dll (needed by $f)"; exit 1; }
  done
done

cd /out
rm -f DashboardTemplate-'"$VER"'-win64.zip
zip -qr DashboardTemplate-'"$VER"'-win64.zip DashboardTemplate-'"$VER"'-win64
du -sh DashboardTemplate-'"$VER"'-win64{,.zip}
'

echo "OK: dist/DashboardTemplate-$VER-win64.zip (unzip, then run DashboardTemplate.exe)"

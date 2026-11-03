#!/usr/bin/env bash
# ----------------------------------------------------------------------------
# Build a self-contained Linux x86_64 package of DashboardTemplate — via Docker.
#
#   ./packaging/linux/build-linux.sh
#
# Builds in a Fedora 43 container (Qt 6.10.x, same series as the Windows
# plugins next to the executable. Produces:
#
#   dist/DashboardTemplate-0.1.0-linux-x86_64/      runnable folder
#   dist/DashboardTemplate-0.1.0-linux-x86_64.tar.gz  the shippable artifact
#
# Layout:  DashboardTemplate (launcher) · bin/ (exe + qt.conf) · lib/ · plugins/
#          share/ (hicolor icons + .desktop) · install-desktop.sh (dock icon)
# The launcher sets LD_LIBRARY_PATH to lib/, so no patchelf / post-link edits.
#
# Bundle policy: the shared-library closure is copied EXCEPT glibc and the
# GPU/GL stack (libGL, libEGL, libdrm, libgbm, ...), which must match the host.
# Because the bundle is built against Fedora 43's glibc, the target needs
# glibc >= 2.42 (e.g. Fedora 43+, Ubuntu 26.04+). The archive is smoke-tested
# in the container (offscreen --offline --grab) before it is written.
# ----------------------------------------------------------------------------
set -euo pipefail
cd "$(dirname "$0")/../.."

VER=0.1.0
IMAGE=dashboardtemplate-linux-build:latest
NAME=DashboardTemplate-$VER-linux-x86_64

mkdir -p dist

# Launcher/dock icons: rendered on the host from the app's own AppIcon painter.
ICONS=$(mktemp -d)
trap 'rm -rf "$ICONS"' EXIT
packaging/common/gen-icons.sh "$ICONS"

docker build -q -t "$IMAGE" - <<'EOF2'
FROM fedora:43
RUN dnf -y --setopt=install_weak_deps=False install \
      cmake ninja-build gcc-c++ file tar gzip findutils \
      qt6-qtbase-devel qt6-qtbase-gui qt6-qtwayland \
      fontconfig dejavu-sans-fonts \
 && dnf clean all
EOF2

docker run --rm \
  -v "$PWD:/src:ro" \
  -v "$PWD/dist:/out" \
  -v "$ICONS:/icons:ro" \
  "$IMAGE" bash -euxo pipefail -c '
NAME='"$NAME"'
D=/out/$NAME
QTP=$(qmake6 -query QT_INSTALL_PLUGINS)

cmake -S /src -B /tmp/b -GNinja -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/b

rm -rf "$D"
mkdir -p "$D"/{bin,lib,plugins}
cp /tmp/b/DashboardTemplate "$D/bin/"
strip "$D/bin/DashboardTemplate"
for p in platforms tls networkinformation styles imageformats xcbglintegrations \
         wayland-decoration-client wayland-graphics-integration-client \
         wayland-shell-integration iconengines; do
  [ -d "$QTP/$p" ] && cp -r "$QTP/$p" "$D/plugins/"
done
printf "[Paths]\nPlugins = ../plugins\n" > "$D/bin/qt.conf"

# Libraries that must come from the host (glibc, GPU stack).
skip() { case "$(basename "$1")" in
  ld-linux*|libc.so*|libm.so*|libdl.so*|libpthread.so*|librt.so*|libutil.so*|\
  libresolv.so*|libnsl.so*|libanl.so*|libGL.so*|libGLX*|libGLdispatch*|libEGL.so*|\
  libOpenGL*|libgbm*|libdrm*|libvulkan*|linux-vdso*) return 0 ;; esac; return 1; }

# Fixpoint: copy the ldd closure of the exe and every bundled plugin.
for _ in 1 2 3 4 5; do
  new=0
  while read -r f; do
    while read -r lib; do
      skip "$lib" && continue
      if [ ! -f "$D/lib/$(basename "$lib")" ]; then
        cp -L "$lib" "$D/lib/"; new=1
      fi
    done < <(ldd "$f" | sed -n "s/.*=> \(\/[^ ]*\) .*/\1/p")
  done < <(find "$D/bin" "$D/plugins" "$D/lib" -type f \( -name "*.so*" -o -name DashboardTemplate \))
  [ "$new" = 0 ] && break
done

# Final gate: nothing may be unresolved when only lib/ is on the path.
for f in "$D/bin/DashboardTemplate" "$D"/plugins/platforms/*.so; do
  if LD_LIBRARY_PATH="$D/lib" ldd "$f" | grep -q "not found"; then
    echo "UNRESOLVED in $f"; LD_LIBRARY_PATH="$D/lib" ldd "$f" | grep "not found"; exit 1
  fi
done

cat > "$D/DashboardTemplate" <<"LAUNCH"
#!/bin/sh
here=$(dirname "$(readlink -f "$0")")
export LD_LIBRARY_PATH="$here/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$here/bin/DashboardTemplate" "$@"
LAUNCH
chmod +x "$D/DashboardTemplate"

# Desktop integration: hicolor icons + launcher entry, installed per-user by
# install-desktop.sh (the .desktop Exec path is absolute, so it is rewritten
# at install time to wherever the folder was extracted).
for px in 16 24 32 48 64 128 256 512; do
  mkdir -p "$D/share/icons/hicolor/${px}x${px}/apps"
  cp /icons/icon-$px.png "$D/share/icons/hicolor/${px}x${px}/apps/dashboardtemplate.png"
done
mkdir -p "$D/share/applications"
cp /src/packaging/linux/dashboardtemplate.desktop "$D/share/applications/"
cat > "$D/install-desktop.sh" <<"INST"
#!/bin/sh
# Install the launcher entry and icons for the current user (dock/app menu).
set -e
here=$(dirname "$(readlink -f "$0")")
data=${XDG_DATA_HOME:-$HOME/.local/share}
mkdir -p "$data/applications"
cp -r "$here/share/icons" "$data/"
sed "s|^Exec=.*|Exec=$here/DashboardTemplate|" \
  "$here/share/applications/dashboardtemplate.desktop" > "$data/applications/dashboardtemplate.desktop"
command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -q -t "$data/icons/hicolor" || true
command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database -q "$data/applications" || true
echo "Installed DashboardTemplate launcher entry in $data"
INST
chmod +x "$D/install-desktop.sh"

# Smoke test with the bundled libs/plugins only (offscreen, no network).
QT_QPA_PLATFORM=offscreen timeout 60 "$D/DashboardTemplate" --offline --grab /tmp/smoke.png
[ -s /tmp/smoke.png ] || { echo "SMOKE TEST FAILED: no frame"; exit 1; }

cd /out
rm -f "$NAME.tar.gz"
tar -czf "$NAME.tar.gz" "$NAME"
du -sh "$NAME" "$NAME.tar.gz"
'

echo "OK: dist/$NAME.tar.gz (extract, then run ./$NAME/DashboardTemplate)"

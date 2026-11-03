# Packaging

Two shippable targets, both built in Docker from Linux — zero source
changes needed. Artifacts land in `dist/` (gitignored). Images build on
first use; the Fedora 43 base layers cache across both targets.

| Target | Command | Artifact |
|---|---|---|
| Linux x86_64 | `packaging/linux/build-linux.sh` | `dist/DashboardTemplate-<ver>-linux-x86_64.tar.gz` |
| Windows 10/11 x64 | `packaging/windows/build-win64.sh` | `dist/DashboardTemplate-<ver>-win64.zip` (~40 MB) |
| Linux (dev) | plain cmake build | `build/DashboardTemplate` |

If a change breaks cross-compilation, the change is wrong, not the scripts.

## Prerequisites

- Docker
- Host Qt 6 (Gui), CMake ≥ 3.21, Ninja, python3 — the scripts render the
  launcher/taskbar icons on the host from the app's own painter
  (`packaging/common/gen-icons.sh`): PNG set + multi-size `icon.ico`.

## Linux tarball

Built in a Fedora 43 container, bundling the Qt libraries plus the
platform/TLS plugins next to the executable behind a launcher that sets
`LD_LIBRARY_PATH` — no patchelf, no post-link edits.

Bundle policy: the shared-library closure is copied **except** glibc and
the GPU/GL stack (libGL, libEGL, libdrm, libgbm…), which must match the
host. Because the bundle is built against Fedora 43's glibc, the target
needs glibc ≥ 2.42 (e.g. Fedora 43+, Ubuntu 26.04+). A final gate fails the
build if anything is unresolved against `lib/` alone.

The archive is smoke-tested in the container (`--offline --grab`,
offscreen) before it is written. The folder carries `install-desktop.sh`:
one run installs the launcher entry + hicolor icons per-user (a plain
`cmake --install` does the same system-wide, rendering the 256 px icon with
the freshly built binary).

## Windows zip

Cross-compiled with Fedora's prebuilt mingw-w64 Qt 6 packages (same 6.10.x
series as the Linux build); Linux moc/rcc host tools run natively, no Wine
needed at build time. The `.exe` embeds the icon via CMake's `APP_ICON_ICO`
(`.rc`) hook and links as a GUI-subsystem binary — no console window.

DLL deployment is a fixpoint pass: every non-system DLL import of the exe,
Qt libs and plugins is copied from the sysroot until the closure resolves;
a final gate fails the build on anything still unresolved. Runtime
verification without a Windows box is a one-liner Wine run (see the
comment block in `packaging/windows/build-win64.sh`).

## Rebranding first

Run `scripts/rename-project.sh NewName` before your first release — it
rewrites the identity tokens (CMake project/target, `.desktop`, icon and
Docker image names) and self-verifies with a build + offscreen grab, so
your artifacts ship under your name.

#!/bin/bash

set -euxo pipefail

# git 2.35+ refuses to run in a repo owned by a different user, and in the build
# container /project is owned by the host/runner while we run as root. That made
# `git describe` in qflipper_common.pri fail, so APP_VERSION baked as "unknown".
# Mark the tree safe so the version/commit are picked up correctly.
git config --global --add safe.directory '*' || true

TARGET="qFlipper"
BUILDDIR="build"
APPDIR="$PWD/$BUILDDIR/AppDir"

export OUTPUT="$TARGET-x86_64.AppImage"
# linuxdeploy + its qt plugin are AppImages; in Docker (no FUSE) they must
# self-extract to run.
export APPIMAGE_EXTRACT_AND_RUN=1

# Fail early with a clear message if build tooling is missing (e.g. when the
# script is run outside the provided Docker image) instead of dying mid-build.
for _tool in qmake make linuxdeploy linuxdeploy-plugin-qt; do
    command -v "$_tool" >/dev/null 2>&1 || { echo "error: required tool '$_tool' not found in PATH" >&2; exit 1; }
done

# Initialise the submodule the Linux build needs. nanopb (3rdparty/nanopb) is
# compiled by 3rdparty/3rdparty.pro, so a plain non-recursive clone leaves it
# empty and the build fails. CI checks out with submodules, so this only bites
# local builds. Mirrors build_mac.sh. (libwdi is Windows-only, skipped here.)
if [ -f .gitmodules ] && command -v git >/dev/null 2>&1 && git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    git submodule update --init 3rdparty/nanopb
fi

# Start from a clean AppDir so stale binaries/libs from a previous run never
# leak into the AppImage.
rm -rf "$APPDIR"

mkdir -p "$BUILDDIR" && cd "$BUILDDIR"

qmake "../$TARGET.pro" -spec linux-g++ "CONFIG+=release qtquickcompiler" PREFIX="$APPDIR/usr"
make qmake_all
make -j"$(nproc)"
make install

# Bundle the dynamic Qt runtime (libs, xcb platform plugin, QML imports) into
# the AppDir. QML_SOURCES_PATHS lets the qt plugin discover which QML modules
# the app imports (our QML is compiled into the binary, so it can't scan that).
export QML_SOURCES_PATHS="$PWD/../application"

# Qt's OpenSSL TLS backend is a plugin that dlopen()s libssl at runtime, so
# linuxdeploy's dependency scan never sees it. Deploy the tls plugin AND
# force-bundle libssl/libcrypto, or every HTTPS request fails on Linux with
# "the backend named 'cert-only' does not support TLS".
# Qt 6.4.2's official binaries want OpenSSL *1.1* (libssl.so.1.1); bundle those
# (installed from the focal .deb in the Dockerfile). The .so.3 pair is kept too,
# harmlessly, for anything else that might link it.
export EXTRA_QT_PLUGINS="tls"

# Locate libssl/libcrypto dynamically instead of hardcoding Debian/Ubuntu
# multiarch paths, so this also works on other distros (Fedora/Arch use
# /usr/lib64) and on OpenSSL-3-only systems (Ubuntu 24.04+, Kali). Bundle
# whichever of the 1.1 / 3 sonames are actually installed. Qt 6.4.2's official
# binaries dlopen() libssl.so.1.1; a distro-Qt build may only have the .so.3.
SSL_LIBS=()
for _soname in libssl.so.1.1 libcrypto.so.1.1 libssl.so.3 libcrypto.so.3; do
    # NB: no early `exit` in awk -- under `set -o pipefail` that would SIGPIPE
    # ldconfig (exit 141) and abort the build. Read all input, keep first match.
    _path=$(ldconfig -p 2>/dev/null | awk -v n="$_soname" '$1 == n && !f { print $NF; f = 1 }')
    if [ -n "${_path:-}" ] && [ -e "$_path" ]; then
        SSL_LIBS+=("--library=$_path")
    fi
done
if [ "${#SSL_LIBS[@]}" -eq 0 ]; then
    echo "warning: no libssl/libcrypto found via ldconfig; Qt HTTPS may fail at runtime" >&2
fi

linuxdeploy --appdir="$APPDIR" \
    --plugin qt \
    "${SSL_LIBS[@]}" \
    --custom-apprun="../installer-assets/appimage/AppRun" \
    --output appimage

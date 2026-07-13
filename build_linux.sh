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

linuxdeploy --appdir="$APPDIR" \
    --plugin qt \
    --library=/usr/lib/x86_64-linux-gnu/libssl.so.1.1 \
    --library=/usr/lib/x86_64-linux-gnu/libcrypto.so.1.1 \
    --library=/usr/lib/x86_64-linux-gnu/libssl.so.3 \
    --library=/usr/lib/x86_64-linux-gnu/libcrypto.so.3 \
    --custom-apprun="../installer-assets/appimage/AppRun" \
    --output appimage

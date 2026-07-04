#!/bin/bash

set -euxo pipefail

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
export EXTRA_QT_PLUGINS="tls"

linuxdeploy --appdir="$APPDIR" \
    --plugin qt \
    --library=/usr/lib/x86_64-linux-gnu/libssl.so.3 \
    --library=/usr/lib/x86_64-linux-gnu/libcrypto.so.3 \
    --custom-apprun="../installer-assets/appimage/AppRun" \
    --output appimage

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

linuxdeploy --appdir="$APPDIR" \
    --plugin qt \
    --custom-apprun="../installer-assets/appimage/AppRun" \
    --output appimage

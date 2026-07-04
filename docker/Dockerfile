FROM ubuntu:22.04

# The upstream static-Qt repo (OBS home:drunkbatya / xUbuntu_20.04) was taken
# down, which broke the old Linux build. Instead we fetch official Qt 6.4.2
# (dynamic) with aqtinstall and bundle it with linuxdeploy-plugin-qt at package
# time. See build_linux.sh.

WORKDIR /project
ENV DEBIAN_FRONTEND=noninteractive

# Toolchain + Qt platform (xcb/GL) + libusb (DFU backend) + fuse (AppImage) deps.
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential git curl wget file ca-certificates pkg-config \
    python3 python3-pip \
    libgl1-mesa-dev libglx-dev libopengl-dev libegl-dev \
    libxkbcommon-dev libxkbcommon-x11-dev \
    libxcb1-dev libxcb-cursor-dev libxcb-glx0-dev libxcb-icccm4-dev \
    libxcb-image0-dev libxcb-keysyms1-dev libxcb-randr0-dev \
    libxcb-render-util0-dev libxcb-shape0-dev libxcb-sync-dev \
    libxcb-util-dev libxcb-xfixes0-dev libxcb-xinerama0-dev libxcb-xkb-dev \
    libx11-xcb-dev libsm-dev libice-dev \
    libdbus-1-dev libfontconfig1-dev libfreetype6-dev \
    libusb-1.0-0-dev zlib1g-dev \
    libfuse2 \
    && rm -rf /var/lib/apt/lists/*

# Qt 6.4.2 (dynamic) via aqtinstall. Base gcc_64 install covers qtbase +
# qtdeclarative (Quick/Controls2) + qtsvg; the rest are explicit modules.
RUN pip3 install --no-cache-dir aqtinstall
RUN aqt install-qt linux desktop 6.4.2 gcc_64 \
        -m qtserialport qt5compat qtimageformats \
        -O /opt/qt
ENV QTDIR="/opt/qt/6.4.2/gcc_64"
ENV PATH="/opt/qt/6.4.2/gcc_64/bin:${PATH}"
ENV LD_LIBRARY_PATH="/opt/qt/6.4.2/gcc_64/lib"

# AppImage packaging tooling.
RUN wget -q "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" \
        -O /usr/local/bin/linuxdeploy && chmod +x /usr/local/bin/linuxdeploy
RUN wget -q "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage" \
        -O /usr/local/bin/linuxdeploy-plugin-qt && chmod +x /usr/local/bin/linuxdeploy-plugin-qt



https://github.com/user-attachments/assets/026177db-b18d-437a-8670-7ccf5799bc9e

<img width="686" height="411" alt="IMG_9806" src="https://github.com/user-attachments/assets/0e165538-80c4-457a-b36a-4331c0b58be4" />
<img width="682" height="408" alt="IMG_9804" src="https://github.com/user-attachments/assets/3ae68568-b233-458a-a736-937bb4b0b3d7" />
<img width="683" height="397" alt="IMG_9805" src="https://github.com/user-attachments/assets/c2223c8c-0d02-4a31-b9bb-34edef6ee2fe" />

# 🐬 Hyper-Zero-UI (HZUI)

[![Latest release](https://img.shields.io/github/v/release/DUNKINKKD/lotei-qflipper?color=ff4fa3&label=release)](https://github.com/DUNKINKKD/lotei-qflipper/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/DUNKINKKD/lotei-qflipper/total?color=ff4fa3)](https://github.com/DUNKINKKD/lotei-qflipper/releases)
[![License](https://img.shields.io/badge/license-GPLv3-ff4fa3.svg)](LICENSE)
[![Stars](https://img.shields.io/github/stars/DUNKINKKD/lotei-qflipper?color=ff4fa3)](https://github.com/DUNKINKKD/lotei-qflipper/stargazers)
[![100% local](https://img.shields.io/badge/AI-100%25%20local-ff4fa3)](#)
[![Powered by Ollama](https://img.shields.io/badge/powered%20by-Ollama-ff4fa3)](https://ollama.com)

> **Unofficial fork** of [qFlipper](https://github.com/flipperdevices/qFlipper), the Flipper Zero desktop app. Not affiliated with or endorsed by Flipper Devices.

A heavily-customized qFlipper UI starring **LOTEI** — a snarky, **100% local** AI dolphin that lives inside the app — plus a full pink makeover, a runtime color editor, neural voice, and a live Flipper-screen mirror.

**No API keys. No cloud. No cost.** LOTEI runs entirely on your machine via [Ollama](https://ollama.com).

<!-- Screenshots: on GitHub, click the pencil (Edit) on this README and drag your
     images / video right here — GitHub uploads + hosts them and inserts the markdown
     for you, no need to commit the files. -->

## ✨ Features

- **🐬 LOTEI** — a local-AI chat assistant (Ollama + `qwen2.5:7b`) built right into the app: a flirty, vain, RAM-glutton pink dolphin with **agentic tools**. He can browse and read your Flipper's SD card, save scripts onto it, and press the device's buttons to navigate menus — all over qFlipper's existing RPC link.
- **🎙️ Neural voice** — local [Piper](https://github.com/rhasspy/piper) text-to-speech with a click-to-cycle voice switcher and mood-based tempo. Falls back to Windows SAPI if Piper isn't installed.
- **🎨 Live color editor** — recolor the *entire* UI at runtime, every color individually, with live preview. Persists across launches. (Click **COLORS**, top-left.)
- **👁️ Flipper-screen mirror** — watch the device's 128×64 screen live in the chat panel as LOTEI works.
- **🎵 Music player** — drop `.mp3`s into a `Music/` folder next to the exe; shuffle-plays in the footer.
- **💖 Full pink theme** throughout.

## 🧰 Requirements

- **Windows 10/11** (64-bit)
- **[Qt 6.4.2](https://www.qt.io/)** (`msvc2019_64`) — easiest via [`aqt`](https://github.com/miurahr/aqtinstall). Modules: `qtdeclarative qttools qtserialport qt5compat qtmultimedia qtspeech qtimageformats svg`. Put Qt on a **different drive** than this source (a qmake quirk — otherwise the build fails in `dfu`).
- **MSVC 2019** build tools + [`jom`](https://wiki.qt.io/Jom)
- **[Ollama](https://ollama.com)** + the `qwen2.5:7b` model (~4.7 GB). A GPU with ≥6 GB VRAM is recommended (runs on CPU too, just slower).

## 🔨 Build

```bat
git clone <your-fork-url> --recursive qFlipper-src
cd qFlipper-src
:: optionally override the defaults: set QT_DIR=...  set VS_VCVARS=...  set JOM=...
build_pink.bat
```

Output: `build\qFlipper.exe`. For the first run, copy Qt's DLLs/plugins next to it with `windeployqt`:

```bat
"%QT_DIR%\bin\windeployqt.exe" --qmldir application build\qFlipper.exe
```

After that, `build_pink_inc.bat` is the fast incremental build for code/QML changes.

### 🐧 Building on Linux (from source)

Prebuilt AppImages ship with each [release](../../releases) — this is only if you want to build it yourself.

```bash
# Debian / Ubuntu — everything the build needs
sudo apt update && sudo apt install -y \
    build-essential git \
    qt6-base-dev qt6-base-dev-tools qt6-declarative-dev \
    qt6-serialport-dev qt6-5compat-dev qt6-svg-dev qt6-connectivity-dev \
    libgl1-mesa-dev libusb-1.0-0-dev

# Clone WITH submodules (nanopb is required; a plain clone fails on pb_common.c)
git clone <your-fork-url> --recursive lotei-qflipper
cd lotei-qflipper

mkdir build && cd build
qmake6 ../qFlipper.pro CONFIG+=qtquickcompiler
make -j"$(nproc)"
```

Output: `build/application/qFlipper`.

- Already cloned without `--recursive`? Run `git submodule update --init --recursive` first.
- `qt6-5compat-dev` (core5compat), `qt6-svg-dev` (svg) and `qt6-connectivity-dev` (Bluetooth/BLE) are easy to miss — qmake fails with `Unknown module(s) in QT: ...` if any are absent.
- Developed against **Qt 6.4.2**, and builds on **6.7 / 6.8** (newer distros) too.
- To build the **AppImage** instead, use `./build_linux.sh` (needs `linuxdeploy` + `linuxdeploy-plugin-qt` on `PATH`).
- USB permissions: `sudo cp installer-assets/*.rules /etc/udev/rules.d/ && sudo udevadm control --reload` — or just run a release AppImage once with `./Hyper-Zero-UI-*.AppImage rules install`.

<details>
<summary>Optional: add a desktop launcher</summary>

```bash
sudo nano /usr/share/applications/lotei-qflipper.desktop
```
```ini
[Desktop Entry]
Type=Application
Name=Hyper-Zero-UI
Comment=AI-powered qFlipper fork for Flipper Zero
Exec=/path/to/lotei-qflipper/build/application/qFlipper
Icon=lotei-qflipper
Terminal=false
Categories=Development;Utility;
```
```bash
sudo update-desktop-database
```
*(Thanks to @MrSwan84 for working this out on Ubuntu 25.10.)*
</details>

## 🐬 Set up LOTEI (the AI + voice)

1. **Ollama** — install it, then: `ollama pull qwen2.5:7b`
2. **Piper voice** — run the setup script, pointing it at the folder that holds `qFlipper.exe`:
   ```powershell
   ./setup-lotei.ps1 -AppDir .\build
   ```
   This downloads Piper + a few neural voices into `<AppDir>\piper\`.
3. **(Optional) Music** — drop `.mp3` files into `<AppDir>\Music\`.
4. Make sure `ollama serve` is running, then launch `qFlipper.exe`. LOTEI wakes up and says hi. 🐬

> LOTEI auto-discovers any extra Piper `.onnx` voices you drop into `piper\voices\` — grab more from the [Piper voices catalog](https://huggingface.co/rhasspy/piper-voices). Click his voice name to cycle them.

## 🧩 What was added on top of qFlipper

All the LOTEI work lives in `application/`: `loteibackend.{h,cpp}` (the AI backend, agentic tools, Piper TTS, and the `LoteiPalette` color engine), `components/LoteiChat.qml` (the chat panel + screen mirror), `components/MusicPlayer.qml`, and the color editor in `components/MainWindow.qml`. The base project structure is unchanged from [upstream qFlipper](https://github.com/flipperdevices/qFlipper#project-structure).

## 🙏 Credits

- **[qFlipper](https://github.com/flipperdevices/qFlipper)** — Flipper Devices (the base app this forks)
- **[Piper](https://github.com/rhasspy/piper)** — Michael Hansen / rhasspy (neural TTS)
- **[Ollama](https://ollama.com)** + **[Qwen2.5](https://github.com/QwenLM/Qwen2.5)** — the local LLM stack

## 📜 License

**GPLv3**, inherited from qFlipper — see [LICENSE](LICENSE). This is an independent, unofficial fork. "Flipper Zero" and "qFlipper" are trademarks of Flipper Devices Inc.; this project is not affiliated with them.

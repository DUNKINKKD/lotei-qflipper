https://github.com/user-attachments/assets/2421251a-8c28-43c7-b14d-e39354daed1e
<img width="686" height="411" alt="IMG_9806" src="https://github.com/user-attachments/assets/2be70ecd-2db6-4e8d-9673-ff7ca632fb24" />
<img width="682" height="408" alt="IMG_9804" src="https://github.com/user-attachments/assets/bc002a81-cac6-4db1-b232-e40b1a7588f1" />
<img width="683" height="397" alt="IMG_9805" src="https://github.com/user-attachments/assets/57ff69c7-4936-456d-891a-0331826365ce" />
# 🐬 Hyper-Zero-UI (HZUI)

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

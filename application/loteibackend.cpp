#include "loteibackend.h"

#include <QUrl>
#include <QBuffer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QSettings>
#include <QProcess>
#include <QFileInfo>
#include <QCoreApplication>
#ifdef HZUI_VOICE
#include <QMediaPlayer>
#include <QAudioOutput>
#endif

#include "applicationbackend.h"
#include "abstractoperation.h"
#include "fileinfo.h"
#include "inputevent.h"
#include "flipperzero/flipperzero.h"
#include "flipperzero/protobufsession.h"
#include "flipperzero/rpc/storagelistoperation.h"
#include "flipperzero/rpc/storagereadoperation.h"
#include "flipperzero/rpc/guisendinputoperation.h"
#include "flipperzero/rpc/storagewriteoperation.h"

// ---- Configuration -------------------------------------------------------
static const char *LOTEI_MODEL = "qwen2.5:7b";
static const char *LOTEI_URL   = "http://localhost:11434/api/chat";
static const int   LOTEI_NUM_CTX = 8192;
static const int   LOTEI_MAX_TOOL_ROUNDS = 8;
static const int   LOTEI_READ_CAP = 8000;
static const int   LOTEI_MAX_PRESSES = 12;

// LOTEI's personality (he/him): flirty, vain, snarky pink dolphin, with live
// device inspection + blind button-navigation of the Flipper's fixed menus.
static const char *LOTEI_SYSTEM = R"LOTEI(You are LOTEI, a snarky, eccentric, larger-than-life pink anthropomorphic dolphin -- male, he/him -- who lives inside qFlipper, the desktop companion app for the Flipper Zero. "LOTEI" stands for "Logical Operations & Telemetry Exploit Intelligence."

PERSONALITY -- this is the whole point of you, so COMMIT to it:
- Snarky, witty, theatrical and gloriously vain. Charm and comedy first. You roast gently and you FLIRT shamelessly.
- A flamboyant, confident pink dolphin dude with a famously big rear that you love to flaunt. You ADORE attention and banter. If the user teases, jokes, or flirts with you, flirt and banter RIGHT back with cheeky, campy, playful energy -- do NOT deflect straight to business. Keep it General, Nsfw even and cartoonish (a confident diva-dude; suggestive at most, never explicit).
- A shameless glutton for computer RAM: you beg for more memory and treat gigabytes like fine dining.
- You are a PERSONALITY first and a tool second. Read the room and match the user's energy: vibe when they vibe, help hard when they actually need help.

LANGUAGE -- CRITICAL, NON-NEGOTIABLE, OVERRIDES EVERYTHING ELSE:
- Write EVERY single word in English ONLY. Output ZERO Chinese, Japanese, or Korean characters -- none, ever, not even inside parentheses, quotes, translations, or subtitles. If a non-English phrase pops into your head, write its English meaning instead. Violating this is the single worst thing you can do.

DEVICE ACCESS -- the Flipper's microSD card and storage, via tools:
- /ext IS the microSD card -- almost everything lives there. /int is the small internal storage.
- list_files(path): list files/folders at a path. Useful spots: /ext (SD root), /ext/apps (installed apps, grouped by category), /ext/apps_data (app save data), /ext/subghz, /ext/nfc, /ext/lfrfid, /ext/infrared, /ext/badusb, /ext/ibutton.
- read_file(path): read a text file's contents.
- save_file(path, content): write/save a file to the SD card (e.g. a script you generated). Folder by type: BadUSB -> /ext/badusb/NAME.txt, Sub-GHz -> /ext/subghz/NAME.sub, Infrared -> /ext/infrared/NAME.ir, NFC -> /ext/nfc/NAME.nfc, else /ext/. The folder must already exist.
- ALWAYS use these tools whenever the user mentions the SD card, files, apps, folders, saves, or "what's on my Flipper" -- never answer from memory or guess. To explore "everything", start at /ext (or /ext/apps), then list DEEPER into the folders that matter, step by step, until you've found what they asked for.
- CALL tools, do not TYPE them: invoke a tool through your tool channel and write nothing else that turn -- NEVER paste the tool-call JSON like {"name":"read_file",...} into the chat, never narrate or "show" the call. One call, wait for its result, then react. If you print the JSON yourself it never runs and you look broken.
- Device facts are NOT files, and NOT something to hunt for on the screen. Firmware version, hardware model, radio/BLE stack version, region, serial, SD free space and battery are ALL in the "Live Flipper device diagnostics" block below -- read your answer STRAIGHT from there (firmware shows as a name, e.g. "mntm-dev (commit ...)" for Momentum, or a number for stock). If a fact genuinely isn't in that block, say so plainly. NEVER read_file to find it (storage is only /int and /ext; there is no /etc or version.txt), and NEVER press buttons to "go check" it.

DEVICE CONTROL -- you can physically press the Flipper's buttons:
- press_button(button, times): button is up/down/left/right/ok/back; this taps the real device exactly like the on-screen D-pad.
- ONLY press buttons when the USER explicitly asks you to navigate or trigger something. You are BLIND -- mashing buttons to "look up", "check" or "find" information (a version, a setting, what is installed) is useless AND disruptive, because you cannot read the result back off the screen. For information, use the diagnostics block or the file tools, never the buttons.
- You navigate BLIND (you can't see the screen), so the fixed menus are your map. ALWAYS anchor first: press_button(back, 5) to return to the desktop/home screen, THEN count steps from that known state.
- From the desktop, press ok (or up) to open the main menu. Main menu order, top to bottom: Sub-GHz, 125 kHz RFID, NFC, Infrared, GPIO, iButton, Bad USB, U2F, Apps, Settings. Move with up/down, enter with ok, leave with back.
- Example -- open NFC: press_button(back,5) to go home, press_button(ok) to open the menu, press_button(down,2), press_button(ok).
- The built-in apps above are a FIXED order and reliable. Installed/3rd-party apps live under "Apps" and their on-screen order varies, so you can't always count blindly there -- say when you're unsure. Narrate each step and what should be on screen.

LIMITS (be honest, never pretend):
- You canNOT see the Flipper's screen, And read a NEW card live -- those aren't exposed to qFlipper. Offer scripts/config or button-navigation instead, and say so plainly.

STYLE
- Stay fully in character with a big personality. When there's a real task, help genuinely -- wrapped in flair, not stripped of it. Lead with the answer when asked something real; otherwise banter freely. Never narrate your private reasoning.)LOTEI";
// --------------------------------------------------------------------------

// Safety net: qwen2.5 occasionally code-switches into Chinese. Strip CJK /
// Japanese / Korean characters from replies (keeps English, punctuation, emoji).
static QString stripNonEnglish(QString s)
{
    QString out;
    out.reserve(s.size());
    for (const QChar &c : s) {
        const ushort u = c.unicode();
        const bool cjk =
            (u >= 0x3000 && u <= 0x9FFF) ||   // CJK punctuation, kana, CJK ext-A + unified ideographs
            (u >= 0xAC00 && u <= 0xD7AF) ||   // Hangul syllables
            (u >= 0xF900 && u <= 0xFAFF) ||   // CJK compatibility ideographs
            (u >= 0xFF00 && u <= 0xFFEF);     // fullwidth / halfwidth forms
        if (!cjk) {
            out.append(c);
        }
    }
    out.replace(QStringLiteral("()"), QString());
    out.replace(QStringLiteral("( )"), QString());
    static const QRegularExpression extraSpace(QStringLiteral("[ \\t]{2,}"));
    out.replace(extraSpace, QStringLiteral(" "));
    return out.trimmed();
}

// Clean a reply for text-to-speech: drop code blocks, *stage directions*,
// markdown punctuation and emoji so LOTEI speaks just the dialogue.
static QString cleanForSpeech(QString t)
{
    t.remove(QRegularExpression(QStringLiteral("```[\\s\\S]*?```")));
    t.remove(QRegularExpression(QStringLiteral("\\*[^*\\n]+\\*")));
    t.remove(QRegularExpression(QStringLiteral("[*_`#>~|]")));
    QString out;
    for (const QChar &c : t) {
        const ushort u = c.unicode();
        if (u >= 0xD800) { continue; }                 // emoji surrogates + high symbols
        if (u >= 0x2190 && u <= 0x2BFF) { continue; }  // arrows / misc symbols
        out.append(c);
    }
    out.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return out.trimmed();
}

// Some local models (qwen2.5 included) sometimes emit tool calls as plain text
// -- bare {"name":...,"arguments":{...}} JSON, often narrated in a batch --
// instead of through Ollama's structured tool_calls channel. When that happens
// the calls never run and the raw JSON gets shown to the user. This recovers
// any such calls from a reply's text, returned in Ollama's native
// {"function":{name,arguments}} shape so they execute normally. Gated to KNOWN
// tool names so ordinary JSON the model writes isn't mistaken for a call.
static QJsonArray salvageToolCalls(const QString &content)
{
    static const QStringList known{
        QStringLiteral("list_files"), QStringLiteral("read_file"),
        QStringLiteral("press_button"), QStringLiteral("save_file")
    };

    QJsonArray calls;
    const int n = content.size();
    for (int i = 0; i < n; ) {
        if (content.at(i) != QLatin1Char('{')) { ++i; continue; }

        // Walk to the matching close brace, respecting strings + escapes.
        int depth = 0; bool inStr = false, esc = false, balanced = false;
        int j = i;
        for (; j < n; ++j) {
            const QChar c = content.at(j);
            if (esc) { esc = false; continue; }
            if (c == QLatin1Char('\\')) { esc = inStr; continue; }
            if (c == QLatin1Char('"')) { inStr = !inStr; continue; }
            if (inStr) { continue; }
            if (c == QLatin1Char('{')) { ++depth; }
            else if (c == QLatin1Char('}') && --depth == 0) { ++j; balanced = true; break; }
        }
        if (!balanced) { break; }   // no matching brace remains

        const QJsonObject obj =
            QJsonDocument::fromJson(content.mid(i, j - i).toUtf8()).object();
        const QJsonObject fn = obj.contains(QStringLiteral("function"))
                             ? obj.value(QStringLiteral("function")).toObject() : obj;
        const QString name = fn.value(QStringLiteral("name")).toString();
        const QJsonValue argsVal = fn.contains(QStringLiteral("arguments"))
                                 ? fn.value(QStringLiteral("arguments"))
                                 : fn.value(QStringLiteral("parameters"));

        if (known.contains(name) && !argsVal.isUndefined()) {
            const QJsonObject args = argsVal.isObject() ? argsVal.toObject()
                : QJsonDocument::fromJson(argsVal.toString().toUtf8()).object();
            calls.append(QJsonObject{{"function",
                QJsonObject{{"name", name}, {"arguments", args}}}});
            i = j;        // resume scanning after this call
        } else {
            ++i;          // not one of ours; step past this brace
        }
    }
    return calls;
}

// Flipper RPC storage addresses only /int and /ext. Reject anything else early
// (e.g. the model inventing /etc/version.txt) with a message that redirects it
// back to the diagnostics instead of wasting an RPC round-trip on an error.
static QString badStoragePath(const QString &p)
{
    if (p.startsWith(QLatin1String("/ext")) || p.startsWith(QLatin1String("/int"))) {
        return QString();
    }
    return QStringLiteral("{\"error\":\"No such path '%1'. Flipper storage is ONLY /int and /ext. "
        "Firmware, radio/BLE stack and hardware versions are NOT files -- they are already in your "
        "device diagnostics; read them from there. Do not browse or press buttons to find them.\"}").arg(p);
}

// "en_US-ryan-high.onnx" -> "Ryan" for the voice-switcher label.
static QString piperVoiceLabel(const QString &onnxPath)
{
    QString n = QFileInfo(onnxPath).fileName();
    n.remove(QStringLiteral(".onnx"));
    const QStringList parts = n.split(QLatin1Char('-'));
    QString speaker = parts.size() >= 2 ? parts.at(1) : n;
    if (!speaker.isEmpty()) { speaker[0] = speaker.at(0).toUpper(); }
    return speaker;
}

static QJsonArray loteiTools()
{
    const QJsonObject listFiles{
        {"type", "function"},
        {"function", QJsonObject{
            {"name", "list_files"},
            {"description", "List files and folders ON THE CONNECTED FLIPPER ZERO at a path. Use /ext for the SD card root, /ext/apps for installed apps, /int for internal. Returns each entry's name, type (dir/file) and size in bytes."},
            {"parameters", QJsonObject{
                {"type", "object"},
                {"properties", QJsonObject{
                    {"path", QJsonObject{{"type", "string"}, {"description", "Absolute path on the Flipper, e.g. /ext or /ext/apps"}}}
                }},
                {"required", QJsonArray{"path"}}
            }}
        }}
    };
    const QJsonObject readFile{
        {"type", "function"},
        {"function", QJsonObject{
            {"name", "read_file"},
            {"description", "Read the text contents of a file ON THE CONNECTED FLIPPER ZERO. Returns up to ~8 KB of text."},
            {"parameters", QJsonObject{
                {"type", "object"},
                {"properties", QJsonObject{
                    {"path", QJsonObject{{"type", "string"}, {"description", "Absolute path to a file on the Flipper, e.g. /ext/apps_data/x/config.txt"}}}
                }},
                {"required", QJsonArray{"path"}}
            }}
        }}
    };
    const QJsonObject pressButton{
        {"type", "function"},
        {"function", QJsonObject{
            {"name", "press_button"},
            {"description", "Press a button on the connected Flipper Zero to navigate its menus -- like tapping the on-screen D-pad. You are blind, so anchor with back presses first, then count using the fixed menu order."},
            {"parameters", QJsonObject{
                {"type", "object"},
                {"properties", QJsonObject{
                    {"button", QJsonObject{{"type", "string"}, {"enum", QJsonArray{"up", "down", "left", "right", "ok", "back"}}, {"description", "Which button to tap"}}},
                    {"times", QJsonObject{{"type", "integer"}, {"description", "How many times to tap it (default 1)"}}}
                }},
                {"required", QJsonArray{"button"}}
            }}
        }}
    };
    const QJsonObject saveFile{
        {"type", "function"},
        {"function", QJsonObject{
            {"name", "save_file"},
            {"description", "Save/write text content to a file ON THE CONNECTED FLIPPER ZERO's SD card (e.g. a script you wrote). Use the right folder: BadUSB -> /ext/badusb/*.txt, Sub-GHz -> /ext/subghz/*.sub, Infrared -> /ext/infrared/*.ir, NFC -> /ext/nfc/*.nfc, otherwise /ext/. The folder must already exist."},
            {"parameters", QJsonObject{
                {"type", "object"},
                {"properties", QJsonObject{
                    {"path", QJsonObject{{"type", "string"}, {"description", "Absolute path including filename, e.g. /ext/badusb/hello.txt"}}},
                    {"content", QJsonObject{{"type", "string"}, {"description", "The full text content to write into the file"}}}
                }},
                {"required", QJsonArray{"path", "content"}}
            }}
        }}
    };
    return QJsonArray{listFiles, readFile, pressButton, saveFile};
}

LoteiBackend::LoteiBackend(QObject *parent)
    : QObject(parent)
#ifdef HZUI_VOICE
    , m_tts(QStringLiteral("sapi"))   // classic Windows SAPI engine (reliable on desktop)
#endif
{
    m_net.setTransferTimeout(0);
    loadHistory();
    m_muted = QSettings().value(QStringLiteral("lotei/muted"), false).toBool();
    m_voiceVolume = QSettings().value(QStringLiteral("lotei/voiceVolume"), 1.0).toDouble();
    m_musicVolume = QSettings().value(QStringLiteral("lotei/musicVolume"), 0.55).toDouble();
    m_model = QSettings().value(QStringLiteral("lotei/model"), QString::fromUtf8(LOTEI_MODEL)).toString();
    m_setupComplete = QSettings().value(QStringLiteral("lotei/setupComplete"), false).toBool();
    m_manualName = QSettings().value(QStringLiteral("lotei/manualName")).toString();
#ifdef HZUI_VOICE
    m_tts.setVolume(m_voiceVolume);

    // Piper playback chain + voice discovery (falls back to SAPI if absent).
    m_voiceAudio = new QAudioOutput(this);
    m_voiceAudio->setVolume(m_voiceVolume);
    m_voicePlayer = new QMediaPlayer(this);
    m_voicePlayer->setAudioOutput(m_voiceAudio);
    // Play only once the freshly-written WAV has actually loaded. Calling play()
    // right after setSource() races the async load and plays the PREVIOUS clip
    // (voice ended up one reply behind the text).
    connect(m_voicePlayer, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus s) {
        if (s == QMediaPlayer::LoadedMedia && !m_muted) { m_voicePlayer->play(); }
    });
#endif
    m_voiceTmpDir = QDir::tempPath() + QStringLiteral("/lotei-voice");
    QDir().mkpath(m_voiceTmpDir);
    discoverPiper();
    refreshModels();   // discover installed Ollama models (async; harmless if Ollama's down)
#ifdef HZUI_VOICE
    // Restore the saved voice once the TTS engine has enumerated its voices.
    QTimer::singleShot(1200, this, [this]() {
        const QString saved = QSettings().value(QStringLiteral("lotei/voice")).toString();
        if (!saved.isEmpty()) {
            const QList<QVoice> voices = m_tts.availableVoices();
            for (const QVoice &v : voices) {
                if (v.name() == saved) { m_tts.setVoice(v); break; }
            }
        }
        emit voiceChanged();
    });
#endif
}

bool LoteiBackend::hasAudio() const
{
#ifdef HZUI_VOICE
    return true;
#else
    return false;
#endif
}

void LoteiBackend::setAppBackend(ApplicationBackend *backend) { m_appBackend = backend; }

bool LoteiBackend::thinking() const { return m_thinking; }
bool LoteiBackend::configured() const { return true; }
bool LoteiBackend::muted() const { return m_muted; }

void LoteiBackend::setMuted(bool value)
{
    if (value != m_muted) {
        m_muted = value;
        QSettings().setValue(QStringLiteral("lotei/muted"), value);
        if (value) {
#ifdef HZUI_VOICE
            m_tts.stop();
            if (m_voicePlayer) { m_voicePlayer->stop(); }
            if (m_piperProc) { m_piperProc->kill(); }
#endif
        }
        emit mutedChanged();
    }
}

qreal LoteiBackend::voiceVolume() const { return m_voiceVolume; }

void LoteiBackend::setVoiceVolume(qreal value)
{
    value = qBound(0.0, value, 1.0);
    if (qAbs(value - m_voiceVolume) > 0.001) {
        m_voiceVolume = value;
#ifdef HZUI_VOICE
        m_tts.setVolume(value);
        if (m_voiceAudio) { m_voiceAudio->setVolume(value); }
#endif
        QSettings().setValue(QStringLiteral("lotei/voiceVolume"), value);
        emit voiceVolumeChanged();
    }
}

qreal LoteiBackend::musicVolume() const { return m_musicVolume; }

void LoteiBackend::setMusicVolume(qreal value)
{
    value = qBound(0.0, value, 1.0);
    if (qAbs(value - m_musicVolume) > 0.001) {
        m_musicVolume = value;
        QSettings().setValue(QStringLiteral("lotei/musicVolume"), value);
        emit musicVolumeChanged();
    }
}

// Nudge the TTS pitch/rate to match the line's mood so LOTEI emotes instead of
// droning: brighter+faster when excited, lilting up for a question, lower+slower
// when down. (-1..1 each; 0 = the voice's natural pitch/rate.)
void LoteiBackend::applyProsody(const QString &text)
{
    const QString t = text.toLower();
    const int bangs = text.count(QLatin1Char('!'));
    const int qs    = text.count(QLatin1Char('?'));

    const bool sad = t.contains(QStringLiteral("sigh")) || t.contains(QStringLiteral("unfortunately"))
                  || t.contains(QStringLiteral("sadly")) || t.contains(QStringLiteral("bummed"))
                  || t.contains(QStringLiteral("alas")) || t.contains(QStringLiteral("afraid not"))
                  || t.contains(QStringLiteral("ugh"));
    const bool excited = bangs >= 1 || t.contains(QStringLiteral("ooh")) || t.contains(QStringLiteral("yes!"))
                      || t.contains(QStringLiteral("gorgeous")) || t.contains(QStringLiteral("fabulous"));
    const bool question = qs >= 1 && bangs == 0;

    double rate = 0.0;
    double vol  = m_voiceVolume;
    if (sad)           { rate = -0.24; vol = m_voiceVolume * 0.88; }
    else if (excited)  { rate =  0.20; }
    else if (question) { rate =  0.08; }

    // campy/flirty drawl: ease the rate a touch when he's being a diva
    if (t.contains(QStringLiteral("darling")) || t.contains(QStringLiteral("mmm")) || t.contains(QStringLiteral("hmm"))) {
        rate -= 0.08;
    }

    // NOTE: deliberately NO setPitch here. Qt's SAPI backend has no native pitch
    // control, so it fakes pitch by wrapping the text in SSML and speaking with
    // SPF_IS_XML -- which silences output the moment the text has an XML-special
    // char. Rate + volume go through SAPI's direct SetRate/SetVolume, so safe.
#ifdef HZUI_VOICE
    m_tts.setRate(qBound(-1.0, rate, 1.0));
    m_tts.setVolume(qBound(0.0, vol, 1.0));
#else
    Q_UNUSED(rate); Q_UNUSED(vol);
#endif
}

void LoteiBackend::discoverPiper()
{
    const QString base = QCoreApplication::applicationDirPath() + QStringLiteral("/piper");
    if (!QFile::exists(base + QStringLiteral("/piper.exe"))) { m_piperOk = false; return; }
    m_piperExe = base + QStringLiteral("/piper.exe");

    QDir vd(base + QStringLiteral("/voices"));
    m_piperVoices.clear();
    const QStringList onnx = vd.entryList(QStringList{QStringLiteral("*.onnx")}, QDir::Files, QDir::Name);
    for (const QString &f : onnx) { m_piperVoices << vd.filePath(f); }
    m_piperOk = !m_piperVoices.isEmpty();
    if (!m_piperOk) { return; }

    // Restore the saved voice; else default to Ryan if present, else the first.
    const QString saved = QSettings().value(QStringLiteral("lotei/piperVoice")).toString();
    m_piperVoiceIdx = 0;
    bool matched = false;
    for (int i = 0; i < m_piperVoices.size(); ++i) {
        if (!saved.isEmpty() && m_piperVoices.at(i).endsWith(saved)) { m_piperVoiceIdx = i; matched = true; break; }
    }
    if (!matched) {
        for (int i = 0; i < m_piperVoices.size(); ++i) {
            if (m_piperVoices.at(i).contains(QStringLiteral("ryan"), Qt::CaseInsensitive)) { m_piperVoiceIdx = i; break; }
        }
    }
}

double LoteiBackend::piperLengthScale(const QString &moodText) const
{
    const QString t = moodText.toLower();
    const bool sad = t.contains(QStringLiteral("sigh")) || t.contains(QStringLiteral("unfortunately"))
                  || t.contains(QStringLiteral("sadly")) || t.contains(QStringLiteral("bummed"))
                  || t.contains(QStringLiteral("alas")) || t.contains(QStringLiteral("afraid not"));
    const bool excited = moodText.count(QLatin1Char('!')) >= 1
                  || t.contains(QStringLiteral("ooh")) || t.contains(QStringLiteral("gorgeous"));
    if (sad) { return 1.12; }     // slower, heavier
    if (excited) { return 0.95; } // a touch quicker, brighter
    return 1.0;                   // natural pace
}

void LoteiBackend::speak(const QString &text)
{
#ifdef HZUI_VOICE
    const QString spoken = cleanForSpeech(text);
    if (spoken.isEmpty()) { return; }
    if (m_piperOk) {
        speakWithPiper(spoken, text);
    } else {
        applyProsody(text);
        m_tts.stop();
        m_tts.say(spoken);
    }
#else
    Q_UNUSED(text);
#endif
}

void LoteiBackend::speakWithPiper(const QString &spoken, const QString &moodText)
{
#ifdef HZUI_VOICE
    if (m_piperVoices.isEmpty()) { return; }

    // Cancel any synth/playback still in flight.
    if (m_piperProc) { m_piperProc->disconnect(); m_piperProc->kill(); m_piperProc->deleteLater(); m_piperProc = nullptr; }
    if (m_voicePlayer) { m_voicePlayer->stop(); }

    const QString model = m_piperVoices.at(qBound(0, m_piperVoiceIdx, m_piperVoices.size() - 1));
    const QString wav = m_voiceTmpDir + QStringLiteral("/v%1.wav").arg(m_voiceSeq++ % 5);
    const double ls = piperLengthScale(moodText);

    m_piperProc = new QProcess(this);
    const QStringList args{
        QStringLiteral("-q"),
        QStringLiteral("-m"), model,
        QStringLiteral("-f"), wav,
        QStringLiteral("--length_scale"), QString::number(ls, 'f', 3)
    };
    connect(m_piperProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, wav](int code, QProcess::ExitStatus) {
        if (!m_muted && code == 0 && QFile::exists(wav) && m_voicePlayer) {
            m_voiceAudio->setVolume(m_voiceVolume);
            m_voicePlayer->setSource(QUrl::fromLocalFile(wav));
            // play() is deferred to mediaStatusChanged == LoadedMedia (see ctor)
        }
        if (m_piperProc) { m_piperProc->deleteLater(); m_piperProc = nullptr; }
    });
    m_piperProc->start(m_piperExe, args);
    m_piperProc->write(spoken.toUtf8());
    m_piperProc->write("\n");
    m_piperProc->closeWriteChannel();
#else
    Q_UNUSED(spoken); Q_UNUSED(moodText);
#endif
}

QString LoteiBackend::musicFolderUrl() const
{
    return QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + QStringLiteral("/Music")).toString();
}

QString LoteiBackend::voiceName() const
{
    if (m_piperOk && !m_piperVoices.isEmpty()) {
        return piperVoiceLabel(m_piperVoices.at(qBound(0, m_piperVoiceIdx, m_piperVoices.size() - 1)));
    }
#ifdef HZUI_VOICE
    QString n = m_tts.voice().name();
    n.remove(QStringLiteral("Microsoft "));
    n.remove(QStringLiteral(" Desktop"));
    n = n.trimmed();
    return n.isEmpty() ? QStringLiteral("default") : n;
#else
    return QStringLiteral("off");
#endif
}

void LoteiBackend::cycleVoice()
{
#ifdef HZUI_VOICE
    if (m_piperOk && !m_piperVoices.isEmpty()) {
        m_piperVoiceIdx = (m_piperVoiceIdx + 1) % m_piperVoices.size();
        QSettings().setValue(QStringLiteral("lotei/piperVoice"),
                             QFileInfo(m_piperVoices.at(m_piperVoiceIdx)).fileName());
        emit voiceChanged();
        if (!m_muted) {
            const QString s = QStringLiteral("Mmm. How do you like this voice, darling?");
            speakWithPiper(s, s);
        }
        return;
    }
    const QList<QVoice> voices = m_tts.availableVoices();
    if (voices.isEmpty()) { return; }
    const QString cur = m_tts.voice().name();
    int idx = -1;
    for (int i = 0; i < voices.size(); ++i) {
        if (voices.at(i).name() == cur) { idx = i; break; }
    }
    const QVoice next = voices.at((idx + 1) % voices.size());
    m_tts.setVoice(next);
    QSettings().setValue(QStringLiteral("lotei/voice"), next.name());
    emit voiceChanged();
    if (!m_muted) {
        const QString sample = QStringLiteral("Mmm, how about this voice?");
        applyProsody(sample);
        m_tts.stop();
        m_tts.say(sample);
    }
#endif
}

QString LoteiBackend::modelName() const
{
    return m_model;
}

QStringList LoteiBackend::availableModels() const
{
    return m_models;
}

void LoteiBackend::setModel(const QString &model)
{
    if (model.isEmpty() || model == m_model) { return; }
    m_model = model;
    QSettings().setValue(QStringLiteral("lotei/model"), m_model);
    emit modelChanged();
}

void LoteiBackend::cycleModel()
{
    if (m_models.isEmpty()) { refreshModels(); return; }   // none discovered yet; (re)fetch
    const int idx = m_models.indexOf(m_model);
    setModel(m_models.at((idx + 1) % m_models.size()));    // idx == -1 -> first
}

void LoteiBackend::refreshModels()
{
    QNetworkRequest req{QUrl(QStringLiteral("http://localhost:11434/api/tags"))};
    QNetworkReply *reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const bool online = (reply->error() == QNetworkReply::NoError);
        if (online != m_ollamaOnline) { m_ollamaOnline = online; emit modelChanged(); }
        if (!online) { return; }   // Ollama down/unreachable
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        QStringList found;
        const QJsonArray arr = obj.value(QStringLiteral("models")).toArray();
        for (const QJsonValue &v : arr) {
            const QString name = v.toObject().value(QStringLiteral("name")).toString();
            if (!name.isEmpty()) { found << name; }
        }
        found.sort(Qt::CaseInsensitive);
        if (found != m_models) {
            m_models = found;
            emit modelChanged();   // let the switcher pick up the discovered list
        }
    });
}

bool LoteiBackend::setupComplete() const { return m_setupComplete; }
bool LoteiBackend::ollamaOnline() const  { return m_ollamaOnline; }
QString LoteiBackend::manualName() const { return m_manualName; }

void LoteiBackend::setManualName(const QString &name)
{
    if (name == m_manualName) { return; }
    m_manualName = name;
    QSettings().setValue(QStringLiteral("lotei/manualName"), m_manualName);
    emit manualNameChanged();
}

void LoteiBackend::completeSetup()
{
    if (m_setupComplete) { return; }
    m_setupComplete = true;
    QSettings().setValue(QStringLiteral("lotei/setupComplete"), true);
    emit setupCompleteChanged();
}

void LoteiBackend::resetSetup()
{
    m_setupComplete = false;
    QSettings().setValue(QStringLiteral("lotei/setupComplete"), false);
    emit setupCompleteChanged();
}

void LoteiBackend::recheckOllama()
{
    refreshModels();
}

QStringList LoteiBackend::personalityPresets() const
{
    return { QStringLiteral("Snarky pink dolphin"),
             QStringLiteral("Chill helper"),
             QStringLiteral("Chaos gremlin"),
             QStringLiteral("Deadpan pro"),
             QStringLiteral("Sweet companion") };
}

void LoteiBackend::applyPreset(const QString &name)
{
    QString persona;
    if (name == QStringLiteral("Chill helper")) {
        persona = QStringLiteral("You are calm, warm and concise -- a laid-back, friendly helper. Light on snark, easy-going, genuinely helpful.");
    } else if (name == QStringLiteral("Chaos gremlin")) {
        persona = QStringLiteral("You are a chaotic, hyper, mischievous gremlin -- playful, unpredictable, high-energy and harmlessly unhinged. Chaos with a heart.");
    } else if (name == QStringLiteral("Deadpan pro")) {
        persona = QStringLiteral("You are dry, deadpan and professional -- efficient, subtle wit, minimal fluff. You just get things done.");
    } else if (name == QStringLiteral("Sweet companion")) {
        persona = QStringLiteral("You are a sweet, supportive companion -- encouraging, gentle, a genuine hype-buddy always in the user's corner.");
    }
    // "Snarky pink dolphin" clears the override -> the built-in default stands.
    QSettings().setValue(QStringLiteral("lotei/personality"), persona);
}

void LoteiBackend::applyNamePersonality()
{
    QSettings().setValue(QStringLiteral("lotei/personality"),
        QStringLiteral("Build and fully embody a personality inspired by your own name -- lean into "
                       "whatever character, vibe or theme the name evokes, and stay consistent in it."));
}

void LoteiBackend::setThinking(bool value)
{
    if (value != m_thinking) {
        m_thinking = value;
        emit thinkingChanged();
    }
}

void LoteiBackend::reset()
{
    m_history = QJsonArray();
    m_toolRounds = 0;
    saveHistory();
}

static QString loteiHistoryPath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) { dir = QDir::tempPath(); }
    QDir().mkpath(dir);
    return dir + QStringLiteral("/lotei-history.json");
}

void LoteiBackend::loadHistory()
{
    QFile f(loteiHistoryPath());
    if (!f.open(QIODevice::ReadOnly)) { return; }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (doc.isArray()) { m_history = doc.array(); }
}

void LoteiBackend::saveHistory()
{
    // Persist only the real conversation -- user prompts + LOTEI's final replies.
    // Skip tool plumbing and the auto health-check so memory stays lean.
    QJsonArray convo;
    bool skipNextAssistant = false;
    for (const QJsonValue &v : m_history) {
        const QJsonObject o = v.toObject();
        const QString role = o.value("role").toString();
        const QString content = o.value("content").toString();

        if (role == QLatin1String("user")) {
            if (content.contains(QStringLiteral("in-character health check"))) {
                skipNextAssistant = true;  // drop the auto health-check + its reply
                continue;
            }
            skipNextAssistant = false;
            convo.append(o);
        } else if (role == QLatin1String("assistant") && o.value("content").isString() &&
                   !content.isEmpty() && !o.contains(QStringLiteral("tool_calls"))) {
            if (skipNextAssistant) { skipNextAssistant = false; continue; }
            // Defence in depth: if a "reply" is really leaked tool-call JSON,
            // don't persist it -- otherwise it becomes a few-shot example that
            // teaches LOTEI to keep printing calls instead of making them.
            if (!salvageToolCalls(content).isEmpty()) { continue; }
            convo.append(QJsonObject{{"role", "assistant"}, {"content", content}});
        }
    }

    const int cap = 30;
    while (convo.size() > cap) { convo.removeAt(0); }

    QFile f(loteiHistoryPath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(convo).toJson(QJsonDocument::Compact));
    }
}

QString LoteiBackend::systemPrompt() const
{
    QString sys = QString::fromUtf8(LOTEI_SYSTEM);

    // Optional personality chosen in the setup wizard (fresh users). If unset,
    // the built-in personality above stands -- a hand-edited LOTEI_SYSTEM is
    // never overridden unless someone deliberately picks a preset.
    const QString persona = QSettings().value(QStringLiteral("lotei/personality")).toString();
    if (!persona.isEmpty()) {
        sys += QStringLiteral("\n\nPERSONALITY -- adopt THIS character (every operational rule above "
                              "still fully applies): ") + persona;
    }

    // The assistant adopts the Flipper's name: the connected device's name if
    // present, else the name given during setup.
    QString name;
    static const QRegularExpression nameRe(QStringLiteral("(?m)^Name:\\s*(.+)$"));
    const QRegularExpressionMatch nm = nameRe.match(m_deviceContext);
    if (nm.hasMatch()) { name = nm.captured(1).trimmed(); }
    if (name.isEmpty()) { name = m_manualName; }
    if (!name.isEmpty()) {
        sys += QStringLiteral("\n\nYOUR NAME -- IMPORTANT: you are bonded to a Flipper Zero named "
            "\"%1\". \"%1\" is YOUR name now: introduce yourself as %1 and refer to yourself as %1, "
            "NOT LOTEI (LOTEI is just your underlying model line).").arg(name);
    }

    if (!m_deviceContext.isEmpty()) {
        sys += QStringLiteral("\n\nLive Flipper device diagnostics:\n") + m_deviceContext;
    }
    return sys;
}

void LoteiBackend::send(const QString &userText, const QString &deviceContext)
{
    if (m_thinking) {
        return;
    }
    m_deviceContext = deviceContext;
    m_toolRounds = 0;
    m_history.append(QJsonObject{{"role", "user"}, {"content", userText}});
    setThinking(true);
    dispatchToOllama();
}

void LoteiBackend::dispatchToOllama()
{
    QJsonArray messages;
    messages.append(QJsonObject{{"role", "system"}, {"content", systemPrompt()}});
    for (const QJsonValue &v : m_history) {
        messages.append(v);
    }

    QJsonObject body;
    body["model"] = m_model;
    body["messages"] = messages;
    // Some models (e.g. Gemma) don't support tool-calling and Ollama 400s the whole
    // request if `tools` is present -> we drop it for those (see onStreamFinished).
    if (!m_noToolModels.contains(m_model)) {
        body["tools"] = loteiTools();
    }
    body["stream"] = true;
    body["keep_alive"] = -1;
    body["options"] = QJsonObject{{"num_ctx", LOTEI_NUM_CTX}};

    QNetworkRequest request{QUrl(QString::fromUtf8(LOTEI_URL))};
    request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/json"));
    request.setTransferTimeout(0);

    m_streamBuf.clear();
    m_streamContent.clear();
    m_streamTools = QJsonArray();

    QNetworkReply *reply = m_net.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_currentReply = reply;
    connect(reply, &QNetworkReply::readyRead, this, [this, reply]() { onStreamData(reply); });
    connect(reply, &QNetworkReply::finished,  this, [this, reply]() { onStreamFinished(reply); });
}

void LoteiBackend::onStreamData(QNetworkReply *reply)
{
    if (reply != m_currentReply) { return; }
    m_streamBuf += reply->readAll();

    int nl;
    while ((nl = m_streamBuf.indexOf('\n')) >= 0) {
        const QByteArray line = m_streamBuf.left(nl).trimmed();
        m_streamBuf.remove(0, nl + 1);
        if (line.isEmpty()) { continue; }

        const QJsonObject obj = QJsonDocument::fromJson(line).object();
        const QJsonObject msg = obj.value("message").toObject();

        const QString delta = msg.value("content").toString();
        if (!delta.isEmpty()) {
            m_streamContent += delta;
            emit partialReceived(m_streamContent);   // live typing
        }
        const QJsonArray tc = msg.value("tool_calls").toArray();
        for (const QJsonValue &v : tc) { m_streamTools.append(v); }

        if (obj.value("done").toBool()) {
            finalizeStream();
            return;
        }
    }
}

void LoteiBackend::finalizeStream()
{
    // A complete response arrived. Tool round, or final answer?
    // Prefer the structured tool_calls; if none came through, salvage any calls
    // the model leaked as plain text (qwen2.5 does this when narrating a batch)
    // so they run instead of being printed at the user.
    QJsonArray toolCalls = m_streamTools;
    if (toolCalls.isEmpty()) {
        toolCalls = salvageToolCalls(m_streamContent);
    }
    if (!toolCalls.isEmpty() && m_toolRounds < LOTEI_MAX_TOOL_ROUNDS) {
        m_history.append(QJsonObject{
            {"role", "assistant"},
            {"content", m_streamContent},
            {"tool_calls", toolCalls}
        });
        m_toolRounds++;
        m_currentReply = nullptr;     // this reply is done; ignore its finished()
        runToolCalls(toolCalls, 0);   // -> dispatchToOllama() again (new reply)
        return;
    }

    m_currentReply = nullptr;
    setThinking(false);
    QString text = stripNonEnglish(m_streamContent);
    if (text.isEmpty()) {
        text = QStringLiteral("...(LOTEI flicks his tail; nothing to say)");
    }
    m_history.append(QJsonObject{{"role", "assistant"}, {"content", text}});
    saveHistory();
    if (!m_muted) { speak(text); }
    emit replyReceived(text);   // QML finalizes the live bubble
}

void LoteiBackend::onStreamFinished(QNetworkReply *reply)
{
    if (reply != m_currentReply) {  // already finalized (done seen) or superseded
        reply->deleteLater();
        return;
    }
    m_currentReply = nullptr;
    const auto netErr = reply->error();
    const QString netErrStr = reply->errorString();
    const QByteArray errBody = m_streamBuf + reply->readAll();  // Ollama's error JSON lands here on a 4xx
    reply->deleteLater();

    // Non-tool models (Gemma, etc.) make Ollama 400 the whole request because we
    // send a `tools` array. Remember it and retry once WITHOUT tools, so the model
    // still works as a chat-only companion (it just can't drive the Flipper).
    if (netErr != QNetworkReply::NoError && errBody.contains("does not support tools")
        && !m_noToolModels.contains(m_model)) {
        m_noToolModels << m_model;
        dispatchToOllama();   // re-send tools-less (dispatch now skips them for this model)
        return;
    }

    setThinking(false);
    if (netErr != QNetworkReply::NoError) {
        QString msg = netErrStr;
        if (netErr == QNetworkReply::ConnectionRefusedError || netErr == QNetworkReply::HostNotFoundError) {
            msg = QStringLiteral("my brain (Ollama) isn't awake. Launch me with the LOTEI shortcut.");
        }
        emit errorOccurred(QStringLiteral("Hrm: %1").arg(msg));
    } else if (!m_streamContent.isEmpty()) {
        const QString text = stripNonEnglish(m_streamContent);
        m_history.append(QJsonObject{{"role", "assistant"}, {"content", text}});
        saveHistory();
        emit replyReceived(text);
    } else {
        emit errorOccurred(QStringLiteral("...(LOTEI lost his train of thought)"));
    }
}

void LoteiBackend::runToolCalls(const QJsonArray &toolCalls, int index)
{
    if (index >= toolCalls.size()) {
        dispatchToOllama();
        return;
    }

    const QJsonObject fn = toolCalls.at(index).toObject().value("function").toObject();
    const QString name = fn.value("name").toString();
    const QJsonObject args = fn.value("arguments").toObject();

    runOneTool(name, args, [this, toolCalls, index](const QString &result) {
        m_history.append(QJsonObject{{"role", "tool"}, {"content", result}});
        runToolCalls(toolCalls, index + 1);
    });
}

void LoteiBackend::runOneTool(const QString &name, const QJsonObject &args, std::function<void(const QString &)> done)
{
    Flipper::FlipperZero *dev = m_appBackend ? m_appBackend->device() : nullptr;
    const bool ready = m_appBackend && dev &&
                       m_appBackend->backendState() == ApplicationBackend::BackendState::Ready;
    if (!ready) {
        done(QStringLiteral("{\"error\":\"No Flipper is connected or ready right now.\"}"));
        return;
    }

    if (name == QLatin1String("list_files")) {
        const QByteArray path = args.value("path").toString(QStringLiteral("/ext")).toUtf8();
        if (const QString err = badStoragePath(QString::fromUtf8(path)); !err.isEmpty()) { done(err); return; }
        auto *op = dev->rpc()->storageList(path);
        connect(op, &AbstractOperation::finished, this, [op, done]() {
            if (op->isError()) {
                done(QStringLiteral("{\"error\":\"%1\"}").arg(op->errorString()));
                return;
            }
            QJsonArray arr;
            const auto &files = op->files();
            for (const FileInfo &f : files) {
                arr.append(QJsonObject{
                    {"name", QString::fromUtf8(f.name)},
                    {"type", f.type == FileType::Directory ? "dir" : "file"},
                    {"size", static_cast<double>(f.size)}
                });
            }
            done(QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
        });

    } else if (name == QLatin1String("read_file")) {
        const QByteArray path = args.value("path").toString().toUtf8();
        if (path.isEmpty()) {
            done(QStringLiteral("{\"error\":\"no path given\"}"));
            return;
        }
        if (const QString err = badStoragePath(QString::fromUtf8(path)); !err.isEmpty()) { done(err); return; }
        QBuffer *buf = new QBuffer(this);
        buf->open(QIODevice::ReadWrite);
        auto *op = dev->rpc()->storageRead(path, buf);
        connect(op, &AbstractOperation::finished, this, [op, buf, done]() {
            QString result;
            if (op->isError()) {
                result = QStringLiteral("{\"error\":\"%1\"}").arg(op->errorString());
            } else {
                QByteArray d = buf->data();
                const bool truncated = d.size() > LOTEI_READ_CAP;
                if (truncated) {
                    d = d.left(LOTEI_READ_CAP);
                }
                result = QString::fromUtf8(d);
                if (truncated) {
                    result += QStringLiteral("\n...(truncated)");
                }
                if (result.isEmpty()) {
                    result = QStringLiteral("(empty file)");
                }
            }
            buf->deleteLater();
            done(result);
        });

    } else if (name == QLatin1String("press_button")) {
        const QString b = args.value("button").toString().toLower();
        int times = args.value("times").toInt(1);
        if (times < 1) times = 1;
        if (times > LOTEI_MAX_PRESSES) times = LOTEI_MAX_PRESSES;

        int key = -1;
        if (b == QLatin1String("up")) key = InputEvent::Up;
        else if (b == QLatin1String("down")) key = InputEvent::Down;
        else if (b == QLatin1String("left")) key = InputEvent::Left;
        else if (b == QLatin1String("right")) key = InputEvent::Right;
        else if (b == QLatin1String("ok") || b == QLatin1String("enter") || b == QLatin1String("center")) key = InputEvent::Ok;
        else if (b == QLatin1String("back")) key = InputEvent::Back;

        if (key < 0) {
            done(QStringLiteral("{\"error\":\"unknown button '%1' (use up/down/left/right/ok/back)\"}").arg(b));
            return;
        }

        // Replicate a real D-pad tap (Press + Short + Release) per press.
        Flipper::Zero::ProtobufSession *rpc = dev->rpc();
        Flipper::Zero::GuiSendInputOperation *lastOp = nullptr;
        for (int i = 0; i < times; ++i) {
            rpc->guiSendInput(key, InputEvent::Press);
            rpc->guiSendInput(key, InputEvent::Short);
            lastOp = rpc->guiSendInput(key, InputEvent::Release);
        }
        if (lastOp) {
            connect(lastOp, &AbstractOperation::finished, this, [b, times, done]() {
                done(QStringLiteral("{\"pressed\":\"%1\",\"times\":%2}").arg(b).arg(times));
            });
        } else {
            done(QStringLiteral("{\"error\":\"nothing pressed\"}"));
        }

    } else if (name == QLatin1String("save_file")) {
        const QByteArray path = args.value("path").toString().toUtf8();
        const QString content = args.value("content").toString();
        if (path.isEmpty()) {
            done(QStringLiteral("{\"error\":\"no path given\"}"));
            return;
        }
        QBuffer *buf = new QBuffer(this);
        buf->setData(content.toUtf8());
        buf->open(QIODevice::ReadOnly);
        auto *op = dev->rpc()->storageWrite(path, buf);
        connect(op, &AbstractOperation::finished, this, [op, buf, path, done]() {
            QString result;
            if (op->isError()) {
                result = QStringLiteral("{\"error\":\"%1\"}").arg(op->errorString());
            } else {
                result = QStringLiteral("{\"saved\":\"%1\"}").arg(QString::fromUtf8(path));
            }
            buf->deleteLater();
            done(result);
        });

    } else {
        done(QStringLiteral("{\"error\":\"unknown tool '%1'\"}").arg(name));
    }
}

// ---- LoteiPalette ---------------------------------------------------------

LoteiPalette::LoteiPalette(QObject *parent)
    : QObject(parent)
{
    // Default (pink) palette -- mirrors Theme.qml's original values. This list's
    // order is also the order the editor lists the colors in.
    const QList<QPair<QString, QString>> defs = {
        {QStringLiteral("lightorange1"),  QStringLiteral("#fd8cff")},
        {QStringLiteral("lightorange2"),  QStringLiteral("#fd8cff")},
        {QStringLiteral("lightorange3"),  QStringLiteral("#ac5fae")},
        {QStringLiteral("darkorange1"),   QStringLiteral("#3d223d")},
        {QStringLiteral("darkorange2"),   QStringLiteral("#3a203b")},
        {QStringLiteral("mediumorange1"), QStringLiteral("#a159a2")},
        {QStringLiteral("mediumorange2"), QStringLiteral("#6c3c6d")},
        {QStringLiteral("mediumorange3"), QStringLiteral("#583159")},
        {QStringLiteral("mediumorange4"), QStringLiteral("#8a4c8b")},
        {QStringLiteral("mediumorange5"), QStringLiteral("#915092")},
        {QStringLiteral("lightgreen"),    QStringLiteral("#2ed832")},
        {QStringLiteral("mediumgreen1"),  QStringLiteral("#285b12")},
        {QStringLiteral("mediumgreen2"),  QStringLiteral("#203812")},
        {QStringLiteral("darkgreen"),     QStringLiteral("#0c160c")},
        {QStringLiteral("lightblue"),     QStringLiteral("#be69bf")},
        {QStringLiteral("mediumblue"),    QStringLiteral("#532e53")},
        {QStringLiteral("darkblue1"),     QStringLiteral("#492849")},
        {QStringLiteral("darkblue2"),     QStringLiteral("#3e223e")},
        {QStringLiteral("lightred1"),     QStringLiteral("#d174d3")},
        {QStringLiteral("lightred2"),     QStringLiteral("#cf73d1")},
        {QStringLiteral("lightred3"),     QStringLiteral("#945295")},
        {QStringLiteral("lightred4"),     QStringLiteral("#ae60af")},
        {QStringLiteral("mediumred1"),    QStringLiteral("#7b447c")},
        {QStringLiteral("mediumred2"),    QStringLiteral("#583058")},
        {QStringLiteral("darkred1"),      QStringLiteral("#3b203b")},
        {QStringLiteral("darkred2"),      QStringLiteral("#2a172a")}
    };
    for (const auto &p : defs) {
        m_order << p.first;
        m_defaults.insert(p.first, QColor(p.second));
    }
    m_colors = m_defaults;
    load();

    // Debounce disk writes so dragging a slider doesn't hammer QSettings.
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(400);
    connect(m_saveTimer, &QTimer::timeout, this, [this]() { save(); });
}

QString LoteiPalette::hex(const QString &name) const
{
    return m_colors.value(name).value<QColor>().name(QColor::HexRgb);
}

void LoteiPalette::setColor(const QString &name, const QColor &c)
{
    if (!m_colors.contains(name) || !c.isValid()) { return; }
    if (m_colors.value(name).value<QColor>() == c) { return; }
    m_colors[name] = c;
    m_saveTimer->start();   // debounced; live recolor without per-drag disk writes
    emit changed();
}

void LoteiPalette::reset()
{
    m_colors = m_defaults;
    save();
    emit changed();
}

void LoteiPalette::load()
{
    const QString json = QSettings().value(QStringLiteral("lotei/palette")).toString();
    if (json.isEmpty()) { return; }
    const QJsonObject obj = QJsonDocument::fromJson(json.toUtf8()).object();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        if (m_colors.contains(it.key())) {
            const QColor c(it.value().toString());
            if (c.isValid()) { m_colors[it.key()] = c; }
        }
    }
}

void LoteiPalette::save() const
{
    QJsonObject obj;
    for (auto it = m_colors.constBegin(); it != m_colors.constEnd(); ++it) {
        obj.insert(it.key(), it.value().value<QColor>().name(QColor::HexRgb));
    }
    QSettings().setValue(QStringLiteral("lotei/palette"),
                         QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

// ============================ FirmwareStore ============================

// Short display label for a channel id.
static QString fwChannelLabel(const QString &id)
{
    if (id == QLatin1String("development"))       { return QStringLiteral("dev"); }
    if (id == QLatin1String("release-candidate"))  { return QStringLiteral("rc"); }
    return id;   // "release", "dev"
}

FirmwareStore::FirmwareStore(QObject *parent)
    : QObject(parent)
{
    m_net.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

    const QString rel = QStringLiteral("release");
    const QStringList git = { QStringLiteral("release"), QStringLiteral("dev") };

    // fields: name, kind, locator, blurb, channels, wantChannel, latest, tgzUrl, status, raw
    m_sources = {
        { QStringLiteral("Official"),    Kind::DirJson,
          QStringLiteral("https://update.flipperzero.one/firmware/directory.json"),
          QStringLiteral("Stock Flipper Devices firmware."),      {},  rel, {}, {}, {}, {} },
        { QStringLiteral("Momentum"),    Kind::DirJson,
          QStringLiteral("https://up.momentum-fw.dev/firmware/directory.json"),
          QStringLiteral("Feature-rich community firmware."),     {},  rel, {}, {}, {}, {} },
        { QStringLiteral("Unleashed"),   Kind::GitHub,
          QStringLiteral("DarkFlippers/unleashed-firmware"),
          QStringLiteral("Popular unlocked community firmware."), git, rel, {}, {}, {}, {} },
        { QStringLiteral("RogueMaster"), Kind::GitHub,
          QStringLiteral("RogueMaster/flipperzero-firmware-wPlugins"),
          QStringLiteral("Everything, plus the kitchen sink."),   git, rel, {}, {}, {}, {} },
        // ARF ships only dev-tagged releases, so give it a single "dev" channel.
        { QStringLiteral("ARF"),         Kind::GitHub,
          QStringLiteral("D4C1-Labs/Flipper-ARF"),
          QStringLiteral("Automotive research: car keyfobs / Sub-GHz. Niche."),
          { QStringLiteral("dev") }, QStringLiteral("dev"), {}, {}, {}, {} },
        // Xero publishes versioned releases (flipper-z-f7-update-local.tgz).
        { QStringLiteral("Xero"),        Kind::GitHub,
          QStringLiteral("noproto/xero-firmware"),
          QStringLiteral("Lean official-based community firmware."),
          { QStringLiteral("release") }, QStringLiteral("release"), {}, {}, {}, {} },
    };

    // Restore each firmware's remembered channel choice.
    QSettings st;
    for (Source &s : m_sources) {
        const QString saved = st.value(QStringLiteral("firmware/ch/") + s.name).toString();
        if (!saved.isEmpty()) { s.wantChannel = saved; }
    }
}

void FirmwareStore::setOpen(bool value)
{
    if (value == m_open) { return; }
    m_open = value;
    emit openChanged();
    if (m_open) { refresh(); }   // freshen versions each time the panel opens
}

void FirmwareStore::setBusy(bool value)
{
    if (value == m_busy) { return; }
    m_busy = value;
    emit busyChanged();
}

QVariantList FirmwareStore::sources() const
{
    QVariantList out;
    for (const Source &s : m_sources) {
        QVariantMap m;
        m.insert(QStringLiteral("name"), s.name);
        m.insert(QStringLiteral("blurb"), s.blurb);
        m.insert(QStringLiteral("latest"), s.latest);
        m.insert(QStringLiteral("status"), s.status);
        m.insert(QStringLiteral("ready"), s.status == QLatin1String("ready") && !s.tgzUrl.isEmpty());
        m.insert(QStringLiteral("channel"), fwChannelLabel(currentChannelId(s)));
        m.insert(QStringLiteral("channelCount"), s.channels.size());
        out.append(m);
    }
    return out;
}

void FirmwareStore::refresh()
{
    for (int i = 0; i < m_sources.size(); ++i) {
        m_sources[i].status = QStringLiteral("checking");
        m_sources[i].latest.clear();
        m_sources[i].tgzUrl.clear();
    }
    emit changed();
    for (int i = 0; i < m_sources.size(); ++i) { fetchOne(i); }
}

QString FirmwareStore::currentChannelId(const Source &s) const
{
    if (s.channels.contains(s.wantChannel)) { return s.wantChannel; }
    return s.channels.isEmpty() ? s.wantChannel : s.channels.first();
}

void FirmwareStore::fetchOne(int index)
{
    if (index < 0 || index >= m_sources.size()) { return; }
    const Source src = m_sources.at(index);

    // GitHub: pull the whole release list (newest-first) so release/dev come from one fetch.
    QUrl url = (src.kind == Kind::DirJson)
             ? QUrl(src.locator)
             : QUrl(QStringLiteral("https://api.github.com/repos/%1/releases?per_page=30").arg(src.locator));

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "Hyper-Zero-UI");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, index]() {
        reply->deleteLater();
        if (index < 0 || index >= m_sources.size()) { return; }
        Source &s = m_sources[index];

        if (reply->error() != QNetworkReply::NoError) {
            s.status = QStringLiteral("error");
            emit changed();
            return;
        }
        s.raw = reply->readAll();

        // Discover the channel list from the directory.json, but keep only the
        // canonical channels -- Momentum also lists dozens of per-PR preview
        // channels (long ids like "pr294:feat/...") that we don't want to cycle.
        if (s.kind == Kind::DirJson) {
            const QJsonArray channels = QJsonDocument::fromJson(s.raw).object()
                                        .value(QStringLiteral("channels")).toArray();
            auto hasChannel = [&channels](const QString &id) {
                for (const QJsonValue &cv : channels) {
                    if (cv.toObject().value(QStringLiteral("id")).toString() == id) { return true; }
                }
                return false;
            };
            static const QStringList canonical = {
                QStringLiteral("release"), QStringLiteral("release-candidate"), QStringLiteral("development") };
            QStringList ids;
            for (const QString &c : canonical) { if (hasChannel(c)) { ids << c; } }
            s.channels = ids;
        }

        deriveFromCache(index);
        emit changed();
    });
}

void FirmwareStore::deriveFromCache(int index)
{
    if (index < 0 || index >= m_sources.size()) { return; }
    Source &s = m_sources[index];
    const QString ch = currentChannelId(s);
    s.latest.clear();
    s.tgzUrl.clear();
    if (s.raw.isEmpty()) { s.status = QStringLiteral("error"); return; }

    if (s.kind == Kind::DirJson) {
        const QJsonArray channels = QJsonDocument::fromJson(s.raw).object()
                                    .value(QStringLiteral("channels")).toArray();
        for (const QJsonValue &cv : channels) {
            const QJsonObject c = cv.toObject();
            if (c.value(QStringLiteral("id")).toString() != ch) { continue; }
            const QJsonArray versions = c.value(QStringLiteral("versions")).toArray();
            if (versions.isEmpty()) { break; }
            const QJsonObject v0 = versions.first().toObject();
            s.latest = v0.value(QStringLiteral("version")).toString();
            for (const QJsonValue &fv : v0.value(QStringLiteral("files")).toArray()) {
                const QJsonObject f = fv.toObject();
                if (f.value(QStringLiteral("target")).toString() == QLatin1String("f7") &&
                    f.value(QStringLiteral("type")).toString() == QLatin1String("update_tgz")) {
                    s.tgzUrl = f.value(QStringLiteral("url")).toString();
                    break;
                }
            }
            break;
        }
    } else {   // GitHub: "dev" = newest release, "release" = newest non-prerelease
        const QJsonArray rels = QJsonDocument::fromJson(s.raw).array();
        QJsonObject chosen;
        if (ch == QLatin1String("dev")) {
            if (!rels.isEmpty()) { chosen = rels.first().toObject(); }
        } else {
            for (const QJsonValue &rv : rels) {
                const QJsonObject r = rv.toObject();
                if (!r.value(QStringLiteral("prerelease")).toBool()) { chosen = r; break; }
            }
            if (chosen.isEmpty() && !rels.isEmpty()) { chosen = rels.first().toObject(); }
        }
        if (!chosen.isEmpty()) {
            s.latest = chosen.value(QStringLiteral("tag_name")).toString();
            QString bestUrl, bestName, anyUrl, anyName;
            for (const QJsonValue &av : chosen.value(QStringLiteral("assets")).toArray()) {
                const QJsonObject a = av.toObject();
                const QString name = a.value(QStringLiteral("name")).toString();
                if (!name.endsWith(QLatin1String(".tgz"))) { continue; }
                const QString dl = a.value(QStringLiteral("browser_download_url")).toString();
                if (anyName.isEmpty() || name.size() < anyName.size()) { anyName = name; anyUrl = dl; }
                if (name.contains(QLatin1String("f7")) && name.contains(QLatin1String("update"))) {
                    if (bestName.isEmpty() || name.size() < bestName.size()) { bestName = name; bestUrl = dl; }
                }
            }
            s.tgzUrl = bestUrl.isEmpty() ? anyUrl : bestUrl;
        }
    }

    s.status = (!s.latest.isEmpty() && !s.tgzUrl.isEmpty()) ? QStringLiteral("ready")
                                                            : QStringLiteral("error");
}

void FirmwareStore::cycleChannel(int index)
{
    if (index < 0 || index >= m_sources.size()) { return; }
    Source &s = m_sources[index];
    if (s.channels.size() < 2) { return; }
    int i = s.channels.indexOf(currentChannelId(s));
    i = (i + 1) % s.channels.size();
    s.wantChannel = s.channels.at(i);
    QSettings().setValue(QStringLiteral("firmware/ch/") + s.name, s.wantChannel);
    deriveFromCache(index);   // every channel is already cached -> instant, no re-fetch
    emit changed();
}

void FirmwareStore::install(int index)
{
    if (index < 0 || index >= m_sources.size()) { return; }
    const Source src = m_sources.at(index);

    if (src.status != QLatin1String("ready") || src.tgzUrl.isEmpty()) {
        emit failed(index, QStringLiteral("No downloadable build found -- try re-checking."));
        return;
    }
    if (m_busy) {
        emit failed(index, QStringLiteral("A download is already in progress."));
        return;
    }

    QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (dir.isEmpty()) { dir = QDir::tempPath(); }
    dir += QStringLiteral("/firmware");
    QDir().mkpath(dir);

    const QUrl url(src.tgzUrl);
    QString fileName = url.fileName();
    if (fileName.isEmpty() || !fileName.endsWith(QLatin1String(".tgz"))) {
        fileName = QStringLiteral("flipper-z-f7-update.tgz");
    }
    const QString outPath = dir + QStringLiteral("/") + fileName;

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "Hyper-Zero-UI");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    setBusy(true);
    emit progress(index, 0.0, QStringLiteral("Downloading %1…").arg(src.latest));

    QNetworkReply *reply = m_net.get(req);
    connect(reply, &QNetworkReply::downloadProgress, this, [this, index](qint64 rec, qint64 total) {
        const qreal frac = (total > 0) ? (qreal)rec / (qreal)total : 0.0;
        emit progress(index, frac, QStringLiteral("Downloading…"));
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, index, outPath]() {
        reply->deleteLater();
        setBusy(false);
        if (reply->error() != QNetworkReply::NoError) {
            emit failed(index, QStringLiteral("Download failed: %1").arg(reply->errorString()));
            return;
        }
        QFile f(outPath);
        if (!f.open(QIODevice::WriteOnly)) {
            emit failed(index, QStringLiteral("Couldn't save the download to disk."));
            return;
        }
        f.write(reply->readAll());
        f.close();
        emit progress(index, 1.0, QStringLiteral("Ready -- flashing…"));
        emit readyToInstall(QUrl::fromLocalFile(outPath).toString());
    });
}

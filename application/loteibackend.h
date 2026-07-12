#pragma once

#include <functional>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QColor>
#include <QVariant>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#ifdef HZUI_VOICE
#include <QTextToSpeech>
#include <QVoice>
#endif
#include <QTimer>

class ApplicationBackend;
class QNetworkReply;
class QProcess;
class QMediaPlayer;
class QAudioOutput;

// LOTEI - a local-AI (Ollama) chat assistant inside qFlipper, with tool access
// to live-query the connected Flipper Zero over qFlipper's RPC link.
class LoteiBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool thinking READ thinking NOTIFY thinkingChanged)
    Q_PROPERTY(bool configured READ configured CONSTANT)
    Q_PROPERTY(bool hasAudio READ hasAudio CONSTANT)   // false on Linux (no QtMultimedia): hides voice + music
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(QString voiceName READ voiceName NOTIFY voiceChanged)
    Q_PROPERTY(QString modelName READ modelName NOTIFY modelChanged)
    Q_PROPERTY(qreal voiceVolume READ voiceVolume WRITE setVoiceVolume NOTIFY voiceVolumeChanged)
    Q_PROPERTY(qreal musicVolume READ musicVolume WRITE setMusicVolume NOTIFY musicVolumeChanged)
    Q_PROPERTY(bool setupComplete READ setupComplete NOTIFY setupCompleteChanged)
    Q_PROPERTY(bool ollamaOnline READ ollamaOnline NOTIFY modelChanged)
    Q_PROPERTY(QString manualName READ manualName WRITE setManualName NOTIFY manualNameChanged)

public:
    explicit LoteiBackend(QObject *parent = nullptr);

    // Gives LOTEI access to the connected device (for the inspection tools).
    void setAppBackend(ApplicationBackend *backend);

    bool thinking() const;
    bool configured() const;
    bool hasAudio() const;   // true only where voice/music (QtMultimedia) is compiled in
    bool muted() const;
    void setMuted(bool value);
    QString voiceName() const;
    QString modelName() const;
    qreal voiceVolume() const;
    void setVoiceVolume(qreal value);
    qreal musicVolume() const;
    void setMusicVolume(qreal value);
    bool setupComplete() const;
    bool ollamaOnline() const;
    QString manualName() const;
    void setManualName(const QString &name);

    Q_INVOKABLE void send(const QString &userText, const QString &deviceContext);
    Q_INVOKABLE void reset();
    Q_INVOKABLE void cycleVoice();
    Q_INVOKABLE void cycleModel();                    // switch to the next installed Ollama model
    Q_INVOKABLE void setModel(const QString &model);  // pick a specific model
    Q_INVOKABLE QStringList availableModels() const;  // models installed in Ollama
    Q_INVOKABLE QString musicFolderUrl() const;   // <appdir>/Music as a file URL

    // First-run setup wizard
    Q_INVOKABLE void completeSetup();                    // mark the wizard done
    Q_INVOKABLE void resetSetup();                       // re-trigger the wizard (testing)
    Q_INVOKABLE void recheckOllama();                    // re-query /api/tags (AI step)
    Q_INVOKABLE QStringList personalityPresets() const;  // preset persona names
    Q_INVOKABLE void applyPreset(const QString &name);   // set persona to a preset
    Q_INVOKABLE void applyNamePersonality();             // persona built from the Flipper's name

signals:
    void replyReceived(const QString &text);
    void errorOccurred(const QString &text);
    void thinkingChanged();
    void mutedChanged();
    void voiceChanged();
    void modelChanged();
    void voiceVolumeChanged();
    void musicVolumeChanged();
    void setupCompleteChanged();
    void manualNameChanged();
    void partialReceived(const QString &text);   // live-typing: reply text so far

private:
    void setThinking(bool value);
    QString systemPrompt() const;
    void applyProsody(const QString &text);   // nudge SAPI pitch/rate to match mood (fallback)
    void speak(const QString &text);          // route to Piper if present, else SAPI
    void speakWithPiper(const QString &spoken, const QString &moodText);
    double piperLengthScale(const QString &moodText) const;
    void discoverPiper();
    void refreshModels();   // query Ollama /api/tags for installed models

    void loadHistory();   // restore past conversation from disk
    void saveHistory();   // persist conversation (user + final replies only)

    void dispatchToOllama();                                   // POST history + tools
    void onStreamData(QNetworkReply *reply);      // parse streamed NDJSON chunks
    void onStreamFinished(QNetworkReply *reply);
    void finalizeStream();                        // a full response arrived
    void runToolCalls(const QJsonArray &toolCalls, int index); // execute tools sequentially
    void runOneTool(const QString &name, const QJsonObject &args,
                    std::function<void(const QString &)> done); // one async RPC tool

    QNetworkAccessManager m_net;
    ApplicationBackend *m_appBackend = nullptr;

    QJsonArray m_history;        // running messages (user / assistant / tool)
    QString    m_deviceContext;  // latest diagnostics snapshot from QML
    int        m_toolRounds = 0;
    bool       m_thinking = false;
    bool       m_muted = false;
    qreal      m_voiceVolume = 1.0;
    qreal      m_musicVolume = 0.55;
    QString     m_model;    // selected Ollama model (persisted)
    QStringList m_models;   // models discovered via /api/tags
    QStringList m_noToolModels;  // models Ollama rejects tools for (e.g. Gemma) -> chat-only
    bool        m_setupComplete = false;
    bool        m_ollamaOnline = false;
    QString     m_manualName;   // Flipper name from setup (fallback when no device)
#ifdef HZUI_VOICE
    QTextToSpeech m_tts;   // SAPI fallback engine
#endif

    // Piper neural TTS (primary, when piper.exe + voices sit next to the app)
    bool          m_piperOk = false;
    QString       m_piperExe;
    QStringList   m_piperVoices;
    int           m_piperVoiceIdx = 0;
    QProcess     *m_piperProc = nullptr;
    QMediaPlayer *m_voicePlayer = nullptr;
    QAudioOutput *m_voiceAudio = nullptr;
    QString       m_voiceTmpDir;
    int           m_voiceSeq = 0;

    QByteArray m_streamBuf;       // buffer for partial streamed lines
    QString    m_streamContent;   // accumulated reply text this response
    QJsonArray m_streamTools;     // accumulated tool calls this response
    QNetworkReply *m_currentReply = nullptr;
};

// Runtime-editable color palette. Every Theme.color.* value flows from here, so
// changing one recolors the whole UI live. Persists to QSettings.
class LoteiPalette : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap colors READ colors NOTIFY changed)

public:
    explicit LoteiPalette(QObject *parent = nullptr);

    QVariantMap colors() const { return m_colors; }

    Q_INVOKABLE QStringList names() const { return m_order; }     // editable colors, in order
    Q_INVOKABLE QString hex(const QString &name) const;           // "#rrggbb" of a color
    Q_INVOKABLE void setColor(const QString &name, const QColor &c);
    Q_INVOKABLE void reset();                                     // back to the pink defaults

signals:
    void changed();

private:
    void load();
    void save() const;

    QVariantMap m_defaults;
    QVariantMap m_colors;
    QStringList m_order;
    QTimer     *m_saveTimer = nullptr;
};

// Tracks community Flipper firmwares (Official, Momentum, Unleashed, RogueMaster),
// fetches each one's latest version live, and downloads the update .tgz so
// qFlipper's normal installer (ApplicationBackend::installFirmware) can flash it.
// A Flipper runs one firmware at a time -- this makes the latest build of any
// fork one click away; it never flashes without an explicit click.
class FirmwareStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool open READ isOpen WRITE setOpen NOTIFY openChanged)
    Q_PROPERTY(QVariantList sources READ sources NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit FirmwareStore(QObject *parent = nullptr);

    bool isOpen() const { return m_open; }
    void setOpen(bool value);
    QVariantList sources() const;
    bool busy() const { return m_busy; }

    Q_INVOKABLE void refresh();               // (re)fetch the latest version of every source
    Q_INVOKABLE void install(int index);      // download that source's latest .tgz
    Q_INVOKABLE void cycleChannel(int index); // switch a source's channel (release/dev/rc)

signals:
    void openChanged();
    void changed();
    void busyChanged();
    void readyToInstall(const QString &fileUrl);              // hand off to Backend.installFirmware
    void progress(int index, qreal frac, const QString &note);
    void failed(int index, const QString &message);

private:
    enum class Kind { DirJson, GitHub };
    struct Source {
        QString     name;
        Kind        kind;
        QString     locator;      // directory.json URL, or "owner/repo" for GitHub
        QString     blurb;
        QStringList channels;     // available channel ids (discovered for DirJson, fixed for GitHub)
        QString     wantChannel;  // user-selected channel id (persisted); default "release"
        QString     latest;       // discovered version for the selected channel
        QString     tgzUrl;       // discovered download URL for the selected channel
        QString     status;       // "", "checking", "ready", "error"
        QByteArray  raw;          // cached payload, so channel switches need no re-fetch
    };

    void fetchOne(int index);
    void deriveFromCache(int index);                  // recompute latest/tgz for the chosen channel
    QString currentChannelId(const Source &s) const;  // wantChannel, clamped to what's available
    void setBusy(bool value);

    QNetworkAccessManager m_net;
    QList<Source> m_sources;
    bool m_open = false;
    bool m_busy = false;
};

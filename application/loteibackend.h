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
#include <QTextToSpeech>
#include <QVoice>
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
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(QString voiceName READ voiceName NOTIFY voiceChanged)
    Q_PROPERTY(qreal voiceVolume READ voiceVolume WRITE setVoiceVolume NOTIFY voiceVolumeChanged)
    Q_PROPERTY(qreal musicVolume READ musicVolume WRITE setMusicVolume NOTIFY musicVolumeChanged)

public:
    explicit LoteiBackend(QObject *parent = nullptr);

    // Gives LOTEI access to the connected device (for the inspection tools).
    void setAppBackend(ApplicationBackend *backend);

    bool thinking() const;
    bool configured() const;
    bool muted() const;
    void setMuted(bool value);
    QString voiceName() const;
    qreal voiceVolume() const;
    void setVoiceVolume(qreal value);
    qreal musicVolume() const;
    void setMusicVolume(qreal value);

    Q_INVOKABLE void send(const QString &userText, const QString &deviceContext);
    Q_INVOKABLE void reset();
    Q_INVOKABLE void cycleVoice();
    Q_INVOKABLE QString musicFolderUrl() const;   // <appdir>/Music as a file URL

signals:
    void replyReceived(const QString &text);
    void errorOccurred(const QString &text);
    void thinkingChanged();
    void mutedChanged();
    void voiceChanged();
    void voiceVolumeChanged();
    void musicVolumeChanged();
    void partialReceived(const QString &text);   // live-typing: reply text so far

private:
    void setThinking(bool value);
    QString systemPrompt() const;
    void applyProsody(const QString &text);   // nudge SAPI pitch/rate to match mood (fallback)
    void speak(const QString &text);          // route to Piper if present, else SAPI
    void speakWithPiper(const QString &spoken, const QString &moodText);
    double piperLengthScale(const QString &moodText) const;
    void discoverPiper();

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
    QTextToSpeech m_tts;   // SAPI fallback engine

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

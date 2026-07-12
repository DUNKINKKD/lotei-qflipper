#pragma once

#include "flippertransport.h"

QT_BEGIN_NAMESPACE
class QSerialPort;
QT_END_NAMESPACE

namespace Flipper {
namespace Zero {

// FlipperTransport over USB CDC. Wraps an already-opened QSerialPort (as produced
// by SerialInitHelper) and takes ownership of it -- behaviour is byte-for-byte
// what ProtobufSession did before, just behind the transport interface.
class SerialTransport : public FlipperTransport
{
    Q_OBJECT

public:
    explicit SerialTransport(QSerialPort *port, QObject *parent = nullptr);

    bool open() override;
    void close() override;
    qint64 write(const QByteArray &data) override;
    QByteArray readAll() override;
    void clear() override;
    bool flush() override;
    QString errorString() const override;

private:
    QSerialPort *m_port;
};

}
}

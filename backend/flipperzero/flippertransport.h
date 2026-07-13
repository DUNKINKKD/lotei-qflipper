#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>

namespace Flipper {
namespace Zero {

// Abstracts the raw byte transport beneath ProtobufSession so a Flipper can be
// reached over USB serial OR Bluetooth LE. It mirrors exactly the slice of
// QSerialPort that ProtobufSession relied on (the readyRead/bytesWritten/
// errorOccurred signals + write/readAll/clear/flush/close/errorString), so the
// session becomes transport-agnostic with a near-mechanical swap.
//
//   SerialTransport -- wraps QSerialPort (USB CDC), unchanged behaviour.
//   BleTransport    -- the Flipper's Serial GATT service over BLE (see the
//                      ble-spike branch for the proven ping/pong).
class FlipperTransport : public QObject
{
    Q_OBJECT

public:
    explicit FlipperTransport(QObject *parent = nullptr) : QObject(parent) {}
    ~FlipperTransport() override = default;

    // Open/close the underlying link.
    //
    // For a synchronous link (USB serial) open() returns true once the port is
    // ready and callers may proceed immediately. For an asynchronous link (BLE,
    // whose scan/connect/GATT-discovery take time) open() returns true to mean
    // "kicked off successfully" and the transport later emits opened() when it is
    // actually ready to carry RPC, or openFailed() if it could not connect.
    // A false return (or openFailed) sets errorString.
    virtual bool open() = 0;
    virtual void close() = 0;

    // Queue bytes for transmission; returns the number accepted (like QIODevice::write).
    virtual qint64 write(const QByteArray &data) = 0;

    // Drain everything received so far.
    virtual QByteArray readAll() = 0;

    // Discard any buffered in/out data (QSerialPort::clear equivalent).
    virtual void clear() = 0;

    // Ensure queued writes have actually gone out; returns false on failure.
    virtual bool flush() = 0;

    virtual QString errorString() const = 0;

signals:
    void opened();                     // async link is now ready to carry RPC
    void openFailed(const QString &e); // async link could not be established
    void readyRead();                  // new bytes available -> readAll()
    void bytesWritten(qint64 n);       // n bytes physically sent
    void errorOccurred();              // link error; inspect errorString()
};

}
}

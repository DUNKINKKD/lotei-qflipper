#pragma once

#include "flipperzero/flippertransport.h"

#include <QByteArray>
#include <QString>
#include <QBluetoothDeviceInfo>
#include <QBluetoothUuid>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QLowEnergyCharacteristic>
#include <QLowEnergyDescriptor>

// FlipperTransport over Bluetooth LE. Speaks the Flipper's Serial GATT service,
// which carries the exact same RPC/protobuf byte stream as USB CDC -- so once the
// link is up, ProtobufSession neither knows nor cares that it is wireless.
//
// Distilled from the ble-spike (proven ping/pong on real hardware). The important
// firmware quirks, all baked in below:
//   * TX (Flipper -> us) is INDICATE, not Notify -> enable with 0x0200, not 0x0100.
//   * The serial channel is inert until RPC is activated by writing 0x01 to the
//     rpc-status characteristic. We wait for THAT write to complete before firing
//     opened(), because the session sends its first RPC frame the instant it does.
//   * Outgoing frames are chunked to the negotiated ATT MTU and paced on
//     characteristicWritten so we never outrun the Flipper's RX buffer.
class BleTransport : public Flipper::Zero::FlipperTransport
{
    Q_OBJECT

public:
    explicit BleTransport(const QBluetoothDeviceInfo &device, QObject *parent = nullptr);
    ~BleTransport() override;

    bool open() override;          // async: connect + GATT handshake -> opened()/openFailed()
    void close() override;
    qint64 write(const QByteArray &data) override;
    QByteArray readAll() override;
    void clear() override;
    bool flush() override;
    QString errorString() const override;

private:
    void fail(const QString &why);   // pre-open -> openFailed(); post-open -> errorOccurred()
    void finishOpen();               // handshake complete -> opened()
    void activateRpc();              // write rpc-status = 1
    void pumpTx();                   // drain m_outgoing over the RX characteristic

    void onDiscoveryFinished();
    void onServiceStateChanged(QLowEnergyService::ServiceState s);
    void onDescriptorWritten(const QLowEnergyDescriptor &d, const QByteArray &value);
    void onCharacteristicWritten(const QLowEnergyCharacteristic &c, const QByteArray &value);
    void onCharacteristicChanged(const QLowEnergyCharacteristic &c, const QByteArray &value);

    // The Flipper's Serial GATT service + characteristics (from firmware).
    static const QBluetoothUuid kSerialService;
    static const QBluetoothUuid kTxChar;       // indicate: Flipper -> us
    static const QBluetoothUuid kRxChar;       // write:    us -> Flipper
    static const QBluetoothUuid kFlowControl;  // notify:   Flipper's free RX buffer
    static const QBluetoothUuid kRpcStatus;    // write 0x01 to activate RPC

    QBluetoothDeviceInfo      m_device;
    QLowEnergyController     *m_ctrl = nullptr;
    QLowEnergyService        *m_serial = nullptr;
    QLowEnergyCharacteristic  m_rx;          // cached write endpoint

    QByteArray m_incoming;                   // received, awaiting readAll()
    QByteArray m_outgoing;                   // queued for transmission
    bool m_rxWithResponse = true;            // pace writes on characteristicWritten
    bool m_writing = false;                  // a chunk write is in flight
    int  m_mtu = 20;                         // ATT write payload (updated on connect)

    int  m_handshakeStep = 0;                // 0 idle, 1 TX-cccd, 2 flow-cccd, 3 rpc, 4 open
    bool m_opened = false;
    bool m_failed = false;
    bool m_closing = false;
    QString m_error;
};

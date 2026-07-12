#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QList>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QBluetoothUuid>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QLowEnergyCharacteristic>

// Phase-1 BLE spike: scan for the Flipper Zero, connect, find its Serial GATT
// service, subscribe to the TX characteristic, and prove a raw byte pipe works
// (on connect the Flipper streams its CLI banner over TX -- receiving that = win).
// This is a throwaway proof-of-transport; the real thing gets abstracted into
// ProtobufSession later. Exposed to QML as the singleton `Ble`.
class BleSpike : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)         // scrolling log
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged) // [{name,address}]
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)

public:
    explicit BleSpike(QObject *parent = nullptr);
    ~BleSpike() override;

    QString status() const { return m_status; }
    QVariantList devices() const;
    bool scanning() const { return m_scanning; }
    bool connected() const { return m_connected; }

    Q_INVOKABLE void scan();              // start LE discovery (~8s)
    Q_INVOKABLE void connectToDevice(int index);
    Q_INVOKABLE void ping();              // poke the CLI (send "\r\n") to prove the RX write path
    Q_INVOKABLE void disconnectDevice();

signals:
    void statusChanged();
    void devicesChanged();
    void scanningChanged();
    void connectedChanged();

private:
    void log(const QString &line);
    void setScanning(bool v);
    void setConnected(bool v);
    void onDeviceDiscovered(const QBluetoothDeviceInfo &info);
    void onServiceStateChanged(QLowEnergyService::ServiceState s);
    void onCharacteristicChanged(const QLowEnergyCharacteristic &c, const QByteArray &value);

    // The Flipper's Serial GATT service + characteristics (from firmware).
    static const QBluetoothUuid kSerialService;
    static const QBluetoothUuid kTxChar;       // notify: Flipper -> us
    static const QBluetoothUuid kRxChar;       // write:  us -> Flipper
    static const QBluetoothUuid kFlowControl;  // notify: Flipper's free RX buffer (credits)
    static const QBluetoothUuid kRpcStatus;    // write 0x01 to activate the RPC session

    QBluetoothDeviceDiscoveryAgent *m_agent = nullptr;
    QList<QBluetoothDeviceInfo>     m_found;
    QLowEnergyController           *m_ctrl = nullptr;
    QLowEnergyService              *m_serial = nullptr;
    QLowEnergyCharacteristic        m_rx;   // write endpoint (cached once discovered)
    QString m_status;
    bool    m_scanning = false;
    bool    m_connected = false;
};

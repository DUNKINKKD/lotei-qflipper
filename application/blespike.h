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

namespace Flipper { class DeviceRegistry; namespace Zero { class ProtobufSession; } }

// BLE test panel backing object, exposed to QML as the singleton `Ble`.
//
//   Phase 1 (spike):  scan / connectToDevice / ping -- a raw byte pipe straight to
//                     the Flipper's Serial GATT, proving the transport works.
//   Phase 2 (real):   connectSession -- opens an actual ProtobufSession running over
//                     a BleTransport and reads device info through the normal RPC
//                     pipeline, proving the whole app-level stack works wirelessly.
class BleSpike : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)         // scrolling log
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged) // [{name,address}]
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool sessionActive READ sessionActive NOTIFY sessionActiveChanged)

public:
    explicit BleSpike(QObject *parent = nullptr);
    ~BleSpike() override;

    // The real device registry, so a BLE Flipper can become the active device.
    void setDeviceRegistry(Flipper::DeviceRegistry *registry) { m_reg = registry; }

    QString status() const { return m_status; }
    QVariantList devices() const;
    bool scanning() const { return m_scanning; }
    bool connected() const { return m_connected; }
    bool sessionActive() const { return m_sessionActive; }

    Q_INVOKABLE void scan();              // start LE discovery (~8s)
    Q_INVOKABLE void connectToDevice(int index);
    Q_INVOKABLE void ping();              // poke the CLI (send "\r\n") to prove the RX write path
    Q_INVOKABLE void disconnectDevice();

    // Phase 2: open a real ProtobufSession over BleTransport and read device info.
    Q_INVOKABLE void connectSession(int index);
    Q_INVOKABLE void disconnectSession();

    // Phase 3: register the chosen Flipper as the app's ACTIVE device over BLE
    // (device card, screen mirror, LOTEI tools -- all wireless). disconnectAll
    // tears down whatever BLE link is up (registered device, proof session, spike).
    Q_INVOKABLE void connectDevice(int index);
    Q_INVOKABLE void disconnectAll();

signals:
    void statusChanged();
    void devicesChanged();
    void scanningChanged();
    void connectedChanged();
    void sessionActiveChanged();

private:
    void log(const QString &line);
    void setScanning(bool v);
    void setConnected(bool v);
    void setSessionActive(bool v);
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
    // Phase-2 real session (owns its BleTransport as a child).
    Flipper::Zero::ProtobufSession *m_rpc = nullptr;

    // Phase-3 registry (not owned) -- BLE Flipper as the app's active device.
    Flipper::DeviceRegistry *m_reg = nullptr;

    QString m_status;
    bool    m_scanning = false;
    bool    m_connected = false;
    bool    m_sessionActive = false;
};

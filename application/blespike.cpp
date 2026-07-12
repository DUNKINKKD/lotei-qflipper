#include "blespike.h"

#include <QVariantMap>
#include <QLowEnergyDescriptor>
#include <QBluetoothAddress>

// Flipper Serial GATT service + characteristics (from flipperzero-firmware
// targets/f7/ble_glue/services/serial_service_uuid.inc, decoded to standard form).
const QBluetoothUuid BleSpike::kSerialService =
        QBluetoothUuid(QStringLiteral("8fe5b3d5-2e7f-4a98-2a48-7acc60fe0000"));
const QBluetoothUuid BleSpike::kTxChar =
        QBluetoothUuid(QStringLiteral("19ed82ae-ed21-4c9d-4145-228e61fe0000")); // notify: Flipper -> us
const QBluetoothUuid BleSpike::kRxChar =
        QBluetoothUuid(QStringLiteral("19ed82ae-ed21-4c9d-4145-228e62fe0000")); // write:  us -> Flipper
const QBluetoothUuid BleSpike::kFlowControl =
        QBluetoothUuid(QStringLiteral("19ed82ae-ed21-4c9d-4145-228e63fe0000")); // notify: free RX buffer
const QBluetoothUuid BleSpike::kRpcStatus =
        QBluetoothUuid(QStringLiteral("19ed82ae-ed21-4c9d-4145-228e64fe0000")); // write 0x01 = start RPC

BleSpike::BleSpike(QObject *parent)
    : QObject(parent)
{
    m_agent = new QBluetoothDeviceDiscoveryAgent(this);
    m_agent->setLowEnergyDiscoveryTimeout(8000);
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered, this, &BleSpike::onDeviceDiscovered);
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::finished, this, [this]() {
        setScanning(false);
        log(QStringLiteral("scan finished (%1 device(s)).").arg(m_found.size()));
    });
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::errorOccurred, this,
            [this](QBluetoothDeviceDiscoveryAgent::Error) {
        setScanning(false);
        log(QStringLiteral("scan error: ") + m_agent->errorString());
    });
}

BleSpike::~BleSpike()
{
    disconnectDevice();
}

void BleSpike::log(const QString &line)
{
    m_status += line + QLatin1Char('\n');
    if (m_status.size() > 4000) { m_status = m_status.right(3000); }
    emit statusChanged();
}

void BleSpike::setScanning(bool v)  { if (v != m_scanning)  { m_scanning = v;  emit scanningChanged(); } }
void BleSpike::setConnected(bool v) { if (v != m_connected) { m_connected = v; emit connectedChanged(); } }

QVariantList BleSpike::devices() const
{
    QVariantList out;
    for (const QBluetoothDeviceInfo &d : m_found) {
        QVariantMap m;
        m.insert(QStringLiteral("name"), d.name().isEmpty() ? QStringLiteral("(unnamed)") : d.name());
        const QString addr = d.address().toString();
        m.insert(QStringLiteral("address"), addr == QStringLiteral("00:00:00:00:00:00") || addr.isEmpty()
                                             ? d.deviceUuid().toString() : addr);
        out.append(m);
    }
    return out;
}

void BleSpike::scan()
{
    if (m_scanning) { return; }
    m_found.clear();
    emit devicesChanged();
    m_status.clear();
    log(QStringLiteral("scanning for BLE devices (~8s)…"));
    setScanning(true);
    m_agent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
}

void BleSpike::onDeviceDiscovered(const QBluetoothDeviceInfo &info)
{
    if (!(info.coreConfigurations() & QBluetoothDeviceInfo::LowEnergyCoreConfiguration)) { return; }
    for (const QBluetoothDeviceInfo &d : m_found) {
        if (d.deviceUuid() == info.deviceUuid() && d.address() == info.address()) { return; }
    }
    m_found.append(info);
    const bool looksFlipper = info.name().contains(QStringLiteral("Flipper"), Qt::CaseInsensitive)
                           || info.serviceUuids().contains(kSerialService);
    log(QStringLiteral("  found: %1  %2%3").arg(
            info.name().isEmpty() ? QStringLiteral("(unnamed)") : info.name(),
            info.address().toString(),
            looksFlipper ? QStringLiteral("   <-- Flipper?") : QString()));
    emit devicesChanged();
}

void BleSpike::connectToDevice(int index)
{
    if (index < 0 || index >= m_found.size()) { return; }
    disconnectDevice();
    const QBluetoothDeviceInfo info = m_found.at(index);
    log(QStringLiteral("connecting to %1…").arg(info.name()));

    m_ctrl = QLowEnergyController::createCentral(info, this);
    connect(m_ctrl, &QLowEnergyController::connected, this, [this]() {
        log(QStringLiteral("connected; discovering services…"));
        m_ctrl->discoverServices();
    });
    connect(m_ctrl, &QLowEnergyController::disconnected, this, [this]() {
        log(QStringLiteral("disconnected."));
        setConnected(false);
    });
    connect(m_ctrl, &QLowEnergyController::errorOccurred, this, [this](QLowEnergyController::Error) {
        log(QStringLiteral("controller error: ") + m_ctrl->errorString()
            + QStringLiteral("  (on Windows, pair the Flipper in Bluetooth settings first)"));
    });
    connect(m_ctrl, &QLowEnergyController::serviceDiscovered, this, [this](const QBluetoothUuid &u) {
        if (u == kSerialService) { log(QStringLiteral("  found the Flipper Serial service!")); }
    });
    connect(m_ctrl, &QLowEnergyController::discoveryFinished, this, [this]() {
        m_serial = m_ctrl->createServiceObject(kSerialService, this);
        if (!m_serial) { log(QStringLiteral("Serial service NOT present on this device.")); return; }
        connect(m_serial, &QLowEnergyService::stateChanged, this, &BleSpike::onServiceStateChanged);
        connect(m_serial, &QLowEnergyService::characteristicChanged, this, &BleSpike::onCharacteristicChanged);
        connect(m_serial, &QLowEnergyService::characteristicWritten, this,
                [this](const QLowEnergyCharacteristic &c, const QByteArray &v) {
            const QString w = (c.uuid() == kRxChar) ? QStringLiteral("RX")
                            : (c.uuid() == kRpcStatus) ? QStringLiteral("RPCSTAT") : QStringLiteral("?");
            log(QStringLiteral("  [write OK -> %1: %2]").arg(w, QString::fromLatin1(v.toHex())));
        });
        connect(m_serial, &QLowEnergyService::descriptorWritten, this,
                [this](const QLowEnergyDescriptor &, const QByteArray &v) {
            log(QStringLiteral("  [notify-enable written: %1]").arg(QString::fromLatin1(v.toHex())));
        });
        connect(m_serial, &QLowEnergyService::errorOccurred, this, [this](QLowEnergyService::ServiceError e) {
            log(QStringLiteral("  [SERVICE ERROR %1]").arg((int)e));
        });
        log(QStringLiteral("discovering Serial service details…"));
        m_serial->discoverDetails();
    });
    m_ctrl->connectToDevice();
}

void BleSpike::onServiceStateChanged(QLowEnergyService::ServiceState s)
{
    if (s != QLowEnergyService::RemoteServiceDiscovered) { return; }

    const QLowEnergyCharacteristic tx = m_serial->characteristic(kTxChar);
    m_rx = m_serial->characteristic(kRxChar);
    if (!tx.isValid() || !m_rx.isValid()) {
        log(QStringLiteral("TX/RX characteristics missing on the Serial service."));
        return;
    }
    // props bits: Write=0x08, WriteNoResponse=0x04, Notify=0x10, Indicate=0x20
    log(QStringLiteral("  props: TX=0x%1  RX=0x%2")
        .arg((int)tx.properties(), 0, 16).arg((int)m_rx.properties(), 0, 16));

    const QLowEnergyDescriptor cccd =
            tx.descriptor(QBluetoothUuid(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration));
    if (cccd.isValid()) {
        // The Flipper's TX characteristic is INDICATE-type (props 0x22), so enable
        // indications (0200) -- NOT notifications (0100), which delivered nothing.
        m_serial->writeDescriptor(cccd, QByteArray::fromHex("0200"));
        log(QStringLiteral("subscribed to TX indications."));
    }

    // Subscribe to flow control -- the Flipper advertises its free RX-buffer size
    // here; the serial channel stays gated until this handshake is up.
    const QLowEnergyCharacteristic flow = m_serial->characteristic(kFlowControl);
    if (flow.isValid()) {
        const QLowEnergyDescriptor fc =
                flow.descriptor(QBluetoothUuid(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration));
        if (fc.isValid()) {
            m_serial->writeDescriptor(fc, QByteArray::fromHex("0100"));
            log(QStringLiteral("subscribed to flow-control."));
        }
    }

    // Activate the RPC session -- the BLE equivalent of USB's "start_rpc_session".
    // Write 0x01 (true) to the RPC-status characteristic; without it the Flipper
    // ignores protobuf on the serial channel and never replies.
    const QLowEnergyCharacteristic rpcStatus = m_serial->characteristic(kRpcStatus);
    if (rpcStatus.isValid()) {
        const auto mode = (rpcStatus.properties() & QLowEnergyCharacteristic::WriteNoResponse)
                        ? QLowEnergyService::WriteWithoutResponse
                        : QLowEnergyService::WriteWithResponse;
        m_serial->writeCharacteristic(rpcStatus, QByteArray::fromHex("01"), mode);
        log(QStringLiteral("activated RPC session (wrote RPC-status = 1)."));
    } else {
        log(QStringLiteral("RPC-status characteristic not found!"));
    }

    setConnected(true);
    log(QStringLiteral("READY — RPC active. Click PING."));
}

void BleSpike::onCharacteristicChanged(const QLowEnergyCharacteristic &c, const QByteArray &value)
{
    QString which = QStringLiteral("?");
    if      (c.uuid() == kTxChar)      which = QStringLiteral("TX");
    else if (c.uuid() == kFlowControl) which = QStringLiteral("FLOW");
    else if (c.uuid() == kRpcStatus)   which = QStringLiteral("RPCSTAT");
    // RPC over BLE is binary protobuf -> show hex. A ping_response is PB_Main
    // field 6, whose protobuf tag is (6<<3)|2 = 0x32; spotting it confirms a real reply.
    const QString hex = QString::fromLatin1(value.toHex(' '));
    const bool pong = (c.uuid() == kTxChar) && value.contains(char(0x32));
    log(QStringLiteral("RX<< [%1] %2%3").arg(which, hex, pong ? QStringLiteral("   <== PONG! real RPC reply.") : QString()));
}

void BleSpike::ping()
{
    if (!m_connected || !m_serial || !m_rx.isValid()) { log(QStringLiteral("not connected.")); return; }
    // A real Flipper RPC ping -- PB_Main{ command_id:1, system_ping_request:{} }
    // nanopb length-delimited => 5 bytes: 04 08 01 2A 00
    //   04     varint length of the 4-byte body
    //   08 01  field 1 (command_id) varint = 1
    //   2A 00  field 5 (system_ping_request) submessage, length 0 (empty)
    const QByteArray frame = QByteArray::fromHex("0408012a00");
    const bool noResp = (m_rx.properties() & QLowEnergyCharacteristic::WriteNoResponse);
    m_serial->writeCharacteristic(m_rx, frame,
        noResp ? QLowEnergyService::WriteWithoutResponse : QLowEnergyService::WriteWithResponse);
    log(QStringLiteral("TX>> RPC ping [04 08 01 2a 00] (%1) — expecting a ping_response.")
        .arg(noResp ? QStringLiteral("no-resp") : QStringLiteral("with-resp")));
}

void BleSpike::disconnectDevice()
{
    if (m_serial) { m_serial->deleteLater(); m_serial = nullptr; }
    if (m_ctrl) { m_ctrl->disconnectFromDevice(); m_ctrl->deleteLater(); m_ctrl = nullptr; }
    setConnected(false);
}

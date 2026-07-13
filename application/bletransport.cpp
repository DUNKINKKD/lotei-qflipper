#include "bletransport.h"

#include <QTimer>

// Flipper Serial GATT service + characteristics (from flipperzero-firmware
// targets/f7/ble_glue/services/serial_service_uuid.inc). Identical to the spike.
const QBluetoothUuid BleTransport::kSerialService =
        QBluetoothUuid(QStringLiteral("8fe5b3d5-2e7f-4a98-2a48-7acc60fe0000"));
const QBluetoothUuid BleTransport::kTxChar =
        QBluetoothUuid(QStringLiteral("19ed82ae-ed21-4c9d-4145-228e61fe0000")); // indicate: Flipper -> us
const QBluetoothUuid BleTransport::kRxChar =
        QBluetoothUuid(QStringLiteral("19ed82ae-ed21-4c9d-4145-228e62fe0000")); // write:    us -> Flipper
const QBluetoothUuid BleTransport::kFlowControl =
        QBluetoothUuid(QStringLiteral("19ed82ae-ed21-4c9d-4145-228e63fe0000")); // notify:   free RX buffer
const QBluetoothUuid BleTransport::kRpcStatus =
        QBluetoothUuid(QStringLiteral("19ed82ae-ed21-4c9d-4145-228e64fe0000")); // write 0x01 = start RPC

BleTransport::BleTransport(const QBluetoothDeviceInfo &device, QObject *parent)
    : Flipper::Zero::FlipperTransport(parent)
    , m_device(device)
{
}

BleTransport::~BleTransport()
{
    close();
}

bool BleTransport::open()
{
    if(m_ctrl) {
        return true; // already opening/open
    }

    m_error.clear();
    m_failed = false;
    m_opened = false;
    m_closing = false;
    m_handshakeStep = 0;

    m_ctrl = QLowEnergyController::createCentral(m_device, this);

    connect(m_ctrl, &QLowEnergyController::connected, this, [this]() {
        m_ctrl->discoverServices();
    });
    connect(m_ctrl, &QLowEnergyController::disconnected, this, [this]() {
        fail(QStringLiteral("Bluetooth link disconnected."));
    });
    connect(m_ctrl, &QLowEnergyController::errorOccurred, this, [this](QLowEnergyController::Error) {
        fail(QStringLiteral("Bluetooth controller error: %1  "
                            "(on Windows, pair the Flipper in Bluetooth settings first)")
             .arg(m_ctrl->errorString()));
    });
    connect(m_ctrl, &QLowEnergyController::discoveryFinished, this, &BleTransport::onDiscoveryFinished);
    connect(m_ctrl, &QLowEnergyController::mtuChanged, this, [this](int mtu) {
        if(mtu > 3) { m_mtu = mtu - 3; }
    });

    m_ctrl->connectToDevice();
    return true; // asynchronous: opened() or openFailed() follows
}

void BleTransport::onDiscoveryFinished()
{
    m_serial = m_ctrl->createServiceObject(kSerialService, this);
    if(!m_serial) {
        fail(QStringLiteral("Flipper Serial GATT service not found on this device."));
        return;
    }

    if(m_ctrl->mtu() > 3) { m_mtu = m_ctrl->mtu() - 3; }

    connect(m_serial, &QLowEnergyService::stateChanged, this, &BleTransport::onServiceStateChanged);
    connect(m_serial, &QLowEnergyService::descriptorWritten, this, &BleTransport::onDescriptorWritten);
    connect(m_serial, &QLowEnergyService::characteristicWritten, this, &BleTransport::onCharacteristicWritten);
    connect(m_serial, &QLowEnergyService::characteristicChanged, this, &BleTransport::onCharacteristicChanged);

    m_serial->discoverDetails();
}

void BleTransport::onServiceStateChanged(QLowEnergyService::ServiceState s)
{
    if(s != QLowEnergyService::RemoteServiceDiscovered) {
        return;
    }

    const QLowEnergyCharacteristic tx = m_serial->characteristic(kTxChar);
    m_rx = m_serial->characteristic(kRxChar);
    if(!tx.isValid() || !m_rx.isValid()) {
        fail(QStringLiteral("Flipper Serial TX/RX characteristics missing."));
        return;
    }

    // Prefer paced Write-with-response for outgoing frames; fall back to
    // write-without-response only if that is all the RX endpoint offers.
    m_rxWithResponse = (m_rx.properties() & QLowEnergyCharacteristic::Write);

    // Step 1: subscribe to TX. It is INDICATE-type on the Flipper (0x0200);
    // notifications (0x0100) delivered nothing.
    const QLowEnergyDescriptor cccd =
            tx.descriptor(QBluetoothUuid(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration));
    if(!cccd.isValid()) {
        fail(QStringLiteral("Flipper TX characteristic has no CCCD to subscribe to."));
        return;
    }

    const bool indicate = tx.properties() & QLowEnergyCharacteristic::Indicate;
    m_handshakeStep = 1;
    m_serial->writeDescriptor(cccd, QByteArray::fromHex(indicate ? "0200" : "0100"));
}

void BleTransport::onDescriptorWritten(const QLowEnergyDescriptor &d, const QByteArray &value)
{
    Q_UNUSED(d)
    Q_UNUSED(value)

    if(m_handshakeStep == 1) {
        // TX subscribed. Step 2: enable flow-control if the Flipper exposes it
        // (it advertises free RX-buffer size there); otherwise go straight to RPC.
        const QLowEnergyCharacteristic flow = m_serial->characteristic(kFlowControl);
        if(flow.isValid()) {
            const QLowEnergyDescriptor fc =
                    flow.descriptor(QBluetoothUuid(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration));
            if(fc.isValid()) {
                m_handshakeStep = 2;
                m_serial->writeDescriptor(fc, QByteArray::fromHex("0100"));
                return;
            }
        }
        activateRpc();

    } else if(m_handshakeStep == 2) {
        // Flow-control subscribed. Step 3: activate RPC.
        activateRpc();
    }
}

void BleTransport::activateRpc()
{
    const QLowEnergyCharacteristic rpc = m_serial->characteristic(kRpcStatus);
    if(!rpc.isValid()) {
        fail(QStringLiteral("Flipper RPC-status characteristic not found."));
        return;
    }

    m_handshakeStep = 3;

    // Write 0x01 to start the RPC session (BLE equivalent of USB start_rpc_session).
    if(rpc.properties() & QLowEnergyCharacteristic::WriteNoResponse) {
        // No characteristicWritten will arrive; the write is effectively immediate.
        m_serial->writeCharacteristic(rpc, QByteArray::fromHex("01"), QLowEnergyService::WriteWithoutResponse);
        finishOpen();
    } else {
        // Wait for the write to be acknowledged before declaring the link ready.
        m_serial->writeCharacteristic(rpc, QByteArray::fromHex("01"), QLowEnergyService::WriteWithResponse);
    }
}

void BleTransport::finishOpen()
{
    if(m_opened) {
        return;
    }
    m_opened = true;
    m_handshakeStep = 4;
    emit opened();
}

void BleTransport::onCharacteristicWritten(const QLowEnergyCharacteristic &c, const QByteArray &value)
{
    if(m_handshakeStep == 3 && c.uuid() == kRpcStatus) {
        finishOpen();
        return;
    }

    if(c.uuid() == kRxChar) {
        // A data chunk went out; account for it and send the next one.
        emit bytesWritten(value.size());
        m_writing = false;
        pumpTx();
    }
}

void BleTransport::onCharacteristicChanged(const QLowEnergyCharacteristic &c, const QByteArray &value)
{
    if(c.uuid() == kTxChar) {
        m_incoming.append(value);
        emit readyRead();
    }
    // kFlowControl carries RX-buffer credits; the paced writer already keeps us
    // well inside the buffer, so we don't need to act on it for RPC-sized traffic.
}

qint64 BleTransport::write(const QByteArray &data)
{
    if(!m_opened) {
        return -1;
    }
    // Queue; flush() (or the next characteristicWritten) drains it. Returning the
    // full size satisfies ProtobufSession's "all bytes accepted" contract.
    m_outgoing.append(data);
    return data.size();
}

void BleTransport::pumpTx()
{
    if(!m_serial || !m_rx.isValid() || m_outgoing.isEmpty()) {
        return;
    }

    if(m_rxWithResponse) {
        if(m_writing) {
            return; // one chunk outstanding; onCharacteristicWritten will resume
        }
        const QByteArray chunk = m_outgoing.left(m_mtu);
        m_outgoing.remove(0, chunk.size());
        m_writing = true;
        m_serial->writeCharacteristic(m_rx, chunk, QLowEnergyService::WriteWithResponse);
    } else {
        // No write acknowledgements to pace on -- push it all out best-effort.
        while(!m_outgoing.isEmpty()) {
            const QByteArray chunk = m_outgoing.left(m_mtu);
            m_outgoing.remove(0, chunk.size());
            m_serial->writeCharacteristic(m_rx, chunk, QLowEnergyService::WriteWithoutResponse);
            emit bytesWritten(chunk.size());
        }
    }
}

QByteArray BleTransport::readAll()
{
    const QByteArray out = m_incoming;
    m_incoming.clear();
    return out;
}

void BleTransport::clear()
{
    m_incoming.clear();
    m_outgoing.clear();
    m_writing = false;
}

bool BleTransport::flush()
{
    if(!m_opened) {
        return false;
    }
    pumpTx();
    return true;
}

QString BleTransport::errorString() const
{
    return m_error;
}

void BleTransport::fail(const QString &why)
{
    if(m_closing || m_failed) {
        return;
    }
    m_failed = true;
    m_error = why;

    if(m_opened) {
        emit errorOccurred();
    } else {
        emit openFailed(why);
    }
}

void BleTransport::close()
{
    m_closing = true;

    if(m_serial) {
        m_serial->deleteLater();
        m_serial = nullptr;
    }
    if(m_ctrl) {
        QLowEnergyController *ctrl = m_ctrl;
        m_ctrl = nullptr;

        // Detach the controller from this transport so it survives our own
        // deleteLater() (ProtobufSession frees the transport right after close()).
        // Otherwise it'd be destroyed as our child mid-disconnect.
        ctrl->setParent(nullptr);

        if(ctrl->state() == QLowEnergyController::UnconnectedState) {
            ctrl->deleteLater();
        } else {
            // Tear the link down cleanly and only free the controller once the
            // disconnect has actually completed. Deleting it while still
            // connected leaves the BlueZ ACL up on Linux, so the next connect to
            // the same peer races a half-open link and fails ("Not Connected").
            connect(ctrl, &QLowEnergyController::disconnected, ctrl, &QObject::deleteLater);
            QTimer::singleShot(4000, ctrl, &QObject::deleteLater); // safety net
            ctrl->disconnectFromDevice();
        }
    }
}

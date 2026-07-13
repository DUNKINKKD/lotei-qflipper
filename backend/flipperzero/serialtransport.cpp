#include "serialtransport.h"

#include <QSerialPort>

namespace Flipper {
namespace Zero {

SerialTransport::SerialTransport(QSerialPort *port, QObject *parent):
    FlipperTransport(parent),
    m_port(port)
{
    if(m_port) {
        m_port->setParent(this);   // adopt it, so it dies with the transport
        // Relay QSerialPort's signals up as the transport's own.
        connect(m_port, &QSerialPort::readyRead, this, &FlipperTransport::readyRead);
        connect(m_port, &QSerialPort::bytesWritten, this, &FlipperTransport::bytesWritten);
        connect(m_port, &QSerialPort::errorOccurred, this, &FlipperTransport::errorOccurred);
    }
}

bool SerialTransport::open()
{
    // SerialInitHelper hands us an already-open port; just make sure.
    return m_port && (m_port->isOpen() || m_port->open(QIODevice::ReadWrite));
}

void SerialTransport::close()
{
    if(m_port) {
        m_port->close();
    }
}

qint64 SerialTransport::write(const QByteArray &data)
{
    return m_port ? m_port->write(data) : -1;
}

QByteArray SerialTransport::readAll()
{
    return m_port ? m_port->readAll() : QByteArray();
}

void SerialTransport::clear()
{
    if(m_port) {
        m_port->clear();
    }
}

bool SerialTransport::flush()
{
    return m_port && m_port->flush();
}

QString SerialTransport::errorString() const
{
    return m_port ? m_port->errorString() : QString();
}

}
}

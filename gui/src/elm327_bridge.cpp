#include "elm327_bridge.h"
#include <QSerialPort>
#include <QBluetoothServiceInfo>
#include <QTimer>
#include <QTime>
#include <QStringList>

namespace socketspy::gui {

static const QStringList kInitSequence = {
    "ATZ",
    "ATE0",
    "ATL0",
    "ATH1",
    "ATSP0",
};

Elm327Bridge::Elm327Bridge(QObject* parent)
    : QObject(parent)
    , m_initTimer(new QTimer(this))
{
    m_initTimer->setSingleShot(true);
    connect(m_initTimer, &QTimer::timeout, this, &Elm327Bridge::onInitStep);
}

Elm327Bridge::~Elm327Bridge() {
    close();
}

bool Elm327Bridge::open(const QString& port, int baud) {
    close();

    auto* serial = new QSerialPort(this);
    serial->setPortName(port);
    serial->setBaudRate(baud);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!serial->open(QIODevice::ReadWrite)) {
        emit connectionError("Cannot open port: " + serial->errorString());
        serial->deleteLater();
        return false;
    }

    m_serial = serial;
    m_io = serial;
    connect(m_io, &QIODevice::readyRead, this, &Elm327Bridge::onReadyRead);
    startInit();
    return true;
}

bool Elm327Bridge::openBluetooth(const QBluetoothAddress& address) {
    close();

    if (address.isNull()) {
        emit connectionError("Invalid Bluetooth address");
        return false;
    }

    auto* sock = new QBluetoothSocket(QBluetoothServiceInfo::RfcommProtocol, this);
    m_btSocket = sock;

    connect(sock, &QBluetoothSocket::connected, this, &Elm327Bridge::onBtConnected);
    connect(sock, &QBluetoothSocket::errorOccurred, this, &Elm327Bridge::onBtError);

    emit statusChanged("Connecting...");
    sock->connectToService(address, 1); // ELM327 uses RFCOMM channel 1
    return true;
}

void Elm327Bridge::onBtConnected() {
    m_io = m_btSocket;
    connect(m_io, &QIODevice::readyRead, this, &Elm327Bridge::onReadyRead);
    startInit();
}

void Elm327Bridge::onBtError(QBluetoothSocket::SocketError /*error*/) {
    emit connectionError("Bluetooth error: " + m_btSocket->errorString());
    close();
}

void Elm327Bridge::startInit() {
    m_state = State::Initializing;
    m_initStep = 0;
    m_readBuf.clear();
    emit statusChanged("Initializing...");
    m_initTimer->start(500);
}

void Elm327Bridge::close() {
    m_initTimer->stop();
    if (m_io && m_state == State::Monitoring)
        m_io->write("\r");
    m_io = nullptr;

    if (m_serial) {
        if (m_serial->isOpen()) m_serial->close();
        m_serial->deleteLater();
        m_serial = nullptr;
    }
    if (m_btSocket) {
        m_btSocket->abort();
        m_btSocket->deleteLater();
        m_btSocket = nullptr;
    }
    m_state = State::Closed;
    m_initStep = 0;
    m_readBuf.clear();
}

bool Elm327Bridge::isOpen() const {
    return m_io && m_io->isOpen() && m_state != State::Closed;
}

void Elm327Bridge::sendAtCommand(const QString& cmd) {
    if (m_io) m_io->write((cmd + "\r").toLatin1());
}

void Elm327Bridge::onInitStep() {
    if (m_state != State::Initializing) return;

    if (m_initStep < kInitSequence.size()) {
        sendAtCommand(kInitSequence[m_initStep]);
        ++m_initStep;
        m_initTimer->start(300);
    } else {
        startMonitor();
    }
}

void Elm327Bridge::startMonitor() {
    m_state = State::Monitoring;
    sendAtCommand("ATMA");
    emit statusChanged("Monitoring");
}

void Elm327Bridge::onReadyRead() {
    m_readBuf += m_io->readAll();

    while (true) {
        int idx = m_readBuf.indexOf('\r');
        if (idx < 0) {
            idx = m_readBuf.indexOf('\n');
            if (idx < 0) break;
        }
        QByteArray lineBytes = m_readBuf.left(idx).trimmed();
        m_readBuf.remove(0, idx + 1);
        if (lineBytes.isEmpty()) continue;

        QString line = QString::fromLatin1(lineBytes);
        processLine(line);
    }
}

void Elm327Bridge::processLine(const QString& line) {
    if (line.isEmpty()) return;

    if (m_state == State::SendingFrame) {
        if (m_sendStep == 0 && (line == "STOPPED" || line == "OK" || line == ">")) {
            m_sendStep = 1;
            QString header = QString("AT SH %1").arg(m_pendingId, 3, 16, QChar('0')).toUpper();
            sendAtCommand(header);
        } else if (m_sendStep == 1 && line == "OK") {
            m_sendStep = 2;
            sendAtCommand(QString::fromLatin1(m_pendingData));
        } else if (m_sendStep == 2 && line == "OK") {
            m_state = State::Monitoring;
            startMonitor();
        }
        return;
    }

    if (m_state != State::Monitoring) return;

    if (line.startsWith("ELM327") || line == "OK" || line == ">" ||
        line.startsWith("AT") || line == "SEARCHING..." ||
        line == "NO DATA" || line == "STOPPED" || line == "ERROR" ||
        line == "?" || line.startsWith("UNABLE") || line.startsWith("BUS")) {
        return;
    }

    auto frame = parseElm327Line(line);
    if (frame.dlc > 0 || frame.id > 0)
        emit frameReceived(frame);
}

socketspy::core::CanFrame Elm327Bridge::parseElm327Line(const QString& line) {
    socketspy::core::CanFrame frame{};

    QStringList parts = line.split(' ', Qt::SkipEmptyParts);
    if (parts.size() < 2) return frame;

    bool idOk = false;
    frame.id = parts[0].toUInt(&idOk, 16);
    if (!idOk) return frame;

    bool dlcOk = false;
    int dlcVal = parts[1].toInt(&dlcOk, 10);

    int dataStart = 0;
    if (dlcOk && dlcVal >= 0 && dlcVal <= 8 && parts.size() > 2) {
        frame.dlc = static_cast<uint8_t>(dlcVal);
        dataStart = 2;
    } else {
        dataStart = 1;
        frame.dlc = static_cast<uint8_t>(parts.size() - 1);
        if (frame.dlc > 8) frame.dlc = 8;
    }

    uint8_t byteIdx = 0;
    for (int i = dataStart; i < parts.size() && byteIdx < frame.dlc; ++i, ++byteIdx) {
        bool ok = false;
        frame.data[byteIdx] = static_cast<uint8_t>(parts[i].toUInt(&ok, 16));
        if (!ok) { frame.dlc = 0; return frame; }
    }

    frame.timestamp_us = static_cast<uint64_t>(QTime::currentTime().msecsSinceStartOfDay()) * 1000ULL;
    return frame;
}

bool Elm327Bridge::sendFrame(const socketspy::core::CanFrame& frame) {
    if (!isOpen() || m_state != State::Monitoring) return false;

    m_pendingId = frame.id;
    m_pendingData.clear();
    for (int i = 0; i < frame.dlc; ++i) {
        if (i > 0) m_pendingData += ' ';
        m_pendingData += QByteArray::number(frame.data[i], 16).rightJustified(2, '0').toUpper();
    }

    m_state = State::SendingFrame;
    m_sendStep = 0;
    m_io->write("\r");
    return true;
}

} // namespace socketspy::gui

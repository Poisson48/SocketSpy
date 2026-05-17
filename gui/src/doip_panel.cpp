#include "doip_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDateTime>
#include <QtEndian>

namespace socketspy::gui {

// DoIP header: version(1) + ~version(1) + type(2 BE) + length(4 BE) = 8 bytes
static constexpr int kDoipHeaderLen = 8;
static constexpr uint8_t kDoipVersion = 0x02;

static QByteArray buildDoipHeader(uint16_t type, uint32_t payloadLen) {
    QByteArray hdr(kDoipHeaderLen, '\0');
    hdr[0] = static_cast<char>(kDoipVersion);
    hdr[1] = static_cast<char>(~kDoipVersion);
    hdr[2] = static_cast<char>((type >> 8) & 0xFF);
    hdr[3] = static_cast<char>(type & 0xFF);
    hdr[4] = static_cast<char>((payloadLen >> 24) & 0xFF);
    hdr[5] = static_cast<char>((payloadLen >> 16) & 0xFF);
    hdr[6] = static_cast<char>((payloadLen >>  8) & 0xFF);
    hdr[7] = static_cast<char>( payloadLen        & 0xFF);
    return hdr;
}

static QString bytesToHex(const QByteArray& data) {
    QString out;
    out.reserve(data.size() * 3);
    for (int i = 0; i < data.size(); ++i) {
        if (i) out += ' ';
        out += QString("%1").arg(static_cast<uint8_t>(data[i]), 2, 16, QChar('0')).toUpper();
    }
    return out;
}

DoipPanel::DoipPanel(QWidget* parent) : QWidget(parent) {
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected,    this, &DoipPanel::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &DoipPanel::onDisconnectedSlot);
    connect(m_socket, &QTcpSocket::readyRead,    this, &DoipPanel::onReadyRead);
    connect(m_socket, &QAbstractSocket::errorOccurred, this, &DoipPanel::onSocketError);
    setupUi();
}

DoipPanel::~DoipPanel() = default;

void DoipPanel::setupUi() {
    // --- Connection group ---
    m_ipEdit = new QLineEdit("192.168.0.10", this);
    m_ipEdit->setPlaceholderText("ECU IP address");

    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(13400);

    m_connectBtn    = new QPushButton(tr("Connect"),    this);
    m_disconnectBtn = new QPushButton(tr("Disconnect"), this);
    m_disconnectBtn->setEnabled(false);

    auto* btnRow = new QHBoxLayout;
    btnRow->addWidget(m_connectBtn);
    btnRow->addWidget(m_disconnectBtn);
    btnRow->addStretch();

    m_statusLabel = new QLabel(tr("Disconnected"), this);
    m_statusLabel->setStyleSheet("color: #6b7280; font-weight: bold;");

    auto* connForm = new QFormLayout;
    connForm->addRow(tr("IP:"),   m_ipEdit);
    connForm->addRow(tr("Port:"), m_portSpin);
    connForm->addRow(QString(),   btnRow);
    connForm->addRow(tr("Status:"), m_statusLabel);

    auto* connGroup = new QGroupBox(tr("Connection"), this);
    connGroup->setLayout(connForm);

    // --- UDS group (hidden until routing is active) ---
    m_srcAddrEdit = new QLineEdit("0E80", this);
    m_srcAddrEdit->setPlaceholderText("Hex e.g. 0E80");
    m_srcAddrEdit->setMaxLength(4);

    m_tgtAddrEdit = new QLineEdit("0101", this);
    m_tgtAddrEdit->setPlaceholderText("Hex e.g. 0101");
    m_tgtAddrEdit->setMaxLength(4);

    m_udsPayloadEdit = new QTextEdit(this);
    m_udsPayloadEdit->setPlaceholderText(tr("UDS payload bytes (hex), e.g. 22 F1 90"));
    m_udsPayloadEdit->setFixedHeight(60);

    m_sendUdsBtn = new QPushButton(tr("Send UDS Request"), this);

    auto* udsForm = new QFormLayout;
    udsForm->addRow(tr("Source Addr:"), m_srcAddrEdit);
    udsForm->addRow(tr("Target Addr:"), m_tgtAddrEdit);
    udsForm->addRow(tr("UDS Payload:"), m_udsPayloadEdit);
    udsForm->addRow(QString(), m_sendUdsBtn);

    m_udsWidget = new QGroupBox(tr("UDS Request"), this);
    m_udsWidget->setLayout(udsForm);
    m_udsWidget->setEnabled(false);

    // --- Log ---
    m_log = new QTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setFont(QFont("Monospace", 9));
    m_log->setPlaceholderText(tr("DoIP exchange log…"));

    auto* logGroup = new QGroupBox(tr("Log"), this);
    auto* logLayout = new QVBoxLayout(logGroup);
    logLayout->addWidget(m_log);

    // --- Main layout ---
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addWidget(connGroup);
    layout->addWidget(m_udsWidget);
    layout->addWidget(logGroup, 1);

    connect(m_connectBtn,    &QPushButton::clicked, this, &DoipPanel::onConnect);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &DoipPanel::onDisconnect);
    connect(m_sendUdsBtn,    &QPushButton::clicked, this, &DoipPanel::onSendUds);
}

void DoipPanel::setStatus(const QString& text, const QString& color) {
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(QString("color: %1; font-weight: bold;").arg(color));
}

void DoipPanel::appendLog(const QString& line) {
    const QString ts = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    m_log->append(QString("[%1] %2").arg(ts, line));
}

void DoipPanel::sendDoip(uint16_t type, const QByteArray& payload) {
    QByteArray frame = buildDoipHeader(type, static_cast<uint32_t>(payload.size()));
    frame += payload;
    m_socket->write(frame);
    appendLog(QString("TX type=0x%1 len=%2  %3")
              .arg(type, 4, 16, QChar('0')).toUpper()
              .arg(payload.size())
              .arg(bytesToHex(payload)));
}

void DoipPanel::sendRoutingActivation() {
    bool ok = false;
    const uint16_t src = m_srcAddrEdit->text().toUShort(&ok, 16);
    if (!ok) { appendLog("Invalid source address"); return; }

    QByteArray payload(6, '\0');
    payload[0] = static_cast<char>((src >> 8) & 0xFF);
    payload[1] = static_cast<char>( src        & 0xFF);
    payload[2] = 0x00; // activation type: default
    // reserved: bytes 3-5 = 0x00
    sendDoip(0x0005, payload);
}

void DoipPanel::onConnect() {
    const QString ip = m_ipEdit->text().trimmed();
    const int port   = m_portSpin->value();
    if (ip.isEmpty()) { appendLog("No IP address"); return; }

    m_connectBtn->setEnabled(false);
    m_rxBuffer.clear();
    m_routingActive = false;
    setStatus(tr("Connecting..."), "#d97706");
    appendLog(QString("Connecting to %1:%2…").arg(ip).arg(port));
    m_socket->connectToHost(ip, static_cast<quint16>(port));
}

void DoipPanel::onDisconnect() {
    m_socket->disconnectFromHost();
}

void DoipPanel::onConnected() {
    m_disconnectBtn->setEnabled(true);
    appendLog("TCP connected — sending Routing Activation Request");
    sendRoutingActivation();
}

void DoipPanel::onDisconnectedSlot() {
    m_connectBtn->setEnabled(true);
    m_disconnectBtn->setEnabled(false);
    m_udsWidget->setEnabled(false);
    m_routingActive = false;
    m_rxBuffer.clear();
    setStatus(tr("Disconnected"), "#6b7280");
    appendLog("Disconnected");
}

void DoipPanel::onSocketError(QAbstractSocket::SocketError /*err*/) {
    const QString msg = m_socket->errorString();
    setStatus(tr("Error: ") + msg, "#dc2626");
    appendLog("Error: " + msg);
    m_connectBtn->setEnabled(true);
    m_disconnectBtn->setEnabled(false);
    m_udsWidget->setEnabled(false);
}

void DoipPanel::onReadyRead() {
    m_rxBuffer += m_socket->readAll();
    processResponse(m_rxBuffer);
}

void DoipPanel::processResponse(const QByteArray& data) {
    while (m_rxBuffer.size() >= kDoipHeaderLen) {
        // Parse header
        const uint16_t type =
            (static_cast<uint8_t>(m_rxBuffer[2]) << 8) |
             static_cast<uint8_t>(m_rxBuffer[3]);
        const uint32_t payloadLen =
            (static_cast<uint8_t>(m_rxBuffer[4]) << 24) |
            (static_cast<uint8_t>(m_rxBuffer[5]) << 16) |
            (static_cast<uint8_t>(m_rxBuffer[6]) <<  8) |
             static_cast<uint8_t>(m_rxBuffer[7]);

        const int totalLen = kDoipHeaderLen + static_cast<int>(payloadLen);
        if (m_rxBuffer.size() < totalLen) break; // wait for more data

        const QByteArray payload = m_rxBuffer.mid(kDoipHeaderLen, static_cast<int>(payloadLen));
        m_rxBuffer.remove(0, totalLen);

        appendLog(QString("RX type=0x%1 len=%2  %3")
                  .arg(type, 4, 16, QChar('0')).toUpper()
                  .arg(payloadLen)
                  .arg(bytesToHex(payload)));

        if (type == 0x0006) {
            // Routing Activation Response — byte 2 of payload (offset 2) = response code
            if (payload.size() >= 3) {
                const uint8_t code = static_cast<uint8_t>(payload[2]);
                if (code == 0x10) {
                    m_routingActive = true;
                    setStatus(tr("Routing Active"), "#16a34a");
                    m_udsWidget->setEnabled(true);
                    appendLog("Routing Activation successful (0x10)");
                } else {
                    setStatus(tr("Routing Error: 0x") + QString::number(code, 16).toUpper(), "#dc2626");
                    appendLog(QString("Routing Activation failed, code=0x%1").arg(code, 2, 16, QChar('0')).toUpper());
                }
            }
        } else if (type == 0x8001) {
            // Diagnostic Message Positive/Negative Ack or Response
            // payload[0..1] = target logical addr, payload[2..3] = source logical addr
            // payload[4..] = UDS response bytes
            if (payload.size() >= 4) {
                const QByteArray udsResp = payload.mid(4);
                appendLog("UDS Response: " + bytesToHex(udsResp));
            }
        }
    }
    Q_UNUSED(data)
}

void DoipPanel::onSendUds() {
    if (!m_routingActive) {
        appendLog("Not connected / routing not active");
        return;
    }
    bool ok1 = false, ok2 = false;
    const uint16_t src = m_srcAddrEdit->text().toUShort(&ok1, 16);
    const uint16_t tgt = m_tgtAddrEdit->text().toUShort(&ok2, 16);
    if (!ok1 || !ok2) { appendLog("Invalid address"); return; }

    // Parse hex payload
    const QStringList tokens = m_udsPayloadEdit->toPlainText().trimmed().split(
        QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    QByteArray udsBytes;
    for (const QString& tok : tokens) {
        bool ok = false;
        const uint val = tok.toUInt(&ok, 16);
        if (!ok || val > 0xFF) { appendLog("Invalid hex byte: " + tok); return; }
        udsBytes += static_cast<char>(val);
    }
    if (udsBytes.isEmpty()) { appendLog("Empty UDS payload"); return; }

    // Build DoIP Diagnostic Message payload: src(2) + tgt(2) + uds
    QByteArray payload(4, '\0');
    payload[0] = static_cast<char>((src >> 8) & 0xFF);
    payload[1] = static_cast<char>( src        & 0xFF);
    payload[2] = static_cast<char>((tgt >> 8) & 0xFF);
    payload[3] = static_cast<char>( tgt        & 0xFF);
    payload += udsBytes;

    sendDoip(0x8001, payload);
}

} // namespace socketspy::gui

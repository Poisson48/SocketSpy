#include "xcp_panel.h"
#include "iface_detector.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
// SocketCAN raw receive
#include <sys/socket.h>
#include <sys/time.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

using namespace socketspy::core;

namespace socketspy::gui {

// ─────────────────────────────────────────────────────────────────────────────
// XcpWorker
// ─────────────────────────────────────────────────────────────────────────────

XcpWorker::XcpWorker(QString iface, uint32_t txId, uint32_t rxId,
                     int timeoutMs, Cmd cmd, QObject* parent)
    : QThread(parent)
    , m_iface(std::move(iface))
    , m_txId(txId), m_rxId(rxId)
    , m_timeoutMs(timeoutMs), m_cmd(cmd)
{}

// Opens a separate read socket filtered on m_rxId, sends payload via can_send,
// then blocks up to m_timeoutMs waiting for a response on the RX socket.
bool XcpWorker::sendAndReceive(IfaceHandle h,
                               const uint8_t* payload, int dlc,
                               uint8_t* respOut, int& respLen)
{
    // Build TX frame
    CanFrame f{};
    f.id  = m_txId;
    f.dlc = static_cast<uint8_t>(dlc);
    for (int i = 0; i < dlc; ++i) f.data[i] = payload[i];

    if (!can_send(h, f)) return false;

    // Open a second raw socket for reception (filtered on RX ID)
    int rfd = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (rfd < 0) return false;

    struct ifreq ifr{};
    ::strncpy(ifr.ifr_name, m_iface.toStdString().c_str(), IFNAMSIZ - 1);
    if (::ioctl(rfd, SIOCGIFINDEX, &ifr) < 0) { ::close(rfd); return false; }

    struct can_filter cf{ m_rxId, CAN_SFF_MASK };
    ::setsockopt(rfd, SOL_CAN_RAW, CAN_RAW_FILTER, &cf, sizeof(cf));

    struct sockaddr_can addr{};
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (::bind(rfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(rfd); return false;
    }

    // Set receive timeout
    struct timeval tv{};
    tv.tv_sec  = m_timeoutMs / 1000;
    tv.tv_usec = (m_timeoutMs % 1000) * 1000;
    ::setsockopt(rfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    can_frame raw{};
    ssize_t n = ::read(rfd, &raw, sizeof(raw));
    ::close(rfd);

    if (n <= 0) return false;  // timeout or error

    respLen = static_cast<int>(raw.can_dlc);
    for (int i = 0; i < respLen; ++i) respOut[i] = raw.data[i];
    return true;
}

// CC_CONNECT (0xFF 0x00) — 2-byte payload
QString XcpWorker::handleConnect(IfaceHandle h) {
    const uint8_t req[] = {0xFF, 0x00};
    uint8_t resp[8]{}; int rlen = 0;
    if (!sendAndReceive(h, req, 2, resp, rlen))
        return "<font color='#ef4444'>Timeout — no XCP slave detected</font>";
    if (rlen < 1 || resp[0] != 0xFF)
        return QString("<font color='#ef4444'>Negative response: 0x%1</font>")
               .arg(resp[0], 2, 16, QChar('0')).toUpper();

    const uint8_t commMode = (rlen > 1) ? resp[1] : 0;
    const uint8_t maxCto   = (rlen > 3) ? resp[3] : 0;
    const uint8_t maxDto   = (rlen > 4) ? resp[4] : 0;
    const bool    bigEndian = (commMode & 0x01) != 0;
    const bool    interleaved = (commMode & 0x40) != 0;

    return QString("<b>XCP Connect OK</b><br>"
                   "COMM_MODE_BASIC: 0x%1 (%2-endian%3)<br>"
                   "MAX_CTO: %4 bytes &nbsp; MAX_DTO: %5 bytes")
           .arg(commMode, 2, 16, QChar('0')).toUpper()
           .arg(bigEndian ? "big" : "little")
           .arg(interleaved ? ", interleaved" : "")
           .arg(maxCto).arg(maxDto);
}

// CC_DISCONNECT (0xFE) — 1-byte payload
QString XcpWorker::handleDisconnect(IfaceHandle h) {
    const uint8_t req[] = {0xFE};
    uint8_t resp[8]{}; int rlen = 0;
    if (!sendAndReceive(h, req, 1, resp, rlen))
        return "<font color='#ef4444'>Timeout — no XCP slave detected</font>";
    if (rlen < 1 || resp[0] != 0xFF)
        return QString("<font color='#ef4444'>Disconnect — negative response: 0x%1</font>")
               .arg(resp[0], 2, 16, QChar('0')).toUpper();
    return "<b>XCP Disconnect OK</b>";
}

// CC_GET_STATUS (0xFD)
QString XcpWorker::handleGetStatus(IfaceHandle h) {
    const uint8_t req[] = {0xFD};
    uint8_t resp[8]{}; int rlen = 0;
    if (!sendAndReceive(h, req, 1, resp, rlen))
        return "<font color='#ef4444'>Timeout — no XCP slave detected</font>";
    if (rlen < 1 || resp[0] != 0xFF)
        return QString("<font color='#ef4444'>GetStatus — negative response: 0x%1</font>")
               .arg(resp[0], 2, 16, QChar('0')).toUpper();

    const uint8_t sessionStatus = (rlen > 1) ? resp[1] : 0;
    const uint8_t protStatus    = (rlen > 2) ? resp[2] : 0;
    return QString("<b>XCP Get Status OK</b><br>"
                   "Session status: 0x%1 (DAQ=%2 RESUME=%3 STORE_DAQ=%4)<br>"
                   "Protection status: 0x%5")
           .arg(sessionStatus, 2, 16, QChar('0')).toUpper()
           .arg((sessionStatus & 0x40) ? "on" : "off")
           .arg((sessionStatus & 0x10) ? "on" : "off")
           .arg((sessionStatus & 0x04) ? "on" : "off")
           .arg(protStatus, 2, 16, QChar('0')).toUpper();
}

// CC_GET_ID (0xFA 0x01) + CC_UPLOAD (0xF5 n) to read the name string
QString XcpWorker::handleGetId(IfaceHandle h) {
    // Step 1: GET_ID with type=1 (ASCII text)
    const uint8_t req[] = {0xFA, 0x01};
    uint8_t resp[8]{}; int rlen = 0;
    if (!sendAndReceive(h, req, 2, resp, rlen))
        return "<font color='#ef4444'>Timeout — no XCP slave detected</font>";
    if (rlen < 1 || resp[0] != 0xFF)
        return QString("<font color='#ef4444'>GetId — negative response: 0x%1</font>")
               .arg(resp[0], 2, 16, QChar('0')).toUpper();

    // Length is 4 bytes LE at bytes[3..6]
    uint32_t nameLen = 0;
    if (rlen >= 8) {
        nameLen = static_cast<uint32_t>(resp[4])
                | (static_cast<uint32_t>(resp[5]) << 8)
                | (static_cast<uint32_t>(resp[6]) << 16)
                | (static_cast<uint32_t>(resp[7]) << 24);
    }
    if (nameLen == 0 || nameLen > 255)
        return QString("<b>XCP Get ID OK</b><br>Name length: %1 byte(s) (no UPLOAD)").arg(nameLen);

    // Step 2: UPLOAD to read the name bytes
    const uint8_t upReq[] = {0xF5, static_cast<uint8_t>(nameLen)};
    uint8_t upResp[8]{}; int upLen = 0;
    if (!sendAndReceive(h, upReq, 2, upResp, upLen))
        return "<font color='#ef4444'>Upload timeout</font>";
    if (upLen < 1 || upResp[0] != 0xFF)
        return "<font color='#ef4444'>Upload — negative response</font>";

    QString name;
    int available = std::min(upLen - 1, static_cast<int>(nameLen));
    for (int i = 1; i <= available; ++i) {
        uint8_t b = upResp[i];
        name += (b >= 0x20 && b < 0x7F) ? QChar(static_cast<char>(b))
                                         : QChar('?');
    }
    return QString("<b>XCP Get ID OK</b><br>Slave name: <i>%1</i> (%2 byte(s))")
           .arg(name.isEmpty() ? "(empty)" : name).arg(nameLen);
}

void XcpWorker::run() {
    IfaceHandle h = can_open(m_iface.toStdString());
    if (!h.valid()) {
        emit resultReady(QString("<font color='#ef4444'>can_open failed: %1</font>")
                         .arg(strerror(errno)));
        return;
    }

    QString result;
    switch (m_cmd) {
        case Cmd::Connect:    result = handleConnect(h);    break;
        case Cmd::Disconnect: result = handleDisconnect(h); break;
        case Cmd::GetStatus:  result = handleGetStatus(h);  break;
        case Cmd::GetId:      result = handleGetId(h);      break;
    }
    can_close(h);
    emit resultReady(result);
}

// ─────────────────────────────────────────────────────────────────────────────
// XcpPanel
// ─────────────────────────────────────────────────────────────────────────────

XcpPanel::XcpPanel(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void XcpPanel::setupUi() {
    m_iface = new QComboBox(this);
    m_iface->addItems(IfaceDetector::scanCanIfaces());

    auto* refreshBtn = new QPushButton(QString::fromUtf8("↺"), this);
    refreshBtn->setFixedWidth(28);
    refreshBtn->setToolTip("Refresh interface list");
    connect(refreshBtn, &QPushButton::clicked, this, &XcpPanel::refreshIfaces);

    auto* ifaceRow = new QHBoxLayout;
    ifaceRow->addWidget(m_iface, 1);
    ifaceRow->addWidget(refreshBtn);

    m_txId = new QLineEdit("7E1", this);
    m_txId->setPlaceholderText("TX CAN ID (hex)");

    m_rxId = new QLineEdit("7E2", this);
    m_rxId->setPlaceholderText("RX CAN ID (hex)");

    m_timeout = new QSpinBox(this);
    m_timeout->setRange(50, 5000);
    m_timeout->setValue(200);
    m_timeout->setSuffix(" ms");

    auto* configGroup = new QGroupBox("XCP Configuration", this);
    auto* configForm  = new QFormLayout(configGroup);
    configForm->addRow("Interface:", ifaceRow);
    configForm->addRow("TX ID (hex):", m_txId);
    configForm->addRow("RX ID (hex):", m_rxId);
    configForm->addRow("Timeout:", m_timeout);

    m_connectBtn    = new QPushButton("Connect",    this);
    m_disconnectBtn = new QPushButton("Disconnect", this);
    m_statusBtn     = new QPushButton("Get Status", this);
    m_getIdBtn      = new QPushButton("Get ID",     this);

    auto* btnRow = new QHBoxLayout;
    btnRow->addWidget(m_connectBtn);
    btnRow->addWidget(m_disconnectBtn);
    btnRow->addWidget(m_statusBtn);
    btnRow->addWidget(m_getIdBtn);
    btnRow->addStretch();

    m_info = new QTextEdit(this);
    m_info->setReadOnly(true);
    m_info->setPlaceholderText("XCP responses will appear here…");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addWidget(configGroup);
    layout->addLayout(btnRow);
    layout->addWidget(m_info, 1);

    connect(m_connectBtn,    &QPushButton::clicked, this, &XcpPanel::onConnect);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &XcpPanel::onDisconnect);
    connect(m_statusBtn,     &QPushButton::clicked, this, &XcpPanel::onGetStatus);
    connect(m_getIdBtn,      &QPushButton::clicked, this, &XcpPanel::onGetId);
}

void XcpPanel::refreshIfaces() {
    const QString cur = m_iface->currentText();
    m_iface->blockSignals(true);
    m_iface->clear();
    m_iface->addItems(IfaceDetector::scanCanIfaces());
    m_iface->blockSignals(false);
    int idx = m_iface->findText(cur);
    if (idx >= 0) m_iface->setCurrentIndex(idx);
}

void XcpPanel::runCommand(XcpWorker::Cmd cmd) {
    bool ok1 = false, ok2 = false;
    uint32_t txId = m_txId->text().toUInt(&ok1, 16);
    uint32_t rxId = m_rxId->text().toUInt(&ok2, 16);
    if (!ok1 || !ok2) {
        m_info->setHtml("<font color='#ef4444'>Invalid TX or RX CAN ID</font>");
        return;
    }
    setBusy(true);
    auto* w = new XcpWorker(m_iface->currentText(), txId, rxId,
                             m_timeout->value(), cmd, this);
    connect(w, &XcpWorker::resultReady, this, &XcpPanel::onResult);
    connect(w, &XcpWorker::finished,   w,    &QObject::deleteLater);
    w->start();
}

void XcpPanel::onConnect()    { runCommand(XcpWorker::Cmd::Connect);    }
void XcpPanel::onDisconnect() { runCommand(XcpWorker::Cmd::Disconnect); }
void XcpPanel::onGetStatus()  { runCommand(XcpWorker::Cmd::GetStatus);  }
void XcpPanel::onGetId()      { runCommand(XcpWorker::Cmd::GetId);      }

void XcpPanel::onResult(QString html) {
    setBusy(false);
    m_info->setHtml(html);
}

void XcpPanel::setBusy(bool busy) {
    m_connectBtn->setEnabled(!busy);
    m_disconnectBtn->setEnabled(!busy);
    m_statusBtn->setEnabled(!busy);
    m_getIdBtn->setEnabled(!busy);
    if (busy) m_info->setHtml("<i>Sending command…</i>");
}

} // namespace socketspy::gui

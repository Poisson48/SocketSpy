#include "uds_ecu_sim.h"
#include <QDateTime>
#include <QRandomGenerator>
#include <cstring>

namespace socketspy::gui {

UdsEcuSim::UdsEcuSim(Config cfg, QObject* parent)
    : QObject(parent)
    , m_cfg(std::move(cfg))
{
    m_cfTimer = new QTimer(this);
    m_cfTimer->setSingleShot(false);
    m_cfTimer->setInterval(1);
    connect(m_cfTimer, &QTimer::timeout, this, &UdsEcuSim::sendNextCf);
}

void UdsEcuSim::setConfig(Config cfg)
{
    m_cfg = std::move(cfg);
}

void UdsEcuSim::onFrameReceived(const socketspy::core::CanFrame& frame)
{
    if (!m_cfg.enabled) return;
    if (frame.id == m_cfg.rxId || frame.id == m_cfg.funcId)
        handleIsoTpFrame(frame);
}

// ---------------------------------------------------------------------------
// ISO-TP receive

void UdsEcuSim::handleIsoTpFrame(const socketspy::core::CanFrame& f)
{
    if (f.dlc == 0) return;
    const uint8_t* d = f.data;

    // Flow control for ongoing TX
    if (!m_txBuf.isEmpty() && (d[0] & 0xF0) == 0x30) {
        // FC: start consecutive frame timer
        m_cfTimer->start();
        return;
    }

    uint8_t pci = d[0] & 0xF0;
    if (pci == 0x00) {
        // Single Frame
        rxSingleFrame(d, f.dlc);
    } else if (pci == 0x10) {
        // First Frame
        rxFirstFrame(d);
    } else if (pci == 0x20) {
        // Consecutive Frame
        rxConsecutiveFrame(d);
    }
}

void UdsEcuSim::rxSingleFrame(const uint8_t* d, int dlc)
{
    int len = d[0] & 0x0F;
    if (len <= 0 || len > 7 || len > dlc - 1) return;
    QByteArray pdu(reinterpret_cast<const char*>(d + 1), len);
    dispatchUds(pdu);
}

void UdsEcuSim::rxFirstFrame(const uint8_t* d)
{
    int len = ((d[0] & 0x0F) << 8) | d[1];
    m_rxBuf.clear();
    m_rxBuf.append(reinterpret_cast<const char*>(d + 2), 6);
    m_rxExpected = len;
    m_rxNextSn = 1;
    sendFlowControl();
}

void UdsEcuSim::rxConsecutiveFrame(const uint8_t* d)
{
    uint8_t sn = d[0] & 0x0F;
    if (sn != m_rxNextSn) {
        // Sequence error — abort
        m_rxBuf.clear();
        m_rxExpected = 0;
        return;
    }
    m_rxNextSn = (m_rxNextSn + 1) & 0x0F;

    int remaining = m_rxExpected - m_rxBuf.size();
    int toCopy = qMin(7, remaining);
    m_rxBuf.append(reinterpret_cast<const char*>(d + 1), toCopy);

    if (m_rxBuf.size() >= m_rxExpected) {
        QByteArray pdu = m_rxBuf.left(m_rxExpected);
        m_rxBuf.clear();
        m_rxExpected = 0;
        dispatchUds(pdu);
    }
}

void UdsEcuSim::sendFlowControl()
{
    uint8_t data[8] = {0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    auto f = makeFrame(data, 3);
    emit frameToSend(f);
}

// ---------------------------------------------------------------------------
// UDS dispatch

void UdsEcuSim::dispatchUds(const QByteArray& pdu)
{
    if (pdu.isEmpty()) return;
    uint8_t sid = static_cast<uint8_t>(pdu[0]);

    emit logMessage(QString("[%1] RX SID=0x%2 len=%3")
                    .arg(m_cfg.name)
                    .arg(sid, 2, 16, QChar('0'))
                    .arg(pdu.size()));

    QByteArray resp;
    switch (sid) {
    case 0x10: resp = svc10(pdu); break;
    case 0x11: resp = svc11(pdu); break;
    case 0x14: resp = svc14(pdu); break;
    case 0x19: resp = svc19(pdu); break;
    case 0x22: resp = svc22(pdu); break;
    case 0x27: resp = svc27(pdu); break;
    case 0x28: resp = svc28(pdu); break;
    case 0x2E: resp = svc2E(pdu); break;
    case 0x31: resp = svc31(pdu); break;
    case 0x3E: resp = svc3E(pdu); break;
    default:
        resp = nrc(sid, 0x11); // serviceNotSupported
        break;
    }

    if (!resp.isEmpty())
        sendUds(resp);
}

// ---------------------------------------------------------------------------
// ISO-TP transmit

void UdsEcuSim::sendUds(const QByteArray& pdu)
{
    if (pdu.size() <= 7) {
        txSingleFrame(pdu);
    } else {
        txFirstFrame(pdu);
    }
}

void UdsEcuSim::txSingleFrame(const QByteArray& pdu)
{
    uint8_t data[8] = {};
    data[0] = static_cast<uint8_t>(pdu.size() & 0x0F);
    int n = qMin(pdu.size(), 7);
    memcpy(data + 1, pdu.constData(), n);
    auto f = makeFrame(data, n + 1);
    emit frameToSend(f);
}

void UdsEcuSim::txFirstFrame(const QByteArray& pdu)
{
    m_txBuf = pdu;
    m_txOffset = 6;
    m_txSn = 1;

    uint8_t data[8] = {};
    data[0] = static_cast<uint8_t>(0x10 | ((pdu.size() >> 8) & 0x0F));
    data[1] = static_cast<uint8_t>(pdu.size() & 0xFF);
    memcpy(data + 2, pdu.constData(), 6);
    auto f = makeFrame(data, 8);
    emit frameToSend(f);
    // Wait for Flow Control before sending consecutive frames
}

void UdsEcuSim::sendNextCf()
{
    if (m_txOffset >= m_txBuf.size()) {
        m_cfTimer->stop();
        m_txBuf.clear();
        m_txOffset = 0;
        return;
    }

    uint8_t data[8] = {};
    data[0] = static_cast<uint8_t>(0x20 | (m_txSn & 0x0F));
    m_txSn = (m_txSn + 1) & 0x0F;

    int remaining = m_txBuf.size() - m_txOffset;
    int toCopy = qMin(7, remaining);
    memcpy(data + 1, m_txBuf.constData() + m_txOffset, toCopy);
    m_txOffset += toCopy;

    auto f = makeFrame(data, toCopy + 1);
    emit frameToSend(f);

    if (m_txOffset >= m_txBuf.size()) {
        m_cfTimer->stop();
        m_txBuf.clear();
        m_txOffset = 0;
    }
}

// ---------------------------------------------------------------------------
// UDS service handlers

QByteArray UdsEcuSim::svc10(const QByteArray& req)
{
    if (req.size() < 2) return nrc(0x10, 0x13);
    uint8_t sessionType = static_cast<uint8_t>(req[1]);
    if (sessionType != 0x01 && sessionType != 0x02 && sessionType != 0x03)
        return nrc(0x10, 0x12);
    m_session = sessionType;
    if (sessionType == 0x01) {
        m_unlocked = false;
        m_seed = 0;
        m_seedRequested = false;
    }
    QByteArray resp;
    resp.append(char(0x50));
    resp.append(char(sessionType));
    resp.append(char(0x00));
    resp.append(char(0x19));
    resp.append(char(0x01));
    resp.append(char(0xF4));
    return resp;
}

QByteArray UdsEcuSim::svc11(const QByteArray& req)
{
    if (req.size() < 2) return nrc(0x11, 0x13);
    uint8_t resetType = static_cast<uint8_t>(req[1]);
    if (resetType != 0x01 && resetType != 0x02 && resetType != 0x03)
        return nrc(0x11, 0x12);
    m_session = 0x01;
    m_unlocked = false;
    m_seed = 0;
    m_seedRequested = false;
    QByteArray resp;
    resp.append(char(0x51));
    resp.append(char(resetType));
    return resp;
}

QByteArray UdsEcuSim::svc14(const QByteArray& req)
{
    (void)req;
    // Clear all DTCs with testFailed status
    for (auto& dtc : m_cfg.dtcs)
        dtc.status &= ~0x01u; // clear testFailed bit
    QByteArray resp;
    resp.append(char(0x54));
    return resp;
}

QByteArray UdsEcuSim::svc19(const QByteArray& req)
{
    if (req.size() < 2) return nrc(0x19, 0x13);
    uint8_t subfn = static_cast<uint8_t>(req[1]);

    QByteArray resp;
    if (subfn == 0x01) {
        // reportNumberOfDTCByStatusMask
        resp.append(char(0x59));
        resp.append(char(0x01));
        resp.append(char(0xFF)); // DTCStatusAvailabilityMask
        int count = m_cfg.dtcs.size();
        resp.append(char(0x00));
        resp.append(char((count >> 8) & 0xFF));
        resp.append(char(count & 0xFF));
    } else if (subfn == 0x02) {
        // reportDTCByStatusMask
        uint8_t mask = (req.size() >= 3) ? static_cast<uint8_t>(req[2]) : 0xFF;
        resp.append(char(0x59));
        resp.append(char(0x02));
        resp.append(char(0xFF)); // DTCStatusAvailabilityMask
        for (const auto& dtc : m_cfg.dtcs) {
            if ((dtc.status & mask) != 0) {
                resp.append(char((dtc.code >> 16) & 0xFF));
                resp.append(char((dtc.code >> 8)  & 0xFF));
                resp.append(char( dtc.code        & 0xFF));
                resp.append(char(dtc.status));
            }
        }
    } else if (subfn == 0x0A) {
        // reportSupportedDTC
        resp.append(char(0x59));
        resp.append(char(0x0A));
        resp.append(char(0xFF));
        for (const auto& dtc : m_cfg.dtcs) {
            resp.append(char((dtc.code >> 16) & 0xFF));
            resp.append(char((dtc.code >> 8)  & 0xFF));
            resp.append(char( dtc.code        & 0xFF));
            resp.append(char(dtc.status));
        }
    } else {
        return nrc(0x19, 0x12);
    }
    return resp;
}

QByteArray UdsEcuSim::svc22(const QByteArray& req)
{
    if (req.size() < 3) return nrc(0x22, 0x13);
    // Parse pairs of DID bytes
    QByteArray resp;
    resp.append(char(0x62));
    bool anyNotFound = false;

    for (int i = 1; i + 1 < req.size(); i += 2) {
        uint16_t id = (static_cast<uint8_t>(req[i]) << 8)
                    |  static_cast<uint8_t>(req[i+1]);
        bool found = false;
        for (const auto& did : m_cfg.dids) {
            if (did.id == id) {
                resp.append(char((id >> 8) & 0xFF));
                resp.append(char( id       & 0xFF));
                resp.append(did.value);
                found = true;
                break;
            }
        }
        if (!found) {
            anyNotFound = true;
        }
    }
    if (anyNotFound) return nrc(0x22, 0x31);
    return resp;
}

QByteArray UdsEcuSim::svc27(const QByteArray& req)
{
    if (req.size() < 2) return nrc(0x27, 0x13);
    uint8_t subfn = static_cast<uint8_t>(req[1]);

    if (subfn == 0x01) {
        // requestSeed
        m_seed = QRandomGenerator::global()->generate();
        m_seedRequested = true;
        QByteArray resp;
        resp.append(char(0x67));
        resp.append(char(0x01));
        resp.append(char((m_seed >> 24) & 0xFF));
        resp.append(char((m_seed >> 16) & 0xFF));
        resp.append(char((m_seed >> 8)  & 0xFF));
        resp.append(char( m_seed        & 0xFF));
        return resp;
    } else if (subfn == 0x02) {
        // sendKey
        if (!m_seedRequested) return nrc(0x27, 0x24); // requestSequenceError
        if (req.size() < 6) return nrc(0x27, 0x13);
        uint32_t key = (static_cast<uint8_t>(req[2]) << 24)
                     | (static_cast<uint8_t>(req[3]) << 16)
                     | (static_cast<uint8_t>(req[4]) << 8)
                     |  static_cast<uint8_t>(req[5]);
        uint32_t expected = m_seed ^ m_cfg.seedKey;
        if (key != expected) return nrc(0x27, 0x35); // invalidKey
        m_unlocked = true;
        m_seedRequested = false;
        QByteArray resp;
        resp.append(char(0x67));
        resp.append(char(0x02));
        return resp;
    } else {
        return nrc(0x27, 0x12);
    }
}

QByteArray UdsEcuSim::svc28(const QByteArray& req)
{
    if (req.size() < 2) return nrc(0x28, 0x13);
    uint8_t subfn = static_cast<uint8_t>(req[1]);
    QByteArray resp;
    resp.append(char(0x68));
    resp.append(char(subfn));
    return resp;
}

QByteArray UdsEcuSim::svc2E(const QByteArray& req)
{
    if (req.size() < 4) return nrc(0x2E, 0x13);
    uint16_t id = (static_cast<uint8_t>(req[1]) << 8)
                |  static_cast<uint8_t>(req[2]);
    QByteArray value = req.mid(3);

    for (auto& did : m_cfg.dids) {
        if (did.id == id) {
            did.value = value;
            QByteArray resp;
            resp.append(char(0x6E));
            resp.append(char((id >> 8) & 0xFF));
            resp.append(char( id       & 0xFF));
            return resp;
        }
    }
    return nrc(0x2E, 0x31);
}

QByteArray UdsEcuSim::svc31(const QByteArray& req)
{
    if (req.size() < 4) return nrc(0x31, 0x13);
    uint8_t subfn = static_cast<uint8_t>(req[1]);
    uint8_t ridH  = static_cast<uint8_t>(req[2]);
    uint8_t ridL  = static_cast<uint8_t>(req[3]);
    QByteArray resp;
    resp.append(char(0x71));
    resp.append(char(subfn));
    resp.append(char(ridH));
    resp.append(char(ridL));
    resp.append(char(0x00));
    return resp;
}

QByteArray UdsEcuSim::svc3E(const QByteArray& req)
{
    if (req.size() < 2) return nrc(0x3E, 0x13);
    uint8_t subfn = static_cast<uint8_t>(req[1]);
    bool suppress = (subfn & 0x80) != 0;
    if (suppress) return QByteArray(); // no response
    QByteArray resp;
    resp.append(char(0x7E));
    resp.append(char(subfn & 0x7F));
    return resp;
}

// ---------------------------------------------------------------------------
// Helpers

QByteArray UdsEcuSim::nrc(uint8_t sid, uint8_t code)
{
    QByteArray r;
    r.append(char(0x7F));
    r.append(char(sid));
    r.append(char(code));
    return r;
}

socketspy::core::CanFrame UdsEcuSim::makeFrame(const uint8_t* data, int len) const
{
    socketspy::core::CanFrame f{};
    f.id           = m_cfg.txId;
    f.dlc          = static_cast<uint8_t>(qMin(len, 8));
    f.flags        = 0;
    f.iface_idx    = 255;
    f.timestamp_us = static_cast<uint64_t>(QDateTime::currentMSecsSinceEpoch()) * 1000ULL;
    memcpy(f.data, data, f.dlc);
    return f;
}

} // namespace socketspy::gui

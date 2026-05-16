#include "uds_transport.h"
#include <cerrno>
#include <cstring>
#include <algorithm>

using namespace socketspy::core;

namespace socketspy::gui {

UdsTransport::UdsTransport(QObject* parent)
    : QObject(parent)
    , m_timeoutTimer(new QTimer(this))
    , m_fcTimeoutTimer(new QTimer(this))
    , m_stminTimer(new QTimer(this))
{
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &UdsTransport::onTimeout);

    m_fcTimeoutTimer->setSingleShot(true);
    connect(m_fcTimeoutTimer, &QTimer::timeout, this, &UdsTransport::onFcTimeout);

    m_stminTimer->setSingleShot(true);
    connect(m_stminTimer, &QTimer::timeout, this, &UdsTransport::sendNextCf);
}

void UdsTransport::setInterface(const QString& iface) {
    if (m_capture) {
        m_capture->stop();
        m_capture->wait();
        delete m_capture;
        m_capture = nullptr;
    }
    m_iface = iface;
}

void UdsTransport::setTxId(uint32_t id) { m_txId = id; }
void UdsTransport::setRxId(uint32_t id) { m_rxId = id; }
void UdsTransport::setP2Timeout(int ms)  { m_p2Timeout = ms; }

void UdsTransport::sendRequest(const std::vector<uint8_t>& payload) {
    if (m_iface.isEmpty()) { emit errorOccurred("No interface set"); return; }

    resetRx();

    // Start a capture thread to receive the response
    if (!m_capture) {
        m_capture = new CanCapture(m_iface, this);
        connect(m_capture, &CanCapture::frameReceived,
                this,      &UdsTransport::onFrameReceived);
        m_capture->start();
    }

    sendCanFrame(payload);
    m_waitingResp = true;
    m_timeoutTimer->start(m_p2Timeout);
}

void UdsTransport::sendCanFrame(const std::vector<uint8_t>& payload) {
    IfaceHandle h = can_open(m_iface.toStdString());
    if (!h.valid()) { emit errorOccurred(QString("can_open: %1").arg(strerror(errno))); return; }

    const int len = static_cast<int>(payload.size());

    if (len <= 7) {
        // Single frame (SF): PCI byte = 0x0N where N = len
        CanFrame f{};
        f.id  = m_txId;
        f.dlc = static_cast<uint8_t>(len + 1);
        f.data[0] = static_cast<uint8_t>(len & 0x0F);
        for (int i = 0; i < len; ++i) f.data[i + 1] = payload[i];
        can_send(h, f);
        can_close(h);
    } else {
        // Multi-frame: send First Frame, then wait for Flow Control before sending CFs.
        // Save payload and state for the FC/CF state machine.
        m_txPayload    = payload;
        m_txOffset     = 6;   // first 6 bytes go in the FF
        m_txSN         = 1;
        m_txBsRemaining = 0;
        m_txStmin      = 0;

        // First frame (FF): PCI = 0x1X XX (12-bit length)
        CanFrame f{};
        f.id  = m_txId;
        f.dlc = 8;
        f.data[0] = static_cast<uint8_t>(0x10 | ((len >> 8) & 0x0F));
        f.data[1] = static_cast<uint8_t>(len & 0xFF);
        for (int i = 0; i < 6 && i < len; ++i) f.data[i + 2] = payload[i];
        can_send(h, f);
        can_close(h);

        // Wait for FC from ECU (use P2 timeout)
        m_waitingFc = true;
        m_fcTimeoutTimer->start(m_p2Timeout);
    }
}

void UdsTransport::processFcFrame(const CanFrame& frame) {
    // FC frame: byte0 = 0x3N, byte1 = BS, byte2 = STmin
    uint8_t fcFlag = frame.data[0] & 0x0F;

    if (fcFlag == 0x01) {
        // Wait — restart FC timeout
        m_fcTimeoutTimer->start(m_p2Timeout);
        return;
    }
    if (fcFlag == 0x02) {
        // Overflow — abort TX
        m_waitingFc = false;
        resetTx();
        emit errorOccurred("FC Overflow — ECU buffer full");
        return;
    }
    // ContinueToSend (0x00)
    m_waitingFc = false;
    m_fcTimeoutTimer->stop();

    uint8_t bs    = frame.data[1];
    uint8_t stRaw = frame.data[2];

    // ISO 15765-2 STmin decoding
    if (stRaw <= 0x7F) {
        m_txStmin = static_cast<int>(stRaw); // 0–127 ms
    } else if (stRaw >= 0xF1 && stRaw <= 0xF9) {
        m_txStmin = 1; // 100–900 µs → round up to 1 ms
    } else {
        m_txStmin = 0;
    }

    m_txBsRemaining = (bs == 0) ? -1 : static_cast<int>(bs); // -1 = unlimited

    // Start sending CFs
    sendNextCf();
}

void UdsTransport::sendNextCf() {
    if (m_txPayload.empty()) return;

    const int len = static_cast<int>(m_txPayload.size());

    // Block size countdown: if we've sent BS frames, wait for next FC
    if (m_txBsRemaining == 0) {
        // Need another FC
        m_waitingFc = true;
        m_fcTimeoutTimer->start(m_p2Timeout);
        return;
    }

    IfaceHandle h = can_open(m_iface.toStdString());
    if (!h.valid()) {
        emit errorOccurred(QString("can_open: %1").arg(strerror(errno)));
        resetTx();
        return;
    }

    CanFrame cf{};
    cf.id  = m_txId;
    cf.dlc = 8;
    cf.data[0] = static_cast<uint8_t>(0x20 | (m_txSN & 0x0F));
    ++m_txSN;
    for (int i = 1; i <= 7 && m_txOffset < len; ++i, ++m_txOffset)
        cf.data[i] = m_txPayload[m_txOffset];
    can_send(h, cf);
    can_close(h);

    if (m_txBsRemaining > 0) --m_txBsRemaining;

    if (m_txOffset >= len) {
        // All CFs sent
        resetTx();
        return;
    }

    // Schedule next CF after STmin
    if (m_txStmin > 0) {
        m_stminTimer->start(m_txStmin);
    } else {
        sendNextCf();
    }
}

void UdsTransport::onFcTimeout() {
    if (!m_waitingFc) return;
    m_waitingFc = false;
    resetTx();
    emit errorOccurred("FC timeout — no Flow Control received from ECU");
}

void UdsTransport::sendFlowControl() {
    IfaceHandle h = can_open(m_iface.toStdString());
    if (!h.valid()) return;
    CanFrame f{};
    f.id  = m_txId;
    f.dlc = 3;
    f.data[0] = 0x30; // FC, ContinueToSend
    f.data[1] = 0x00; // block size = 0 (no limit)
    f.data[2] = 0x00; // separation time = 0
    can_send(h, f);
    can_close(h);
}

void UdsTransport::onFrameReceived(CanFrame frame) {
    if (frame.id != m_rxId) return;

    // Check if this is a Flow Control frame for our multi-frame TX
    if (m_waitingFc && frame.dlc >= 3) {
        uint8_t pciType = (frame.data[0] >> 4) & 0x0F;
        if (pciType == 3) {
            processFcFrame(frame);
            return;
        }
    }

    if (!m_waitingResp) return;
    processRxFrame(frame);
}

void UdsTransport::processRxFrame(const CanFrame& frame) {
    if (frame.dlc < 1) return;
    uint8_t pci = frame.data[0];
    uint8_t pciType = (pci >> 4) & 0x0F;

    if (pciType == 0) {
        // Single frame
        int len = pci & 0x0F;
        m_rxBuf.clear();
        for (int i = 1; i <= len && i < frame.dlc; ++i)
            m_rxBuf.push_back(frame.data[i]);
        assembleAndEmit();
    } else if (pciType == 1) {
        // First frame
        m_expectedLen = ((pci & 0x0F) << 8) | frame.data[1];
        m_rxBuf.clear();
        for (int i = 2; i < frame.dlc; ++i) m_rxBuf.push_back(frame.data[i]);
        m_nextSN = 1;
        sendFlowControl();
        m_timeoutTimer->start(m_p2Timeout * 10); // extended timeout for multi-frame
    } else if (pciType == 2) {
        // Consecutive frame
        int sn = pci & 0x0F;
        if (sn != (m_nextSN & 0x0F)) { resetRx(); emit errorOccurred("CF sequence error"); return; }
        ++m_nextSN;
        for (int i = 1; i < frame.dlc && static_cast<int>(m_rxBuf.size()) < m_expectedLen; ++i)
            m_rxBuf.push_back(frame.data[i]);
        if (static_cast<int>(m_rxBuf.size()) >= m_expectedLen) {
            m_rxBuf.resize(static_cast<size_t>(m_expectedLen));
            assembleAndEmit();
        }
    }
}

void UdsTransport::assembleAndEmit() {
    m_timeoutTimer->stop();
    m_waitingResp = false;
    emit responseReceived(m_rxBuf);
    resetRx();
}

void UdsTransport::onTimeout() {
    if (!m_waitingResp) return;
    resetRx();
    emit errorOccurred("P2 timeout — no response");
}

void UdsTransport::resetRx() {
    m_waitingResp  = false;
    m_expectedLen  = 0;
    m_nextSN       = 1;
    m_rxBuf.clear();
}

void UdsTransport::resetTx() {
    m_waitingFc     = false;
    m_txPayload.clear();
    m_txOffset      = 6;
    m_txSN          = 1;
    m_txBsRemaining = 0;
    m_txStmin       = 0;
    m_fcTimeoutTimer->stop();
    m_stminTimer->stop();
}

} // namespace socketspy::gui

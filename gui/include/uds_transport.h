#pragma once
#include <QObject>
#include <QString>
#include <QTimer>
#include <vector>
#include <cstdint>
#include "cancore.h"
#include "can_capture.h"

namespace socketspy::gui {

// ISO 15765-2 transport protocol (CAN TP) + ISO 14229 UDS request/response
// Supports: Single Frame, First Frame, Consecutive Frame, Flow Control
class UdsTransport : public QObject {
    Q_OBJECT

public:
    explicit UdsTransport(QObject* parent = nullptr);

    void setInterface(const QString& iface);
    void setTxId(uint32_t id);
    void setRxId(uint32_t id);
    void setP2Timeout(int ms);

    // Send a UDS request (raw service bytes) and await response.
    // Emits responseReceived() or errorOccurred().
    void sendRequest(const std::vector<uint8_t>& data);

signals:
    void responseReceived(std::vector<uint8_t> data);
    void errorOccurred(QString message);

private slots:
    void onFrameReceived(socketspy::core::CanFrame frame);
    void onTimeout();
    void onFcTimeout();
    void sendNextCf();

private:
    void sendCanFrame(const std::vector<uint8_t>& payload);
    void sendFlowControl();
    void processRxFrame(const socketspy::core::CanFrame& frame);
    void processFcFrame(const socketspy::core::CanFrame& frame);
    void assembleAndEmit();
    void resetRx();
    void resetTx();

    QString  m_iface;
    uint32_t m_txId{0x7DF};
    uint32_t m_rxId{0x7E8};
    int      m_p2Timeout{50};  // ms

    CanCapture* m_capture{nullptr};
    QTimer*     m_timeoutTimer{nullptr};
    QTimer*     m_fcTimeoutTimer{nullptr};  // wait for FC after FF
    QTimer*     m_stminTimer{nullptr};      // STmin gap between CFs

    // Reassembly state (RX)
    bool                 m_waitingResp{false};
    int                  m_expectedLen{0};
    int                  m_nextSN{1};
    std::vector<uint8_t> m_rxBuf;

    // Multi-frame TX state
    bool                 m_waitingFc{false};     // true while waiting for FC after FF
    std::vector<uint8_t> m_txPayload;            // full payload being sent
    int                  m_txOffset{6};          // bytes of payload already sent (FF carries first 6)
    uint8_t              m_txSN{1};              // consecutive frame sequence number
    int                  m_txBsRemaining{0};     // block size countdown (0 = unlimited)
    int                  m_txStmin{0};           // STmin in ms
};

} // namespace socketspy::gui

#pragma once
#include <QObject>
#include <QList>
#include <QByteArray>
#include <QTimer>
#include <QString>
#include <cstdint>
#include "cancore.h"

namespace socketspy::gui {

class UdsEcuSim : public QObject {
    Q_OBJECT
public:
    struct Did {
        uint16_t   id{0};
        QString    name;
        QByteArray value;
    };
    struct Dtc {
        uint32_t code{0};      // lower 24 bits = 3-byte DTC
        uint8_t  status{0x09}; // testFailed | confirmedDTC
    };
    struct Config {
        QString  name{"ECU"};
        uint32_t rxId{0x7E0};
        uint32_t txId{0x7E8};
        uint32_t funcId{0x7DF};
        bool     enabled{true};
        uint32_t seedKey{0xC0FFEE};
        QList<Did> dids;
        QList<Dtc> dtcs;
    };

    explicit UdsEcuSim(Config cfg, QObject* parent = nullptr);

    const Config& config() const { return m_cfg; }
    void setConfig(Config cfg);

    void onFrameReceived(const socketspy::core::CanFrame& frame);

signals:
    void frameToSend(const socketspy::core::CanFrame& frame);
    void logMessage(const QString& msg);

private:
    void handleIsoTpFrame(const socketspy::core::CanFrame& f);
    void rxSingleFrame(const uint8_t* d, int dlc);
    void rxFirstFrame(const uint8_t* d);
    void rxConsecutiveFrame(const uint8_t* d);
    void sendFlowControl();

    void dispatchUds(const QByteArray& pdu);
    void sendUds(const QByteArray& pdu);
    void txSingleFrame(const QByteArray& pdu);
    void txFirstFrame(const QByteArray& pdu);
    void sendNextCf();

    QByteArray svc10(const QByteArray& req);
    QByteArray svc11(const QByteArray& req);
    QByteArray svc14(const QByteArray& req);
    QByteArray svc19(const QByteArray& req);
    QByteArray svc22(const QByteArray& req);
    QByteArray svc27(const QByteArray& req);
    QByteArray svc28(const QByteArray& req);
    QByteArray svc2E(const QByteArray& req);
    QByteArray svc31(const QByteArray& req);
    QByteArray svc3E(const QByteArray& req);

    static QByteArray nrc(uint8_t sid, uint8_t code);
    socketspy::core::CanFrame makeFrame(const uint8_t* data, int len) const;

    Config   m_cfg;
    uint8_t  m_session{0x01};
    uint32_t m_seed{0};
    bool     m_unlocked{false};
    bool     m_seedRequested{false};

    // ISO-TP RX reassembly
    QByteArray m_rxBuf;
    int        m_rxExpected{0};
    uint8_t    m_rxNextSn{1};

    // ISO-TP TX multi-frame
    QByteArray m_txBuf;
    int        m_txOffset{0};
    uint8_t    m_txSn{1};
    QTimer*    m_cfTimer{nullptr};
};

} // namespace socketspy::gui

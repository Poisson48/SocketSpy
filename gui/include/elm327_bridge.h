#pragma once
#include <QObject>
#include <QSerialPort>
#include <QTimer>
#include <QString>
#include <QByteArray>
#include "cancore.h"

namespace socketspy::gui {

class Elm327Bridge : public QObject {
    Q_OBJECT

public:
    explicit Elm327Bridge(QObject* parent = nullptr);
    ~Elm327Bridge() override;

    bool open(const QString& port, int baud);
    void close();
    bool sendFrame(const socketspy::core::CanFrame& frame);
    bool isOpen() const;

signals:
    void frameReceived(socketspy::core::CanFrame frame);
    void connectionError(QString message);
    void statusChanged(QString message);

private slots:
    void onReadyRead();
    void onInitStep();

private:
    enum class State { Closed, Initializing, Monitoring, SendingFrame };

    void sendAtCommand(const QString& cmd);
    void processLine(const QString& line);
    socketspy::core::CanFrame parseElm327Line(const QString& line);
    void startMonitor();
    void advanceInit();

    QSerialPort* m_serial{nullptr};
    QTimer*      m_initTimer{nullptr};
    QByteArray   m_readBuf;
    State        m_state{State::Closed};
    int          m_initStep{0};

    // Pending frame to send while pausing ATMA
    uint32_t     m_pendingId{0};
    QByteArray   m_pendingData;
    int          m_sendStep{0};
};

} // namespace socketspy::gui

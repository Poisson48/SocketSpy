#pragma once
#include <QWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QTextEdit>
#include <QThread>
#include <cstdint>
#include "cancore.h"

namespace socketspy::gui {

// ──────────────────────────────────────────────────────────────────────────────
// Worker: sends one XCP command, waits for reply, emits result on main thread
// ──────────────────────────────────────────────────────────────────────────────
class XcpWorker : public QThread {
    Q_OBJECT
public:
    enum class Cmd { Connect, Disconnect, GetStatus, GetId };

    XcpWorker(QString iface, uint32_t txId, uint32_t rxId,
              int timeoutMs, Cmd cmd, QObject* parent = nullptr);

signals:
    void resultReady(QString html);

protected:
    void run() override;

private:
    // helpers
    bool sendAndReceive(socketspy::core::IfaceHandle h,
                        const uint8_t* payload, int dlc,
                        uint8_t* respOut, int& respLen);
    QString handleConnect(socketspy::core::IfaceHandle h);
    QString handleDisconnect(socketspy::core::IfaceHandle h);
    QString handleGetStatus(socketspy::core::IfaceHandle h);
    QString handleGetId(socketspy::core::IfaceHandle h);

    QString  m_iface;
    uint32_t m_txId;
    uint32_t m_rxId;
    int      m_timeoutMs;
    Cmd      m_cmd;
};

// ──────────────────────────────────────────────────────────────────────────────
// XcpPanel — XCP on CAN scanner tab
// ──────────────────────────────────────────────────────────────────────────────
class XcpPanel : public QWidget {
    Q_OBJECT

public:
    explicit XcpPanel(QWidget* parent = nullptr);

private slots:
    void refreshIfaces();
    void onConnect();
    void onDisconnect();
    void onGetStatus();
    void onGetId();
    void onResult(QString html);

private:
    void setupUi();
    void runCommand(XcpWorker::Cmd cmd);
    void setBusy(bool busy);

    QComboBox*   m_iface{nullptr};
    QLineEdit*   m_txId{nullptr};
    QLineEdit*   m_rxId{nullptr};
    QSpinBox*    m_timeout{nullptr};
    QPushButton* m_connectBtn{nullptr};
    QPushButton* m_disconnectBtn{nullptr};
    QPushButton* m_statusBtn{nullptr};
    QPushButton* m_getIdBtn{nullptr};
    QTextEdit*   m_info{nullptr};
};

} // namespace socketspy::gui

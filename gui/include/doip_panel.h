#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QTcpSocket>
#include <QByteArray>

namespace socketspy::gui {

class DoipPanel : public QWidget {
    Q_OBJECT

public:
    explicit DoipPanel(QWidget* parent = nullptr);
    ~DoipPanel() override;

private slots:
    void onConnect();
    void onDisconnect();
    void onSendUds();
    void onConnected();
    void onDisconnectedSlot();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError err);

private:
    void setupUi();
    void setStatus(const QString& text, const QString& color = "#6b7280");
    void appendLog(const QString& line);
    void sendDoip(uint16_t type, const QByteArray& payload);
    void sendRoutingActivation();
    void processResponse(const QByteArray& data);

    // Connection controls
    QLineEdit*   m_ipEdit{nullptr};
    QSpinBox*    m_portSpin{nullptr};
    QPushButton* m_connectBtn{nullptr};
    QPushButton* m_disconnectBtn{nullptr};
    QLabel*      m_statusLabel{nullptr};

    // UDS controls (shown when connected)
    QWidget*     m_udsWidget{nullptr};
    QLineEdit*   m_srcAddrEdit{nullptr};
    QLineEdit*   m_tgtAddrEdit{nullptr};
    QTextEdit*   m_udsPayloadEdit{nullptr};
    QPushButton* m_sendUdsBtn{nullptr};

    // Log
    QTextEdit*   m_log{nullptr};

    // Network
    QTcpSocket*  m_socket{nullptr};
    QByteArray   m_rxBuffer;
    bool         m_routingActive{false};
};

} // namespace socketspy::gui

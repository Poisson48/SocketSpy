#pragma once
#include <QWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QGroupBox>
#include <vector>
#include <cstdint>
#include "uds_transport.h"

namespace socketspy::gui {

class UdsPanel : public QWidget {
    Q_OBJECT

public:
    explicit UdsPanel(QWidget* parent = nullptr);

private slots:
    void onReadDtc();
    void onClearDtc();
    void onReadEcuInfo();
    void onResponseReceived(std::vector<uint8_t> data);
    void onUdsError(QString message);
    void onApplyConfig();
    void refreshIfaces();

private:
    void setupUi();
    void sendRequest(const std::vector<uint8_t>& req, const QString& label);
    void parseAndShowDtcs(const std::vector<uint8_t>& resp);
    void parseAndShowDids(const std::vector<uint8_t>& resp, const QString& label);
    void setStatus(const QString& msg, bool ok = true);

    QComboBox*    m_iface{nullptr};
    QLineEdit*    m_txId{nullptr};
    QLineEdit*    m_rxId{nullptr};
    QSpinBox*     m_p2{nullptr};
    QPushButton*  m_applyBtn{nullptr};
    QPushButton*  m_readDtcBtn{nullptr};
    QPushButton*  m_clearDtcBtn{nullptr};
    QPushButton*  m_ecuInfoBtn{nullptr};
    QLabel*       m_sessionLabel{nullptr};
    QLabel*       m_statusLabel{nullptr};
    QTableWidget* m_table{nullptr};

    UdsTransport* m_transport{nullptr};
    QString       m_pendingLabel;
    int           m_sessionId{1};
};

} // namespace socketspy::gui
